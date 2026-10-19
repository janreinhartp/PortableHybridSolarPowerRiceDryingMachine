#include "batch_setup_screen.h"

#include <cstdio>
#include <cstring>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_value_lbls[10] = {};
static lv_obj_t *s_ready_items[8] = {};
static lv_obj_t *s_start = nullptr;
static lv_obj_t *s_ack = nullptr;

static void on_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_start() == ESP_OK) {
        ui_manager_set_banner("Batch Started");
    } else {
        ui_manager_set_banner("Start Refused - Check Readiness");
    }
}

static void on_settings(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_SETTINGS);
    }
}

static void on_ack(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_acknowledge_interrupted() == ESP_OK) {
        ui_manager_set_banner("Interrupted Batch Acknowledged");
    }
}

static lv_obj_t *make_snap_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *name)
{
    lv_obj_t *card = ui_theme_make_card(parent, 348, 48);
    lv_obj_set_pos(card, x, y);
    lv_obj_t *lbl = ui_theme_make_label(card, name, ui_font_body(), UI_COLOR_MUTED);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *val = ui_theme_make_label(card, "-", ui_font_body(), UI_COLOR_CREAM);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
    return val;
}

void batch_setup_detach(void)
{
    s_alive = false;
    s_start = nullptr;
    s_ack = nullptr;
    for (int i = 0; i < 10; ++i) {
        s_value_lbls[i] = nullptr;
    }
    for (int i = 0; i < 8; ++i) {
        s_ready_items[i] = nullptr;
    }
}

void batch_setup_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "Confirm Settings Before Start", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    static const char *names[10] = {
        "Target Moisture", "Drying Temp",  "Cooling Temp", "Hysteresis",
        "Circulation",     "Circ Interval", "Circ Run",     "Discharge",
        "Min Drying",      "Max Drying",
    };
    for (int i = 0; i < 10; ++i) {
        const int row = i / 2;
        const int col = i % 2;
        s_value_lbls[i] =
            make_snap_card(parent, (lv_coord_t)(col * 356), (lv_coord_t)(48 + row * 56), names[i]);
    }

    lv_obj_t *ready = ui_theme_make_card(parent, 272, 272);
    lv_obj_set_pos(ready, 720, 48);
    lv_obj_t *ready_title =
        ui_theme_make_label(ready, "Readiness Checklist", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(ready_title, LV_ALIGN_TOP_LEFT, 0, 0);

    static const char *checks[8] = {"SD / Logger", "RTC",     "Relay",         "Hot Air",
                                    "Chamber",    "Moisture", "Fault Clear",   "No Interrupt"};
    for (int i = 0; i < 8; ++i) {
        lv_obj_t *row = lv_obj_create(ready);
        lv_obj_set_size(row, 240, 24);
        lv_obj_set_pos(row, 0, (lv_coord_t)(36 + i * 28));
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *n = ui_theme_make_label(row, checks[i], ui_font_caption(), UI_COLOR_MUTED);
        lv_obj_align(n, LV_ALIGN_LEFT_MID, 0, 0);
        s_ready_items[i] = ui_theme_make_label(row, "--", ui_font_caption(), UI_COLOR_DIM);
        lv_obj_align(s_ready_items[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }

    lv_obj_t *edit =
        ui_theme_make_btn(parent, "Edit Settings", on_settings, nullptr, 488, 64, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(edit, 0, 336);
    s_start = ui_theme_make_btn(parent, "Start", on_start, nullptr, 488, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(s_start, 504, 336);

    s_ack = ui_theme_make_btn(parent, "Acknowledge Interrupt", on_ack, nullptr, 272, 48,
                              UI_COLOR_ORANGE);
    lv_obj_set_pos(s_ack, 720, 328);

    s_alive = true;
    batch_setup_update(status);
}

void batch_setup_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    char buf[48];
    snprintf(buf, sizeof(buf), "%.1f %%", status->target_moisture_pct);
    lv_label_set_text(s_value_lbls[0], buf);
    snprintf(buf, sizeof(buf), "%.0f °C", status->drying_temperature_c);
    lv_label_set_text(s_value_lbls[1], buf);
    snprintf(buf, sizeof(buf), "%.0f °C", status->cooling_temperature_c);
    lv_label_set_text(s_value_lbls[2], buf);
    snprintf(buf, sizeof(buf), "%.1f °C", status->temperature_hysteresis_c);
    lv_label_set_text(s_value_lbls[3], buf);
    lv_label_set_text(s_value_lbls[4], ui_circ_mode_name(status->circulation_mode));
    snprintf(buf, sizeof(buf), "%u s", (unsigned)status->circulation_interval_s);
    lv_label_set_text(s_value_lbls[5], buf);
    snprintf(buf, sizeof(buf), "%u s", (unsigned)status->circulation_run_time_s);
    lv_label_set_text(s_value_lbls[6], buf);
    snprintf(buf, sizeof(buf), "%u s", (unsigned)status->discharge_time_s);
    lv_label_set_text(s_value_lbls[7], buf);
    snprintf(buf, sizeof(buf), "%u s", (unsigned)status->min_drying_time_s);
    lv_label_set_text(s_value_lbls[8], buf);
    snprintf(buf, sizeof(buf), "%u s", (unsigned)status->max_drying_time_s);
    lv_label_set_text(s_value_lbls[9], buf);

    const bool ok[8] = {
        status->sd_ok && status->logger_ok,
        status->rtc_ok,
        status->relay_ok,
        status->hotair_ok,
        status->chamber_ok,
        status->moisture_ok,
        status->active_fault == 0,
        !status->batch_interrupted,
    };
    for (int i = 0; i < 8; ++i) {
        if (s_ready_items[i] == nullptr) {
            continue;
        }
        lv_label_set_text(s_ready_items[i], ok[i] ? "Ready" : "Fail");
        lv_obj_set_style_text_color(s_ready_items[i],
                                    ui_theme_color(ok[i] ? UI_COLOR_TEAL : UI_COLOR_RED), 0);
    }

    if (status->batch_interrupted) {
        lv_obj_clear_flag(s_ack, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_ack, LV_OBJ_FLAG_HIDDEN);
    }

    const bool start_ok = status->precheck_ready &&
                          (status->dryer_state == DRYER_STATE_IDLE ||
                           status->dryer_state == DRYER_STATE_COMPLETE ||
                           status->dryer_state == DRYER_STATE_PRECHECK);
    if (start_ok) {
        lv_obj_clear_state(s_start, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_start, LV_STATE_DISABLED);
    }
}
