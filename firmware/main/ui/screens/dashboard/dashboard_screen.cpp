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
static ui_metric_t s_ec {};
static ui_actuator_leds_t s_leds {};
static lv_obj_t *s_note = nullptr;
static lv_obj_t *s_ack = nullptr;
static lv_obj_t *s_start = nullptr;

static void on_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_manager_navigate(UI_SCREEN_BATCH_SETUP);
}

static void on_ack(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_acknowledge_interrupted() == ESP_OK) {
        ui_manager_set_banner("Interrupted Batch Acknowledged");
    } else {
        ui_manager_set_banner("Could Not Acknowledge Interrupted Batch");
    }
}

void dashboard_detach(void)
{
    s_alive = false;
    s_moist = {};
    s_hot = {};
    s_chamber = {};
    s_humidity = {};
    s_ec = {};
    s_leds = {};
    s_note = nullptr;
    s_ack = nullptr;
    s_start = nullptr;
}

void dashboard_build(lv_obj_t *parent, const system_status_t *status)
{
    /* Content parent already has 16 px pad — positions match Figma content−16. */
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

    s_ec = ui_metric_create(parent, "EC", 236, 112);
    lv_obj_set_pos(s_ec.root, 0, 208);

    s_leds = ui_actuator_row_create(parent, 740, 112);
    lv_obj_set_pos(s_leds.root, 252, 208);

    s_note = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_set_width(s_note, 640);
    lv_label_set_long_mode(s_note, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_note, 40, 360);

    s_ack = ui_theme_make_btn(parent, "Acknowledge Interrupt", on_ack, nullptr, 260, 64,
                              UI_COLOR_ORANGE);
    lv_obj_set_pos(s_ack, 392, 344);

    s_start =
        ui_theme_make_btn(parent, "Start Batch", on_start, nullptr, 320, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(s_start, 656, 344);

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
        snprintf(detail, sizeof(detail), "Target %.0f °C", status->drying_temperature_c);
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

    if (status->ec_ok) {
        snprintf(value, sizeof(value), "%.2f", status->ec_value);
        snprintf(detail, sizeof(detail), "Conductivity");
    } else {
        snprintf(value, sizeof(value), "Offline");
        snprintf(detail, sizeof(detail), "Sensor unavailable");
    }
    ui_metric_set(&s_ec, value, "", detail);
    ui_leds_set(&s_leds, status);

    if (status->batch_interrupted) {
        lv_label_set_text(s_note, "Interrupted Batch Pending. Acknowledge Before A New Start.");
        lv_obj_clear_state(s_ack, LV_STATE_DISABLED);
        lv_obj_clear_flag(s_ack, LV_OBJ_FLAG_HIDDEN);
    } else if (status->active_fault != 0) {
        lv_label_set_text(s_note, "Active Fault. Open Alarms Before Starting.");
        lv_obj_add_state(s_ack, LV_STATE_DISABLED);
        lv_obj_add_flag(s_ack, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(s_note, "Load Rice, Then Start Batch");
        lv_obj_add_state(s_ack, LV_STATE_DISABLED);
        lv_obj_add_flag(s_ack, LV_OBJ_FLAG_HIDDEN);
    }
}
