#include "complete_screen.h"

#include <cstdio>

#include "screen_common.h"
#include "ui_manager.h"
#include "ui_widgets.h"

static bool s_alive = false;
static ui_kpi_t s_batch {};
static ui_kpi_t s_moist {};
static ui_kpi_t s_temp {};
static ui_kpi_t s_time {};
static ui_trend_t s_trend {};

static void on_new(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_BATCH_SETUP);
    }
}

static void on_history(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_HISTORY);
    }
}

void complete_detach(void)
{
    s_alive = false;
    s_batch = {};
    s_moist = {};
    s_temp = {};
    s_time = {};
    s_trend = {};
}

void complete_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "Batch Complete", ui_font_title(), UI_COLOR_TEAL);
    lv_obj_set_pos(title, 8, 4);

    s_batch = ui_kpi_create(parent, "Batch", 230, 90);
    lv_obj_set_pos(s_batch.root, 8, 44);
    s_moist = ui_kpi_create(parent, "Moisture", 230, 90);
    lv_obj_set_pos(s_moist.root, 256, 44);
    s_temp = ui_kpi_create(parent, "Max Chamber", 230, 90);
    lv_obj_set_pos(s_temp.root, 504, 44);
    s_time = ui_kpi_create(parent, "Drying Time", 230, 90);
    lv_obj_set_pos(s_time.root, 752, 44);

    s_trend = ui_trend_create(parent, 980, 180);
    lv_obj_set_pos(s_trend.root, 8, 150);

    lv_obj_t *again =
        ui_theme_make_btn(parent, "New Batch", on_new, nullptr, 200, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(again, 8, 350);
    lv_obj_t *hist =
        ui_theme_make_btn(parent, "History", on_history, nullptr, 180, 64, UI_COLOR_ACCENT);
    lv_obj_set_pos(hist, 224, 350);

    s_alive = true;
    complete_update(status);
}

void complete_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", status->batch_id[0] ? status->batch_id : "-");
    ui_kpi_set(&s_batch, buf);
    snprintf(buf, sizeof(buf), "%.1f -> %.1f %%", status->initial_moisture_pct,
             status->final_moisture_pct);
    ui_kpi_set(&s_moist, buf);
    snprintf(buf, sizeof(buf), "%.1f C", status->max_chamber_temp_c);
    ui_kpi_set(&s_temp, buf);
    ui_format_hms(buf, sizeof(buf), status->drying_elapsed_s);
    ui_kpi_set(&s_time, buf);
    ui_trend_refresh(&s_trend);
}
