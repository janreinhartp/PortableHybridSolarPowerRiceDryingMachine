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
    F_COOL,
    F_KP,
    F_KI,
    F_KD,
    F_WIN,
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
    if (s_draft.cooling_temperature_c < 10.0f) {
        s_draft.cooling_temperature_c = 10.0f;
    }
    if (s_draft.cooling_temperature_c > 60.0f) {
        s_draft.cooling_temperature_c = 60.0f;
    }
    if (s_draft.cooling_temperature_c >= s_draft.drying_temperature_c) {
        s_draft.cooling_temperature_c = s_draft.drying_temperature_c - 1.0f;
    }
    if (s_draft.pid_kp < 0.0f) {
        s_draft.pid_kp = 0.0f;
    }
    if (s_draft.pid_kp > 100.0f) {
        s_draft.pid_kp = 100.0f;
    }
    if (s_draft.pid_ki < 0.0f) {
        s_draft.pid_ki = 0.0f;
    }
    if (s_draft.pid_ki > 10.0f) {
        s_draft.pid_ki = 10.0f;
    }
    if (s_draft.pid_kd < 0.0f) {
        s_draft.pid_kd = 0.0f;
    }
    if (s_draft.pid_kd > 100.0f) {
        s_draft.pid_kd = 100.0f;
    }
    if (s_draft.pid_window_s < 10) {
        s_draft.pid_window_s = 10;
    }
    if (s_draft.pid_window_s > 60) {
        s_draft.pid_window_s = 60;
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
    snprintf(storage[F_DRY], sizeof(storage[0]), "%.0f °C", s_draft.drying_temperature_c);
    snprintf(storage[F_COOL], sizeof(storage[0]), "%.0f °C", s_draft.cooling_temperature_c);
    snprintf(storage[F_KP], sizeof(storage[0]), "%.1f", s_draft.pid_kp);
    snprintf(storage[F_KI], sizeof(storage[0]), "%.2f", s_draft.pid_ki);
    snprintf(storage[F_KD], sizeof(storage[0]), "%.1f", s_draft.pid_kd);
    snprintf(storage[F_WIN], sizeof(storage[0]), "%u s", (unsigned)s_draft.pid_window_s);
    snprintf(storage[F_MODE], sizeof(storage[0]), "%s",
             ui_circ_mode_name((uint8_t)s_draft.circulation_mode));
    snprintf(storage[F_INTERVAL], sizeof(storage[0]), "%u s",
             (unsigned)s_draft.circulation_interval_s);
    snprintf(storage[F_RUN], sizeof(storage[0]), "%u s", (unsigned)s_draft.circulation_run_time_s);
    snprintf(storage[F_DISCHARGE], sizeof(storage[0]), "%u s", (unsigned)s_draft.discharge_time_s);
    snprintf(storage[F_MIN], sizeof(storage[0]), "%u s", (unsigned)s_draft.min_drying_time_s);
    snprintf(storage[F_MAX], sizeof(storage[0]), "%u s", (unsigned)s_draft.max_drying_time_s);
    snprintf(storage[F_HOT], sizeof(storage[0]), "%.0f °C", s_draft.max_hot_air_temperature_c);
    for (int i = 0; i < F_COUNT; ++i) {
        if (s_values[i] != nullptr) {
            ui_label_set_text_if_changed(s_values[i], storage[i]);
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
    case F_COOL:
        s_draft.cooling_temperature_c += (float)arg->sign;
        break;
    case F_KP:
        s_draft.pid_kp += 0.5f * (float)arg->sign;
        break;
    case F_KI:
        s_draft.pid_ki += 0.01f * (float)arg->sign;
        break;
    case F_KD:
        s_draft.pid_kd += 0.5f * (float)arg->sign;
        break;
    case F_WIN:
        s_draft.pid_window_s = step_u32(s_draft.pid_window_s, 5 * arg->sign, 10);
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
        "Target Moisture", "Drying Temp",   "Cooling Temp", "PID Kp",
        "PID Ki",          "PID Kd",        "PID Window",   "Circulation",
        "Circ Interval",   "Circ Run",      "Discharge",    "Min Drying",
        "Max Drying",      "Max Hot Air",
    };

    lv_obj_t *title = ui_theme_make_label(parent, "Settings", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    /* Non-scrollable grid: 5×3 denser cards fit above Defaults/Save. */
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_set_size(grid, 992, 280);
    lv_obj_set_pos(grid, 0, 40);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(grid, LV_SCROLLBAR_MODE_OFF);

    const lv_coord_t card_w = 320;
    const lv_coord_t card_h = 52;
    const lv_coord_t gap = 16;
    const lv_coord_t row_pitch = 56;
    for (int i = 0; i < F_COUNT; ++i) {
        const int row = i / 3;
        const int col = i % 3;
        lv_obj_t *card = ui_theme_make_card(grid, card_w, card_h);
        lv_obj_set_pos(card, (lv_coord_t)(col * (card_w + gap)),
                       (lv_coord_t)(row * row_pitch));
        lv_obj_t *name = ui_theme_make_label(card, names[i], ui_font_caption(), UI_COLOR_MUTED);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        s_args[i][0].field = i;
        s_args[i][0].sign = -1;
        s_args[i][1].field = i;
        s_args[i][1].sign = 1;

        lv_obj_t *down =
            ui_theme_make_btn(card, "-", on_adjust, &s_args[i][0], 36, 36, UI_COLOR_SURFACE_ALT);
        lv_obj_align(down, LV_ALIGN_RIGHT_MID, -108, 0);
        s_values[i] = ui_theme_make_label(card, "", ui_font_body(), UI_COLOR_CREAM);
        lv_obj_set_width(s_values[i], 64);
        lv_label_set_long_mode(s_values[i], LV_LABEL_LONG_CLIP);
        lv_obj_align(s_values[i], LV_ALIGN_RIGHT_MID, -48, 0);
        lv_obj_t *up =
            ui_theme_make_btn(card, "+", on_adjust, &s_args[i][1], 36, 36, UI_COLOR_SURFACE_ALT);
        lv_obj_align(up, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    lv_obj_t *defaults =
        ui_theme_make_btn(parent, "Defaults", on_defaults, nullptr, 488, 56, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(defaults, 0, 332);
    s_save = ui_theme_make_btn(parent, "Save", on_save, nullptr, 488, 56, UI_COLOR_TEAL);
    lv_obj_set_pos(s_save, 504, 332);

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
