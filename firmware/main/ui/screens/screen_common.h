#pragma once

#include "lvgl.h"

#include <stddef.h>

#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_make_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user,
                      lv_coord_t w, lv_coord_t h);
lv_obj_t *ui_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                   const lv_font_t *font, uint32_t hex_color);
void ui_format_hms(char *buf, size_t len, uint32_t seconds);
const char *ui_on_off(bool on);
const char *ui_door_text(bool door_open, bool door_close);
const char *ui_circ_mode_name(uint8_t mode);
lv_color_t ui_state_color(uint8_t dryer_state);

#ifdef __cplusplus
}
#endif
