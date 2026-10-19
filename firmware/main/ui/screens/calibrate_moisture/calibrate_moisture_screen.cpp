#include "calibrate_moisture_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "sensor_manager.h"
#include "ui_manager.h"

struct point_ui_t {
    lv_obj_t *v_lbl;
    lv_obj_t *y_lbl;
};

static bool s_alive = false;
static sensor_moisture_cal_t s_draft {};
static point_ui_t s_pts[3] {};
static lv_obj_t *s_live = nullptr;
static lv_obj_t *s_note = nullptr;
static lv_obj_t *s_save = nullptr;
static int s_adj_point = 0;
static int s_adj_sign = 0;

static void render_points(void)
{
    char buf[32];
    const float vs[3] = {s_draft.v1, s_draft.v2, s_draft.v3};
    const float ys[3] = {s_draft.y1, s_draft.y2, s_draft.y3};
    for (int i = 0; i < 3; ++i) {
        if (s_pts[i].v_lbl) {
            snprintf(buf, sizeof(buf), "%.3f V", vs[i]);
            lv_label_set_text(s_pts[i].v_lbl, buf);
        }
        if (s_pts[i].y_lbl) {
            snprintf(buf, sizeof(buf), "%.1f %%", ys[i]);
            lv_label_set_text(s_pts[i].y_lbl, buf);
        }
    }
}

static void on_back(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_SYSTEM);
    }
}

static void on_capture(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!process_controller_manual_control_allowed()) {
        ui_manager_set_banner("Calibration Available In Idle Only");
        return;
    }
    const int idx = (int)(intptr_t)lv_event_get_user_data(e);
    sensor_data_t snap {};
    sensor_manager_get(&snap);
    if (!snap.moisture_valid && !snap.ads1115_ok) {
        ui_manager_set_banner("Moisture Sensor Offline");
        return;
    }
    const float v = snap.moisture_voltage;
    if (idx == 0) {
        s_draft.v1 = v;
    } else if (idx == 1) {
        s_draft.v2 = v;
    } else {
        s_draft.v3 = v;
    }
    render_points();
    ui_manager_set_banner("Voltage Captured");
}

static void on_y_adjust(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!process_controller_manual_control_allowed()) {
        ui_manager_set_banner("Calibration Available In Idle Only");
        return;
    }
    const intptr_t packed = (intptr_t)lv_event_get_user_data(e);
    const int idx = (int)(packed >> 8);
    const int sign = (int)(int8_t)(packed & 0xff);
    float *y = (idx == 0) ? &s_draft.y1 : (idx == 1) ? &s_draft.y2 : &s_draft.y3;
    *y += 0.5f * (float)sign;
    if (*y < 0.0f) {
        *y = 0.0f;
    }
    if (*y > 100.0f) {
        *y = 100.0f;
    }
    render_points();
}

static void on_save(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!process_controller_manual_control_allowed()) {
        ui_manager_set_banner("Calibration Available In Idle Only");
        return;
    }
    if (sensor_manager_save_moisture_cal(&s_draft) == ESP_OK) {
        ui_manager_set_banner("Moisture Calibration Saved");
    } else {
        ui_manager_set_banner("Calibration Rejected - Check Points");
    }
}

static void on_reset(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!process_controller_manual_control_allowed()) {
        ui_manager_set_banner("Calibration Available In Idle Only");
        return;
    }
    sensor_manager_get_default_moisture_cal(&s_draft);
    if (sensor_manager_save_moisture_cal(&s_draft) == ESP_OK) {
        render_points();
        ui_manager_set_banner("Defaults Saved");
    } else {
        ui_manager_set_banner("Could Not Save Defaults");
    }
}

