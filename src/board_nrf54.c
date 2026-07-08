#include "board_nrf54.h"
#include "nrf54_gpio.h"

#include <zephyr/kernel.h>

#if defined(NRF54_BOARD_LM20)

/* P1.13 — nPM1300 ship-mode / hold (see XIAO nRF54LM20 schematic). */
#define NRF54LM20_SHPHLD 0x1Du

void board_nrf54_early_init(void)
{
	nrf54_gpio_configure_output(NRF54LM20_SHPHLD, true);
	k_msleep(10);
}

void board_nrf54_prepare_epd_rail(void)
{
	k_msleep(50);
}

#else

#define NRF54L15_RFSW_PWR  0x23u /* P2.3 — RF switch power (keep high) */
#define NRF54L15_RFSW_SEL  0x25u /* P2.5 — RF switch select: low=ceramic, high=external */
#define NRF54L15_BS_PIN    0x2Au

void board_nrf54_early_init(void)
{
	/* Seeed wiki: rfsw-ctl low selects the onboard ceramic antenna. */
	nrf54_gpio_configure_output(NRF54L15_RFSW_PWR, true);
	nrf54_gpio_configure_output(NRF54L15_RFSW_SEL, false);
	nrf54_gpio_configure_output(NRF54L15_BS_PIN, false);
	k_msleep(10);
}

void board_nrf54_prepare_epd_rail(void)
{
	nrf54_gpio_write(NRF54L15_BS_PIN, false);
	k_msleep(50);
}

#endif

/* Bit-bang the 0xB9 deep power-down command to an external SPI NOR flash,
 * then park the bus (CLK/MOSI low, CS high) — mirrors the nRF52840 Firmware
 * powerDownExternalFlashFromConfig(). */
void board_nrf54_flash_powerdown(uint8_t mosi_cfg, uint8_t sck_cfg, uint8_t cs_cfg)
{
	uint8_t cmd = 0xB9u;

	nrf54_gpio_configure_output(mosi_cfg, false);
	nrf54_gpio_configure_output(sck_cfg, false);
	nrf54_gpio_configure_output(cs_cfg, false);

	for (uint8_t bit = 0; bit < 8u; bit++) {
		nrf54_gpio_write(mosi_cfg, (cmd & 0x80u) != 0u);
		cmd = (uint8_t)(cmd << 1);
		k_busy_wait(1);
		nrf54_gpio_write(sck_cfg, true);
		k_busy_wait(1);
		nrf54_gpio_write(sck_cfg, false);
	}
	nrf54_gpio_write(cs_cfg, true);
	k_busy_wait(30);

	nrf54_gpio_write(mosi_cfg, false);
	nrf54_gpio_write(sck_cfg, false);
	nrf54_gpio_write(cs_cfg, true);
}
