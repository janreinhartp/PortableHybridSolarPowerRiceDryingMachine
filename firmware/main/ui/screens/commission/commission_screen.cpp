#include "commission_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "th_sensor.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_note = nullptr;

static void on_hotair(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    char msg[128];
    process_controller_assign_sensor(TH_SENSOR_HOTAIR_ADDR, "Hot Air", msg, sizeof(msg));
    ui_manager_set_banner(msg);
    if (s_note != nullptr) {
        lv_label_set_text(s_note, msg);
    }
}

static void on_chamber(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    char msg[128];
    process_controller_assign_sensor(TH_SENSOR_CHAMBER_ADDR, "Chamber", msg, sizeof(msg));
    ui_manager_set_banner(msg);
    if (s_note != nullptr) {
        lv_label_set_text(s_note, msg);
    }
}

static void on_back(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_SYSTEM);
    }
}

void commission_detach(void)
{
    s_alive = false;
    s_note = nullptr;
}

void commission_build(lv_obj_t *parent, const system_status_t *status)
{
    (void)status;
    lv_obj_t *title =
        ui_theme_make_label(parent, "Commission Sensors", ui_font_title(), UI_COLOR_ORANGE);
    lv_obj_set_pos(title, 8, 4);
    lv_obj_t *help = ui_theme_make_label(
        parent,
        "Wire One XY-MD03 Probe. Assign Hot Air = Address 1 Or Chamber = Address 2, Then Power-Cycle.",
        ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_width(help, 980);
    lv_obj_set_pos(help, 8, 44);

    lv_obj_t *hot = ui_theme_make_btn(parent, "Assign Hot Air (1)", on_hotair, nullptr, 260, 72,
                                      UI_COLOR_ACCENT);
    lv_obj_set_pos(hot, 8, 120);
    lv_obj_t *ch = ui_theme_make_btn(parent, "Assign Chamber (2)", on_chamber, nullptr, 260, 72,
                                     UI_COLOR_TEAL);
    lv_obj_set_pos(ch, 284, 120);
    lv_obj_t *back =
        ui_theme_make_btn(parent, "Back", on_back, nullptr, 160, 64, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(back, 8, 340);

    s_note = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_AMBER);
    lv_obj_set_width(s_note, 980);
    lv_obj_set_pos(s_note, 8, 220);
    s_alive = true;
}

void commission_update(const system_status_t *status)
{
    (void)status;
    (void)s_alive;
}
