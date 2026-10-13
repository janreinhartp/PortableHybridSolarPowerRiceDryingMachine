#include "system_info_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "th_sensor.h"
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

static void on_hotair(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    char msg[128];
    process_controller_assign_sensor(TH_SENSOR_HOTAIR_ADDR, "Hot-air", msg, sizeof(msg));
    ui_manager_set_banner(msg);
}

static void on_chamber(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    char msg[128];
    process_controller_assign_sensor(TH_SENSOR_CHAMBER_ADDR, "Chamber", msg, sizeof(msg));
    ui_manager_set_banner(msg);
}

void system_info_detach(void)
{
    s_alive = false;
    s_body = nullptr;
}

void system_info_build(lv_obj_t *parent, const system_status_t *status, unsigned touch_taps)
{
    (void)touch_taps;
    ui_label(parent, "SYSTEM INFO", 8, 0, &lv_font_montserrat_24, 0xE8EEF4);
    s_body = ui_label(parent, "", 8, 44, &lv_font_montserrat_14, 0xE8EEF4);
    lv_obj_set_width(s_body, 960);

    /* The tap counter lives in main and is bound before the screen is shown. */
    lv_obj_t *tap = ui_make_btn(parent, "Touch+", on_tap, nullptr, 120, 44);
    lv_obj_set_pos(tap, 8, 360);
    lv_obj_t *hot = ui_make_btn(parent, "-> Hot-air (1)", on_hotair, nullptr, 180, 44);
    lv_obj_set_pos(hot, 144, 360);
    lv_obj_t *chamber = ui_make_btn(parent, "-> Chamber (2)", on_chamber, nullptr, 190, 44);
    lv_obj_set_pos(chamber, 340, 360);

    s_alive = true;
    system_info_update(status, touch_taps);
}

void system_info_update(const system_status_t *status, unsigned touch_taps)
{
    if (!s_alive || status == nullptr || s_body == nullptr) {
        return;
    }
    char line[512];
    snprintf(line, sizeof(line),
             "Phase 7    Display %s   Touch %s   LVGL %s\n"
             "SD %s   %u MB free / %u MB    Logger %s   dropped %u\n"
             "RTC %s   %04u-%02u-%02u %02u:%02u:%02u\n"
             "RS485 %s   Relay %s   ADS1115 %s\n"
             "Heap %u    internal %u\n"
             "Hot-air %s   Chamber %s   Moisture %s   EC %s\n"
             "Touch taps %u\n"
             "Sensor address buttons expect ONE probe on the bus.",
             status->display_ok ? "OK" : "FAIL",
             status->touch_ok ? "OK" : "FAIL",
             status->lvgl_ok ? "OK" : "FAIL",
             status->sd_ok ? "OK" : "FAIL",
             (unsigned)status->sd_free_mb, (unsigned)status->sd_total_mb,
             status->logger_ok ? "OK" : "FAIL",
             (unsigned)status->logger_dropped,
             status->rtc_ok ? "OK" : "FAIL",
             (unsigned)(status->rtc_year % 10000u), (unsigned)(status->rtc_month % 100u),
             (unsigned)(status->rtc_day % 100u), (unsigned)(status->rtc_hour % 100u),
             (unsigned)(status->rtc_minute % 100u), (unsigned)(status->rtc_second % 100u),
             status->rs485_ok ? "OK" : "FAIL",
             status->relay_ok ? "OK" : "FAIL",
             status->ads1115_ok ? "OK" : "FAIL",
             (unsigned)status->free_heap, (unsigned)status->free_internal_heap,
             status->hotair_ok ? "OK" : "FAIL",
             status->chamber_ok ? "OK" : "FAIL",
             status->moisture_ok ? "OK" : "FAIL",
             status->ec_ok ? "OK" : "FAIL",
             touch_taps);
    lv_label_set_text(s_body, line);
}

void system_info_set_tap_counter(unsigned *counter)
{
    s_tap_counter = counter;
}
