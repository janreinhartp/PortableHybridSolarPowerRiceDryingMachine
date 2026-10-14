#include "ui_manager.h"

#include <cstdio>

#include "alarms_screen.h"
#include "batch_setup_screen.h"
#include "commission_screen.h"
#include "complete_screen.h"
#include "dashboard_screen.h"
#include "history_screen.h"
#include "lvgl.h"
#include "manual_screen.h"
#include "more_screen.h"
#include "process_controller.h"
#include "run_screen.h"
#include "safety_manager.h"
#include "screen_common.h"
#include "settings_screen.h"
#include "system_info_screen.h"
#include "trend_buffer.h"
#include "ui_theme.h"
#include "ui_widgets.h"

static lv_obj_t *s_chip = nullptr;
static lv_obj_t *s_batch = nullptr;
static lv_obj_t *s_phase = nullptr;
static lv_obj_t *s_clock = nullptr;
static lv_obj_t *s_fault = nullptr;
static lv_obj_t *s_content = nullptr;
static lv_obj_t *s_nav[4] = {};
static ui_screen_t s_screen = UI_SCREEN_DASHBOARD;
static uint8_t s_seen_state = 0xFF;
static system_status_t s_status {};
static unsigned s_taps = 0;
static unsigned *s_tap_counter = nullptr;
static char s_ephemeral[120] = {};
static bool s_ephemeral_active = false;
static uint8_t s_ephemeral_state = 0xFF;

static const char *phase_text_for(const system_status_t *status)
{
    if (status == nullptr) {
        return "Ready";
    }
    switch ((dryer_state_t)status->dryer_state) {
    case DRYER_STATE_PRECHECK:
        return "Running Precheck";
    case DRYER_STATE_DRYING:
        return "Drying";
    case DRYER_STATE_COOLING:
        return "Cooling";
    case DRYER_STATE_DISCHARGING:
        return "Discharging";
    case DRYER_STATE_COMPLETE:
        return "Batch Complete";
    case DRYER_STATE_FAULT:
        if (status->active_fault != 0) {
            return safety_manager_fault_name((fault_code_t)status->active_fault);
        }
        return "Fault Active";
    case DRYER_STATE_IDLE:
    default:
        if (status->batch_interrupted) {
            return "Interrupted Batch Pending";
        }
        return "Ready";
    }
}

static void refresh_header_phase(const system_status_t *status)
{
    if (s_phase == nullptr || status == nullptr) {
        return;
    }
    if (s_ephemeral_active) {
        if (status->dryer_state == s_ephemeral_state && s_ephemeral[0] != '\0') {
            lv_label_set_text(s_phase, s_ephemeral);
            lv_obj_set_style_text_color(s_phase, ui_theme_color(UI_COLOR_AMBER), 0);
            return;
        }
        s_ephemeral_active = false;
        s_ephemeral[0] = '\0';
    }
    lv_label_set_text(s_phase, phase_text_for(status));
    lv_obj_set_style_text_color(s_phase, ui_theme_state_color(status->dryer_state), 0);
}

static ui_screen_t screen_for_state(uint8_t state)
{
    switch ((dryer_state_t)state) {
    case DRYER_STATE_PRECHECK:
        return UI_SCREEN_BATCH_SETUP;
    case DRYER_STATE_DRYING:
    case DRYER_STATE_COOLING:
    case DRYER_STATE_DISCHARGING:
        return UI_SCREEN_RUN;
    case DRYER_STATE_COMPLETE:
        return UI_SCREEN_COMPLETE;
    case DRYER_STATE_FAULT:
        return UI_SCREEN_ALARMS;
    case DRYER_STATE_IDLE:
    default:
        return UI_SCREEN_DASHBOARD;
    }
}

static bool screen_follows_process(ui_screen_t screen)
{
    switch (screen) {
    case UI_SCREEN_MANUAL:
    case UI_SCREEN_SETTINGS:
    case UI_SCREEN_HISTORY:
    case UI_SCREEN_SYSTEM:
    case UI_SCREEN_MORE:
    case UI_SCREEN_COMMISSION:
        return false;
    default:
        return true;
    }
}

static void detach_all(void)
{
    dashboard_detach();
    batch_setup_detach();
    run_detach();
    complete_detach();
    manual_detach();
    settings_detach();
    history_detach();
    alarms_detach();
    system_info_detach();
    more_detach();
    commission_detach();
}

