#include "screen_common.h"

#include <cstdio>

#include "process_controller.h"
#include "settings_manager.h"

lv_obj_t *ui_make_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user,
                      lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    return btn;
}

lv_obj_t *ui_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                   const lv_font_t *font, uint32_t hex_color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(hex_color), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

void ui_format_hms(char *buf, size_t len, uint32_t seconds)
{
    if (buf == nullptr || len == 0) {
        return;
    }
    snprintf(buf, len, "%02u:%02u:%02u", (unsigned)(seconds / 3600u),
             (unsigned)((seconds / 60u) % 60u), (unsigned)(seconds % 60u));
}

const char *ui_on_off(bool on)
{
    return on ? "ON" : "OFF";
}

const char *ui_door_text(bool door_open, bool door_close)
{
    if (door_open) {
        return "OPEN";
    }
    if (door_close) {
        return "CLOSING";
    }
    return "CLOSED";
}

const char *ui_circ_mode_name(uint8_t mode)
{
    return mode == (uint8_t)CIRCULATION_MODE_CONTINUOUS ? "Continuous" : "Interval";
}

lv_color_t ui_state_color(uint8_t dryer_state)
{
    switch ((dryer_state_t)dryer_state) {
    case DRYER_STATE_DRYING:
        return lv_color_hex(0xF0C808);
    case DRYER_STATE_COOLING:
        return lv_color_hex(0x7EC8FF);
    case DRYER_STATE_DISCHARGING:
        return lv_color_hex(0xF2A65A);
    case DRYER_STATE_COMPLETE:
        return lv_color_hex(0x7DCFB6);
    case DRYER_STATE_FAULT:
        return lv_color_hex(0xE85D4C);
    case DRYER_STATE_PRECHECK:
        return lv_color_hex(0xC9B6FF);
    case DRYER_STATE_IDLE:
    default:
        return lv_color_hex(0xE8EEF4);
    }
}
