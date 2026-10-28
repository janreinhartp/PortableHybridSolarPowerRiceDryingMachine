#include "system_info_screen.h"

#include <cstdio>

#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_tiles[6] = {};
static unsigned *s_tap_counter = nullptr;

static void on_commission(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_COMMISSION);
    }
}

static void on_calibrate(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_CALIBRATE_MOISTURE);
    }
}

static void on_set_clock(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_SET_CLOCK);
    }
}

static lv_obj_t *make_tile(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *heading)
{
    lv_obj_t *card = ui_theme_make_card(parent, 320, 120);
    lv_obj_set_pos(card, x, y);
    lv_obj_t *h = ui_theme_make_label(card, heading, ui_font_body(), UI_COLOR_MUTED);
    lv_obj_align(h, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *v = ui_theme_make_label(card, "-", ui_font_title(), UI_COLOR_CREAM);
    lv_obj_align(v, LV_ALIGN_TOP_LEFT, 0, 36);
    lv_obj_t *d = ui_theme_make_label(card, "", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_width(d, 288);
    lv_label_set_long_mode(d, LV_LABEL_LONG_DOT);
    lv_obj_align(d, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_user_data(card, v);
    lv_obj_set_user_data(v, d);
    return card;
}

static void set_tile(lv_obj_t *card, const char *value, const char *detail)
{
    if (card == nullptr) {
        return;
    }
    lv_obj_t *v = (lv_obj_t *)lv_obj_get_user_data(card);
    if (v == nullptr) {
        return;
    }
    ui_label_set_text_if_changed(v, value != nullptr ? value : "-");
    lv_obj_t *d = (lv_obj_t *)lv_obj_get_user_data(v);
    if (d != nullptr) {
        ui_label_set_text_if_changed(d, detail != nullptr ? detail : "");
    }
}

void system_info_detach(void)
{
    s_alive = false;
    for (int i = 0; i < 6; ++i) {
        s_tiles[i] = nullptr;
    }
}

void system_info_set_tap_counter(unsigned *counter)
{
    s_tap_counter = counter;
}

void system_info_build(lv_obj_t *parent, const system_status_t *status, unsigned touch_taps)
{
    (void)touch_taps;
    lv_obj_t *title =
        ui_theme_make_label(parent, "System", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    s_tiles[0] = make_tile(parent, 0, 48, "Display");
    s_tiles[1] = make_tile(parent, 336, 48, "Storage");
    s_tiles[2] = make_tile(parent, 672, 48, "RTC");
    s_tiles[3] = make_tile(parent, 0, 184, "Network");
    s_tiles[4] = make_tile(parent, 336, 184, "Heap");
    s_tiles[5] = make_tile(parent, 672, 184, "Sensors");

    lv_obj_t *comm = ui_theme_make_btn(parent, "Commission Sensors", on_commission, nullptr, 320,
                                       56, UI_COLOR_TEAL);
    lv_obj_set_pos(comm, 0, 336);
    lv_obj_t *cal = ui_theme_make_btn(parent, "Calibrate Moisture", on_calibrate, nullptr, 320,
                                      56, UI_COLOR_ACCENT);
    lv_obj_set_pos(cal, 336, 336);
    lv_obj_t *clock =
        ui_theme_make_btn(parent, "Set Clock", on_set_clock, nullptr, 320, 56, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(clock, 672, 336);

    s_alive = true;
    system_info_update(status, touch_taps);
}

void system_info_update(const system_status_t *status, unsigned touch_taps)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    char v[64];
    char d[96];

    snprintf(v, sizeof(v), "%s", status->display_ok && status->touch_ok ? "Ok" : "Fail");
    snprintf(d, sizeof(d), "Touch %s / LVGL %s / Taps %u", status->touch_ok ? "Ok" : "Fail",
             status->lvgl_ok ? "Ok" : "Fail", touch_taps);
    set_tile(s_tiles[0], v, d);

    snprintf(v, sizeof(v), "%s", status->sd_ok ? "Ok" : "Fail");
    snprintf(d, sizeof(d), "%u / %u MB / Logger %s", (unsigned)status->sd_free_mb,
             (unsigned)status->sd_total_mb, status->logger_ok ? "Ok" : "Fail");
    set_tile(s_tiles[1], v, d);

    snprintf(v, sizeof(v), "%s", status->rtc_ok ? "Ok" : "Fail");
    snprintf(d, sizeof(d), "%04u-%02u-%02u %02u:%02u", (unsigned)(status->rtc_year % 10000u),
             (unsigned)(status->rtc_month % 100u), (unsigned)(status->rtc_day % 100u),
             (unsigned)(status->rtc_hour % 100u), (unsigned)(status->rtc_minute % 100u));
    set_tile(s_tiles[2], v, d);

    snprintf(v, sizeof(v), "%s", status->wifi_ok ? "Online" : "Offline");
    snprintf(d, sizeof(d), "%s / http://192.168.4.1",
             status->wifi_ssid[0] ? status->wifi_ssid : "-");
    set_tile(s_tiles[3], v, d);

    snprintf(v, sizeof(v), "%u", (unsigned)status->free_heap);
    snprintf(d, sizeof(d), "Internal %u", (unsigned)status->free_internal_heap);
    set_tile(s_tiles[4], v, d);

    snprintf(v, sizeof(v), "%s",
             (status->hotair_ok && status->chamber_ok && status->moisture_ok) ? "Ok" : "Check");
    snprintf(d, sizeof(d), "Hot %s / Ch %s / Moist %s", status->hotair_ok ? "Ok" : "X",
             status->chamber_ok ? "Ok" : "X", status->moisture_ok ? "Ok" : "X");
    set_tile(s_tiles[5], v, d);

    (void)s_tap_counter;
}
