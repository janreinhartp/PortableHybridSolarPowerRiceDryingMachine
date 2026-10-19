#include "commission_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "th_sensor.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_rows[4] = {};
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

static lv_obj_t *make_sensor_row(lv_obj_t *parent, lv_coord_t y, const char *name)
{
    lv_obj_t *row = ui_theme_make_card(parent, 992, 64);
    lv_obj_set_pos(row, 0, y);
    lv_obj_t *lbl = ui_theme_make_label(row, name, ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *st = ui_theme_make_label(row, "--", ui_font_body(), UI_COLOR_DIM);
    lv_obj_align(st, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_user_data(row, st);
    return row;
}

static void set_row(lv_obj_t *row, bool ok, const char *detail)
{
    if (row == nullptr) {
        return;
    }
    lv_obj_t *st = (lv_obj_t *)lv_obj_get_user_data(row);
    if (st == nullptr) {
        return;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "%s  %s", ok ? "CONNECTED" : "OFFLINE",
             detail != nullptr ? detail : "");
    lv_label_set_text(st, buf);
    lv_obj_set_style_text_color(st, ui_theme_color(ok ? UI_COLOR_TEAL : UI_COLOR_RED), 0);
}

void commission_detach(void)
{
    s_alive = false;
    s_note = nullptr;
    for (int i = 0; i < 4; ++i) {
        s_rows[i] = nullptr;
    }
}

void commission_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "Commission Sensors", ui_font_title(), UI_COLOR_ORANGE);
    lv_obj_set_pos(title, 0, 0);

    s_rows[0] = make_sensor_row(parent, 48, "Hot Air Temperature Probe");
    s_rows[1] = make_sensor_row(parent, 120, "Chamber Temperature / Humidity");
    s_rows[2] = make_sensor_row(parent, 192, "Rice Moisture");
    s_rows[3] = nullptr;

    lv_obj_t *hot = ui_theme_make_btn(parent, "Assign Hot Air (1)", on_hotair, nullptr, 320, 48,
                                      UI_COLOR_ACCENT);
    lv_obj_set_pos(hot, 0, 336);
    lv_obj_t *ch = ui_theme_make_btn(parent, "Assign Chamber (2)", on_chamber, nullptr, 320, 48,
                                     UI_COLOR_TEAL);
    lv_obj_set_pos(ch, 336, 336);
    lv_obj_t *back =
        ui_theme_make_btn(parent, "Back", on_back, nullptr, 320, 48, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(back, 672, 336);

    s_note = ui_theme_make_label(
        parent, "Wire One XY-MD03 Probe, Assign Address, Then Power-Cycle.", ui_font_caption(),
        UI_COLOR_MUTED);
    lv_obj_set_width(s_note, 992);
    lv_obj_set_pos(s_note, 0, 392);

    s_alive = true;
    commission_update(status);
}

void commission_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    char detail[48];
    snprintf(detail, sizeof(detail), "%.1f °C", status->hotair_temp_c);
    set_row(s_rows[0], status->hotair_ok, status->hotair_ok ? detail : "");
    snprintf(detail, sizeof(detail), "%.1f °C / %.0f %%", status->chamber_temp_c,
             status->chamber_rh);
    set_row(s_rows[1], status->chamber_ok, status->chamber_ok ? detail : "");
    snprintf(detail, sizeof(detail), "%.1f %%", status->moisture_pct);
    set_row(s_rows[2], status->moisture_ok, status->moisture_ok ? detail : "");
}
