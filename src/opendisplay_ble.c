#include "opendisplay_ble.h"
#include "opendisplay_config_parser.h"
#include "opendisplay_config_storage.h"
#include "opendisplay_constants.h"
#include "opendisplay_display.h"
#include "opendisplay_button.h"
#include "opendisplay_led.h"
#include "opendisplay_touch.h"
#include "opendisplay_buzzer.h"
#include "opendisplay_pipe.h"
#include "opendisplay_battery.h"
#include "opendisplay_sensor_sht40.h"
#include "opendisplay_sensor_bq27220.h"
#include "board_nrf54.h"

#include <stdio.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#define OPENDISPLAY_COMPANY_ID 0x2446u
#define MSD_PAYLOAD_LEN        16u
#define OD_NAME_PREFIX         "OD"
#ifndef OD_APP_VERSION
#define OD_APP_VERSION         0x0100u
#endif

/* BLE adv interval units are 0.625 ms (same as nRF52840 Firmware). Matches the
 * reference nRF window (NRF_ADV_INTERVAL_MIN/MAX, ble_init.cpp:48-49): a 160 ms
 * floor for faster discovery up to a 1000 ms ceiling; the controller picks a
 * value within the window. */
#define OD_ADV_INTERVAL_MIN       256u                   /* 160 ms */
#define OD_ADV_INTERVAL_MAX       BT_GAP_ADV_SLOW_INT_MIN /* 1600 = 1000 ms (~1 adv/s) */
#define OD_ADV_BOOST_INTERVAL_MIN 32u   /* 20 ms */
#define OD_ADV_BOOST_INTERVAL_MAX 48u   /* 30 ms */
#define OD_ADV_BOOST_MS           3000u

static struct GlobalConfig s_od_global_config;
static uint8_t msd_payload[MSD_PAYLOAD_LEN];
static uint8_t dynamic_return[11];
static char s_dev_name[16];
static struct bt_conn *s_conn;
static bool s_notify_enabled;
static bool s_adv_active;
static uint32_t s_adv_boost_until_ms;
static uint8_t s_msd_loop_counter;
static uint32_t s_last_adv_retry_ms;
static uint8_t s_reboot_flag = 1; /* set after boot, cleared on first BLE connect */
static uint8_t s_connection_requested; /* MSD status bit2; see opendisplay_ble_set_connection_requested */
static uint8_t s_last_published_msd[MSD_PAYLOAD_LEN];
static bool s_msd_published;
static bool s_adv_was_boosted;
static struct bt_le_adv_param s_adv_param = BT_LE_ADV_PARAM_INIT(
	BT_LE_ADV_OPT_CONN,
	OD_ADV_INTERVAL_MIN, OD_ADV_INTERVAL_MAX, NULL);

static struct k_work_delayable s_adv_restart_work;
static bool s_adv_work_msd_publish;

static int start_advertising(void);
static bool publish_msd_to_advertising(void);
static void apply_tx_power(uint8_t handle_type, uint16_t handle);

static void adv_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	if (s_conn != NULL) {
		s_adv_work_msd_publish = false;
		return;
	}
	if (s_adv_work_msd_publish) {
		s_adv_work_msd_publish = false;
		(void)publish_msd_to_advertising();
		return;
	}
	int err = start_advertising();
	if (err != 0) {
		printf("[OD] adv restart retry (err %d)\r\n", err);
		(void)k_work_schedule(&s_adv_restart_work, K_MSEC(200));
	}
}

static void schedule_adv_restart(uint32_t delay_ms)
{
	s_adv_work_msd_publish = false;
	(void)k_work_cancel_delayable(&s_adv_restart_work);
	(void)k_work_schedule(&s_adv_restart_work, K_MSEC(delay_ms));
}

static void schedule_msd_publish(void)
{
	s_adv_work_msd_publish = true;
	(void)k_work_cancel_delayable(&s_adv_restart_work);
	(void)k_work_schedule(&s_adv_restart_work, K_NO_WAIT);
}

