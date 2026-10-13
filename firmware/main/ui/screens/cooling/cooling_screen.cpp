#include "cooling_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_temp = nullptr;
static lv_obj_t *s_outputs = nullptr;
static lv_obj_t *s_time = nullptr;

static void on_abort(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    process_controller_abort();
    ui_manager_set_banner("Batch aborted - outputs set safe");
}

void cooling_detach(void)
{
    s_alive = false;
    s_temp = nullptr;
    s_outputs = nullptr;
    s_time = nullptr;
}

void cooling_build(lv_obj_t *parent, const system_status_t *status)
{
    ui_label(parent, "COOLING", 8, 0, &lv_font_montserrat_24, 0x7EC8FF);
    s_temp = ui_label(parent, "", 8, 56, &lv_font_montserrat_20, 0xE8EEF4);
    s_outputs = ui_label(parent, "", 8, 170, &lv_font_montserrat_20, 0xF0C808);
    s_time = ui_label(parent, "", 8, 280, &lv_font_montserrat_20, 0x7EC8FF);
    lv_obj_t *abort = ui_make_btn(parent, "ABORT", on_abort, nullptr, 140, 48);
    lv_obj_set_pos(abort, 8, 350);
    s_alive = true;
    cooling_update(status);
}

void cooling_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    char line[160];
    char clock[16];
    ui_format_hms(clock, sizeof(clock), status->cooling_elapsed_s);
    snprintf(line, sizeof(line), "Temperature  %.1f C\nTarget       %.0f C",
             status->chamber_temp_c, status->cooling_temperature_c);
    lv_label_set_text(s_temp, line);
    snprintf(line, sizeof(line), "Fan %s\nHeater %s\nElevator %s\nDoor %s",
             ui_on_off(status->relay_fan), ui_on_off(status->relay_heater),
             ui_on_off(status->relay_elevator),
             ui_door_text(status->relay_door_open, status->relay_door_close));
    lv_label_set_text(s_outputs, line);
    snprintf(line, sizeof(line), "Cooling time  %s", clock);
    lv_label_set_text(s_time, line);
}
