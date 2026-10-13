#include "dashboard_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_state = nullptr;
static lv_obj_t *s_batch = nullptr;
static lv_obj_t *s_sensors = nullptr;
static lv_obj_t *s_actuators = nullptr;
static lv_obj_t *s_note = nullptr;

static void on_setup(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_BATCH_SETUP);
    }
}

static void on_settings(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_SETTINGS);
    }
}

static void on_history(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_HISTORY);
    }
}

static void on_ack_int(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_acknowledge_interrupted() == ESP_OK) {
        ui_manager_set_banner("Interrupted batch acknowledged - START is allowed");
    } else {
        ui_manager_set_banner("ACK interrupted failed");
    }
}

static void on_abort(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    process_controller_abort();
    ui_manager_set_banner("Batch aborted - outputs set safe");
}

void dashboard_detach(void)
{
    s_alive = false;
    s_state = nullptr;
    s_batch = nullptr;
    s_sensors = nullptr;
    s_actuators = nullptr;
    s_note = nullptr;
}

void dashboard_build(lv_obj_t *parent, const system_status_t *status)
{
    s_state = ui_label(parent, "IDLE", 8, 0, &lv_font_montserrat_24, 0xE8EEF4);
    s_batch = ui_label(parent, "", 8, 40, &lv_font_montserrat_16, 0x9BB0C4);
    s_sensors = ui_label(parent, "", 8, 78, &lv_font_montserrat_20, 0xE8EEF4);
    lv_obj_set_width(s_sensors, 960);
    s_actuators = ui_label(parent, "", 8, 210, &lv_font_montserrat_16, 0xF0C808);
    s_note = ui_label(parent, "", 8, 268, &lv_font_montserrat_14, 0xF2A65A);
    lv_obj_set_width(s_note, 960);

    ui_make_btn(parent, "START BATCH", on_setup, nullptr, 180, 48);
    lv_obj_align(lv_obj_get_child(parent, -1), LV_ALIGN_TOP_LEFT, 8, 320);
    ui_make_btn(parent, "SETTINGS", on_settings, nullptr, 150, 48);
    lv_obj_align(lv_obj_get_child(parent, -1), LV_ALIGN_TOP_LEFT, 204, 320);
    ui_make_btn(parent, "HISTORY", on_history, nullptr, 150, 48);
    lv_obj_align(lv_obj_get_child(parent, -1), LV_ALIGN_TOP_LEFT, 370, 320);
    ui_make_btn(parent, "ACK INT", on_ack_int, nullptr, 140, 48);
    lv_obj_align(lv_obj_get_child(parent, -1), LV_ALIGN_TOP_LEFT, 536, 320);
    ui_make_btn(parent, "ABORT", on_abort, nullptr, 120, 48);
    lv_obj_align(lv_obj_get_child(parent, -1), LV_ALIGN_TOP_LEFT, 692, 320);

    s_alive = true;
    dashboard_update(status);
}

void dashboard_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    char line[192];
    snprintf(line, sizeof(line), "%s",
             process_controller_state_name((dryer_state_t)status->dryer_state));
    lv_label_set_text(s_state, line);
    lv_obj_set_style_text_color(s_state, ui_state_color(status->dryer_state), 0);

    snprintf(line, sizeof(line), "Batch %s    %s",
             status->batch_id[0] ? status->batch_id : "-",
             status->batch_interrupted ? "INTERRUPTED - acknowledge before a new start" : "");
    lv_label_set_text(s_batch, line);

    char elapsed[16];
    ui_format_hms(elapsed, sizeof(elapsed), status->drying_elapsed_s);
    snprintf(line, sizeof(line),
             "Moisture  %s %.1f %%     EC  %s %.2f\n"
             "Hot-air   %s %.1f C   %.1f %%RH\n"
             "Chamber   %s %.1f C   %.1f %%RH\n"
             "Target    %.1f %% at %.0f C     cooling %.0f C     dry time %s",
             status->moisture_ok ? "" : "OFFLINE",
             status->moisture_pct,
             status->ec_ok ? "" : "OFFLINE",
             status->ec_value,
             status->hotair_ok ? "" : "OFFLINE",
             status->hotair_temp_c, status->hotair_rh,
             status->chamber_ok ? "" : "OFFLINE",
             status->chamber_temp_c, status->chamber_rh,
             status->target_moisture_pct, status->drying_temperature_c,
             status->cooling_temperature_c, elapsed);
    lv_label_set_text(s_sensors, line);

    snprintf(line, sizeof(line), "Heater %s    Fan %s    Elevator %s    Door %s",
             ui_on_off(status->relay_heater), ui_on_off(status->relay_fan),
             ui_on_off(status->relay_elevator),
             ui_door_text(status->relay_door_open, status->relay_door_close));
    lv_label_set_text(s_actuators, line);

    if (status->batch_interrupted) {
        lv_label_set_text(s_note, "An interrupted batch is waiting. Press ACK INT, then START BATCH.");
    } else if (status->active_fault != 0) {
        lv_label_set_text(s_note, "A fault is active. Open Alarms before starting.");
    } else if (!status->precheck_ready) {
        lv_label_set_text(s_note, "START BATCH opens setup. Precheck is not ready yet.");
    } else {
        lv_label_set_text(s_note, "Load rice, review setup, then start. Heater power stays off until safety tests pass.");
    }
}
