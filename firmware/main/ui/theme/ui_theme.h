#pragma once

#include "lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* Light industrial SCADA — high contrast on a 7" outdoor-adjacent panel */
    UI_COLOR_BG = 0xF1F5F9,
    UI_COLOR_SURFACE = 0xFFFFFF,
    UI_COLOR_SURFACE_ALT = 0xE2E8F0,
    UI_COLOR_BORDER = 0xCBD5E1,
    UI_COLOR_TEXT = 0x0F172A,
    UI_COLOR_MUTED = 0x475569,
    UI_COLOR_ACCENT = 0x0369A1,
    UI_COLOR_TEAL = 0x0F766E,
    UI_COLOR_AMBER = 0xB45309,
    UI_COLOR_ORANGE = 0xC2410C,
    UI_COLOR_RED = 0xB91C1C,
    UI_COLOR_GREEN = 0x15803D,
    UI_COLOR_HEATER = 0xEA580C,
    UI_COLOR_ON_PRIMARY = 0xFFFFFF,
    UI_COLOR_FAULT_BG = 0xFEE2E2,
};

/** Ignore repeat taps on the same control within this window (ms). */
#define UI_CLICK_DEBOUNCE_MS 450

void ui_theme_apply_screen(lv_obj_t *scr);
lv_color_t ui_theme_color(uint32_t hex);
lv_color_t ui_theme_state_color(uint8_t dryer_state);
bool ui_theme_is_light_on_color(uint32_t bg_hex);

const lv_font_t *ui_font_caption(void);
const lv_font_t *ui_font_body(void);
const lv_font_t *ui_font_title(void);
const lv_font_t *ui_font_kpi(void);

lv_obj_t *ui_theme_make_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);
lv_obj_t *ui_theme_make_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user,
                            lv_coord_t w, lv_coord_t h, uint32_t bg_hex);
lv_obj_t *ui_theme_make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                              uint32_t hex);

#ifdef __cplusplus
}
#endif
