#ifndef NRF54_BBEP_BUSY_INL
#define NRF54_BBEP_BUSY_INL

#include "nrf54_gpio.h"
#include "nrf54_zephyr_compat.h"

static int nrf54_epd_busy_read(BBEPDISP *pBBEP)
{
	if (pBBEP->iBUSYPin == 0xff) {
		return 1;
	}
	return nrf54_gpio_read(pBBEP->iBUSYPin);
}

static uint8_t nrf54_busy_idle_level(BBEPDISP *pBBEP)
{
	return (pBBEP->chip_type == BBEP_CHIP_UC81xx) ? 1u : 0u;
}

void bbepWaitBusy(BBEPDISP *pBBEP)
{
	int iTimeout = 0;
	int iMaxTime = 5000;
	uint8_t busy_idle;

	if (!pBBEP) {
		return;
	}
	if (pBBEP->iBUSYPin == 0xff) {
		return;
	}
	od_msleep(10);
	busy_idle = nrf54_busy_idle_level(pBBEP);
	od_msleep(1);
	if (pBBEP->iFlags & (BBEP_3COLOR | BBEP_4COLOR | BBEP_7COLOR)) {
		iMaxTime = 30000;
	}
	while (iTimeout < iMaxTime) {
		if (nrf54_epd_busy_read(pBBEP) == (int)busy_idle) {
			break;
		}
		od_msleep(20);
		iTimeout += 20;
	}
	if (iTimeout >= iMaxTime) {
		printf("[OD] busy TIMEOUT pin=0x%02X level=%d expected_idle=%d chip=%d\r\n",
		       (unsigned)pBBEP->iBUSYPin, nrf54_epd_busy_read(pBBEP), (int)busy_idle,
		       (int)pBBEP->chip_type);
	}
}

bool bbepIsBusy(BBEPDISP *pBBEP)
{
	uint8_t busy_idle;

	if (!pBBEP) {
		return false;
	}
	if (pBBEP->iBUSYPin == 0xff) {
		return false;
	}
	od_msleep(10);
	busy_idle = nrf54_busy_idle_level(pBBEP);
	od_msleep(1);
	return (nrf54_epd_busy_read(pBBEP) != (int)busy_idle);
}

#endif /* NRF54_BBEP_BUSY_INL */
