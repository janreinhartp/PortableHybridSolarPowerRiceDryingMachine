#include "manual_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_banner = nullptr;
static lv_obj_t *s_state = nullptr;
static lv_obj_t *s_btns[8] = {};

static void note_result(esp_err_t err, const char *ok_text)
{
    if (err == ESP_OK) {
        ui_manager_set_banner(ok_text);
    } else {
        ui_manager_set_banner("Manual Command Refused");
    }
}

static void on_fan(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_toggle_fan(), "Fan Toggled");
    }
}
static void on_elev(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_toggle_elevator(), "Elevator Toggled");
    }
}
static void on_heater(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_toggle_heater(), "Heater Request Sent");
    }
}
static void on_off(lv_event_t *e)
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
static void on_stop(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_door_stop(), "Door Stopped");
    }
}

void manual_detach(void)
{
    s_alive = false;
    s_banner = nullptr;
    s_state = nullptr;
    for (size_t i = 0; i < sizeof(s_btns) / sizeof(s_btns[0]); ++i) {
        s_btns[i] = nullptr;
    }
}

void manual_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "Manual Control", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 8, 4);
    s_banner = ui_theme_make_label(
        parent, "Technician Mode. Safety Manager Can Still Force The Heater Off.",
        ui_font_body(), UI_COLOR_AMBER);
    lv_obj_set_width(s_banner, 980);
    lv_obj_set_pos(s_banner, 8, 40);
    s_state = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_pos(s_state, 8, 72);

    struct {
        const char *text;
        lv_event_cb_t cb;
        uint32_t color;
        lv_coord_t x;
        lv_coord_t y;
    } rows[] = {
        {"Fan", on_fan, UI_COLOR_ACCENT, 8, 120},
        {"Elevator", on_elev, UI_COLOR_ACCENT, 220, 120},
        {"Heater", on_heater, UI_COLOR_HEATER, 432, 120},
        {"All Off", on_off, UI_COLOR_SURFACE_ALT, 644, 120},
        {"Circulation", on_circ, UI_COLOR_TEAL, 8, 210},
        {"Door Open", on_open, UI_COLOR_ORANGE, 220, 210},
        {"Door Close", on_close, UI_COLOR_ORANGE, 432, 210},
        {"Door Stop", on_stop, UI_COLOR_SURFACE_ALT, 644, 210},
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        s_btns[i] = ui_theme_make_btn(parent, rows[i].text, rows[i].cb, nullptr, 200, 72,
                                      rows[i].color);
        lv_obj_set_pos(s_btns[i], rows[i].x, rows[i].y);
    }
    s_alive = true;
    manual_update(status);
}

void manual_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    const bool allowed = process_controller_manual_control_allowed();
    for (size_t i = 0; i < sizeof(s_btns) / sizeof(s_btns[0]); ++i) {
        if (s_btns[i] == nullptr) {
            continue;
        }
        if (allowed) {
            lv_obj_clear_state(s_btns[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_btns[i], LV_STATE_DISABLED);
        }
    }
    char line[192];
    snprintf(line, sizeof(line), "%s    Heater %s  Fan %s  Elev %s  Door %s",
             allowed ? "Manual Enabled" : "Locked While A Batch Is Active",
             ui_on_off(status->relay_heater), ui_on_off(status->relay_fan),
             ui_on_off(status->relay_elevator),
             ui_door_text(status->relay_door_open, status->relay_door_close));
    lv_label_set_text(s_state, line);
}
