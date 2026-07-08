#include "nrf54_gpio.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>

static const struct device *gpio_dev(uint8_t port)
{
	switch (port) {
	case 0:
		return DEVICE_DT_GET(DT_NODELABEL(gpio0));
	case 1:
		return DEVICE_DT_GET(DT_NODELABEL(gpio1));
	case 2:
		return DEVICE_DT_GET(DT_NODELABEL(gpio2));
#if DT_NODE_HAS_STATUS(DT_NODELABEL(gpio3), okay)
	case 3:
		return DEVICE_DT_GET(DT_NODELABEL(gpio3));
#endif
	default:
		return NULL;
	}
}

bool nrf54_pin_decode(uint8_t cfg, uint8_t *port_out, uint8_t *pin_out)
{
	if (cfg == NRF54_GPIO_PIN_UNUSED) {
		return false;
	}
	uint8_t port = (uint8_t)((cfg >> 4) & 0x0Fu);
	uint8_t pin = (uint8_t)(cfg & 0x0Fu);
	if (port > 3u || pin > 15u) {
		return false;
	}
	if (gpio_dev(port) == NULL || !device_is_ready(gpio_dev(port))) {
		return false;
	}
	*port_out = port;
	*pin_out = pin;
	return true;
}

void nrf54_gpio_configure_output(uint8_t cfg, bool initial_high)
{
	uint8_t port;
	uint8_t pin;
	gpio_flags_t flags = GPIO_OUTPUT | (initial_high ? GPIO_OUTPUT_INIT_HIGH
							 : GPIO_OUTPUT_INIT_LOW);

	if (!nrf54_pin_decode(cfg, &port, &pin)) {
		return;
	}
	(void)gpio_pin_configure(gpio_dev(port), pin, flags);
}

void nrf54_gpio_configure_input(uint8_t cfg, bool pull_up, bool pull_down)
{
	uint8_t port;
	uint8_t pin;
	gpio_flags_t flags = GPIO_INPUT;

	if (pull_up) {
		flags |= GPIO_PULL_UP;
	} else if (pull_down) {
		flags |= GPIO_PULL_DOWN;
	}
	if (!nrf54_pin_decode(cfg, &port, &pin)) {
		return;
	}
	(void)gpio_pin_configure(gpio_dev(port), pin, flags);
}

void nrf54_gpio_write(uint8_t cfg, bool level_high)
{
	uint8_t port;
	uint8_t pin;

	if (!nrf54_pin_decode(cfg, &port, &pin)) {
		return;
	}
	(void)gpio_pin_set(gpio_dev(port), pin, level_high ? 1 : 0);
}

int nrf54_gpio_read(uint8_t cfg)
{
	uint8_t port;
	uint8_t pin;

	if (!nrf54_pin_decode(cfg, &port, &pin)) {
		return 0;
	}
	return gpio_pin_get(gpio_dev(port), pin);
}

void nrf54_gpio_park(uint8_t cfg)
{
	uint8_t port;
	uint8_t pin;

	if (!nrf54_pin_decode(cfg, &port, &pin)) {
		return;
	}
	(void)gpio_pin_configure(gpio_dev(port), pin, GPIO_DISCONNECTED);
}

ssize_t od_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
	return hwinfo_get_device_id(buffer, length);
}
