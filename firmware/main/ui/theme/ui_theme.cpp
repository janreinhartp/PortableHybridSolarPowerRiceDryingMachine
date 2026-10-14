#include "ui_theme.h"

#include "process_controller.h"

static uint32_t s_last_click_ms = 0;
static lv_obj_t *s_last_click_obj = nullptr;

lv_color_t ui_theme_color(uint32_t hex)
{
    return lv_color_hex(hex);
}

bool ui_theme_is_light_on_color(uint32_t bg_hex)
{
    switch (bg_hex) {
    case UI_COLOR_SURFACE:
    case UI_COLOR_SURFACE_ALT:
    case UI_COLOR_BG:
    case UI_COLOR_BORDER:
    case UI_COLOR_FAULT_BG:
        return false;
    default:
        return true;
    }
}

lv_color_t ui_theme_state_color(uint8_t dryer_state)
{
    switch ((dryer_state_t)dryer_state) {
    case DRYER_STATE_DRYING:
        return ui_theme_color(UI_COLOR_AMBER);
    case DRYER_STATE_COOLING:
        return ui_theme_color(UI_COLOR_ACCENT);
    case DRYER_STATE_DISCHARGING:
        return ui_theme_color(UI_COLOR_ORANGE);
    case DRYER_STATE_COMPLETE:
        return ui_theme_color(UI_COLOR_TEAL);
    case DRYER_STATE_FAULT:
        return ui_theme_color(UI_COLOR_RED);
    case DRYER_STATE_PRECHECK:
        return ui_theme_color(UI_COLOR_ACCENT);
    case DRYER_STATE_IDLE:
    default:
        return ui_theme_color(UI_COLOR_MUTED);
    }
}

const lv_font_t *ui_font_caption(void)
{
    return &lv_font_montserrat_14;
}

const lv_font_t *ui_font_body(void)
{
    return &lv_font_montserrat_16;
}

const lv_font_t *ui_font_title(void)
{
    return &lv_font_montserrat_24;
}

const lv_font_t *ui_font_kpi(void)
{
#if LV_FONT_MONTSERRAT_28
    return &lv_font_montserrat_28;
#else
    return &lv_font_montserrat_24;
#endif
}

void ui_theme_apply_screen(lv_obj_t *scr)
{
    if (scr == nullptr) {
        return;
    }
    lv_obj_set_style_bg_color(scr, ui_theme_color(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

lv_obj_t *ui_theme_make_card(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, ui_theme_color(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(card, ui_theme_color(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static void on_click_guard(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    const uint32_t now = lv_tick_get();
    if (obj == s_last_click_obj && lv_tick_elaps(s_last_click_ms) < UI_CLICK_DEBOUNCE_MS) {
        lv_event_stop_processing(e);
        return;
    }
    s_last_click_obj = obj;
    s_last_click_ms = now;
}

lv_obj_t *ui_theme_make_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user,
                            lv_coord_t w, lv_coord_t h, uint32_t bg_hex)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, ui_theme_color(bg_hex), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, 8, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(btn, on_click_guard, LV_EVENT_CLICKED, nullptr);
    if (cb != nullptr) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
    }
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text != nullptr ? text : "");
    lv_obj_set_style_text_font(label, ui_font_body(), 0);
    const uint32_t fg =
        ui_theme_is_light_on_color(bg_hex) ? UI_COLOR_ON_PRIMARY : UI_COLOR_TEXT;
    lv_obj_set_style_text_color(label, ui_theme_color(fg), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_center(label);
    return btn;
}

lv_obj_t *ui_theme_make_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                              uint32_t hex)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text != nullptr ? text : "");
    lv_obj_set_style_text_font(label, font != nullptr ? font : ui_font_body(), 0);
    lv_obj_set_style_text_color(label, ui_theme_color(hex), 0);
    return label;
}
