#include "ui_manager.h"

#include <cstdio>
#include <cstring>

#include "lvgl.h"
#include "relay_modbus.h"

static lv_obj_t *s_status_box = nullptr;
static lv_obj_t *s_sensor_label = nullptr;
static lv_obj_t *s_relay_label = nullptr;
static lv_obj_t *s_rtc_label = nullptr;
static unsigned *s_tap_counter = nullptr;

static const char *ok_fail(bool ok)
{
    return ok ? "OK" : "FAIL";
}

static const char *on_off(bool on)
{
    return on ? "ON" : "off";
}

static void on_tap_btn(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || s_tap_counter == nullptr) {
        return;
    }
    (*s_tap_counter)++;
}

static void on_fan_toggle(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    relay_state_t st {};
    relay_modbus_refresh(&st);
    relay_modbus_set_fan(!st.fan);
}

static void on_elev_toggle(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    relay_state_t st {};
    relay_modbus_refresh(&st);
    relay_modbus_set_elevator(!st.elevator);
}

static void on_heater_toggle(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    relay_state_t st {};
    relay_modbus_refresh(&st);
    /* Control-line test only — keep heater power disconnected in Phase 3. */
    relay_modbus_set_heater(!st.heater);
}

static void on_door_open(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    relay_modbus_open_door();
}

static void on_door_close(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    relay_modbus_close_door();
}

static void on_door_stop(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    relay_modbus_stop_door();
}

static void on_all_off(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    relay_modbus_all_off();
}

static lv_obj_t *make_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb, lv_coord_t w)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, 48);
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
    lv_label_set_text(title, "Rice Dryer — Phase 3");
    lv_obj_set_style_text_color(title, lv_color_hex(0xF2F5F8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *subtitle = lv_label_create(scr);
    lv_label_set_text(subtitle, "RS485 / Modbus  |  heater power stay disconnected");
    lv_obj_set_style_text_color(subtitle, lv_color_hex(0x9BB0C4), 0);
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 44);

    s_status_box = lv_label_create(scr);
    lv_obj_set_style_text_color(s_status_box, lv_color_hex(0xE8EEF4), 0);
    lv_obj_set_style_text_font(s_status_box, &lv_font_montserrat_14, 0);
    lv_obj_align(s_status_box, LV_ALIGN_TOP_LEFT, 32, 78);

    s_sensor_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_sensor_label, lv_color_hex(0x7DCFB6), 0);
    lv_obj_set_style_text_font(s_sensor_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_sensor_label, LV_ALIGN_TOP_LEFT, 32, 150);

    s_relay_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_relay_label, lv_color_hex(0xF0C808), 0);
    lv_obj_set_style_text_font(s_relay_label, &lv_font_montserrat_16, 0);
    lv_obj_align(s_relay_label, LV_ALIGN_TOP_LEFT, 32, 230);

    s_rtc_label = lv_label_create(scr);
    lv_obj_set_style_text_color(s_rtc_label, lv_color_hex(0x9BB0C4), 0);
    lv_obj_set_style_text_font(s_rtc_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_rtc_label, LV_ALIGN_TOP_LEFT, 32, 300);

    /* Row 1 */
    lv_obj_t *b1 = make_btn(scr, "Fan", on_fan_toggle, 120);
    lv_obj_align(b1, LV_ALIGN_BOTTOM_LEFT, 24, -84);
    lv_obj_t *b2 = make_btn(scr, "Elevator", on_elev_toggle, 130);
    lv_obj_align(b2, LV_ALIGN_BOTTOM_LEFT, 156, -84);
    lv_obj_t *b3 = make_btn(scr, "Heater*", on_heater_toggle, 120);
    lv_obj_align(b3, LV_ALIGN_BOTTOM_LEFT, 300, -84);
    lv_obj_t *b4 = make_btn(scr, "All OFF", on_all_off, 120);
    lv_obj_align(b4, LV_ALIGN_BOTTOM_LEFT, 436, -84);
    lv_obj_t *b5 = make_btn(scr, "Touch+", on_tap_btn, 110);
    lv_obj_align(b5, LV_ALIGN_BOTTOM_LEFT, 570, -84);

    /* Row 2 door */
    lv_obj_t *d1 = make_btn(scr, "Door OPEN", on_door_open, 150);
    lv_obj_align(d1, LV_ALIGN_BOTTOM_LEFT, 24, -24);
    lv_obj_t *d2 = make_btn(scr, "Door CLOSE", on_door_close, 150);
    lv_obj_align(d2, LV_ALIGN_BOTTOM_LEFT, 188, -24);
    lv_obj_t *d3 = make_btn(scr, "Door STOP", on_door_stop, 140);
    lv_obj_align(d3, LV_ALIGN_BOTTOM_LEFT, 352, -24);

    ui_manager_update_runtime(status, 0);
}

void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps)
{
    if (status == nullptr) {
        return;
    }

    if (s_status_box != nullptr) {
        char line[160];
        snprintf(line, sizeof(line),
                 "Disp %s  Touch %s  SD %s  RTC %s  RS485 %s  Relay %s  taps %u",
                 ok_fail(status->display_ok),
                 ok_fail(status->touch_ok),
                 ok_fail(status->sd_ok),
                 ok_fail(status->rtc_ok),
                 ok_fail(status->rs485_ok),
                 ok_fail(status->relay_ok),
                 touch_taps);
        lv_label_set_text(s_status_box, line);
    }

    if (s_sensor_label != nullptr) {
        char line[192];
        if (status->hotair_ok) {
            snprintf(line, sizeof(line), "Hot-air  #%u : %.1f C   %.1f %%RH\n",
                     1, status->hotair_temp_c, status->hotair_rh);
        } else {
            snprintf(line, sizeof(line), "Hot-air  #%u : OFFLINE\n", 1);
        }
        char chamber[96];
        if (status->chamber_ok) {
            snprintf(chamber, sizeof(chamber), "Chamber #%u : %.1f C   %.1f %%RH",
                     2, status->chamber_temp_c, status->chamber_rh);
        } else {
            snprintf(chamber, sizeof(chamber), "Chamber #%u : OFFLINE", 2);
        }
        strncat(line, chamber, sizeof(line) - strlen(line) - 1);
        lv_label_set_text(s_sensor_label, line);
    }

    if (s_relay_label != nullptr) {
        char line[160];
        snprintf(line, sizeof(line),
                 "Relays  Elev %s  Heat %s  Fan %s  DoorOpen %s  DoorClose %s",
                 on_off(status->relay_elevator),
                 on_off(status->relay_heater),
                 on_off(status->relay_fan),
                 on_off(status->relay_door_open),
                 on_off(status->relay_door_close));
        lv_label_set_text(s_relay_label, line);
    }

    if (s_rtc_label != nullptr) {
        if (status->rtc_ok) {
            char buf[80];
            snprintf(buf, sizeof(buf), "RTC %04u-%02u-%02u %02u:%02u:%02u   heap %u",
                     status->rtc_year, status->rtc_month, status->rtc_day,
                     status->rtc_hour, status->rtc_minute, status->rtc_second,
                     (unsigned)status->free_heap);
            lv_label_set_text(s_rtc_label, buf);
        } else {
            char buf[64];
            snprintf(buf, sizeof(buf), "RTC FAIL   heap %u", (unsigned)status->free_heap);
            lv_label_set_text(s_rtc_label, buf);
        }
    }
}

void ui_manager_bind_tap_counter(unsigned *counter)
{
    s_tap_counter = counter;
}
