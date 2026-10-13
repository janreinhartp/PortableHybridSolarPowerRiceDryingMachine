#include "ui_manager.h"

#include <cstdio>

#include "alarms_screen.h"
#include "batch_setup_screen.h"
#include "complete_screen.h"
#include "cooling_screen.h"
#include "dashboard_screen.h"
#include "discharge_screen.h"
#include "drying_screen.h"
#include "history_screen.h"
#include "lvgl.h"
#include "manual_screen.h"
#include "process_controller.h"
#include "safety_manager.h"
#include "screen_common.h"
#include "settings_screen.h"
#include "system_info_screen.h"

static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_fault = nullptr;
static lv_obj_t *s_banner = nullptr;
static lv_obj_t *s_content = nullptr;
static lv_obj_t *s_nav[8] = {};
static ui_screen_t s_screen = UI_SCREEN_DASHBOARD;
static uint8_t s_seen_state = 0xFF;
static system_status_t s_status {};
static unsigned s_taps = 0;
static unsigned *s_tap_counter = nullptr;

static const int NAV_RUN = 100;

static ui_screen_t screen_for_state(uint8_t state)
{
    switch ((dryer_state_t)state) {
    case DRYER_STATE_PRECHECK:
        return UI_SCREEN_BATCH_SETUP;
    case DRYER_STATE_DRYING:
        return UI_SCREEN_DRYING;
    case DRYER_STATE_COOLING:
        return UI_SCREEN_COOLING;
    case DRYER_STATE_DISCHARGING:
        return UI_SCREEN_DISCHARGE;
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
        return false;
    default:
        return true;
    }
}

static const char *screen_title(ui_screen_t screen)
{
    switch (screen) {
    case UI_SCREEN_DASHBOARD:
        return "Rice Dryer";
    case UI_SCREEN_BATCH_SETUP:
        return "Batch Setup";
    case UI_SCREEN_DRYING:
        return "Drying";
    case UI_SCREEN_COOLING:
        return "Cooling";
    case UI_SCREEN_DISCHARGE:
        return "Discharge";
    case UI_SCREEN_COMPLETE:
        return "Batch Complete";
    case UI_SCREEN_MANUAL:
        return "Manual Control";
    case UI_SCREEN_SETTINGS:
        return "Settings";
    case UI_SCREEN_HISTORY:
        return "History";
    case UI_SCREEN_ALARMS:
        return "Alarms";
    case UI_SCREEN_SYSTEM:
        return "System Info";
    default:
        return "Rice Dryer";
    }
}

static void detach_all(void)
{
    dashboard_detach();
    batch_setup_detach();
    drying_detach();
    cooling_detach();
    discharge_detach();
    complete_detach();
    manual_detach();
    settings_detach();
    history_detach();
    alarms_detach();
    system_info_detach();
}

