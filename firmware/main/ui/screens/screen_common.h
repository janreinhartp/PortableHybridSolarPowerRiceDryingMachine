#pragma once

#include "lvgl.h"

#include <stddef.h>
#include <stdint.h>

#include "system_status.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_format_hms(char *buf, size_t len, uint32_t seconds);
/** Header clock: "Oct 14, 2026  2:19 PM" */
void ui_format_header_datetime(char *buf, size_t len, uint16_t year, uint8_t month, uint8_t day,
                               uint8_t hour, uint8_t minute);
const char *ui_on_off(bool on);
const char *ui_door_text(bool door_open, bool door_close);
const char *ui_circ_mode_name(uint8_t mode);
/** Title-Case process state for on-screen labels (not for logs). */
const char *ui_state_title(uint8_t dryer_state);

#ifdef __cplusplus
}
#endif
