#ifndef OPENDISPLAY_STRUCTS_H
#define OPENDISPLAY_STRUCTS_H

#include <stdbool.h>
#include <stdint.h>

struct SystemConfig {
  uint16_t ic_type;
  uint8_t communication_modes;
  uint8_t device_flags;
  uint8_t pwr_pin;
  uint8_t reserved[17];
} __attribute__((packed));

struct ManufacturerData {
  uint16_t manufacturer_id;
  uint8_t board_type;
  uint8_t board_revision;
  uint16_t simple_config_driver_index;
  uint16_t simple_config_display_index;
  uint16_t simple_config_power_index;
  uint8_t simple_config_configured_at[6];
  uint8_t reserved[6];
} __attribute__((packed));

struct PowerOption {
  uint8_t power_mode;
  uint8_t battery_capacity_mah[3];
  uint16_t sleep_timeout_ms;
  uint8_t tx_power;
  uint8_t sleep_flags;
  uint8_t battery_sense_pin;
  uint8_t battery_sense_enable_pin;
  uint8_t battery_sense_flags;
  uint8_t capacity_estimator;
  uint16_t voltage_scaling_factor;
  uint32_t deep_sleep_current_ua;
  uint16_t deep_sleep_time_seconds;
  uint8_t reserved[10];
} __attribute__((packed));

struct DisplayConfig {
  uint8_t instance_number;
  uint8_t display_technology;
  uint16_t panel_ic_type;
  uint16_t pixel_width;
  uint16_t pixel_height;
  uint16_t active_width_mm;
  uint16_t active_height_mm;
  uint16_t tag_type;
  uint8_t rotation;
  uint8_t reset_pin;
  uint8_t busy_pin;
  uint8_t dc_pin;
  uint8_t cs_pin;
  uint8_t data_pin;
  uint8_t partial_update_support;
  uint8_t color_scheme;
  uint8_t transmission_modes;
  uint8_t clk_pin;
  uint8_t reserved_pin_2;
  uint8_t reserved_pin_3;
  uint8_t reserved_pin_4;
  uint8_t reserved_pin_5;
  uint8_t reserved_pin_6;
  uint8_t reserved_pin_7;
  uint8_t reserved_pin_8;
  uint8_t reserved[15];
} __attribute__((packed));

struct LedConfig {
  uint8_t instance_number;
  uint8_t led_type;
  uint8_t led_1_r;
  uint8_t led_2_g;
  uint8_t led_3_b;
  uint8_t led_4;
  uint8_t led_flags;
  uint8_t reserved[15];
} __attribute__((packed));

struct SensorData {
  uint8_t instance_number;
  uint16_t sensor_type;
  uint8_t bus_id;
  uint8_t reserved[26];
} __attribute__((packed));

struct DataBus {
  uint8_t instance_number;
  uint8_t bus_type;
  uint8_t pin_1;
  uint8_t pin_2;
  uint8_t pin_3;
  uint8_t pin_4;
  uint8_t pin_5;
  uint8_t pin_6;
  uint8_t pin_7;
  uint32_t bus_speed_hz;
  uint8_t bus_flags;
  uint8_t pullups;
  uint8_t pulldowns;
  uint8_t reserved[14];
} __attribute__((packed));

struct BinaryInputs {
  uint8_t instance_number;
  uint8_t input_type;
  uint8_t display_as;
  uint8_t reserved_pin_1;
  uint8_t reserved_pin_2;
  uint8_t reserved_pin_3;
  uint8_t reserved_pin_4;
  uint8_t reserved_pin_5;
  uint8_t reserved_pin_6;
  uint8_t reserved_pin_7;
  uint8_t reserved_pin_8;
  uint8_t input_flags;
  uint8_t invert;
  uint8_t pullups;
  uint8_t pulldowns;
  uint8_t button_data_byte_index;
  uint8_t reserved[14];
} __attribute__((packed));

/* 0x28: touch_controller (repeatable, max 4 instances). 32-byte fixed packet;
 * on-wire layout matches OpenDisplay-Firmware src/structs.h TouchController and
 * the py-opendisplay serializer (config_serializer.serialize_touch_controller).
 * touch_ic_type: 0 = disabled / none, 1 = GT911. */
#define TOUCH_IC_NONE   0u
#define TOUCH_IC_GT911  1u
#define TOUCH_FLAG_INVERT_X  (1u << 0)
#define TOUCH_FLAG_INVERT_Y  (1u << 1)
#define TOUCH_FLAG_SWAP_XY   (1u << 2)