static void highlight_nav(void)
{
    const ui_screen_t mapped[] = {UI_SCREEN_DASHBOARD, UI_SCREEN_RUN, UI_SCREEN_ALARMS,
                                  UI_SCREEN_MORE};
    for (size_t i = 0; i < 4; ++i) {
        if (s_nav[i] == nullptr) {
            continue;
        }
        bool on = false;
        if (mapped[i] == UI_SCREEN_RUN) {
            on = (s_screen == UI_SCREEN_RUN || s_screen == UI_SCREEN_BATCH_SETUP ||
                  s_screen == UI_SCREEN_COMPLETE);
        } else if (mapped[i] == UI_SCREEN_MORE) {
            on = (s_screen == UI_SCREEN_MORE || s_screen == UI_SCREEN_MANUAL ||
                  s_screen == UI_SCREEN_SETTINGS || s_screen == UI_SCREEN_HISTORY ||
                  s_screen == UI_SCREEN_SYSTEM || s_screen == UI_SCREEN_COMMISSION);
        } else {
            on = (s_screen == mapped[i]);
        }
        lv_obj_set_style_bg_color(s_nav[i],
                                  ui_theme_color(on ? UI_COLOR_TEAL : UI_COLOR_SURFACE_ALT), 0);
    }
}

static void update_active(void)
{
    switch (s_screen) {
    case UI_SCREEN_DASHBOARD:
        dashboard_update(&s_status);
        break;
    case UI_SCREEN_BATCH_SETUP:
        batch_setup_update(&s_status);
        break;
    case UI_SCREEN_RUN:
        run_update(&s_status);
        break;
    case UI_SCREEN_COMPLETE:
        complete_update(&s_status);
        break;
    case UI_SCREEN_MANUAL:
        manual_update(&s_status);
        break;
    case UI_SCREEN_SETTINGS:
        settings_update(&s_status);
        break;
    case UI_SCREEN_HISTORY:
        history_update(&s_status);
        break;
    case UI_SCREEN_ALARMS:
        alarms_update(&s_status);
        break;
    case UI_SCREEN_SYSTEM:
        system_info_update(&s_status, s_taps);
        break;
    case UI_SCREEN_MORE:
        more_update(&s_status);
        break;
    case UI_SCREEN_COMMISSION:
        commission_update(&s_status);
        break;
    }
}

void ui_manager_navigate(ui_screen_t screen)
{
    if (s_content == nullptr) {
        return;
    }
    detach_all();
    s_screen = screen;
    lv_obj_clean(s_content);
    highlight_nav();

    switch (screen) {
    case UI_SCREEN_DASHBOARD:
        dashboard_build(s_content, &s_status);
        break;
    case UI_SCREEN_BATCH_SETUP:
        batch_setup_build(s_content, &s_status);
        break;
    case UI_SCREEN_RUN:
        run_build(s_content, &s_status);
        break;
    case UI_SCREEN_COMPLETE:
        complete_build(s_content, &s_status);
        break;
    case UI_SCREEN_MANUAL:
        manual_build(s_content, &s_status);
        break;
    case UI_SCREEN_SETTINGS:
        settings_build(s_content, &s_status);
        break;
    case UI_SCREEN_HISTORY:
        history_build(s_content, &s_status);
        break;
    case UI_SCREEN_ALARMS:
        alarms_build(s_content, &s_status);
        break;
    case UI_SCREEN_SYSTEM:
        system_info_build(s_content, &s_status, s_taps);
        break;
    case UI_SCREEN_MORE:
        more_build(s_content, &s_status);
        break;
    case UI_SCREEN_COMMISSION:
        commission_build(s_content, &s_status);
        break;
    }
}

static void on_nav(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const int id = (int)(intptr_t)lv_event_get_user_data(e);
    if (id == (int)UI_SCREEN_RUN) {
        ui_manager_navigate(screen_for_state(s_status.dryer_state));
    } else {
        ui_manager_navigate((ui_screen_t)id);
    }
}