static void make_point_card(lv_obj_t *parent, int idx, lv_coord_t x, const char *title)
{
    lv_obj_t *card = ui_theme_make_card(parent, 320, 200);
    lv_obj_set_pos(card, x, 120);
    lv_obj_t *t = ui_theme_make_label(card, title, ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

    s_pts[idx].v_lbl = ui_theme_make_label(card, "- V", ui_font_title(), UI_COLOR_CREAM);
    lv_obj_align(s_pts[idx].v_lbl, LV_ALIGN_TOP_LEFT, 0, 40);

    lv_obj_t *cap = ui_theme_make_btn(card, "Capture Voltage", on_capture, (void *)(intptr_t)idx,
                                      280, 40, UI_COLOR_TEAL);
    lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 0, 88);

    s_pts[idx].y_lbl = ui_theme_make_label(card, "- %", ui_font_body(), UI_COLOR_CREAM);
    lv_obj_align(s_pts[idx].y_lbl, LV_ALIGN_TOP_LEFT, 80, 144);

    const intptr_t down = ((intptr_t)idx << 8) | (intptr_t)(int8_t)-1;
    const intptr_t up = ((intptr_t)idx << 8) | (intptr_t)(int8_t)1;
    lv_obj_t *minus =
        ui_theme_make_btn(card, "-", on_y_adjust, (void *)down, 48, 40, UI_COLOR_SURFACE_ALT);
    lv_obj_align(minus, LV_ALIGN_TOP_LEFT, 0, 136);
    lv_obj_t *plus =
        ui_theme_make_btn(card, "+", on_y_adjust, (void *)up, 48, 40, UI_COLOR_SURFACE_ALT);
    lv_obj_align(plus, LV_ALIGN_TOP_LEFT, 200, 136);
}

void calibrate_moisture_detach(void)
{
    s_alive = false;
    s_live = nullptr;
    s_note = nullptr;
    s_save = nullptr;
    for (int i = 0; i < 3; ++i) {
        s_pts[i] = {};
    }
}

void calibrate_moisture_build(lv_obj_t *parent, const system_status_t *status)
{
    sensor_manager_get_moisture_cal(&s_draft);

    lv_obj_t *title =
        ui_theme_make_label(parent, "Calibrate Moisture", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    s_live = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_CREAM);
    lv_obj_set_pos(s_live, 0, 48);
    lv_obj_set_width(s_live, 992);

    s_note = ui_theme_make_label(parent, "", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_pos(s_note, 0, 84);
    lv_obj_set_width(s_note, 992);

    make_point_card(parent, 0, 0, "Point 1 — Low");
    make_point_card(parent, 1, 336, "Point 2 — Mid");
    make_point_card(parent, 2, 672, "Point 3 — High");

    lv_obj_t *back =
        ui_theme_make_btn(parent, "Back", on_back, nullptr, 240, 56, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(back, 0, 348);
    lv_obj_t *reset =
        ui_theme_make_btn(parent, "Reset Defaults", on_reset, nullptr, 352, 56, UI_COLOR_ORANGE);
    lv_obj_set_pos(reset, 256, 348);
    s_save = ui_theme_make_btn(parent, "Save Calibration", on_save, nullptr, 352, 56, UI_COLOR_TEAL);
    lv_obj_set_pos(s_save, 640, 348);

    s_alive = true;
    render_points();
    calibrate_moisture_update(status);
}

void calibrate_moisture_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    char buf[96];
    if (status->moisture_ok) {
        snprintf(buf, sizeof(buf), "Live: %.3f V → %.1f %%", status->moisture_voltage,
                 status->moisture_pct);
    } else {
        snprintf(buf, sizeof(buf), "Live: moisture offline");
    }
    lv_label_set_text(s_live, buf);

    const bool ok = process_controller_manual_control_allowed();
    lv_label_set_text(s_note, ok ? "Capture three reference points, then Save."
                                 : "Available in Idle / Complete only.");
    if (s_save != nullptr) {
        if (ok) {
            lv_obj_clear_state(s_save, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(s_save, LV_STATE_DISABLED);
        }
    }
    (void)s_adj_point;
    (void)s_adj_sign;
}
