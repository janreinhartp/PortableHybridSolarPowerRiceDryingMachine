#include "ui_manager.h"

#include <cstdio>
#include <cstring>

#include "lvgl.h"

static lv_obj_t *s_heap_label = nullptr;
static lv_obj_t *s_touch_label = nullptr;
static lv_obj_t *s_tap_btn = nullptr;
static unsigned *s_tap_counter = nullptr;

static const char *ok_fail(bool ok)
{
    return ok ? "OK" : "FAIL";
}

static void on_tap_btn(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || s_tap_counter == nullptr) {
        return;
    }
    (*s_tap_counter)++;
    if (s_touch_label != nullptr) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Touch taps: %u", *s_tap_counter);
        lv_label_set_text(s_touch_label, buf);
    }
}

void ui_manager_show_foundation_screen(const system_status_t *status)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B2838), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Rice Dryer — Phase 1");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF2F5F8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "Foundation bring-up");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9BB0C4), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 60);

    char line[128];
    lv_obj_t *status_box = lv_label_create(scr);
    snprintf(line, sizeof(line),
             "Display 1024x600 : %s\n"
             "Touch GT911      : %s\n"
             "LVGL             : %s\n"
             "SD /sdcard       : %s",
             ok_fail(status->display_ok),
             ok_fail(status->touch_ok),
             ok_fail(status->lvgl_ok),
             ok_fail(status->sd_ok));
    if (status->sd_ok) {
        char sd_line[64];
        snprintf(sd_line, sizeof(sd_line), "\nSD free/total    : %u / %u MB",
                 (unsigned)status->sd_free_mb, (unsigned)status->sd_total_mb);
        strncat(line, sd_line, sizeof(line) - strlen(line) - 1);
    }
    lv_label_set_text(status_box, line);
    lv_obj_set_style_text_color(status_box, lv_color_hex(0xE8EEF4), 0);
    lv_obj_set_style_text_font(status_box, &lv_font_montserrat_16, 0);
    lv_obj_align(status_box, LV_ALIGN_TOP_LEFT, 48, 110);

    s_heap_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_heap_label, lv_color_hex(0x7DCFB6), 0);
    lv_obj_set_style_text_font(s_heap_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_heap_label, LV_ALIGN_TOP_LEFT, 48, 280);

    s_touch_label = lv_label_create(scr);
    lv_label_set_text(s_touch_label, "Touch taps: 0");
    lv_obj_set_style_text_color(s_touch_label, lv_color_hex(0xF0C808), 0);
    lv_obj_set_style_text_font(s_touch_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_touch_label, LV_ALIGN_TOP_LEFT, 48, 320);

    s_tap_btn = lv_btn_create(scr);
    lv_obj_set_size(s_tap_btn, 220, 64);
    lv_obj_align(s_tap_btn, LV_ALIGN_BOTTOM_MID, 0, -48);
    lv_obj_add_event_cb(s_tap_btn, on_tap_btn, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *btn_label = lv_label_create(s_tap_btn);
    lv_label_set_text(btn_label, "Tap to test touch");
    lv_obj_center(btn_label);

    ui_manager_update_runtime(status, 0);
}

void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps)
{
    if (s_heap_label != nullptr && status != nullptr) {
        char buf[96];
        snprintf(buf, sizeof(buf), "Free heap: %u  |  Internal: %u",
                 (unsigned)status->free_heap, (unsigned)status->free_internal_heap);
        lv_label_set_text(s_heap_label, buf);
    }
    if (s_touch_label != nullptr) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Touch taps: %u", touch_taps);
        lv_label_set_text(s_touch_label, buf);
    }
}

void ui_manager_bind_tap_counter(unsigned *counter)
{
    s_tap_counter = counter;
}
