#include "ui_manager.h"

#include <cstdio>

#include "alarms_screen.h"
#include "batch_setup_screen.h"
#include "calibrate_moisture_screen.h"
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

static lv_obj_t *s_header = nullptr;
static lv_obj_t *s_brand = nullptr;
static lv_obj_t *s_chip = nullptr;
static lv_obj_t *s_batch = nullptr;
static lv_obj_t *s_phase = nullptr;
static lv_obj_t *s_clock_date = nullptr;
static lv_obj_t *s_clock_time = nullptr;
static lv_obj_t *s_fault_strip = nullptr;
static lv_obj_t *s_fault_text = nullptr;
static lv_obj_t *s_content = nullptr;
static lv_obj_t *s_nav_bar = nullptr;
static lv_obj_t *s_nav[4] = {};
static lv_obj_t *s_nav_lbl[4] = {};
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
        return "Running Drying";
    case DRYER_STATE_COOLING:
        return "Running Cooling";
    case DRYER_STATE_DISCHARGING:
        return "Discharging";
    case DRYER_STATE_COMPLETE:
        return "Batch Complete";
    case DRYER_STATE_FAULT:
        return "Fault";
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
    lv_obj_set_style_text_color(s_phase, ui_theme_color(UI_COLOR_MUTED), 0);
}

