#pragma once

#include "lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* Warm charcoal SCADA palette from Rice Dryer Design Figma. */
    UI_COLOR_BG = 0x171A19,
    UI_COLOR_SURFACE = 0x222625,
    UI_COLOR_SURFACE_ALT = 0x2B302E,
    UI_COLOR_BORDER = 0x3A403D,
    UI_COLOR_TEXT = 0xF5F1E8,
    UI_COLOR_CREAM = 0xF2E8CF,
    UI_COLOR_MUTED = 0xB8B5AC,
    UI_COLOR_DIM = 0x777C78,
    UI_COLOR_ACCENT = 0x0F766E,
    UI_COLOR_TEAL = 0x0F766E,
    UI_COLOR_TEAL_SOFT = 0xCCFBF1,
    UI_COLOR_AMBER = 0xEAB308,
    UI_COLOR_ORANGE = 0xD97745,
    UI_COLOR_RED = 0xDC2626,
    UI_COLOR_GREEN = 0x22C55E,
    UI_COLOR_HEATER = 0xEAB308,
    UI_COLOR_ON_PRIMARY = 0xFFFFFF,
    UI_COLOR_FAULT_BG = 0x7F1D1D,
    UI_COLOR_FAULT_STRIP = 0x991B1B,
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
