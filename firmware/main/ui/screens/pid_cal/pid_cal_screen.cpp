#include "pid_cal_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "ui_manager.h"
#include "ui_theme.h"

static bool s_alive = false;
static lv_obj_t *s_temp = nullptr;
static lv_obj_t *s_sp = nullptr;
static lv_obj_t *s_cycles = nullptr;
static lv_obj_t *s_note = nullptr;
static lv_obj_t *s_stop = nullptr;
static lv_obj_t *s_back = nullptr;

static void on_stop(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        process_controller_stop_pid_cal();
    }
}

static void on_back(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_SETTINGS);
    }
}

void pid_cal_detach(void)
{
    if (s_alive && process_controller_pid_cal_active()) {
        process_controller_stop_pid_cal();
    }
    s_alive = false;
    s_temp = nullptr;
    s_sp = nullptr;
    s_cycles = nullptr;
    s_note = nullptr;
    s_stop = nullptr;
    s_back = nullptr;
}

void pid_cal_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "Calibrate PID", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    lv_obj_t *hint = ui_theme_make_label(
        parent, "Heater oscillates around the hot-air setpoint. Leave the chamber empty.",
        ui_font_body(), UI_COLOR_AMBER);
    lv_obj_set_width(hint, 992);
    lv_obj_set_pos(hint, 0, 48);

    lv_obj_t *card = ui_theme_make_card(parent, 992, 220);
    lv_obj_set_pos(card, 0, 96);
    s_temp = ui_theme_make_label(card, "Hot Air -- °C", ui_font_title(), UI_COLOR_CREAM);
    lv_obj_align(s_temp, LV_ALIGN_TOP_LEFT, 0, 0);
    s_sp = ui_theme_make_label(card, "Setpoint -- °C", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(s_sp, LV_ALIGN_TOP_LEFT, 0, 48);
    s_cycles = ui_theme_make_label(card, "Cycles 0 / 2", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(s_cycles, LV_ALIGN_TOP_LEFT, 0, 88);
    s_note = ui_theme_make_label(card, "Running", ui_font_body(), UI_COLOR_TEAL);
    lv_obj_set_width(s_note, 900);
    lv_obj_align(s_note, LV_ALIGN_TOP_LEFT, 0, 132);

    s_stop = ui_theme_make_btn(parent, "Stop", on_stop, nullptr, 488, 56, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(s_stop, 0, 340);
    s_back = ui_theme_make_btn(parent, "Back", on_back, nullptr, 488, 56, UI_COLOR_TEAL);
    lv_obj_set_pos(s_back, 504, 340);

    s_alive = true;
    pid_cal_update(status);
}

void pid_cal_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    const bool running = process_controller_pid_cal_active();
    char buf[64];
    if (status->hotair_ok) {
        snprintf(buf, sizeof(buf), "Hot Air %.1f °C", (double)status->hotair_temp_c);
    } else {
        snprintf(buf, sizeof(buf), "Hot Air -- °C");
    }
    ui_label_set_text_if_changed(s_temp, buf);

    const float sp = (running && status->pid_cal_setpoint_c > 0.0f)
                          ? status->pid_cal_setpoint_c
                          : status->drying_temperature_c;
    snprintf(buf, sizeof(buf), "Setpoint %.0f °C", (double)sp);
    ui_label_set_text_if_changed(s_sp, buf);

    snprintf(buf, sizeof(buf), "Cycles %u / 2", (unsigned)status->pid_cal_cycles);
    ui_label_set_text_if_changed(s_cycles, buf);

    if (running) {
        ui_label_set_text_if_changed(s_note, "Running. Stop leaves the old gains.");
        lv_obj_clear_flag(s_stop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_back, LV_OBJ_FLAG_HIDDEN);
    } else {
        ui_label_set_text_if_changed(
            s_note, status->pid_cal_note[0] ? status->pid_cal_note : "Calibration Stopped");
        lv_obj_add_flag(s_stop, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_back, LV_OBJ_FLAG_HIDDEN);
    }
}
