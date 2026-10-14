#include "alarms_screen.h"

#include <cstdio>

#include "logger.h"
#include "process_controller.h"
#include "safety_manager.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_card = nullptr;
static lv_obj_t *s_code = nullptr;
static lv_obj_t *s_hint = nullptr;
static lv_obj_t *s_rows[LOGGER_ALARM_MAX] = {};

static void on_ack(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_acknowledge_fault() == ESP_OK) {
        ui_manager_set_banner("Fault Acknowledged");
    } else {
        ui_manager_set_banner("Acknowledge Refused - Safety Condition Still Active");
    }
}

void alarms_detach(void)
{
    s_alive = false;
    s_card = nullptr;
    s_code = nullptr;
    s_hint = nullptr;
    for (size_t i = 0; i < LOGGER_ALARM_MAX; ++i) {
        s_rows[i] = nullptr;
    }
}

void alarms_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title = ui_theme_make_label(parent, "Alarms", ui_font_title(), UI_COLOR_RED);
    lv_obj_set_pos(title, 8, 4);

    s_card = ui_theme_make_card(parent, 760, 130);
    lv_obj_set_pos(s_card, 8, 44);
    s_code = ui_theme_make_label(s_card, "", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_width(s_code, 720);
    lv_label_set_long_mode(s_code, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_code, LV_ALIGN_TOP_LEFT, 0, 0);
    s_hint = ui_theme_make_label(s_card, "", ui_font_body(), UI_COLOR_AMBER);
    lv_obj_set_width(s_hint, 720);
    lv_label_set_long_mode(s_hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_hint, LV_ALIGN_TOP_LEFT, 0, 56);

    lv_obj_t *ack = ui_theme_make_btn(parent, "Acknowledge Fault", on_ack, nullptr, 200, 72,
                                      UI_COLOR_ORANGE);
    lv_obj_set_pos(ack, 784, 44);

    lv_obj_t *recent =
        ui_theme_make_label(parent, "Recent Alarms", ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_pos(recent, 8, 188);

    lv_obj_t *list = ui_theme_make_card(parent, 980, 220);
    lv_obj_set_pos(list, 8, 220);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_row(list, 4, 0);

    for (size_t i = 0; i < LOGGER_ALARM_MAX; ++i) {
        s_rows[i] = ui_theme_make_label(list, "", ui_font_body(), UI_COLOR_TEXT);
        lv_obj_set_width(s_rows[i], 940);
        lv_label_set_long_mode(s_rows[i], LV_LABEL_LONG_DOT);
        lv_obj_set_pos(s_rows[i], 0, (lv_coord_t)((int)i * 26));
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
        lv_label_set_text(s_code, "No Active Fault");
        lv_obj_set_style_text_color(s_code, ui_theme_color(UI_COLOR_TEAL), 0);
        lv_label_set_text(s_hint, "Acknowledgement Does Not Bypass An Active Safety Condition.");
        lv_obj_set_style_bg_color(s_card, ui_theme_color(UI_COLOR_SURFACE), 0);
    } else {
        char line[180];
        snprintf(line, sizeof(line), "%s\n%s   %s", safety_manager_fault_name(code),
                 status->fault_state[0] ? status->fault_state : "?",
                 status->fault_time[0] ? status->fault_time : "");
        lv_label_set_text(s_code, line);
        lv_obj_set_style_text_color(s_code, ui_theme_color(UI_COLOR_RED), 0);
        lv_label_set_text(s_hint, safety_manager_fault_hint(code));
        lv_obj_set_style_bg_color(s_card, ui_theme_color(UI_COLOR_FAULT_BG), 0);
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
        char line[112];
        snprintf(line, sizeof(line), "%.23s   %.39s   %.15s", rows[i].timestamp, rows[i].code,
                 rows[i].state);
        lv_label_set_text(s_rows[i], line);
    }
}
