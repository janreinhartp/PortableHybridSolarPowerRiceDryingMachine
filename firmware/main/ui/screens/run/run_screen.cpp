#include "run_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_mode = nullptr;
static ui_gauge_t s_moist {};
static ui_gauge_t s_chamber {};
static ui_trend_t s_trend {};
static ui_actuator_leds_t s_leds {};
static ui_kpi_t s_timer {};

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
    s_mode = nullptr;
    s_moist = {};
    s_chamber = {};
    s_trend = {};
    s_leds = {};
    s_timer = {};
}

void run_build(lv_obj_t *parent, const system_status_t *status)
{
    s_mode = ui_theme_make_label(parent, "Drying", ui_font_title(), UI_COLOR_AMBER);
    lv_obj_set_pos(s_mode, 8, 4);

    s_moist = ui_gauge_create(parent, "Moisture", "%", 0, 30, 170);
    lv_obj_set_pos(s_moist.root, 8, 36);
    s_chamber = ui_gauge_create(parent, "Chamber", "C", 0, 80, 170);
    lv_obj_set_pos(s_chamber.root, 230, 36);

    s_trend = ui_trend_create(parent, 540, 250);
    lv_obj_set_pos(s_trend.root, 450, 36);

    s_leds = ui_leds_create(parent, 420);
    lv_obj_set_pos(s_leds.root, 8, 340);

    s_timer = ui_kpi_create(parent, "Drying Time", 260, 78);
    lv_obj_set_pos(s_timer.root, 450, 302);
    if (s_timer.title != nullptr) {
        lv_obj_set_width(s_timer.title, 230);
        lv_label_set_long_mode(s_timer.title, LV_LABEL_LONG_DOT);
    }
    if (s_timer.value != nullptr) {
        lv_obj_set_width(s_timer.value, 230);
        lv_label_set_long_mode(s_timer.value, LV_LABEL_LONG_CLIP);
    }

    lv_obj_t *abort =
        ui_theme_make_btn(parent, "Abort Batch", on_abort, nullptr, 180, 64, UI_COLOR_RED);
    lv_obj_set_pos(abort, 740, 310);

    s_alive = true;
    run_update(status);
}

void run_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    lv_label_set_text(s_mode, ui_state_title(status->dryer_state));
    lv_obj_set_style_text_color(s_mode, ui_theme_state_color(status->dryer_state), 0);

    ui_gauge_set(&s_moist, status->moisture_pct);
    ui_gauge_set_target(&s_moist, true, status->target_moisture_pct);

    ui_gauge_set(&s_chamber, status->chamber_temp_c);
    if (status->dryer_state == DRYER_STATE_COOLING) {
        ui_gauge_set_target(&s_chamber, true, status->cooling_temperature_c);
    } else if (status->dryer_state == DRYER_STATE_DRYING ||
               status->dryer_state == DRYER_STATE_PRECHECK) {
        ui_gauge_set_target(&s_chamber, true, status->drying_temperature_c);
    } else {
        /* Discharge / complete — keep last drying setpoint visible as reference. */
        ui_gauge_set_target(&s_chamber, true, status->drying_temperature_c);
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

    if (s_timer.title != nullptr) {
        lv_label_set_text(s_timer.title, title);
    }
    char buf[24];
    ui_format_hms(buf, sizeof(buf), seconds);
    ui_kpi_set(&s_timer, buf);
}