static void highlight_nav(void)
{
    for (size_t i = 0; i < sizeof(s_nav) / sizeof(s_nav[0]); ++i) {
        if (s_nav[i] == nullptr) {
            continue;
        }
        const int id = (int)(intptr_t)lv_obj_get_user_data(s_nav[i]);
        bool on = false;
        if (id == NAV_RUN) {
            on = s_screen == UI_SCREEN_DRYING || s_screen == UI_SCREEN_COOLING ||
                 s_screen == UI_SCREEN_DISCHARGE || s_screen == UI_SCREEN_COMPLETE;
        } else {
            on = id == (int)s_screen;
        }
        lv_obj_set_style_bg_color(s_nav[i],
                                  lv_color_hex(on ? 0x2F6F4E : 0x2A3A4A), 0);
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
    case UI_SCREEN_DRYING:
        drying_update(&s_status);
        break;
    case UI_SCREEN_COOLING:
        cooling_update(&s_status);
        break;
    case UI_SCREEN_DISCHARGE:
        discharge_update(&s_status);
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
    if (s_title != nullptr) {
        lv_label_set_text(s_title, screen_title(screen));
    }
    highlight_nav();

    switch (screen) {
    case UI_SCREEN_DASHBOARD:
        dashboard_build(s_content, &s_status);
        break;
    case UI_SCREEN_BATCH_SETUP:
        batch_setup_build(s_content, &s_status);
        break;
    case UI_SCREEN_DRYING:
        drying_build(s_content, &s_status);
        break;
    case UI_SCREEN_COOLING:
        cooling_build(s_content, &s_status);
        break;
    case UI_SCREEN_DISCHARGE:
        discharge_build(s_content, &s_status);
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
    }
}

static void on_nav(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const int id = (int)(intptr_t)lv_event_get_user_data(e);
    if (id == NAV_RUN) {
        ui_manager_navigate(screen_for_state(s_status.dryer_state));
    } else {
        ui_manager_navigate((ui_screen_t)id);
    }
}

void ui_manager_set_banner(const char *msg)
{
    if (s_banner != nullptr && msg != nullptr) {
        lv_label_set_text(s_banner, msg);
    }
}

void ui_manager_bind_tap_counter(unsigned *counter)
{
    s_tap_counter = counter;
    system_info_set_tap_counter(counter);
}

void ui_manager_show_hmi(const system_status_t *status)
{
    if (status != nullptr) {
        s_status = *status;
    }
    s_seen_state = s_status.dryer_state;
    s_taps = s_tap_counter != nullptr ? *s_tap_counter : 0;

    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B2838), 0);

    s_title = ui_label(scr, "Rice Dryer", 16, 8, &lv_font_montserrat_20, 0xF2F5F8);

    const struct {
        const char *text;
        int id;
    } nav[] = {
        {"Home", UI_SCREEN_DASHBOARD},
        {"Setup", UI_SCREEN_BATCH_SETUP},
        {"Run", NAV_RUN},
        {"Manual", UI_SCREEN_MANUAL},
        {"Settings", UI_SCREEN_SETTINGS},
        {"History", UI_SCREEN_HISTORY},
        {"Alarms", UI_SCREEN_ALARMS},
        {"Info", UI_SCREEN_SYSTEM},
    };
    for (size_t i = 0; i < sizeof(nav) / sizeof(nav[0]); ++i) {
        s_nav[i] = ui_make_btn(scr, nav[i].text, on_nav, (void *)(intptr_t)nav[i].id, 112, 36);
        lv_obj_set_user_data(s_nav[i], (void *)(intptr_t)nav[i].id);
        lv_obj_set_pos(s_nav[i], (lv_coord_t)(16 + (int)i * 124), 40);
    }

    s_fault = ui_label(scr, "", 16, 82, &lv_font_montserrat_14, 0xE85D4C);
    lv_obj_set_width(s_fault, 990);

    s_banner = ui_label(scr, "Load rice, then open Setup to start a batch.", 16, 104,
                        &lv_font_montserrat_14, 0xF2A65A);
    lv_obj_set_width(s_banner, 990);
    lv_label_set_long_mode(s_banner, LV_LABEL_LONG_DOT);

    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, 1000, 450);
    lv_obj_set_pos(s_content, 12, 132);
    lv_obj_set_style_bg_color(s_content, lv_color_hex(0x1B2838), 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 4, 0);
    lv_obj_set_scrollbar_mode(s_content, LV_SCROLLBAR_MODE_AUTO);

    ui_manager_navigate(screen_for_state(s_status.dryer_state));
}

void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps)
{
    if (status == nullptr || s_content == nullptr) {
        return;
    }
    s_status = *status;
    s_taps = touch_taps;

    if (s_fault != nullptr) {
        if (status->active_fault == 0) {
            lv_label_set_text(s_fault, "");
        } else {
            char line[128];
            snprintf(line, sizeof(line), "%s",
                     safety_manager_fault_name((fault_code_t)status->active_fault));
            lv_label_set_text(s_fault, line);
        }
    }

    if (status->dryer_state != s_seen_state) {
        s_seen_state = status->dryer_state;
        const ui_screen_t want = screen_for_state(status->dryer_state);
        if (screen_follows_process(s_screen) && want != s_screen) {
            ui_manager_navigate(want);
            return;
        }
    }
    update_active();
}
