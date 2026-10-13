#include "alarms_screen.h"

#include <cstdio>

#include "logger.h"
#include "process_controller.h"
#include "safety_manager.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_active = nullptr;
static lv_obj_t *s_hint = nullptr;
static lv_obj_t *s_rows[LOGGER_ALARM_MAX] = {};

static void on_ack(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_acknowledge_fault() == ESP_OK) {
        ui_manager_set_banner("Fault acknowledged");
    } else {
        ui_manager_set_banner("ACK refused - safety condition is still active");
    }
}

void alarms_detach(void)
{
    s_alive = false;
    s_active = nullptr;
    s_hint = nullptr;
    for (size_t i = 0; i < LOGGER_ALARM_MAX; ++i) {
        s_rows[i] = nullptr;
    }
}

void alarms_build(lv_obj_t *parent, const system_status_t *status)
{
    ui_label(parent, "ALARMS", 8, 0, &lv_font_montserrat_24, 0xE85D4C);
    s_active = ui_label(parent, "", 8, 44, &lv_font_montserrat_20, 0xE8EEF4);
    lv_obj_set_width(s_active, 760);
    s_hint = ui_label(parent, "", 8, 110, &lv_font_montserrat_16, 0xF2A65A);
    lv_obj_set_width(s_hint, 960);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);

    lv_obj_t *ack = ui_make_btn(parent, "ACK Fault", on_ack, nullptr, 150, 44);
    lv_obj_set_pos(ack, 800, 44);

    ui_label(parent, "Recent", 8, 170, &lv_font_montserrat_14, 0x9BB0C4);
    for (size_t i = 0; i < LOGGER_ALARM_MAX; ++i) {
        s_rows[i] = ui_label(parent, "", 8, (lv_coord_t)(196 + (int)i * 28),
                             &lv_font_montserrat_14, 0xE8EEF4);
        lv_obj_set_width(s_rows[i], 960);
    }
    s_alive = true;
    alarms_update(status);
}

void alarms_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    const fault_code_t code = (fault_code_t)status->active_fault;
    if (code == FAULT_NONE) {
        lv_label_set_text(s_active, "No active fault");
        lv_obj_set_style_text_color(s_active, lv_color_hex(0x7DCFB6), 0);
        lv_label_set_text(s_hint, "Acknowledgement does not bypass an active safety condition.");
    } else {
        char line[160];
        snprintf(line, sizeof(line), "%s\n%s   %s", safety_manager_fault_name(code),
                 status->fault_state[0] ? status->fault_state : "state ?",
                 status->fault_time[0] ? status->fault_time : "");
        lv_label_set_text(s_active, line);
        lv_obj_set_style_text_color(s_active, lv_color_hex(0xE85D4C), 0);
        lv_label_set_text(s_hint, safety_manager_fault_hint(code));
    }

    logger_alarm_t rows[LOGGER_ALARM_MAX] = {};
    const size_t n = logger_copy_alarms(rows, LOGGER_ALARM_MAX);
    for (size_t i = 0; i < LOGGER_ALARM_MAX; ++i) {
        if (s_rows[i] == nullptr) {
            continue;
        }
        if (i >= n) {
            lv_label_set_text(s_rows[i], "");
            continue;
        }
        char line[96];
        snprintf(line, sizeof(line), "%.23s   %.39s   %.15s", rows[i].timestamp, rows[i].code,
                 rows[i].state);
        lv_label_set_text(s_rows[i], line);
    }
}
