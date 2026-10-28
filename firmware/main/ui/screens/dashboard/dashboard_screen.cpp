#include "dashboard_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"
#include "ui_widgets.h"

static bool s_alive = false;
static ui_metric_t s_moist {};
static ui_metric_t s_hot {};
static ui_metric_t s_chamber {};
static ui_metric_t s_humidity {};
static ui_actuator_leds_t s_leds {};
static lv_obj_t *s_note = nullptr;
static lv_obj_t *s_resume = nullptr;
static lv_obj_t *s_discard = nullptr;
static lv_obj_t *s_start = nullptr;

static void on_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_manager_navigate(UI_SCREEN_BATCH_SETUP);
}

static void on_resume(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_resume_interrupted() == ESP_OK) {
        ui_manager_set_banner("Batch Resumed");
        ui_manager_navigate(UI_SCREEN_RUN);
    } else {
        ui_manager_set_banner("Resume Blocked — Check Sensors");
    }
}

static void on_discard(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_acknowledge_interrupted() == ESP_OK) {
        ui_manager_set_banner("Interrupted Batch Discarded");
    } else {
        ui_manager_set_banner("Could Not Discard Interrupted Batch");
    }
}

void dashboard_detach(void)
{
    s_alive = false;
    s_moist = {};
    s_hot = {};
    s_chamber = {};
    s_humidity = {};
    s_leds = {};
    s_note = nullptr;
    s_resume = nullptr;
    s_discard = nullptr;
    s_start = nullptr;
}

void dashboard_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "System Overview", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    s_moist = ui_metric_create(parent, "Rice Moisture", 236, 144);
    lv_obj_set_pos(s_moist.root, 0, 48);
    s_hot = ui_metric_create(parent, "Hot Air Temp", 236, 144);
    lv_obj_set_pos(s_hot.root, 252, 48);
    s_chamber = ui_metric_create(parent, "Chamber Temp", 236, 144);
    lv_obj_set_pos(s_chamber.root, 504, 48);
    s_humidity = ui_metric_create(parent, "Chamber Humidity", 236, 144);
    lv_obj_set_pos(s_humidity.root, 756, 48);

    s_leds = ui_actuator_row_create(parent, 992, 112);
    lv_obj_set_pos(s_leds.root, 0, 208);

    s_note = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_set_width(s_note, 480);
    lv_label_set_long_mode(s_note, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_note, 0, 360);

    s_resume = ui_theme_make_btn(parent, "Resume", on_resume, nullptr, 160, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(s_resume, 496, 344);

    s_discard =
        ui_theme_make_btn(parent, "Discard", on_discard, nullptr, 160, 64, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(s_discard, 672, 344);

    s_start =
        ui_theme_make_btn(parent, "Start Batch", on_start, nullptr, 320, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(s_start, 672, 344);

    s_alive = true;
    dashboard_update(status);
}

void dashboard_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    char value[32];
    char detail[48];
    if (status->moisture_ok) {
        snprintf(value, sizeof(value), "%.1f", status->moisture_pct);
    } else {
        snprintf(value, sizeof(value), "Offline");
    }
    snprintf(detail, sizeof(detail), "Target %.1f %%", status->target_moisture_pct);
    ui_metric_set(&s_moist, value, status->moisture_ok ? "%" : "", detail);

    if (status->hotair_ok) {
        snprintf(value, sizeof(value), "%.1f", status->hotair_temp_c);
        snprintf(detail, sizeof(detail), "Cap %.0f °C", status->max_hot_air_temperature_c);
    } else {
        snprintf(value, sizeof(value), "Offline");
        snprintf(detail, sizeof(detail), "Sensor unavailable");
    }
    ui_metric_set(&s_hot, value, status->hotair_ok ? "°C" : "", detail);

    if (status->chamber_ok) {
        snprintf(value, sizeof(value), "%.1f", status->chamber_temp_c);
        snprintf(detail, sizeof(detail), "Target %.0f °C", status->drying_temperature_c);
    } else {
        snprintf(value, sizeof(value), "Offline");
        snprintf(detail, sizeof(detail), "Sensor unavailable");
    }
    ui_metric_set(&s_chamber, value, status->chamber_ok ? "°C" : "", detail);

    if (status->chamber_ok) {
        snprintf(value, sizeof(value), "%.0f", status->chamber_rh);
        snprintf(detail, sizeof(detail), "Current");
    } else {
        snprintf(value, sizeof(value), "Offline");
        snprintf(detail, sizeof(detail), "Sensor unavailable");
    }
    ui_metric_set(&s_humidity, value, status->chamber_ok ? "%" : "", detail);

    ui_leds_set(&s_leds, status);

    if (status->batch_interrupted) {
        char note[96];
        snprintf(note, sizeof(note), "%s interrupted in %s. Resume or Discard.",
                 status->batch_id[0] ? status->batch_id : "Batch",
                 process_controller_state_name((dryer_state_t)status->interrupted_state));
        ui_label_set_text_if_changed(s_note, note);
        lv_obj_clear_flag(s_resume, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_discard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_start, LV_OBJ_FLAG_HIDDEN);
    } else if (status->active_fault != 0) {
        ui_label_set_text_if_changed(s_note, "Active Fault. Open Alarms Before Starting.");
        lv_obj_add_flag(s_resume, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_discard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_start, LV_OBJ_FLAG_HIDDEN);
    } else {
        ui_label_set_text_if_changed(s_note, "Load Rice, Then Start Batch");
        lv_obj_add_flag(s_resume, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_discard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_start, LV_OBJ_FLAG_HIDDEN);
    }
}
