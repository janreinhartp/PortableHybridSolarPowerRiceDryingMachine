#include "ui_manager.h"

#include <cstdio>
#include <cstring>

#include "actuator_manager.h"
#include "batch_manager.h"
#include "circulation_manager.h"
#include "esp_log.h"
#include "lvgl.h"
#include "process_controller.h"
#include "safety_manager.h"
#include "settings_manager.h"
#include "th_sensor.h"

static const char *TAG_UI = "ui";

static lv_obj_t *s_status_box = nullptr;
static lv_obj_t *s_sensor_label = nullptr;
static lv_obj_t *s_relay_label = nullptr;
static lv_obj_t *s_process_label = nullptr;
static lv_obj_t *s_setup_label = nullptr;
static unsigned *s_tap_counter = nullptr;

static lv_obj_t *s_btn_fan = nullptr;
static lv_obj_t *s_btn_elev = nullptr;
static lv_obj_t *s_btn_heater = nullptr;
static lv_obj_t *s_btn_all_off = nullptr;
static lv_obj_t *s_btn_circ = nullptr;
static lv_obj_t *s_btn_door_open = nullptr;
static lv_obj_t *s_btn_door_close = nullptr;
static lv_obj_t *s_btn_door_stop = nullptr;

static void set_setup_msg(const char *msg);

static const char *ok_fail(bool ok)
{
    return ok ? "OK" : "FAIL";
}

static const char *on_off(bool on)
{
    return on ? "ON" : "off";
}

static bool manual_ok(void)
{
    return process_controller_manual_control_allowed();
}

static void set_manual_enabled(bool enabled)
{
    lv_obj_t *btns[] = {
        s_btn_fan,         s_btn_elev,       s_btn_heater,    s_btn_all_off,
        s_btn_circ,         s_btn_door_open,  s_btn_door_close, s_btn_door_stop,
    };
    for (size_t i = 0; i < sizeof(btns) / sizeof(btns[0]); ++i) {
        if (btns[i] == nullptr) {
            continue;
        }
        if (enabled) {
            lv_obj_clear_state(btns[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btns[i], LV_STATE_DISABLED);
        }
    }
}

static void on_tap_btn(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || s_tap_counter == nullptr) {
        return;
    }
    (*s_tap_counter)++;
}

static void on_start_batch(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    esp_err_t err = process_controller_start();
    if (err == ESP_OK) {
        set_setup_msg("Batch START → PRECHECK");
    } else if (batch_manager_interrupted_pending()) {
        set_setup_msg("START refused — ACK interrupted batch first");
    } else if (process_controller_get_state() != DRYER_STATE_IDLE &&
               process_controller_get_state() != DRYER_STATE_COMPLETE) {
        set_setup_msg("START refused — batch already running");
    } else {
        set_setup_msg("START refused (fault or invalid state)");
    }
}

static void on_ack_interrupted(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (process_controller_acknowledge_interrupted() == ESP_OK) {
        set_setup_msg("Interrupted batch acknowledged — START allowed");
    } else {
        set_setup_msg("ACK interrupted failed");
    }
}

static void on_abort_batch(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    process_controller_abort();
    set_setup_msg("Batch ABORT → IDLE");
}

static void on_fan_toggle(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!manual_ok()) {
        set_setup_msg("Manual blocked — batch active");
        return;
    }
    relay_state_t st {};
    actuator_manager_refresh(&st);
    actuator_manager_set_fan(!st.fan);
}

static void on_elev_toggle(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!manual_ok()) {
        set_setup_msg("Manual blocked — batch active");
        return;
    }
    if (circulation_manager_is_active()) {
        circulation_manager_stop();
        set_setup_msg("Circulation STOP (manual elevator)");
    }
    relay_state_t st {};
    actuator_manager_refresh(&st);
    actuator_manager_set_elevator(!st.elevator);
}

static void on_heater_toggle(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!manual_ok()) {
        set_setup_msg("Manual blocked — batch active");
        return;
    }
    relay_state_t st {};
    actuator_manager_refresh(&st);
    safety_manager_request_heater(!st.heater);
}

