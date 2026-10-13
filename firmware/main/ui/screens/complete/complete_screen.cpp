#include "complete_screen.h"

#include <cstdio>

#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_body = nullptr;

static void on_new_batch(lv_event_t *e)
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
    s_body = nullptr;
}

void complete_build(lv_obj_t *parent, const system_status_t *status)
{
    ui_label(parent, "COMPLETE", 8, 0, &lv_font_montserrat_24, 0x7DCFB6);
    s_body = ui_label(parent, "", 8, 48, &lv_font_montserrat_16, 0xE8EEF4);
    lv_obj_set_width(s_body, 960);
    lv_obj_t *again = ui_make_btn(parent, "NEW BATCH", on_new_batch, nullptr, 170, 48);
    lv_obj_set_pos(again, 8, 360);
    lv_obj_t *history = ui_make_btn(parent, "HISTORY", on_history, nullptr, 150, 48);
    lv_obj_set_pos(history, 194, 360);
    s_alive = true;
    complete_update(status);
}

void complete_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }
    char dry[16];
    char cool[16];
    char disc[16];
    ui_format_hms(dry, sizeof(dry), status->drying_elapsed_s);
    ui_format_hms(cool, sizeof(cool), status->cooling_elapsed_s);
    ui_format_hms(disc, sizeof(disc), status->discharge_elapsed_s);

    char line[420];
    snprintf(line, sizeof(line),
             "Batch %s\n"
             "Started %s\nEnded   %s\n"
             "Moisture  %.1f %%  ->  %.1f %%    target %.1f %%\n"
             "Max chamber  %.1f C    average %.1f C\n"
             "Final EC  %.2f\n"
             "Drying %s    Cooling %s    Discharge %s\n"
             "Summary is queued to the SD card.",
             status->batch_id[0] ? status->batch_id : "-",
             status->batch_started[0] ? status->batch_started : "-",
             status->batch_ended[0] ? status->batch_ended : "-",
             status->initial_moisture_pct, status->final_moisture_pct,
             status->target_moisture_pct, status->max_chamber_temp_c,
             status->avg_chamber_temp_c, status->final_ec, dry, cool, disc);
    lv_label_set_text(s_body, line);
}
