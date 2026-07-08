#ifndef BOARD_NRF54_H
#define BOARD_NRF54_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void board_nrf54_early_init(void);
void board_nrf54_prepare_epd_rail(void);
void board_nrf54_flash_powerdown(uint8_t mosi_cfg, uint8_t sck_cfg, uint8_t cs_cfg);

#ifdef __cplusplus
}
#endif

#endif
