#include "set_clock_screen.h"

#include <cstdio>

#include "process_controller.h"
#include "rtc_manager.h"
#include "screen_common.h"
#include "ui_manager.h"

enum {
    F_YEAR = 0,
    F_MONTH,
    F_DAY,
    F_HOUR,
    F_MINUTE,
    F_COUNT
};

struct adjust_arg_t {
    int field;
    int sign;
};

static bool s_alive = false;
static rtc_datetime_t s_draft {};
static adjust_arg_t s_args[F_COUNT][2] = {};
static lv_obj_t *s_values[F_COUNT] = {};
static lv_obj_t *s_preview = nullptr;
static lv_obj_t *s_note = nullptr;
static lv_obj_t *s_save = nullptr;

static int days_in_month(uint16_t year, uint8_t month)
{
    static const int k_days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 31;
    }
    int d = k_days[month];
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        if (leap) {
            d = 29;
        }
    }
    return d;
}

static void clamp_draft(void)
{
    if (s_draft.year < 2020) {
        s_draft.year = 2020;
    }
    if (s_draft.year > 2099) {
        s_draft.year = 2099;
    }
    if (s_draft.month < 1) {
        s_draft.month = 1;
    }
    if (s_draft.month > 12) {
        s_draft.month = 12;
    }
    const int dim = days_in_month(s_draft.year, s_draft.month);
    if (s_draft.day < 1) {
        s_draft.day = 1;
    }
    if (s_draft.day > dim) {
        s_draft.day = (uint8_t)dim;
    }
    if (s_draft.hour > 23) {
        s_draft.hour = 23;
    }
    if (s_draft.minute > 59) {
        s_draft.minute = 59;
    }
    s_draft.second = 0;
}

static void render(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%04u", (unsigned)s_draft.year);
    ui_label_set_text_if_changed(s_values[F_YEAR], buf);
    snprintf(buf, sizeof(buf), "%02u", (unsigned)s_draft.month);
    ui_label_set_text_if_changed(s_values[F_MONTH], buf);
    snprintf(buf, sizeof(buf), "%02u", (unsigned)s_draft.day);
    ui_label_set_text_if_changed(s_values[F_DAY], buf);
    snprintf(buf, sizeof(buf), "%02u", (unsigned)s_draft.hour);
    ui_label_set_text_if_changed(s_values[F_HOUR], buf);
    snprintf(buf, sizeof(buf), "%02u", (unsigned)s_draft.minute);
    ui_label_set_text_if_changed(s_values[F_MINUTE], buf);

    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u", (unsigned)s_draft.year,
             (unsigned)s_draft.month, (unsigned)s_draft.day, (unsigned)s_draft.hour,
             (unsigned)s_draft.minute);
    ui_label_set_text_if_changed(s_preview, buf);
}

static void on_adjust(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!process_controller_manual_control_allowed()) {
        ui_manager_set_banner("Clock Set Available In Idle Only");
        return;
    }
    const adjust_arg_t *arg = (const adjust_arg_t *)lv_event_get_user_data(e);
    if (arg == nullptr) {
        return;
    }
    switch (arg->field) {
    case F_YEAR:
        s_draft.year = (uint16_t)((int)s_draft.year + arg->sign);
        break;
    case F_MONTH:
        s_draft.month = (uint8_t)((int)s_draft.month + arg->sign);
        break;
    case F_DAY:
        s_draft.day = (uint8_t)((int)s_draft.day + arg->sign);
        break;
    case F_HOUR:
        s_draft.hour = (uint8_t)((int)s_draft.hour + arg->sign);
        break;
    case F_MINUTE:
        s_draft.minute = (uint8_t)((int)s_draft.minute + arg->sign);
        break;
    default:
        break;
    }
    clamp_draft();
    render();
}

static void on_back(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_SYSTEM);
    }
}

static void on_save(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!process_controller_manual_control_allowed()) {
        ui_manager_set_banner("Clock Set Available In Idle Only");
        return;
    }
    clamp_draft();
    if (rtc_manager_set(&s_draft) == ESP_OK) {
        ui_manager_set_banner("Clock Saved");
    } else {
        ui_manager_set_banner("Clock Save Failed");
    }
}

void set_clock_detach(void)
{
    s_alive = false;
    s_preview = nullptr;
    s_note = nullptr;
    s_save = nullptr;
    for (int i = 0; i < F_COUNT; ++i) {
        s_values[i] = nullptr;
    }
}

void set_clock_build(lv_obj_t *parent, const system_status_t *status)
{
    (void)status;
    if (rtc_manager_get(&s_draft) != ESP_OK) {
        s_draft.year = 2026;
        s_draft.month = 1;
        s_draft.day = 1;
        s_draft.hour = 0;
        s_draft.minute = 0;
        s_draft.second = 0;
        ui_manager_set_banner("RTC Read Failed — Edit Carefully");
    }
    clamp_draft();

    static const char *names[F_COUNT] = {"Year", "Month", "Day", "Hour", "Minute"};

    lv_obj_t *title = ui_theme_make_label(parent, "Set Clock", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    s_preview = ui_theme_make_label(parent, "", ui_font_kpi(), UI_COLOR_CREAM);
    lv_obj_set_pos(s_preview, 0, 48);

    s_note = ui_theme_make_label(parent, "", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_pos(s_note, 0, 108);
    lv_obj_set_width(s_note, 992);

    for (int i = 0; i < F_COUNT; ++i) {
        lv_obj_t *card = ui_theme_make_card(parent, 184, 120);
        lv_obj_set_pos(card, (lv_coord_t)(i * 200), 148);
        lv_obj_t *name = ui_theme_make_label(card, names[i], ui_font_caption(), UI_COLOR_MUTED);
        lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 0);

        s_args[i][0].field = i;
        s_args[i][0].sign = -1;
        s_args[i][1].field = i;
        s_args[i][1].sign = 1;

        lv_obj_t *down =
            ui_theme_make_btn(card, "-", on_adjust, &s_args[i][0], 48, 40, UI_COLOR_SURFACE_ALT);
        lv_obj_align(down, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        s_values[i] = ui_theme_make_label(card, "", ui_font_title(), UI_COLOR_CREAM);
        lv_obj_align(s_values[i], LV_ALIGN_CENTER, 0, 8);
        lv_obj_t *up =
            ui_theme_make_btn(card, "+", on_adjust, &s_args[i][1], 48, 40, UI_COLOR_SURFACE_ALT);
        lv_obj_align(up, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }

    lv_obj_t *back =
        ui_theme_make_btn(parent, "Back", on_back, nullptr, 488, 56, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(back, 0, 332);
    s_save = ui_theme_make_btn(parent, "Save", on_save, nullptr, 488, 56, UI_COLOR_TEAL);
    lv_obj_set_pos(s_save, 504, 332);

    s_alive = true;
    render();
    set_clock_update(status);
}

void set_clock_update(const system_status_t *status)
{
    (void)status;
    if (!s_alive || s_save == nullptr) {
        return;
    }
    const bool ok = process_controller_manual_control_allowed();
    ui_label_set_text_if_changed(s_note, ok ? "Adjust date and time, then Save."
                                            : "Available in Idle / Complete only.");
    if (ok) {
        lv_obj_clear_state(s_save, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_save, LV_STATE_DISABLED);
    }
}
