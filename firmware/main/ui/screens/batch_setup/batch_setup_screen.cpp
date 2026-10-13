#include "batch_setup_screen.h"

#include <cstdio>
#include <cstring>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_review = nullptr;
static lv_obj_t *s_ready = nullptr;
static lv_obj_t *s_start = nullptr;

static void on_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    esp_err_t err = process_controller_start();
    if (err == ESP_OK) {
        ui_manager_set_banner("Batch started - running precheck");
    } else {
        ui_manager_set_banner("START refused - acknowledge faults or wait for IDLE");
    }
}

static void on_settings(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_SETTINGS);
    }
}

static void on_ack(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_acknowledge_interrupted() == ESP_OK) {
        ui_manager_set_banner("Interrupted batch acknowledged");
    } else {
        ui_manager_set_banner("ACK interrupted failed");
    }
}

void batch_setup_detach(void)
{
    s_alive = false;
    s_review = nullptr;
    s_ready = nullptr;
    s_start = nullptr;
}

void batch_setup_build(lv_obj_t *parent, const system_status_t *status)
{
    ui_label(parent, "Confirm these settings before starting", 8, 0,
             &lv_font_montserrat_16, 0x9BB0C4);

    s_review = ui_label(parent, "", 8, 36, &lv_font_montserrat_20, 0xE8EEF4);
    lv_obj_set_width(s_review, 960);

    s_ready = ui_label(parent, "", 8, 250, &lv_font_montserrat_16, 0xF2A65A);
    lv_obj_set_width(s_ready, 960);
    lv_label_set_long_mode(s_ready, LV_LABEL_LONG_WRAP);

    s_start = ui_make_btn(parent, "START", on_start, nullptr, 160, 48);
    lv_obj_align(s_start, LV_ALIGN_TOP_LEFT, 8, 360);
    lv_obj_t *settings = ui_make_btn(parent, "EDIT SETTINGS", on_settings, nullptr, 180, 48);
    lv_obj_align(settings, LV_ALIGN_TOP_LEFT, 184, 360);
    lv_obj_t *ack = ui_make_btn(parent, "ACK INT", on_ack, nullptr, 140, 48);
    lv_obj_align(ack, LV_ALIGN_TOP_LEFT, 380, 360);

    s_alive = true;
    batch_setup_update(status);
}

void batch_setup_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    char line[320];
    snprintf(line, sizeof(line),
             "Target moisture     %.1f %%\n"
             "Drying temperature  %.0f C    hysteresis %.1f C\n"
             "Cooling temperature %.0f C\n"
             "Circulation         %s    every %u s    run %u s\n"
             "Discharge time      %u s\n"
             "Drying limits       %u s min    %u s max",
             status->target_moisture_pct, status->drying_temperature_c,
             status->temperature_hysteresis_c, status->cooling_temperature_c,
             ui_circ_mode_name(status->circulation_mode),
             (unsigned)status->circulation_interval_s,
             (unsigned)status->circulation_run_time_s,
             (unsigned)status->discharge_time_s,
             (unsigned)status->min_drying_time_s,
             (unsigned)status->max_drying_time_s);
    lv_label_set_text(s_review, line);

    char why[256];
    why[0] = '\0';
    if (status->batch_interrupted) {
        strncat(why, "Acknowledge the interrupted batch. ", sizeof(why) - strlen(why) - 1);
    }
    if (status->active_fault != 0) {
        strncat(why, "Active fault. ", sizeof(why) - strlen(why) - 1);
    }
    if (status->dryer_state != DRYER_STATE_IDLE &&
        status->dryer_state != DRYER_STATE_COMPLETE &&
        status->dryer_state != DRYER_STATE_PRECHECK) {
        strncat(why, "A batch is already running. ", sizeof(why) - strlen(why) - 1);
    }
    if (!status->sd_ok || !status->logger_ok) {
        strncat(why, "SD logging unavailable. ", sizeof(why) - strlen(why) - 1);
    }
    if (!status->rtc_ok) {
        strncat(why, "RTC offline. ", sizeof(why) - strlen(why) - 1);
    }
    if (!status->relay_ok) {
        strncat(why, "Relay board offline. ", sizeof(why) - strlen(why) - 1);
    }
    if (!status->hotair_ok) {
        strncat(why, "Hot-air sensor offline. ", sizeof(why) - strlen(why) - 1);
    }
    if (!status->chamber_ok) {
        strncat(why, "Chamber sensor offline. ", sizeof(why) - strlen(why) - 1);
    }
    if (!status->moisture_ok) {
        strncat(why, "Moisture sensor invalid. ", sizeof(why) - strlen(why) - 1);
    }

    const bool start_ok = status->precheck_ready &&
                          (status->dryer_state == DRYER_STATE_IDLE ||
                           status->dryer_state == DRYER_STATE_COMPLETE);
    if (start_ok) {
        lv_label_set_text(s_ready, "Precheck can be satisfied. START will begin the batch.");
        lv_obj_set_style_text_color(s_ready, lv_color_hex(0x7DCFB6), 0);
        lv_obj_clear_state(s_start, LV_STATE_DISABLED);
    } else if (status->dryer_state == DRYER_STATE_PRECHECK) {
        lv_label_set_text(s_ready, "Precheck is running.");
        lv_obj_set_style_text_color(s_ready, lv_color_hex(0xC9B6FF), 0);
        lv_obj_add_state(s_start, LV_STATE_DISABLED);
    } else {
        if (why[0] == '\0') {
            strncat(why, "Settings are not valid for a start.", sizeof(why) - strlen(why) - 1);
        }
        lv_label_set_text(s_ready, why);
        lv_obj_set_style_text_color(s_ready, lv_color_hex(0xF2A65A), 0);
        lv_obj_add_state(s_start, LV_STATE_DISABLED);
    }
}