static void chip_id_hex6(char out[7])
{
	uint8_t id[8];
	uint64_t uid = 0;

	(void)hwinfo_get_device_id(id, sizeof(id));
	for (unsigned i = 0; i < sizeof(id); i++) {
		uid = (uid << 8) | id[i];
	}
	snprintf(out, 7, "%06lX", (unsigned long)(uid & 0xFFFFFFu));
}

static float s_chip_temperature = -999.0f;

/*
 * The nrf temp driver's sample_fetch blocks on the TEMP interrupt with
 * K_FOREVER. On this nRF54L15 port that interrupt stops arriving once the
 * BLE controller is running, permanently hanging the calling thread (main
 * loop / system workqueue). Read the die temperature once before bt_enable()
 * and reuse the cached value in the MSD payload.
 */
static void read_chip_temperature_once(void)
{
#if DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_temp)
	const struct device *temp_dev = DEVICE_DT_GET(DT_NODELABEL(temp));
	struct sensor_value val;

	if (device_is_ready(temp_dev) && sensor_sample_fetch(temp_dev) == 0 &&
	    sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, &val) == 0) {
		s_chip_temperature = (float)sensor_value_to_double(&val);
	}
#endif
}

static void update_msd_payload(void)
{
	uint16_t battery_voltage_10mv;
	int16_t temp_encoded;
	uint8_t temperature_byte;
	uint8_t battery_voltage_low_byte;
	uint8_t status_byte;

	/* Mirror the reference updatemsdata() ordering: refresh the sensor
	 * dynamic slots, then the battery source (BQ27220-preferred, else SAADC),
	 * before packing the frame. All three are TTL-cached (30 s). */
	opendisplay_sensor_sht40_poll();
	opendisplay_sensor_bq27220_poll();
	battery_voltage_10mv = opendisplay_battery_get_10mv();

	temp_encoded = (int16_t)((s_chip_temperature + 40.0f) * 2.0f);
	if (temp_encoded < 0) {
		temp_encoded = 0;
	} else if (temp_encoded > 255) {
		temp_encoded = 255;
	}
	temperature_byte = (uint8_t)temp_encoded;
	battery_voltage_low_byte = (uint8_t)(battery_voltage_10mv & 0xFFu);
	/* Matches nRF52840 Firmware status byte (display_service.cpp:1293-1297):
	 * bit0 battery high bit, bit1 rebootFlag, bit2 connectionRequested,
	 * bits 4-7 loop counter. */
	status_byte = (uint8_t)(((battery_voltage_10mv >> 8) & 0x01u) |
				((s_reboot_flag & 0x01u) << 1) |
				((s_connection_requested & 0x01u) << 2) |
				((s_msd_loop_counter & 0x0Fu) << 4));

	memset(msd_payload, 0, sizeof(msd_payload));
	msd_payload[0] = (uint8_t)(OPENDISPLAY_COMPANY_ID & 0xFFu);
	msd_payload[1] = (uint8_t)((OPENDISPLAY_COMPANY_ID >> 8) & 0xFFu);
	memcpy(&msd_payload[2], dynamic_return, sizeof(dynamic_return));
	msd_payload[13] = temperature_byte;
	msd_payload[14] = battery_voltage_low_byte;
	msd_payload[15] = status_byte;
	s_msd_loop_counter = (uint8_t)((s_msd_loop_counter + 1u) & 0x0Fu);
}

static void log_msd(const char *tag)
{
	printf("[OD] msd %s:", tag);
	for (unsigned i = 0; i < MSD_PAYLOAD_LEN; i++) {
		printf(" %02X", msd_payload[i]);
	}
	printf("\r\n");
}

static ssize_t od_gatt_write(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	if (len == 0) {
		return 0;
	}
	opendisplay_pipe_on_write(buf, len, (flags & BT_GATT_WRITE_FLAG_CMD) != 0);
	return len;
}

static void od_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	s_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	opendisplay_pipe_on_notify_changed(s_notify_enabled);
}

BT_GATT_SERVICE_DEFINE(
	od_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0x2446)),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x2446),
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP |
				       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_WRITE, NULL, od_gatt_write, NULL),
	BT_GATT_CCC(od_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, msd_payload, MSD_PAYLOAD_LEN),
};

static struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, s_dev_name, 0),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0x46, 0x24),
};

static bool publish_msd_to_advertising(void)
{
	if (s_conn != NULL) {
		return false;
	}
	if (s_msd_published && memcmp(s_last_published_msd, msd_payload, MSD_PAYLOAD_LEN) == 0) {
		return false;
	}
	memcpy(s_last_published_msd, msd_payload, MSD_PAYLOAD_LEN);
	s_msd_published = true;
	log_msd("publish");

	if (s_adv_active) {
		if (bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd)) != 0) {
			return start_advertising() == 0;
		}
		return true;
	}
	return start_advertising() == 0;
}

void opendisplay_ble_update_msd(bool refresh_advertising)
{
	update_msd_payload();
	if (!refresh_advertising) {
		return;
	}
	/* Matches ESP32 Firmware: skip redundant adv refresh when payload unchanged. */
	if (s_msd_published && memcmp(s_last_published_msd, msd_payload, MSD_PAYLOAD_LEN) == 0) {
		return;
	}
	schedule_msd_publish();
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err != 0) {
		printf("[OD] connect failed: %u\r\n", (unsigned)err);
		schedule_adv_restart(150);
		return;
	}
	(void)k_work_cancel_delayable(&s_adv_restart_work);
	s_conn = bt_conn_ref(conn);
	s_adv_active = false;
	s_reboot_flag = 0;
#if defined(CONFIG_BT_HCI_VS)
	{
		uint16_t conn_handle = 0;

		if (bt_hci_get_conn_handle(conn, &conn_handle) == 0) {
			apply_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, conn_handle);
		}
	}
#endif
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	ARG_UNUSED(conn);
	printf("[OD] disconnected reason=%u\r\n", (unsigned)reason);
	opendisplay_pipe_on_connection_closed();
	if (s_conn != NULL) {
		bt_conn_unref(s_conn);
		s_conn = NULL;
	}
	s_adv_active = false;
	schedule_adv_restart(150);
}