static void on_circ_toggle(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!manual_ok()) {
        set_setup_msg("Manual blocked — batch active");
        return;
    }
    dryer_settings_t cfg {};
    settings_manager_get(&cfg);
    circulation_manager_configure(&cfg);
    if (circulation_manager_is_active()) {
        circulation_manager_stop();
        set_setup_msg("Circulation STOP");
    } else {
        if (circulation_manager_start() == ESP_OK) {
            set_setup_msg(cfg.circulation_mode == CIRCULATION_MODE_CONTINUOUS
                              ? "Circulation START continuous"
                              : "Circulation START interval");
        } else {
            set_setup_msg("Circulation START failed (check settings)");
        }
    }
}

static void on_fault_ack(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (safety_manager_acknowledge() == ESP_OK) {
        set_setup_msg("Fault acknowledged");
    } else {
        set_setup_msg("Fault ACK refused (condition still active)");
    }
}

static void on_door_open(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!manual_ok()) {
        set_setup_msg("Manual blocked — batch active");
        return;
    }
    actuator_manager_open_door();
}

static void on_door_close(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!manual_ok()) {
        set_setup_msg("Manual blocked — batch active");
        return;
    }
    actuator_manager_close_door();
}

static void on_door_stop(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!manual_ok()) {
        set_setup_msg("Manual blocked — batch active");
        return;
    }
    actuator_manager_stop_door();
}

static void on_all_off(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!manual_ok()) {
        set_setup_msg("Manual blocked — use ABORT");
        return;
    }
    if (circulation_manager_is_active()) {
        circulation_manager_stop();
    }
    actuator_manager_all_off();
}

static void set_setup_msg(const char *msg)
{
    if (s_setup_label != nullptr) {
        lv_label_set_text(s_setup_label, msg);
    }
    ESP_LOGI(TAG_UI, "%s", msg);
}

/**
 * Assign a factory-default XY-MD03 (usually still at ID 1) to a product role.
 * Connect only ONE probe. Relay should already be moved off address 1 (DIP=10).
 */
static void assign_sensor_role(uint8_t new_addr, const char *role)
{
    static const uint8_t k_try[] = {1, 2, 3, 4, 5, 8, 10};
    uint8_t found = 0;
    th_reading_t reading {};

    for (size_t i = 0; i < sizeof(k_try); ++i) {
        if (th_sensor_read(k_try[i], &reading) == ESP_OK) {
            found = k_try[i];
            break;
        }
    }

    if (found == 0) {
        set_setup_msg("Sensor setup: no XY-MD03 answering (wire ONE probe only)");
        return;
    }

    if (found == new_addr) {
        char buf[96];
        snprintf(buf, sizeof(buf), "Sensor setup: already %s @%u (%.1fC)", role, found,
                 reading.temperature_c);
        set_setup_msg(buf);
        return;
    }

    esp_err_t err = th_sensor_set_address(found, new_addr);
    char buf[128];
    if (err == ESP_OK) {
        snprintf(buf, sizeof(buf),
                 "Sensor setup: %s wrote %u→%u OK — power-cycle probe", role, found, new_addr);
    } else {
        snprintf(buf, sizeof(buf), "Sensor setup: %s %u→%u FAILED (%s)", role, found, new_addr,
                 esp_err_to_name(err));
    }
    set_setup_msg(buf);
}

static void on_assign_hotair(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    assign_sensor_role(TH_SENSOR_HOTAIR_ADDR, "Hot-air");
}

static void on_assign_chamber(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    assign_sensor_role(TH_SENSOR_CHAMBER_ADDR, "Chamber");
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, lv_coord_t w)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, 44);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
    return btn;
}

