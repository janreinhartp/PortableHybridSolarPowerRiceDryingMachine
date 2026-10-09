#include "ui_manager.h"

#include <cstdio>
#include <cstring>

#include "lvgl.h"
#include "rtc_manager.h"

static lv_obj_t *s_heap_label = nullptr;
static lv_obj_t *s_touch_label = nullptr;
static lv_obj_t *s_rtc_label = nullptr;
static lv_obj_t *s_status_box = nullptr;
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

static void on_plus_hour(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    rtc_manager_adjust_minutes(60);
}

static void on_plus_min(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    rtc_manager_adjust_minutes(1);
}

static void on_set_build_time(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    rtc_manager_set_build_time();
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, lv_coord_t w)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, 56);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}

void ui_manager_show_foundation_screen(const system_status_t *status)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B2838), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Rice Dryer — Phase 2");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF2F5F8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "GPIO map + TinyRTC");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9BB0C4), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 54);

    s_status_box = lv_label_create(scr);
    lv_obj_set_style_text_color(s_status_box, lv_color_hex(0xE8EEF4), 0);
    lv_obj_set_style_text_font(s_status_box, &lv_font_montserrat_16, 0);
    lv_obj_align(s_status_box, LV_ALIGN_TOP_LEFT, 48, 100);

    s_rtc_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_rtc_label, lv_color_hex(0x7DCFB6), 0);
    lv_obj_set_style_text_font(s_rtc_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_rtc_label, LV_ALIGN_TOP_LEFT, 48, 250);

    s_heap_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_heap_label, lv_color_hex(0x9BB0C4), 0);
    lv_obj_set_style_text_font(s_heap_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_heap_label, LV_ALIGN_TOP_LEFT, 48, 290);

    s_touch_label = lv_label_create(scr);
    lv_label_set_text(s_touch_label, "Touch taps: 0");
    lv_obj_set_style_text_color(s_touch_label, lv_color_hex(0xF0C808), 0);
    lv_obj_set_style_text_font(s_touch_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_touch_label, LV_ALIGN_TOP_LEFT, 48, 320);

    lv_obj_t *btn_hour = make_btn(scr, "+1 hour", on_plus_hour, 160);
    lv_obj_align(btn_hour, LV_ALIGN_BOTTOM_LEFT, 48, -28);

    lv_obj_t *btn_min = make_btn(scr, "+1 min", on_plus_min, 160);
    lv_obj_align(btn_min, LV_ALIGN_BOTTOM_LEFT, 240, -28);

    lv_obj_t *btn_build = make_btn(scr, "Set build time", on_set_build_time, 220);
    lv_obj_align(btn_build, LV_ALIGN_BOTTOM_LEFT, 432, -28);

    lv_obj_t *btn_touch = make_btn(scr, "Tap touch", on_tap_btn, 160);
    lv_obj_align(btn_touch, LV_ALIGN_BOTTOM_LEFT, 684, -28);

    ui_manager_update_runtime(status, 0);
}

void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps)
{
    if (status == nullptr) {
        return;
    }

    if (s_status_box != nullptr) {
        char line[192];
        snprintf(line, sizeof(line),
                 "Display : %s    Touch : %s    LVGL : %s\n"
                 "SD card : %s    RTC   : %s",
                 ok_fail(status->display_ok),
                 ok_fail(status->touch_ok),
                 ok_fail(status->lvgl_ok),
                 ok_fail(status->sd_ok),
                 ok_fail(status->rtc_ok));
        if (status->sd_ok) {
            char sd_line[64];
            snprintf(sd_line, sizeof(sd_line), "\nSD free/total : %u / %u MB",
                     (unsigned)status->sd_free_mb, (unsigned)status->sd_total_mb);
            strncat(line, sd_line, sizeof(line) - strlen(line) - 1);
        }
        lv_label_set_text(s_status_box, line);
    }

    if (s_rtc_label != nullptr) {
        if (status->rtc_ok) {
            char buf[80];
            snprintf(buf, sizeof(buf), "RTC  %04u-%02u-%02u  %02u:%02u:%02u",
                     status->rtc_year, status->rtc_month, status->rtc_day,
                     status->rtc_hour, status->rtc_minute, status->rtc_second);
            lv_label_set_text(s_rtc_label, buf);
        } else {
            lv_label_set_text(s_rtc_label, "RTC  not detected (check 0x68 on I2C)");
        }
    }

    if (s_heap_label != nullptr) {
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
