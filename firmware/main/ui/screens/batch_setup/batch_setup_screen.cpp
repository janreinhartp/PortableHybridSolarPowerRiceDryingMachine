#include "batch_setup_screen.h"

#include <cstdio>
#include <cstring>

#include "process_controller.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_cards = nullptr;
static lv_obj_t *s_ready = nullptr;
static lv_obj_t *s_checks = nullptr;
static lv_obj_t *s_start = nullptr;

static void on_start(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_start() == ESP_OK) {
        /* Header phase text tracks PRECHECK → Drying → … automatically. */
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

void batch_setup_detach(void)
{
    s_alive = false;
    s_cards = nullptr;
    s_ready = nullptr;
    s_checks = nullptr;
    s_start = nullptr;
}

void batch_setup_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "Confirm Settings Before Start", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 8, 4);

    s_cards = ui_theme_make_card(parent, 640, 220);
    lv_obj_set_pos(s_cards, 8, 44);
    s_ready = ui_theme_make_label(s_cards, "", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_set_width(s_ready, 600);
    lv_label_set_long_mode(s_ready, LV_LABEL_LONG_WRAP);

    s_checks = ui_theme_make_card(parent, 320, 220);
    lv_obj_set_pos(s_checks, 668, 44);
    lv_obj_t *ch_title =
        ui_theme_make_label(s_checks, "Readiness", ui_font_body(), UI_COLOR_MUTED);
    lv_obj_align(ch_title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *list = ui_theme_make_label(s_checks, "", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_set_width(list, 280);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_set_user_data(s_checks, list);

    s_start = ui_theme_make_btn(parent, "Start", on_start, nullptr, 180, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(s_start, 8, 290);
    lv_obj_t *edit =
        ui_theme_make_btn(parent, "Edit Settings", on_settings, nullptr, 200, 64, UI_COLOR_ACCENT);
    lv_obj_set_pos(edit, 204, 290);
    lv_obj_t *ack = ui_theme_make_btn(parent, "Acknowledge Interrupt", on_ack, nullptr, 260, 64,
                                      UI_COLOR_ORANGE);
    lv_obj_set_pos(ack, 420, 290);

    s_alive = true;
    batch_setup_update(status);
}

void batch_setup_update(const system_status_t *status)
{
    if (!s_alive || status == nullptr) {
        return;
    }

    char line[360];
    snprintf(line, sizeof(line),
             "Target Moisture     %.1f %%\n"
             "Drying / Cool       %.0f C / %.0f C   Hyst %.1f C\n"
             "Circulation         %s   %us / %us\n"
             "Discharge           %u s\n"
             "Drying Window       %u s Min   %u s Max",
             status->target_moisture_pct, status->drying_temperature_c,
             status->cooling_temperature_c, status->temperature_hysteresis_c,
             ui_circ_mode_name(status->circulation_mode),
             (unsigned)status->circulation_interval_s, (unsigned)status->circulation_run_time_s,
             (unsigned)status->discharge_time_s, (unsigned)status->min_drying_time_s,
             (unsigned)status->max_drying_time_s);
    lv_label_set_text(s_ready, line);

    lv_obj_t *list = (lv_obj_t *)lv_obj_get_user_data(s_checks);
    char checks[320];
    snprintf(checks, sizeof(checks),
             "%s SD / Logger\n"
             "%s RTC\n"
             "%s Relay\n"
             "%s Hot Air\n"
             "%s Chamber\n"
             "%s Moisture\n"
             "%s Fault Clear\n"
             "%s No Interrupt",
             (status->sd_ok && status->logger_ok) ? "[Ok]" : "[--]",
             status->rtc_ok ? "[Ok]" : "[--]", status->relay_ok ? "[Ok]" : "[--]",
             status->hotair_ok ? "[Ok]" : "[--]", status->chamber_ok ? "[Ok]" : "[--]",
             status->moisture_ok ? "[Ok]" : "[--]",
             status->active_fault == 0 ? "[Ok]" : "[--]",
             !status->batch_interrupted ? "[Ok]" : "[--]");
    if (list != nullptr) {
        lv_label_set_text(list, checks);
    }

    const bool start_ok = status->precheck_ready &&
                          (status->dryer_state == DRYER_STATE_IDLE ||
                           status->dryer_state == DRYER_STATE_COMPLETE);
    if (start_ok) {
        lv_obj_clear_state(s_start, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_start, LV_STATE_DISABLED);
    }
}