static void recycled(void)
{
	/* Runs in ISR-like context: only queue work, no BT API calls here. */
	if (s_conn == NULL) {
		(void)k_work_schedule(&s_adv_restart_work, K_NO_WAIT);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled,
};

const struct GlobalConfig *opendisplay_get_global_config(void)
{
	return &s_od_global_config;
}

uint16_t opendisplay_ble_get_app_version(void)
{
	return OD_APP_VERSION;
}

float opendisplay_ble_get_chip_temperature(void)
{
	return s_chip_temperature;
}

void opendisplay_ble_copy_msd_bytes(uint8_t out[16])
{
	log_msd("read 0x0044");
	memcpy(out, msd_payload, 16);
}

bool opendisplay_ble_pipe_notify(const uint8_t *data, uint16_t len)
{
	if (!s_notify_enabled || s_conn == NULL || len == 0) {
		return false;
	}
	return bt_gatt_notify(s_conn, &od_svc.attrs[2], data, len) == 0;
}

bool opendisplay_ble_pipe_notify_enabled(void)
{
	return s_notify_enabled;
}

void opendisplay_ble_pipe_on_write(const uint8_t *data, uint16_t len, bool write_cmd)
{
	opendisplay_pipe_on_write(data, len, write_cmd);
}

void opendisplay_ble_pipe_on_connection_closed(void)
{
	opendisplay_pipe_on_connection_closed();
}

void opendisplay_ble_set_connection_requested(bool requested)
{
	s_connection_requested = requested ? 1u : 0u;
}

void opendisplay_ble_set_dynamic_byte(uint8_t index, uint8_t value)
{
	if (index < sizeof(dynamic_return)) {
		dynamic_return[index] = value;
	}
}

/*
 * Apply the configured TX power (power_option.tx_power, dBm as a signed int8).
 * The reference nRF52840 build calls Bluefruit.setTxPower(power_option.tx_power)
 * once at init (ble_init.cpp:90). On Zephyr with the SoftDevice Controller there
 * is no stable bt_le_* runtime API for this, so use the standard HCI vendor-
 * specific Write_Tx_Power_Level command (as in Zephyr's hci_pwr_ctrl sample):
 * the controller clamps the requested value to its supported set and returns the
 * value it actually selected, which we log. handle_type selects advertising vs a
 * specific connection.
 */
static void apply_tx_power(uint8_t handle_type, uint16_t handle)
{
#if defined(CONFIG_BT_HCI_VS)
	int8_t requested = (int8_t)s_od_global_config.power_option.tx_power;
	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct bt_hci_rp_vs_write_tx_power_level *rp;
	struct net_buf *buf;
	struct net_buf *rsp = NULL;
	int err;

	buf = bt_hci_cmd_alloc(K_FOREVER);
	if (buf == NULL) {
		printf("[OD] tx_power: no HCI cmd buffer\r\n");
		return;
	}
	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;
	cp->tx_power_level = requested;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
	if (err != 0) {
		printf("[OD] tx_power set failed (type=%u req=%d dBm): %d\r\n",
		       (unsigned)handle_type, (int)requested, err);
		return;
	}
	rp = (struct bt_hci_rp_vs_write_tx_power_level *)rsp->data;
	printf("[OD] tx_power type=%u requested=%d selected=%d dBm\r\n",
	       (unsigned)handle_type, (int)requested, (int)rp->selected_tx_power);
	net_buf_unref(rsp);
#else
	ARG_UNUSED(handle_type);
	ARG_UNUSED(handle);
	printf("[OD] tx_power: CONFIG_BT_HCI_VS disabled; not applied\r\n");
#endif
}

static void apply_adv_interval(void)
{
	uint32_t now = k_uptime_get_32();

	if (s_adv_boost_until_ms != 0u && now < s_adv_boost_until_ms) {
		s_adv_param.interval_min = OD_ADV_BOOST_INTERVAL_MIN;
		s_adv_param.interval_max = OD_ADV_BOOST_INTERVAL_MAX;
	} else {
		s_adv_boost_until_ms = 0;
		s_adv_param.interval_min = OD_ADV_INTERVAL_MIN;
		s_adv_param.interval_max = OD_ADV_INTERVAL_MAX;
	}
}

static int start_advertising(void)
{
	char hex[7];
	int err;

	if (s_conn != NULL) {
		return 0;
	}

	chip_id_hex6(hex);
	snprintf(s_dev_name, sizeof(s_dev_name), "%s%s", OD_NAME_PREFIX, hex);
	sd[0].data_len = strlen(s_dev_name);
	/* MSD payload is updated only via opendisplay_ble_update_msd(), not on every
	 * adv stop/start (disconnect restart, boost end, retry fallback). */

	apply_adv_interval();
	(void)bt_le_adv_stop();
	err = bt_le_adv_start(&s_adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	s_adv_active = (err == 0);
	if (err != 0) {
		printf("[OD] adv start failed: %d (will retry)\r\n", err);
	} else {
		if (!s_msd_published) {
			memcpy(s_last_published_msd, msd_payload, MSD_PAYLOAD_LEN);
			s_msd_published = true;
		}
		printf("[OD] advertising as %s (interval=%u ms)\r\n", s_dev_name,
		       (unsigned)BT_GAP_ADV_INTERVAL_TO_MS(s_adv_param.interval_max));
	}
	return err;
}

/* Put a configured external SPI NOR flash into deep power-down after every
 * config load (matches nRF52840 Firmware powerDownExternalFlashFromConfig). */
static void flash_powerdown_from_config(void)
{
	if (!s_od_global_config.loaded) {
		return;
	}
	for (uint8_t i = 0; i < s_od_global_config.flash_config_count; i++) {
		const struct FlashConfig *fc = &s_od_global_config.flash_configs[i];

		if ((fc->flags & FLASH_CONFIG_FLAG_ENABLED) == 0u) {
			continue;
		}
		if (fc->mosi_pin == 0xFFu || fc->sck_pin == 0xFFu || fc->cs_pin == 0xFFu) {
			continue;
		}
		printf("[OD] flash powerdown MOSI=%u SCK=%u CS=%u\r\n",
		       fc->mosi_pin, fc->sck_pin, fc->cs_pin);
		board_nrf54_flash_powerdown(fc->mosi_pin, fc->sck_pin, fc->cs_pin);
		break;
	}
}

void opendisplay_ble_reload_config_from_nvm(void)
{
	if (!loadGlobalConfig(&s_od_global_config)) {
		memset(&s_od_global_config, 0, sizeof(s_od_global_config));
	}
	flash_powerdown_from_config();
	/* Re-apply advertising TX power in case the new config changed it. */
	apply_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0);
}

void opendisplay_ble_restart_advertising(void)
{
	schedule_adv_restart(0);
}

void opendisplay_ble_boost_advertising(void)
{
	s_adv_boost_until_ms = k_uptime_get_32() + OD_ADV_BOOST_MS;
	if (s_conn == NULL) {
		schedule_adv_restart(0);
	}
}

void opendisplay_ble_advertising_tick(void)
{
	uint32_t now = k_uptime_get_32();
	const bool boosting = (s_adv_boost_until_ms != 0u && now < s_adv_boost_until_ms);

	if (boosting) {
		s_adv_was_boosted = true;
		return;
	}
	if (!s_adv_was_boosted || s_conn != NULL || !s_adv_active) {
		s_adv_was_boosted = false;
		s_adv_boost_until_ms = 0;
		return;
	}
	s_adv_was_boosted = false;
	s_adv_boost_until_ms = 0;
	schedule_adv_restart(0);
}

void opendisplay_ble_init(void)
{
	int err;

	(void)initConfigStorage();
	if (loadGlobalConfig(&s_od_global_config)) {
		printf("[OD] config loaded: displays=%u\r\n",
		       (unsigned)s_od_global_config.display_count);
	} else {
		printf("[OD] config: defaults\r\n");
	}
	flash_powerdown_from_config();

	read_chip_temperature_once();

	opendisplay_sensor_bq27220_init();
	opendisplay_sensor_sht40_init();

	opendisplay_led_init();
	opendisplay_buzzer_init();
	opendisplay_display_boot_apply();

	err = bt_enable(NULL);
	if (err != 0) {
		printf("[OD] bt_enable failed: %d\r\n", err);
		return;
	}
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		(void)settings_load();
	}

	opendisplay_button_init();
	opendisplay_touch_init();
	k_work_init_delayable(&s_adv_restart_work, adv_work_handler);
	apply_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0);
	update_msd_payload();
	(void)start_advertising();
	printf("[OD] BLE ready as %s\r\n", s_dev_name);
}