struct TouchController {
  uint8_t instance_number;
  uint16_t touch_ic_type;
  uint8_t bus_id;             /* data_bus index for the I2C SCL/SDA pins */
  uint8_t i2c_addr_7bit;      /* GT911: 0x5D or 0x14; 0 or 0xFF = auto */
  uint8_t int_pin;            /* GT911 INT, 0xFF = poll only */
  uint8_t rst_pin;            /* GT911 RST, 0xFF = skip hardware reset */
  uint8_t display_instance;   /* clip/scale to displays[instance] pixel size */
  uint8_t flags;              /* TOUCH_FLAG_* */
  uint8_t poll_interval_ms;   /* 0 = default */
  uint8_t touch_data_start_byte; /* first of 5 MSD dynamic bytes (0-6) */
  uint8_t enable_pin;         /* optional touch panel power enable; 0/0xFF = unused */
  uint8_t reserved[20];
} __attribute__((packed));

struct NfcConfig {
  uint8_t instance_number;
  uint8_t nfc_ic_type;
  uint8_t bus_instance;
  uint8_t flags;
  uint8_t field_detect_pin;
  uint8_t field_detect_mode;
  uint8_t field_detect_active;
  uint8_t field_detect_debounce_ms;
  uint8_t power_pin;
  uint8_t power_active;
  uint8_t power_on_delay_ms;
  uint8_t power_off_delay_ms;
  uint8_t adv_button_byte_index;
  uint8_t adv_button_button_id;
  uint8_t reserved_pin_1;
  uint8_t reserved_pin_2;
  uint8_t reserved[16];
} __attribute__((packed));

#define FLASH_CONFIG_FLAG_ENABLED (1u << 0)

struct FlashConfig {
  uint8_t instance_number;
  uint8_t flash_ic_type;
  uint8_t bus_instance;
  uint8_t flags;
  uint8_t mosi_pin;
  uint8_t sck_pin;
  uint8_t cs_pin;
  uint8_t power_pin;
  uint8_t power_active;
  uint8_t power_on_delay_ms;
  uint8_t power_off_delay_ms;
  uint8_t mode;
  uint8_t reserved[20];
} __attribute__((packed));

struct DataExtended {
  uint8_t manufacturer_name[32];
  uint8_t model_name[32];
  uint8_t serial_number[32];
  uint8_t friendly_name[32];
  uint8_t device_location[32];
  uint8_t device_id[32];
  uint8_t custom_string_1[32];
  uint8_t custom_string_2[32];
  uint8_t custom_string_3[32];
} __attribute__((packed));

struct GlobalConfig {
  struct SystemConfig system_config;
  struct ManufacturerData manufacturer_data;
  struct PowerOption power_option;
  struct DisplayConfig displays[4];
  uint8_t display_count;
  struct LedConfig leds[4];
  uint8_t led_count;
  struct SensorData sensors[4];
  uint8_t sensor_count;
  struct DataBus data_buses[4];
  uint8_t data_bus_count;
  struct BinaryInputs binary_inputs[4];
  uint8_t binary_input_count;
  struct TouchController touch_controllers[4];
  uint8_t touch_controller_count;
  struct NfcConfig nfc_configs[2];
  uint8_t nfc_config_count;
  struct FlashConfig flash_configs[2];
  uint8_t flash_config_count;
  struct DataExtended data_extended;
  bool data_extended_loaded;
  uint8_t version;
  uint8_t minor_version;
  bool loaded;
};

struct SecurityConfig {
  uint8_t encryption_enabled;
  uint8_t encryption_key[16];
  uint16_t session_timeout_seconds;
  uint8_t flags;
  uint8_t reset_pin;
  uint8_t reserved[43];
} __attribute__((packed));

struct EncryptionSession {
  bool authenticated;
  uint8_t session_key[16];
  uint8_t session_id[8];
  uint64_t nonce_counter;
  uint64_t last_seen_counter;
  uint64_t replay_window[64];
  uint8_t replay_idx;
  uint32_t last_activity_ms;
  uint8_t integrity_failures;
  uint32_t session_start_ms;
  uint8_t auth_attempts;
  uint32_t last_auth_time_ms;
  uint8_t client_nonce[16];
  uint8_t server_nonce[16];
  uint8_t pending_server_nonce[16];
  uint32_t server_nonce_time_ms;
};

#define SECURITY_FLAG_REWRITE_ALLOWED     (1 << 0)
#define SECURITY_FLAG_SHOW_KEY_ON_SCREEN  (1 << 1)
#define SECURITY_FLAG_RESET_PIN_ENABLED   (1 << 2)
#define SECURITY_FLAG_RESET_PIN_POLARITY  (1 << 3)
#define SECURITY_FLAG_RESET_PIN_PULLUP    (1 << 4)
#define SECURITY_FLAG_RESET_PIN_PULLDOWN  (1 << 5)

#endif