static void layout_content(bool fault_visible)
{
    if (s_content == nullptr) {
        return;
    }
    if (fault_visible) {
        if (s_fault_strip != nullptr) {
            lv_obj_clear_flag(s_fault_strip, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_pos(s_content, 0, 136);
        lv_obj_set_size(s_content, 1024, 400);
    } else {
        if (s_fault_strip != nullptr) {
            lv_obj_add_flag(s_fault_strip, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_pos(s_content, 0, 88);
        lv_obj_set_size(s_content, 1024, 448);
    }
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
    case UI_SCREEN_CALIBRATE_MOISTURE:
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
    calibrate_moisture_detach();
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
                  s_screen == UI_SCREEN_SYSTEM || s_screen == UI_SCREEN_COMMISSION ||
                  s_screen == UI_SCREEN_CALIBRATE_MOISTURE);
        } else {
            on = (s_screen == mapped[i]);
        }
        lv_obj_set_style_bg_color(s_nav[i],
                                  ui_theme_color(on ? UI_COLOR_TEAL : UI_COLOR_SURFACE), 0);
        if (s_nav_lbl[i] != nullptr) {
            lv_obj_set_style_text_color(
                s_nav_lbl[i], ui_theme_color(on ? UI_COLOR_ON_PRIMARY : UI_COLOR_DIM), 0);
        }
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
    case UI_SCREEN_CALIBRATE_MOISTURE:
        calibrate_moisture_update(&s_status);
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
    case UI_SCREEN_CALIBRATE_MOISTURE:
        calibrate_moisture_build(s_content, &s_status);
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

    s_header = lv_obj_create(scr);
    lv_obj_set_size(s_header, 1024, 88);
    lv_obj_set_pos(s_header, 0, 0);
    lv_obj_set_style_bg_color(s_header, ui_theme_color(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(s_header, ui_theme_color(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(s_header, 0, 0);
    lv_obj_set_style_border_side(s_header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(s_header, 1, 0);
    lv_obj_set_style_pad_all(s_header, 16, 0);
    lv_obj_set_style_radius(s_header, 0, 0);
    lv_obj_clear_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

    s_brand = ui_theme_make_label(s_header, "SOLAR RICE DRYER\n10 kg / batch - SRD-01",
                                  ui_font_body(), UI_COLOR_TEXT);
    lv_obj_set_width(s_brand, 264);
    lv_obj_set_pos(s_brand, 0, 4);
    lv_obj_set_style_text_font(s_brand, ui_font_body(), 0);

    s_chip = ui_state_chip_create(s_header);
    lv_obj_set_pos(s_chip, 280, 8);
    ui_state_chip_set(s_chip, s_status.dryer_state);

    s_batch = ui_theme_make_label(s_header, "BATCH-", ui_font_body(), UI_COLOR_CREAM);
    lv_obj_set_width(s_batch, 220);
    lv_label_set_long_mode(s_batch, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_batch, 456, 4);

    s_phase = ui_theme_make_label(s_header, phase_text_for(&s_status), ui_font_body(),
                                  UI_COLOR_MUTED);
    lv_obj_set_width(s_phase, 220);
    lv_label_set_long_mode(s_phase, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_phase, 456, 32);
    refresh_header_phase(&s_status);

    s_clock_date = ui_theme_make_label(s_header, "", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_set_style_text_align(s_clock_date, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_clock_date, LV_ALIGN_TOP_RIGHT, 0, 4);

    s_clock_time = ui_theme_make_label(s_header, "", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_style_text_align(s_clock_time, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_clock_time, LV_ALIGN_TOP_RIGHT, 0, 28);

    s_fault_strip = lv_obj_create(scr);
    lv_obj_set_size(s_fault_strip, 1024, 48);
    lv_obj_set_pos(s_fault_strip, 0, 88);
    lv_obj_set_style_bg_color(s_fault_strip, ui_theme_color(UI_COLOR_FAULT_STRIP), 0);
    lv_obj_set_style_border_width(s_fault_strip, 0, 0);
    lv_obj_set_style_radius(s_fault_strip, 0, 0);
    lv_obj_set_style_pad_hor(s_fault_strip, 16, 0);
    lv_obj_clear_flag(s_fault_strip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *fault_tag =
        ui_theme_make_label(s_fault_strip, "FAULT", ui_font_body(), UI_COLOR_ON_PRIMARY);
    lv_obj_align(fault_tag, LV_ALIGN_LEFT_MID, 0, 0);
    s_fault_text =
        ui_theme_make_label(s_fault_strip, "", ui_font_body(), UI_COLOR_ON_PRIMARY);
    lv_obj_set_width(s_fault_text, 800);
    lv_label_set_long_mode(s_fault_text, LV_LABEL_LONG_DOT);
    lv_obj_align(s_fault_text, LV_ALIGN_LEFT_MID, 80, 0);
    lv_obj_add_flag(s_fault_strip, LV_OBJ_FLAG_HIDDEN);

    s_content = lv_obj_create(scr);
    lv_obj_set_style_bg_color(s_content, ui_theme_color(UI_COLOR_BG), 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 16, 0);
    lv_obj_set_style_radius(s_content, 0, 0);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(s_content, LV_SCROLLBAR_MODE_OFF);
    layout_content(s_status.active_fault != 0);

    s_nav_bar = lv_obj_create(scr);
    lv_obj_set_size(s_nav_bar, 1024, 64);
    lv_obj_set_pos(s_nav_bar, 0, 536);
    lv_obj_set_style_bg_color(s_nav_bar, ui_theme_color(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_border_color(s_nav_bar, ui_theme_color(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_side(s_nav_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_width(s_nav_bar, 1, 0);
    lv_obj_set_style_radius(s_nav_bar, 0, 0);
    lv_obj_set_style_pad_all(s_nav_bar, 8, 0);
    lv_obj_clear_flag(s_nav_bar, LV_OBJ_FLAG_SCROLLABLE);

    struct {
        const char *symbol;
        const char *text;
        ui_screen_t id;
    } nav[] = {
        {LV_SYMBOL_HOME, "Home", UI_SCREEN_DASHBOARD},
        {LV_SYMBOL_PLAY, "Run", UI_SCREEN_RUN},
        {LV_SYMBOL_WARNING, "Alarms", UI_SCREEN_ALARMS},
        {LV_SYMBOL_LIST, "More", UI_SCREEN_MORE},
    };
    const lv_coord_t nav_w = 240;
    for (size_t i = 0; i < 4; ++i) {
        s_nav[i] = lv_btn_create(s_nav_bar);
        lv_obj_set_size(s_nav[i], nav_w, 48);
        lv_obj_set_pos(s_nav[i], (lv_coord_t)((int)i * (nav_w + 8)), 0);
        lv_obj_set_style_radius(s_nav[i], 8, 0);
        lv_obj_set_style_shadow_width(s_nav[i], 0, 0);
        lv_obj_set_style_bg_color(s_nav[i], ui_theme_color(UI_COLOR_SURFACE), 0);
        lv_obj_add_event_cb(s_nav[i], on_nav, LV_EVENT_CLICKED, (void *)(intptr_t)nav[i].id);

        char label[48];
        snprintf(label, sizeof(label), "%s %s", nav[i].symbol, nav[i].text);
        s_nav_lbl[i] = ui_theme_make_label(s_nav[i], label, ui_font_body(), UI_COLOR_DIM);
        lv_obj_center(s_nav_lbl[i]);
    }

    if (s_status.active_fault != 0) {
        lv_label_set_text(s_fault_text,
                          safety_manager_fault_name((fault_code_t)s_status.active_fault));
    }
    layout_content(s_status.active_fault != 0);
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
        snprintf(buf, sizeof(buf), "%s", status->batch_id[0] ? status->batch_id : "BATCH-");
        lv_label_set_text(s_batch, buf);
    }
    refresh_header_phase(status);
    if (status->rtc_ok) {
        char date[24];
        char time[16];
        ui_format_header_date(date, sizeof(date), status->rtc_year, status->rtc_month,
                              status->rtc_day);
        ui_format_header_time(time, sizeof(time), status->rtc_hour, status->rtc_minute);
        if (s_clock_date != nullptr) {
            lv_label_set_text(s_clock_date, date);
        }
        if (s_clock_time != nullptr) {
            lv_label_set_text(s_clock_time, time);
        }
    }

    const bool fault_on = status->active_fault != 0;
    if (s_fault_text != nullptr) {
        if (fault_on) {
            lv_label_set_text(s_fault_text,
                              safety_manager_fault_name((fault_code_t)status->active_fault));
        } else {
            lv_label_set_text(s_fault_text, "");
        }
    }
    layout_content(fault_on);

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
