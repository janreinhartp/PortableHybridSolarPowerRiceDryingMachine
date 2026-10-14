#include "settings_screen.h"

#include <cstdint>
#include <cstdio>

#include "process_controller.h"
#include "screen_common.h"
#include "settings_manager.h"
#include "ui_manager.h"

enum {
    F_MOIST = 0,
    F_DRY,
    F_HYST,
    F_COOL,
    F_MODE,
    F_INTERVAL,
    F_RUN,
    F_DISCHARGE,
    F_MIN,
    F_MAX,
    F_HOT,
    F_COUNT
};

struct adjust_arg_t {
    int field;
    int sign;
};

static bool s_alive = false;
static dryer_settings_t s_draft {};
static adjust_arg_t s_args[F_COUNT][2] = {};
static lv_obj_t *s_values[F_COUNT] = {};
static lv_obj_t *s_save = nullptr;

static uint32_t step_u32(uint32_t value, int delta, uint32_t min_v)
{
    const int64_t next = (int64_t)value + (int64_t)delta;
    if (next < (int64_t)min_v) {
        return min_v;
    }
    return (uint32_t)next;
}

static void clamp_draft(void)
{
    if (s_draft.target_moisture_pct < 0.5f) {
        s_draft.target_moisture_pct = 0.5f;
    }
    if (s_draft.target_moisture_pct > 40.0f) {
        s_draft.target_moisture_pct = 40.0f;
    }
    if (s_draft.drying_temperature_c < 20.0f) {
        s_draft.drying_temperature_c = 20.0f;
    }
    if (s_draft.drying_temperature_c > 65.0f) {
        s_draft.drying_temperature_c = 65.0f;
    }
    if (s_draft.temperature_hysteresis_c < 0.5f) {
        s_draft.temperature_hysteresis_c = 0.5f;
    }
    if (s_draft.temperature_hysteresis_c > 20.0f) {
        s_draft.temperature_hysteresis_c = 20.0f;
    }
    if (s_draft.cooling_temperature_c < 10.0f) {
        s_draft.cooling_temperature_c = 10.0f;
    }
    if (s_draft.cooling_temperature_c > 60.0f) {
        s_draft.cooling_temperature_c = 60.0f;
    }
    if (s_draft.cooling_temperature_c >= s_draft.drying_temperature_c) {
        s_draft.cooling_temperature_c = s_draft.drying_temperature_c - 1.0f;
    }
    if (s_draft.max_hot_air_temperature_c <= s_draft.drying_temperature_c) {
        s_draft.max_hot_air_temperature_c = s_draft.drying_temperature_c + 1.0f;
    }
    if (s_draft.max_hot_air_temperature_c > 90.0f) {
        s_draft.max_hot_air_temperature_c = 90.0f;
    }
    if (s_draft.circulation_interval_s < 2) {
        s_draft.circulation_interval_s = 2;
    }
    if (s_draft.circulation_run_time_s < 1) {
        s_draft.circulation_run_time_s = 1;
    }
    if (s_draft.circulation_run_time_s >= s_draft.circulation_interval_s) {
        s_draft.circulation_run_time_s = s_draft.circulation_interval_s - 1;
    }
    if (s_draft.discharge_time_s < 1) {
        s_draft.discharge_time_s = 1;
    }
    if (s_draft.min_drying_time_s < 1) {
        s_draft.min_drying_time_s = 1;
    }
    if (s_draft.max_drying_time_s <= s_draft.min_drying_time_s) {
        s_draft.max_drying_time_s = s_draft.min_drying_time_s + 60;
    }
}

static void render_values(void)
{
    char storage[F_COUNT][48];
    snprintf(storage[F_MOIST], sizeof(storage[0]), "%.1f %%", s_draft.target_moisture_pct);
    snprintf(storage[F_DRY], sizeof(storage[0]), "%.0f C", s_draft.drying_temperature_c);
    snprintf(storage[F_HYST], sizeof(storage[0]), "%.1f C", s_draft.temperature_hysteresis_c);
    snprintf(storage[F_COOL], sizeof(storage[0]), "%.0f C", s_draft.cooling_temperature_c);
    snprintf(storage[F_MODE], sizeof(storage[0]), "%s",
             ui_circ_mode_name((uint8_t)s_draft.circulation_mode));
    snprintf(storage[F_INTERVAL], sizeof(storage[0]), "%u s",
             (unsigned)s_draft.circulation_interval_s);
    snprintf(storage[F_RUN], sizeof(storage[0]), "%u s", (unsigned)s_draft.circulation_run_time_s);
    snprintf(storage[F_DISCHARGE], sizeof(storage[0]), "%u s", (unsigned)s_draft.discharge_time_s);
    snprintf(storage[F_MIN], sizeof(storage[0]), "%u s", (unsigned)s_draft.min_drying_time_s);
    snprintf(storage[F_MAX], sizeof(storage[0]), "%u s", (unsigned)s_draft.max_drying_time_s);
    snprintf(storage[F_HOT], sizeof(storage[0]), "%.0f C", s_draft.max_hot_air_temperature_c);
    for (int i = 0; i < F_COUNT; ++i) {
        if (s_values[i] != nullptr) {
            lv_label_set_text(s_values[i], storage[i]);
        }
    }
}

