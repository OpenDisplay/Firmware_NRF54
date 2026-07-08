#include "board_nrf54.h"
#include "opendisplay_ble.h"
#include "opendisplay_config_parser.h"
#include "opendisplay_display.h"
#include "opendisplay_structs.h"

#include <stdio.h>
#include <zephyr/kernel.h>

static void idle_delay_ms(uint32_t delay_ms)
{
	const uint32_t chunk_ms = 100u;
	uint32_t remaining = delay_ms;

	while (remaining > 0u) {
		uint32_t step = (remaining > chunk_ms) ? chunk_ms : remaining;

		opendisplay_ble_process();
		k_msleep(step);
		remaining -= step;
	}
}

int main(void)
{
	const struct GlobalConfig *cfg;
	uint32_t ticks = 0;

	printf("OpenDisplay nRF54 starting\r\n");
	board_nrf54_early_init();
	board_nrf54_prepare_epd_rail();
	opendisplay_ble_init();

	while (1) {
		cfg = opendisplay_get_global_config();

		if (opendisplay_ble_is_connected()) {
			opendisplay_ble_process();
			if ((ticks++ % 100u) == 0u) {
				printf("OpenDisplay alive uptime=%u ms\r\n", k_uptime_get_32());
			}
			k_msleep(10);
			continue;
		}

		/* Matches nRF52840 Firmware: MSD refreshes once per sleep_timeout_ms
		 * idle cycle; without a configured timeout there is no periodic MSD
		 * update (buttons and adv restarts still refresh it). */
		if (cfg != NULL && cfg->loaded && cfg->power_option.sleep_timeout_ms > 0u) {
			idle_delay_ms(cfg->power_option.sleep_timeout_ms);
			opendisplay_ble_update_msd(true);
		} else {
			idle_delay_ms(500u);
		}

		if ((ticks++ % 10u) == 0u) {
			printf("OpenDisplay alive uptime=%u ms\r\n", k_uptime_get_32());
		}
	}
	return 0;
}