void ui_manager_set_banner(const char *msg)
{
    if (msg == nullptr || msg[0] == '\0') {
        s_ephemeral_active = false;
        s_ephemeral[0] = '\0';
        refresh_header_phase(&s_status);
        return;
    }
    snprintf(s_ephemeral, sizeof(s_ephemeral), "%s", msg);
    s_ephemeral_active = true;
    s_ephemeral_state = s_status.dryer_state;
    if (s_phase != nullptr) {
        lv_label_set_text(s_phase, s_ephemeral);
        lv_obj_set_style_text_color(s_phase, ui_theme_color(UI_COLOR_AMBER), 0);
    }
}

void ui_manager_bind_tap_counter(unsigned *counter)
{
    s_tap_counter = counter;
    system_info_set_tap_counter(counter);
}

void ui_manager_show_hmi(const system_status_t *status)
{
    trend_buffer_init();
    if (status != nullptr) {
        s_status = *status;
        trend_buffer_push(&s_status);
    }
    s_seen_state = s_status.dryer_state;
    s_taps = s_tap_counter != nullptr ? *s_tap_counter : 0;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    ui_theme_apply_screen(scr);

    s_chip = ui_state_chip_create(scr);
    lv_obj_set_pos(s_chip, 12, 10);
    ui_state_chip_set(s_chip, s_status.dryer_state);

    s_batch = ui_theme_make_label(scr, "Batch -", ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_pos(s_batch, 170, 16);

    s_phase = ui_theme_make_label(scr, phase_text_for(&s_status), ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_width(s_phase, 300);
    lv_label_set_long_mode(s_phase, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_phase, 380, 16);
    refresh_header_phase(&s_status);

    s_clock = ui_theme_make_label(scr, "", ui_font_body(), UI_COLOR_MUTED);
    lv_obj_align(s_clock, LV_ALIGN_TOP_RIGHT, -16, 16);

    s_fault = ui_theme_make_label(scr, "", ui_font_body(), UI_COLOR_RED);
    lv_obj_set_width(s_fault, 700);
    lv_obj_set_pos(s_fault, 170, 48);

    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, 1000, 448);
    lv_obj_set_pos(s_content, 12, 80);
    lv_obj_set_style_bg_color(s_content, ui_theme_color(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 4, 0);
    /* Avoid scroll-drag eating taps / producing double activations. */
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_content, LV_SCROLLBAR_MODE_OFF);

    struct {
        const char *text;
        ui_screen_t id;
    } nav[] = {
        {"Home", UI_SCREEN_DASHBOARD},
        {"Run", UI_SCREEN_RUN},
        {"Alarms", UI_SCREEN_ALARMS},
        {"More", UI_SCREEN_MORE},
    };
    for (size_t i = 0; i < 4; ++i) {
        s_nav[i] = ui_theme_make_btn(scr, nav[i].text, on_nav, (void *)(intptr_t)nav[i].id, 240,
                                     56, UI_COLOR_SURFACE_ALT);
        lv_obj_set_pos(s_nav[i], (lv_coord_t)(12 + (int)i * 252), 536);
    }

    ui_manager_navigate(screen_for_state(s_status.dryer_state));
}

void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps)
{
    if (status == nullptr || s_content == nullptr) {
        return;
    }
    s_status = *status;
    s_taps = touch_taps;
    trend_buffer_push(status);

    if (s_chip != nullptr) {
        ui_state_chip_set(s_chip, status->dryer_state);
    }
    if (s_batch != nullptr) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Batch %s", status->batch_id[0] ? status->batch_id : "-");
        lv_label_set_text(s_batch, buf);
    }
    refresh_header_phase(status);
    if (s_clock != nullptr && status->rtc_ok) {
        char buf[40];
        ui_format_header_datetime(buf, sizeof(buf), status->rtc_year, status->rtc_month,
                                  status->rtc_day, status->rtc_hour, status->rtc_minute);
        lv_label_set_text(s_clock, buf);
    }
    if (s_fault != nullptr) {
        if (status->active_fault == 0) {
            lv_label_set_text(s_fault, "");
        } else {
            lv_label_set_text(s_fault,
                              safety_manager_fault_name((fault_code_t)status->active_fault));
        }
    }

    if (status->dryer_state != s_seen_state) {
        s_seen_state = status->dryer_state;
        s_ephemeral_active = false;
        s_ephemeral[0] = '\0';
        refresh_header_phase(status);
        const ui_screen_t want = screen_for_state(status->dryer_state);
        if (screen_follows_process(s_screen) && want != s_screen) {
            ui_manager_navigate(want);
            return;
        }
    }
    update_active();
}