void ui_manager_show_foundation_screen(const system_status_t *status)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1B2838), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Rice Dryer — Phase 6");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF2F5F8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle,
                      "Process + Batch  |  START / ACK INT / ABORT  |  keep heater power disconnected");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9BB0C4), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 40);

    s_status_box = lv_label_create(scr);
    lv_obj_set_style_text_color(s_status_box, lv_color_hex(0xE8EEF4), 0);
    lv_obj_set_style_text_font(s_status_box, &lv_font_montserrat_14, 0);
    lv_obj_align(s_status_box, LV_ALIGN_TOP_LEFT, 24, 68);

    s_process_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_process_label, lv_color_hex(0x7EC8FF), 0);
    lv_obj_set_style_text_font(s_process_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_process_label, LV_ALIGN_TOP_LEFT, 24, 96);

    s_sensor_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_sensor_label, lv_color_hex(0x7DCFB6), 0);
    lv_obj_set_style_text_font(s_sensor_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_sensor_label, LV_ALIGN_TOP_LEFT, 24, 150);

    s_relay_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_relay_label, lv_color_hex(0xF0C808), 0);
    lv_obj_set_style_text_font(s_relay_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_relay_label, LV_ALIGN_TOP_LEFT, 24, 230);

    s_setup_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_setup_label, lv_color_hex(0xF2A65A), 0);
    lv_obj_set_style_text_font(s_setup_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_setup_label, LV_ALIGN_TOP_LEFT, 24, 320);
    lv_label_set_text(s_setup_label,
                      "Load rice, then START. If interrupted flag set, press ACK INT first.");

    /* Row 0 — Process controls */
    lv_obj_t *p1 = make_btn(scr, "START", on_start_batch, 110);
    lv_obj_align(p1, LV_ALIGN_BOTTOM_LEFT, 16, -140);
    lv_obj_t *p2 = make_btn(scr, "ACK INT", on_ack_interrupted, 110);
    lv_obj_align(p2, LV_ALIGN_BOTTOM_LEFT, 136, -140);
    lv_obj_t *p3 = make_btn(scr, "ABORT", on_abort_batch, 110);
    lv_obj_align(p3, LV_ALIGN_BOTTOM_LEFT, 256, -140);
    lv_obj_t *p4 = make_btn(scr, "ACK Fault", on_fault_ack, 120);
    lv_obj_align(p4, LV_ALIGN_BOTTOM_LEFT, 376, -140);
    lv_obj_t *p5 = make_btn(scr, "Touch+", on_tap_btn, 100);
    lv_obj_align(p5, LV_ALIGN_BOTTOM_LEFT, 508, -140);

    /* Row 1 — Manual Control (disabled while batch active) */
    s_btn_fan = make_btn(scr, "Fan", on_fan_toggle, 90);
    lv_obj_align(s_btn_fan, LV_ALIGN_BOTTOM_LEFT, 16, -84);
    s_btn_elev = make_btn(scr, "Elev", on_elev_toggle, 90);
    lv_obj_align(s_btn_elev, LV_ALIGN_BOTTOM_LEFT, 116, -84);
    s_btn_heater = make_btn(scr, "Heater*", on_heater_toggle, 100);
    lv_obj_align(s_btn_heater, LV_ALIGN_BOTTOM_LEFT, 216, -84);
    s_btn_all_off = make_btn(scr, "All OFF", on_all_off, 100);
    lv_obj_align(s_btn_all_off, LV_ALIGN_BOTTOM_LEFT, 328, -84);
    s_btn_circ = make_btn(scr, "Circ", on_circ_toggle, 90);
    lv_obj_align(s_btn_circ, LV_ALIGN_BOTTOM_LEFT, 440, -84);

    /* Row 2 door + sensor address commissioning */
    s_btn_door_open = make_btn(scr, "Door OPEN", on_door_open, 130);
    lv_obj_align(s_btn_door_open, LV_ALIGN_BOTTOM_LEFT, 16, -24);
    s_btn_door_close = make_btn(scr, "Door CLOSE", on_door_close, 130);
    lv_obj_align(s_btn_door_close, LV_ALIGN_BOTTOM_LEFT, 156, -24);
    s_btn_door_stop = make_btn(scr, "Door STOP", on_door_stop, 120);
    lv_obj_align(s_btn_door_stop, LV_ALIGN_BOTTOM_LEFT, 296, -24);
    lv_obj_t *s1 = make_btn(scr, "->Hotair(1)", on_assign_hotair, 140);
    lv_obj_align(s1, LV_ALIGN_BOTTOM_LEFT, 436, -24);
    lv_obj_t *s2 = make_btn(scr, "->Chamber(2)", on_assign_chamber, 150);
    lv_obj_align(s2, LV_ALIGN_BOTTOM_LEFT, 588, -24);

    ui_manager_update_runtime(status, 0);
}