void opendisplay_ble_process(void)
{
	uint32_t now = k_uptime_get_32();

	opendisplay_pipe_process();
	opendisplay_led_process();
	opendisplay_buzzer_process();
	opendisplay_button_process();
	opendisplay_touch_process();
	opendisplay_ble_advertising_tick();

	/* Fallback if the delayed work restart fails or was cancelled. */
	if (s_conn == NULL && !s_adv_active && (now - s_last_adv_retry_ms) >= 500u) {
		s_last_adv_retry_ms = now;
		schedule_adv_restart(0);
	}
}

void opendisplay_ble_schedule_dfu(void)
{
	printf("[OD] DFU not implemented on nRF54 yet\r\n");
}

void opendisplay_ble_schedule_deep_sleep(void)
{
	printf("[OD] deep sleep not implemented on nRF54 yet\r\n");
}

bool opendisplay_ble_nfc_read(uint8_t *type_out, uint8_t *data_out, uint16_t *data_len_io,
			      uint16_t max_len)
{
	ARG_UNUSED(type_out);
	ARG_UNUSED(data_out);
	ARG_UNUSED(data_len_io);
	ARG_UNUSED(max_len);
	return false;
}

bool opendisplay_ble_nfc_write(uint8_t type, const uint8_t *data, uint16_t data_len)
{
	ARG_UNUSED(type);
	ARG_UNUSED(data);
	ARG_UNUSED(data_len);
	return false;
}

bool opendisplay_ble_is_connected(void)
{
	return s_conn != NULL;
}