static void on_adjust(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const adjust_arg_t *arg = (const adjust_arg_t *)lv_event_get_user_data(e);
    if (arg == nullptr) {
        return;
    }
    switch (arg->field) {
    case F_MOIST:
        s_draft.target_moisture_pct += 0.5f * (float)arg->sign;
        break;
    case F_DRY:
        s_draft.drying_temperature_c += (float)arg->sign;
        break;
    case F_HYST:
        s_draft.temperature_hysteresis_c += 0.5f * (float)arg->sign;
        break;
    case F_COOL:
        s_draft.cooling_temperature_c += (float)arg->sign;
        break;
    case F_MODE:
        s_draft.circulation_mode =
            arg->sign > 0 ? CIRCULATION_MODE_CONTINUOUS : CIRCULATION_MODE_INTERVAL;
        break;
    case F_INTERVAL:
        s_draft.circulation_interval_s =
            step_u32(s_draft.circulation_interval_s, 5 * arg->sign, 2);
        break;
    case F_RUN:
        s_draft.circulation_run_time_s = step_u32(s_draft.circulation_run_time_s, arg->sign, 1);
        break;
    case F_DISCHARGE:
        s_draft.discharge_time_s = step_u32(s_draft.discharge_time_s, 5 * arg->sign, 1);
        break;
    case F_MIN:
        s_draft.min_drying_time_s = step_u32(s_draft.min_drying_time_s, 60 * arg->sign, 1);
        break;
    case F_MAX:
        s_draft.max_drying_time_s = step_u32(s_draft.max_drying_time_s, 300 * arg->sign, 1);
        break;
    case F_HOT:
        s_draft.max_hot_air_temperature_c += (float)arg->sign;
        break;
    default:
        break;
    }
    clamp_draft();
    render_values();
}

static void on_save(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    clamp_draft();
    const esp_err_t err = process_controller_save_settings(&s_draft);
    if (err == ESP_OK) {
        ui_manager_set_banner("Settings Saved");
    } else if (err == ESP_ERR_INVALID_STATE) {
        ui_manager_set_banner("Save Blocked While A Batch Is Running");
    } else {
        ui_manager_set_banner("Settings Rejected - Check The Ranges");
    }
}

static void on_defaults(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    settings_manager_get_defaults(&s_draft);
    clamp_draft();
    render_values();
    ui_manager_set_banner("Defaults Loaded - Press Save To Keep Them");
}

void settings_detach(void)
{
    s_alive = false;
    s_save = nullptr;
    for (int i = 0; i < F_COUNT; ++i) {
        s_values[i] = nullptr;
    }
}

void settings_build(lv_obj_t *parent, const system_status_t *status)
{
    (void)status;
    settings_manager_get(&s_draft);
    clamp_draft();

    static const char *names[F_COUNT] = {
        "Target Moisture", "Drying Temp",     "Hysteresis",  "Cooling Temp",
        "Circulation",     "Circ Interval",   "Circ Run",    "Discharge",
        "Min Drying",      "Max Drying",      "Max Hot Air",
    };

    lv_obj_t *scroll = lv_obj_create(parent);
    lv_obj_set_size(scroll, 1000, 320);
    lv_obj_set_pos(scroll, 0, 0);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, 0, 0);

    for (int i = 0; i < F_COUNT; ++i) {
        const lv_coord_t y = (lv_coord_t)(i * 48);
        lv_obj_t *card = ui_theme_make_card(scroll, 980, 44);
        lv_obj_set_pos(card, 8, y);
        lv_obj_t *name = ui_theme_make_label(card, names[i], ui_font_body(), UI_COLOR_TEXT);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
        s_values[i] = ui_theme_make_label(card, "", ui_font_body(), UI_COLOR_ACCENT);
        lv_obj_align(s_values[i], LV_ALIGN_LEFT_MID, 220, 0);
        s_args[i][0].field = i;
        s_args[i][0].sign = -1;
        s_args[i][1].field = i;
        s_args[i][1].sign = 1;
        const char *minus = (i == F_MODE) ? "Interval" : "-";
        const char *plus = (i == F_MODE) ? "Continuous" : "+";
        const lv_coord_t bw = (i == F_MODE) ? 130 : 56;
        lv_obj_t *down =
            ui_theme_make_btn(card, minus, on_adjust, &s_args[i][0], bw, 48, UI_COLOR_SURFACE_ALT);
        lv_obj_align(down, LV_ALIGN_RIGHT_MID, (lv_coord_t)(-(bw + 16)), 0);
        lv_obj_t *up =
            ui_theme_make_btn(card, plus, on_adjust, &s_args[i][1], bw, 48, UI_COLOR_SURFACE_ALT);
        lv_obj_align(up, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    s_save = ui_theme_make_btn(parent, "Save", on_save, nullptr, 180, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(s_save, 8, 340);
    lv_obj_t *defaults =
        ui_theme_make_btn(parent, "Restore Defaults", on_defaults, nullptr, 220, 64, UI_COLOR_ACCENT);
    lv_obj_set_pos(defaults, 204, 340);

    s_alive = true;
    render_values();
    settings_update(status);
}

void settings_update(const system_status_t *status)
{
    (void)status;
    if (!s_alive || s_save == nullptr) {
        return;
    }
    if (process_controller_settings_editable()) {
        lv_obj_clear_state(s_save, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_save, LV_STATE_DISABLED);
    }
}
