#include "drying_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_moisture = nullptr;
static lv_obj_t *s_climate = nullptr;
static lv_obj_t *s_actuators = nullptr;
static lv_obj_t *s_elapsed = nullptr;
static lv_obj_t *s_bar = nullptr;

static void on_abort(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    process_controller_abort();
    ui_manager_set_banner("Batch aborted - outputs set safe");
}

static int moisture_progress(const system_status_t *status)
{
    const float span = status->initial_moisture_pct - status->target_moisture_pct;
    if (span <= 0.1f || !status->moisture_ok) {
        return 0;
    }
    float pct = (status->initial_moisture_pct - status->moisture_pct) / span * 100.0f;
    if (pct < 0.0f) {
        pct = 0.0f;
    }
    if (pct > 100.0f) {
        pct = 100.0f;
    }
    return (int)pct;
}

void drying_detach(void)
{
    s_alive = false;
    s_moisture = nullptr;
    s_climate = nullptr;
    s_actuators = nullptr;
    s_elapsed = nullptr;
    s_bar = nullptr;
}

void drying_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title = ui_label(parent, "DRYING", 8, 0, &lv_font_montserrat_24, 0xF0C808);
    (void)title;

    s_moisture = ui_label(parent, "", 8, 48, &lv_font_montserrat_20, 0xE8EEF4);
    s_bar = lv_bar_create(parent);
    lv_obj_set_size(s_bar, 640, 18);
    lv_obj_set_pos(s_bar, 8, 130);
    lv_bar_set_range(s_bar, 0, 100);

    s_climate = ui_label(parent, "", 8, 164, &lv_font_montserrat_20, 0xE8EEF4);
    s_actuators = ui_label(parent, "", 8, 250, &lv_font_montserrat_16, 0xF0C808);
    s_elapsed = ui_label(parent, "", 8, 290, &lv_font_montserrat_20, 0x7EC8FF);

    lv_obj_t *abort = ui_make_btn(parent, "ABORT", on_abort, nullptr, 140, 48);
    lv_obj_set_pos(abort, 8, 350);

    s_alive = true;
    drying_update(status);
}

void drying_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    char line[192];
    char clock[16];
    ui_format_hms(clock, sizeof(clock), status->drying_elapsed_s);
    snprintf(line, sizeof(line), "Moisture  %.1f %%\nTarget    %.1f %%",
             status->moisture_pct, status->target_moisture_pct);
    lv_label_set_text(s_moisture, line);
    lv_bar_set_value(s_bar, moisture_progress(status), LV_ANIM_OFF);

    snprintf(line, sizeof(line),
             "Chamber   %.1f C    %.1f %%RH\nHot-air   %.1f C    %.1f %%RH",
             status->chamber_temp_c, status->chamber_rh, status->hotair_temp_c,
             status->hotair_rh);
    lv_label_set_text(s_climate, line);

    snprintf(line, sizeof(line), "Heater %s    Fan %s    Elevator %s    Door %s",
             ui_on_off(status->relay_heater), ui_on_off(status->relay_fan),
             ui_on_off(status->relay_elevator),
             ui_door_text(status->relay_door_open, status->relay_door_close));
    lv_label_set_text(s_actuators, line);

    snprintf(line, sizeof(line), "Elapsed  %s", clock);
    lv_label_set_text(s_elapsed, line);
}