void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps)
{
    if (status == nullptr) {
        return;
    }

    set_manual_enabled(process_controller_manual_control_allowed());

    if (s_status_box != nullptr) {
        char line[192];
        snprintf(line, sizeof(line),
                 "Disp %s  Touch %s  SD %s  RTC %s  RS485 %s  Relay %s  ADS %s  taps %u",
                 ok_fail(status->display_ok),
                 ok_fail(status->touch_ok),
                 ok_fail(status->sd_ok),
                 ok_fail(status->rtc_ok),
                 ok_fail(status->rs485_ok),
                 ok_fail(status->relay_ok),
                 ok_fail(status->ads1115_ok),
                 touch_taps);
        lv_label_set_text(s_status_box, line);
    }

    if (s_process_label != nullptr) {
        char line[256];
        snprintf(line, sizeof(line),
                 "State %s   Batch %s   %s\n"
                 "Elapsed dry %us  cool %us  disc %us   tgt moist %.1f%%  dry %.0fC  cool %.0fC",
                 process_controller_state_name((dryer_state_t)status->dryer_state),
                 status->batch_id[0] ? status->batch_id : "-",
                 status->batch_interrupted ? "INTERRUPTED — ACK INT" : "",
                 (unsigned)status->drying_elapsed_s,
                 (unsigned)status->cooling_elapsed_s,
                 (unsigned)status->discharge_elapsed_s,
                 status->target_moisture_pct,
                 status->drying_temperature_c,
                 status->cooling_temperature_c);
        lv_label_set_text(s_process_label, line);
    }

    if (s_sensor_label != nullptr) {
        char line[280];
        if (status->hotair_ok) {
            snprintf(line, sizeof(line), "Hot-air  #%u : %.1f C   %.1f %%RH\n",
                     1, status->hotair_temp_c, status->hotair_rh);
        } else {
            snprintf(line, sizeof(line), "Hot-air  #%u : OFFLINE\n", 1);
        }
        char chamber[96];
        if (status->chamber_ok) {
            snprintf(chamber, sizeof(chamber), "Chamber #%u : %.1f C   %.1f %%RH\n",
                     2, status->chamber_temp_c, status->chamber_rh);
        } else {
            snprintf(chamber, sizeof(chamber), "Chamber #%u : OFFLINE\n", 2);
        }
        strncat(line, chamber, sizeof(line) - strlen(line) - 1);

        char analog[128];
        if (status->moisture_ok || status->ec_ok) {
            snprintf(analog, sizeof(analog),
                     "Moisture %s %.1f%% (%.2fV)   EC %s %.2f (%.2fV)",
                     status->moisture_ok ? "OK" : "FAIL",
                     status->moisture_pct, status->moisture_voltage,
                     status->ec_ok ? "OK" : "FAIL",
                     status->ec_value, status->ec_voltage);
        } else if (status->ads1115_ok) {
            snprintf(analog, sizeof(analog), "Moisture/EC : ADS online, reading invalid");
        } else {
            snprintf(analog, sizeof(analog), "Moisture/EC : ADS1115 OFFLINE");
        }
        strncat(line, analog, sizeof(line) - strlen(line) - 1);
        lv_label_set_text(s_sensor_label, line);
    }

    if (s_relay_label != nullptr) {
        char line[220];
        snprintf(line, sizeof(line),
                 "Relays  Elev %s  Heat %s  Fan %s  DoorOpen %s  DoorClose %s\n"
                 "Safety  heat_ok=%s  forceHeatOff=%s  forceFanOn=%s  %s\n"
                 "Circ    %s  elev=%s  mode=%s",
                 on_off(status->relay_elevator),
                 on_off(status->relay_heater),
                 on_off(status->relay_fan),
                 on_off(status->relay_door_open),
                 on_off(status->relay_door_close),
                 status->heating_permitted ? "YES" : "NO",
                 status->safety_heater_forced_off ? "YES" : "no",
                 status->safety_fan_forced_on ? "YES" : "no",
                 safety_manager_fault_name((fault_code_t)status->active_fault),
                 status->circulation_active ? "ACTIVE" : "idle",
                 on_off(status->circulation_elevator_on),
                 status->circulation_mode == (uint8_t)CIRCULATION_MODE_CONTINUOUS
                     ? "continuous"
                     : "interval");
        lv_label_set_text(s_relay_label, line);
    }
}

void ui_manager_bind_tap_counter(unsigned *counter)
{
    s_tap_counter = counter;
}
