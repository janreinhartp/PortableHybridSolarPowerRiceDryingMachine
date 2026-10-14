#include "system_info_screen.h"

#include <cstdio>

#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_body = nullptr;
static unsigned *s_tap_counter = nullptr;

static void on_tap(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_tap_counter != nullptr) {
        (*s_tap_counter)++;
    }
}

static void on_commission(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_COMMISSION);
    }
}

void system_info_detach(void)
{
    s_alive = false;
    s_body = nullptr;
}

void system_info_set_tap_counter(unsigned *counter)
{
    s_tap_counter = counter;
}

void system_info_build(lv_obj_t *parent, const system_status_t *status, unsigned touch_taps)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "System Info", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 8, 4);

    s_body = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_set_width(s_body, 980);
    lv_obj_set_pos(s_body, 8, 44);

    lv_obj_t *tap =
        ui_theme_make_btn(parent, "Touch Test", on_tap, nullptr, 160, 64, UI_COLOR_ACCENT);
    lv_obj_set_pos(tap, 8, 340);
    lv_obj_t *comm = ui_theme_make_btn(parent, "Commission Sensors", on_commission, nullptr, 240,
                                       64, UI_COLOR_ORANGE);
    lv_obj_set_pos(comm, 184, 340);

    s_alive = true;
    system_info_update(status, touch_taps);
}

void system_info_update(const system_status_t *status, unsigned touch_taps)
{
    if (!s_alive || status == nullptr || s_body == nullptr) {
        return;
    }
    char line[640];
    snprintf(line, sizeof(line),
             "Display %s   Touch %s   LVGL %s\n"
             "SD %s   %u MB Free / %u MB    Logger %s   Dropped %u\n"
             "RTC %s   %04u-%02u-%02u %02u:%02u:%02u\n"
             "RS485 %s   Relay %s   ADS1115 %s\n"
             "Wi-Fi %s   %s   http://192.168.4.1\n"
             "Heap %u    Internal %u\n"
             "Hot Air %s   Chamber %s   Moisture %s   Conductivity %s\n"
             "Touch Taps %u",
             status->display_ok ? "Ok" : "Fail", status->touch_ok ? "Ok" : "Fail",
             status->lvgl_ok ? "Ok" : "Fail", status->sd_ok ? "Ok" : "Fail",
             (unsigned)status->sd_free_mb, (unsigned)status->sd_total_mb,
             status->logger_ok ? "Ok" : "Fail", (unsigned)status->logger_dropped,
             status->rtc_ok ? "Ok" : "Fail",
             (unsigned)(status->rtc_year % 10000u), (unsigned)(status->rtc_month % 100u),
             (unsigned)(status->rtc_day % 100u), (unsigned)(status->rtc_hour % 100u),
             (unsigned)(status->rtc_minute % 100u), (unsigned)(status->rtc_second % 100u),
             status->rs485_ok ? "Ok" : "Fail", status->relay_ok ? "Ok" : "Fail",
             status->ads1115_ok ? "Ok" : "Fail",
             status->wifi_ok ? "Ok" : "Fail",
             status->wifi_ssid[0] ? status->wifi_ssid : "—",
             (unsigned)status->free_heap, (unsigned)status->free_internal_heap,
             status->hotair_ok ? "Ok" : "Fail", status->chamber_ok ? "Ok" : "Fail",
             status->moisture_ok ? "Ok" : "Fail", status->ec_ok ? "Ok" : "Fail",
             touch_taps);
    lv_label_set_text(s_body, line);
}
