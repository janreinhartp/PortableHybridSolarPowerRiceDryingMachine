#include "manual_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_banner = nullptr;
static lv_obj_t *s_status_lbls[4] = {};
static lv_obj_t *s_leds[4] = {};
static lv_obj_t *s_btns[10] = {};
static bool s_heater = false;
static bool s_fan = false;
static bool s_elev = false;

static void note_result(esp_err_t err, const char *ok_text)
{
    if (err == ESP_OK) {
        ui_manager_set_banner(ok_text);
    } else {
        ui_manager_set_banner("Manual Command Refused");
    }
}

static void on_heater_on(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && !s_heater) {
        note_result(process_controller_manual_toggle_heater(), "Heater On");
    }
}
static void on_heater_off(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && s_heater) {
        note_result(process_controller_manual_toggle_heater(), "Heater Off");
    }
}
static void on_fan_on(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && !s_fan) {
        note_result(process_controller_manual_toggle_fan(), "Fan On");
    }
}
static void on_fan_off(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && s_fan) {
        note_result(process_controller_manual_toggle_fan(), "Fan Off");
    }
}
static void on_elev_on(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && !s_elev) {
        note_result(process_controller_manual_toggle_elevator(), "Elevator On");
    }
}
static void on_elev_off(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && s_elev) {
        note_result(process_controller_manual_toggle_elevator(), "Elevator Off");
    }
}
static void on_open(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_door_open(), "Door Opening");
    }
}
static void on_close(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_door_close(), "Door Closing");
    }
}
static void on_all_off(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_all_off(), "All Outputs Off");
    }
}
static void on_circ(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_toggle_circulation(), "Circulation Toggled");
    }
}

static void make_row(lv_obj_t *parent, lv_coord_t y, const char *name, int idx,
                     lv_event_cb_t on_a, lv_event_cb_t on_b, const char *a_lbl, const char *b_lbl)
{
    lv_obj_t *row = ui_theme_make_card(parent, 992, 64);
    lv_obj_set_pos(row, 0, y);
    lv_obj_t *lbl = ui_theme_make_label(row, name, ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    s_leds[idx] = lv_obj_create(row);
    lv_obj_set_size(s_leds[idx], 12, 12);
    lv_obj_set_style_radius(s_leds[idx], LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_leds[idx], 0, 0);
    lv_obj_clear_flag(s_leds[idx], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_leds[idx], LV_ALIGN_LEFT_MID, 120, 0);

    s_status_lbls[idx] = ui_theme_make_label(row, "OFF", ui_font_body(), UI_COLOR_DIM);
    lv_obj_align(s_status_lbls[idx], LV_ALIGN_LEFT_MID, 140, 0);

    s_btns[idx * 2] =
        ui_theme_make_btn(row, a_lbl, on_a, nullptr, 112, 48, UI_COLOR_TEAL);
    lv_obj_align(s_btns[idx * 2], LV_ALIGN_RIGHT_MID, -128, 0);
    s_btns[idx * 2 + 1] =
        ui_theme_make_btn(row, b_lbl, on_b, nullptr, 112, 48, UI_COLOR_SURFACE_ALT);
    lv_obj_align(s_btns[idx * 2 + 1], LV_ALIGN_RIGHT_MID, 0, 0);
}

void manual_detach(void)
{
    s_alive = false;
    s_banner = nullptr;
    for (int i = 0; i < 4; ++i) {
        s_status_lbls[i] = nullptr;
        s_leds[i] = nullptr;
    }
    for (int i = 0; i < 10; ++i) {
        s_btns[i] = nullptr;
    }
}

void manual_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "Manual Control", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    s_banner = ui_theme_make_card(parent, 992, 48);
    lv_obj_set_pos(s_banner, 0, 40);
    lv_obj_t *banner_lbl = ui_theme_make_label(
        s_banner, "Technician Mode. Safety Manager Can Still Force The Heater Off.",
        ui_font_body(), UI_COLOR_AMBER);
    lv_obj_align(banner_lbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_user_data(s_banner, banner_lbl);

    make_row(parent, 96, "Heater", 0, on_heater_on, on_heater_off, "On", "Off");
    make_row(parent, 168, "Fan", 1, on_fan_on, on_fan_off, "On", "Off");
    make_row(parent, 240, "Elevator", 2, on_elev_on, on_elev_off, "On", "Off");
    make_row(parent, 312, "Door", 3, on_open, on_close, "Open", "Close");

    s_btns[8] =
        ui_theme_make_btn(parent, "All Off", on_all_off, nullptr, 240, 48, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(s_btns[8], 0, 384);
    s_btns[9] = ui_theme_make_btn(parent, "Circulation", on_circ, nullptr, 240, 48, UI_COLOR_TEAL);
    lv_obj_set_pos(s_btns[9], 256, 384);

    s_alive = true;
    manual_update(status);
}

void manual_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    s_heater = status->relay_heater;
    s_fan = status->relay_fan;
    s_elev = status->relay_elevator;

    const char *texts[4] = {
        s_heater ? "ON" : "OFF",
        s_fan ? "ON" : "OFF",
        s_elev ? "RUNNING" : "OFF",
        ui_door_text(status->relay_door_open, status->relay_door_close),
    };
    const bool active[4] = {s_heater, s_fan, s_elev, status->relay_door_open};
    for (int i = 0; i < 4; ++i) {
        if (s_status_lbls[i] != nullptr) {
            lv_label_set_text(s_status_lbls[i], texts[i]);
            lv_obj_set_style_text_color(
                s_status_lbls[i], ui_theme_color(active[i] ? UI_COLOR_TEAL : UI_COLOR_DIM), 0);
        }
        if (s_leds[i] != nullptr) {
            lv_obj_set_style_bg_color(
                s_leds[i], ui_theme_color(active[i] ? UI_COLOR_TEAL : UI_COLOR_BORDER), 0);
        }
    }

    lv_obj_t *banner_lbl =
        s_banner != nullptr ? (lv_obj_t *)lv_obj_get_user_data(s_banner) : nullptr;
    const bool allowed = process_controller_manual_control_allowed();
    if (banner_lbl != nullptr) {
        lv_label_set_text(banner_lbl,
                          allowed ? "Manual Available. Safety Interlock Permits Commands."
                                  : "Locked While A Batch Is Active.");
        lv_obj_set_style_text_color(banner_lbl,
                                    ui_theme_color(allowed ? UI_COLOR_TEAL : UI_COLOR_AMBER), 0);
    }
    for (int i = 0; i < 10; ++i) {
        if (s_btns[i] == nullptr) {
            continue;
        }
        if (allowed) {
            lv_obj_clear_state(s_btns[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_btns[i], LV_STATE_DISABLED);
        }
    }
}
