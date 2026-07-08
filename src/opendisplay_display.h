#ifndef OPENDISPLAY_DISPLAY_H
#define OPENDISPLAY_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int opendisplay_display_direct_write_start(const uint8_t *payload, uint16_t payload_len);
int opendisplay_display_direct_write_data(const uint8_t *payload, uint16_t payload_len);
int opendisplay_display_direct_write_end_prepare(const uint8_t *payload, uint16_t payload_len);
int opendisplay_display_direct_write_end_refresh(const uint8_t *payload, uint16_t payload_len,
                                                 bool *refresh_ok);
int opendisplay_display_partial_write_start(const uint8_t *payload, uint16_t payload_len,
                                            uint8_t *err_code_out);
bool opendisplay_display_partial_active(void);
void opendisplay_display_abort(void);
void opendisplay_display_boot_apply(void);
void opendisplay_display_park_pins(void);
void opendisplay_display_power_off(void);

#ifdef __cplusplus
}
#endif

#endif
