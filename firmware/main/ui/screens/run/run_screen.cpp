#include "run_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static ui_bar_gauge_t s_moist {};
static ui_bar_gauge_t s_chamber {};
static ui_trend_t s_trend {};
static ui_actuator_leds_t s_leds {};
static lv_obj_t *s_timer_title = nullptr;
static lv_obj_t *s_timer_value = nullptr;

static void on_abort_confirmed(void *user)
{
    (void)user;
    process_controller_abort();
    ui_manager_set_banner("Batch Aborted - Outputs Set Safe");
}

static void on_abort(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_confirm_show("Abort Batch?", "Stop This Batch And Set Outputs Safe?", on_abort_confirmed,
                    nullptr);
}

void run_detach(void)
{
    s_alive = false;
    s_moist = {};
    s_chamber = {};
    s_trend = {};
    s_leds = {};
    s_timer_title = nullptr;
    s_timer_value = nullptr;
}

void run_build(lv_obj_t *parent, const system_status_t *status)
{
    s_moist = ui_bar_gauge_create(parent, "Rice Moisture", "%", 0, 30, 328, 128, UI_COLOR_TEAL);
    lv_obj_set_pos(s_moist.root, 0, 0);
    s_chamber =
        ui_bar_gauge_create(parent, "Chamber Temperature", "°C", 0, 80, 328, 128, UI_COLOR_AMBER);
    lv_obj_set_pos(s_chamber.root, 0, 136);

    s_trend = ui_trend_create(parent, 648, 264);
    lv_obj_set_pos(s_trend.root, 344, 0);

    s_leds = ui_actuator_row_create(parent, 992, 64);
    lv_obj_set_pos(s_leds.root, 0, 272);

    lv_obj_t *timer = ui_theme_make_card(parent, 720, 64);
    lv_obj_set_pos(timer, 0, 344);
    s_timer_title = ui_theme_make_label(timer, "Drying Time", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(s_timer_title, LV_ALIGN_LEFT_MID, 0, 0);
    s_timer_value = ui_theme_make_label(timer, "00:00:00", ui_font_kpi(), UI_COLOR_CREAM);
    lv_obj_align(s_timer_value, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *abort =
        ui_theme_make_btn(parent, "Abort Batch", on_abort, nullptr, 256, 64, UI_COLOR_RED);
    lv_obj_set_pos(abort, 736, 344);

    s_alive = true;
    run_update(status);
}

void run_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    ui_bar_gauge_set(&s_moist, status->moisture_pct);
    ui_bar_gauge_set_target(&s_moist, true, status->target_moisture_pct);

    ui_bar_gauge_set(&s_chamber, status->chamber_temp_c);
    if (status->dryer_state == DRYER_STATE_COOLING) {
        ui_bar_gauge_set_target(&s_chamber, true, status->cooling_temperature_c);
    } else {
        ui_bar_gauge_set_target(&s_chamber, true, status->drying_temperature_c);
    }

    ui_trend_refresh(&s_trend);
    ui_leds_set(&s_leds, status);

    const char *title = "Drying Time";
    uint32_t seconds = status->drying_elapsed_s;
    if (status->dryer_state == DRYER_STATE_COOLING) {
        title = "Cooling Time";
        seconds = status->cooling_elapsed_s;
    } else if (status->dryer_state == DRYER_STATE_DISCHARGING) {
        title = "Remaining";
        seconds = status->discharge_remaining_s;
    }

    if (s_timer_title != nullptr) {
        lv_label_set_text(s_timer_title, title);
    }
    char buf[24];
    ui_format_hms(buf, sizeof(buf), seconds);
    if (s_timer_value != nullptr) {
        lv_label_set_text(s_timer_value, buf);
    }
}
