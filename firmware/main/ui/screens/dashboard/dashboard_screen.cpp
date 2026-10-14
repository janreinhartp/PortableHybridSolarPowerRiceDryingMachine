#include "dashboard_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static ui_gauge_t s_moist {};
static ui_gauge_t s_chamber {};
static ui_kpi_t s_hot {};
static ui_kpi_t s_ec {};
static ui_actuator_leds_t s_leds {};
static lv_obj_t *s_note = nullptr;
static lv_obj_t *s_ack = nullptr;

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
    s_chamber = {};
    s_hot = {};
    s_ec = {};
    s_leds = {};
    s_note = nullptr;
    s_ack = nullptr;
}

void dashboard_build(lv_obj_t *parent, const system_status_t *status)
{
    s_moist = ui_gauge_create(parent, "Moisture", "%", 0, 30, 200);
    lv_obj_set_pos(s_moist.root, 8, 8);
    s_chamber = ui_gauge_create(parent, "Chamber", "C", 0, 80, 200);
    lv_obj_set_pos(s_chamber.root, 260, 8);

    s_hot = ui_kpi_create(parent, "Hot Air", 220, 88);
    lv_obj_set_pos(s_hot.root, 520, 8);
    s_ec = ui_kpi_create(parent, "Conductivity", 220, 88);
    lv_obj_set_pos(s_ec.root, 760, 8);

    s_leds = ui_leds_create(parent, 460);
    lv_obj_set_pos(s_leds.root, 520, 112);

    s_note = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_AMBER);
    lv_obj_set_width(s_note, 960);
    lv_obj_set_pos(s_note, 8, 310);

    lv_obj_t *start =
        ui_theme_make_btn(parent, "Start Batch", on_start, nullptr, 220, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(start, 8, 352);
    s_ack = ui_theme_make_btn(parent, "Acknowledge Interrupt", on_ack, nullptr, 260, 64,
                              UI_COLOR_ORANGE);
    lv_obj_set_pos(s_ack, 244, 352);

    s_alive = true;
    dashboard_update(status);
}

void dashboard_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    ui_gauge_set(&s_moist, status->moisture_pct);
    ui_gauge_set_target(&s_moist, true, status->target_moisture_pct);
    ui_gauge_set(&s_chamber, status->chamber_temp_c);
    ui_gauge_set_target(&s_chamber, true, status->drying_temperature_c);

    char buf[48];
    if (status->hotair_ok) {
        snprintf(buf, sizeof(buf), "%.1f C", status->hotair_temp_c);
    } else {
        snprintf(buf, sizeof(buf), "Offline");
    }
    ui_kpi_set(&s_hot, buf);
    if (status->ec_ok) {
        snprintf(buf, sizeof(buf), "%.2f", status->ec_value);
    } else {
        snprintf(buf, sizeof(buf), "Offline");
    }
    ui_kpi_set(&s_ec, buf);
    ui_leds_set(&s_leds, status);

    if (status->batch_interrupted) {
        lv_label_set_text(s_note, "Interrupted Batch Pending. Acknowledge Before A New Start.");
        lv_obj_clear_state(s_ack, LV_STATE_DISABLED);
    } else if (status->active_fault != 0) {
        lv_label_set_text(s_note, "Active Fault. Open Alarms Before Starting.");
        lv_obj_add_state(s_ack, LV_STATE_DISABLED);
    } else {
        lv_label_set_text(s_note, "Load Rice, Then Start Batch To Review Settings And Begin.");
        lv_obj_add_state(s_ack, LV_STATE_DISABLED);
    }
}
