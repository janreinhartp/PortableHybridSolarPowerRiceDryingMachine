#include "discharge_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_status = nullptr;
static lv_obj_t *s_remain = nullptr;
static lv_obj_t *s_bar = nullptr;

static const char *phase_name(uint8_t phase)
{
    switch (phase) {
    case 1:
        return "Door settling";
    case 2:
        return "Elevator running";
    case 3:
        return "Door closing";
    default:
        return "Opening door";
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

void discharge_detach(void)
{
    s_alive = false;
    s_status = nullptr;
    s_remain = nullptr;
    s_bar = nullptr;
}

void discharge_build(lv_obj_t *parent, const system_status_t *status)
{
    ui_label(parent, "DISCHARGING", 8, 0, &lv_font_montserrat_24, 0xF2A65A);
    s_status = ui_label(parent, "", 8, 56, &lv_font_montserrat_20, 0xE8EEF4);
    s_remain = ui_label(parent, "", 8, 160, &lv_font_montserrat_24, 0xF0C808);
    s_bar = lv_bar_create(parent);
    lv_obj_set_size(s_bar, 640, 18);
    lv_obj_set_pos(s_bar, 8, 220);
    lv_bar_set_range(s_bar, 0, 100);
    lv_obj_t *abort = ui_make_btn(parent, "ABORT", on_abort, nullptr, 140, 48);
    lv_obj_set_pos(abort, 8, 270);
    s_alive = true;
    discharge_update(status);
}

void discharge_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    char line[160];
    char clock[16];
    ui_format_hms(clock, sizeof(clock), status->discharge_remaining_s);
    snprintf(line, sizeof(line), "Door %s\nElevator %s\nPhase  %s",
             ui_door_text(status->relay_door_open, status->relay_door_close),
             ui_on_off(status->relay_elevator), phase_name(status->discharge_phase));
    lv_label_set_text(s_status, line);
    snprintf(line, sizeof(line), "Remaining  %s", clock);
    lv_label_set_text(s_remain, line);

    int pct = 0;
    if (status->discharge_phase == 2 && status->discharge_time_s > 0) {
        const uint32_t done =
            status->discharge_time_s > status->discharge_remaining_s
                ? status->discharge_time_s - status->discharge_remaining_s
                : 0;
        pct = (int)((done * 100u) / status->discharge_time_s);
    }
    lv_bar_set_value(s_bar, pct, LV_ANIM_OFF);
}
