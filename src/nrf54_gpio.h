#ifndef NRF54_GPIO_H
#define NRF54_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NRF54_GPIO_PIN_UNUSED 0xFFu

bool nrf54_pin_decode(uint8_t cfg, uint8_t *port_out, uint8_t *pin_out);
void nrf54_gpio_configure_output(uint8_t cfg, bool initial_high);
void nrf54_gpio_configure_input(uint8_t cfg, bool pull_up, bool pull_down);
void nrf54_gpio_write(uint8_t cfg, bool level_high);
int nrf54_gpio_read(uint8_t cfg);
void nrf54_gpio_park(uint8_t cfg);

#ifdef __cplusplus
}
#endif

#endif
