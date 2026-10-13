#include "manual_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_note = nullptr;
static lv_obj_t *s_state = nullptr;
static lv_obj_t *s_btns[8] = {};

static void note_result(esp_err_t err, const char *ok_text)
{
    if (err == ESP_OK) {
        ui_manager_set_banner(ok_text);
    } else if (err == ESP_ERR_INVALID_STATE) {
        ui_manager_set_banner("Manual command refused");
    } else {
        ui_manager_set_banner("Manual command failed");
    }
}

static void on_fan(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_toggle_fan(), "Fan toggled");
    }
}

static void on_elev(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_toggle_elevator(), "Elevator toggled");
    }
}

static void on_heater(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_toggle_heater(), "Heater request sent");
    }
}

static void on_off(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_all_off(), "All outputs off");
    }
}

static void on_circ(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_toggle_circulation(), "Circulation toggled");
    }
}

static void on_open(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_door_open(), "Door opening");
    }
}

static void on_close(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_door_close(), "Door closing");
    }
}

static void on_stop(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        note_result(process_controller_manual_door_stop(), "Door stopped");
    }
}

void manual_detach(void)
{
    s_alive = false;
    s_note = nullptr;
    s_state = nullptr;
    for (size_t i = 0; i < sizeof(s_btns) / sizeof(s_btns[0]); ++i) {
        s_btns[i] = nullptr;
    }
}

void manual_build(lv_obj_t *parent, const system_status_t *status)
{
    ui_label(parent, "MANUAL CONTROL", 8, 0, &lv_font_montserrat_24, 0xE8EEF4);
    s_note = ui_label(parent,
                      "Commissioning and maintenance. Safety Manager can still force the heater off.",
                      8, 40, &lv_font_montserrat_14, 0x9BB0C4);
    lv_obj_set_width(s_note, 960);
    s_state = ui_label(parent, "", 8, 68, &lv_font_montserrat_16, 0xF0C808);

    const struct {
        const char *text;
        lv_event_cb_t cb;
        lv_coord_t x;
        lv_coord_t y;
        lv_coord_t w;
    } rows[] = {
        {"Fan", on_fan, 8, 120, 140},
        {"Elevator", on_elev, 160, 120, 140},
        {"Heater", on_heater, 312, 120, 140},
        {"All OFF", on_off, 464, 120, 140},
        {"Circulation", on_circ, 616, 120, 160},
        {"Door OPEN", on_open, 8, 180, 160},
        {"Door CLOSE", on_close, 180, 180, 160},
        {"Door STOP", on_stop, 352, 180, 160},
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i) {
        s_btns[i] = ui_make_btn(parent, rows[i].text, rows[i].cb, nullptr, rows[i].w, 48);
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
    snprintf(line, sizeof(line),
             "%s    Heater %s  Fan %s  Elevator %s  Door %s%s",
             allowed ? "Manual enabled" : "Manual locked while the batch is active",
             ui_on_off(status->relay_heater), ui_on_off(status->relay_fan),
             ui_on_off(status->relay_elevator),
             ui_door_text(status->relay_door_open, status->relay_door_close),
             status->heating_permitted ? "" : "    heater blocked by safety");
    lv_label_set_text(s_state, line);
}
