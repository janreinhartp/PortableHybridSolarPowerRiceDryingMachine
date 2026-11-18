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
void ui_format_header_date(char *buf, size_t len, uint16_t year, uint8_t month, uint8_t day);
void ui_format_header_time(char *buf, size_t len, uint8_t hour, uint8_t minute);
const char *ui_door_text(bool door_open);
const char *ui_circ_mode_name(uint8_t mode);

#ifdef __cplusplus
}
#endif
