#include "dryer_controller.h"

#include "board_hal.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "modbus_rtu.h"
#include "relay_modbus.h"
#include "rtc_manager.h"
#include "th_sensor.h"

static const char *TAG = "dryer_ctrl";

static void refresh_heap(system_status_t *status)
{
    status->free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    status->free_internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void refresh_rtc(system_status_t *status)
{
    rtc_datetime_t dt {};
    if (rtc_manager_get(&dt) == ESP_OK) {
        status->rtc_ok = true;
        status->rtc_year = dt.year;
        status->rtc_month = dt.month;
        status->rtc_day = dt.day;
        status->rtc_hour = dt.hour;
        status->rtc_minute = dt.minute;
        status->rtc_second = dt.second;
    } else {
        status->rtc_ok = false;
    }
}

esp_err_t dryer_controller_init(system_status_t *status)
{
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *status = {};
    refresh_heap(status);

    ESP_LOGI(TAG, "Phase 3 bring-up starting");

    board_hal_status_t board {};
    esp_err_t err = board_hal_init(&board);

    status->display_ok = board.display_ok;
    status->touch_ok = board.touch_ok;
    status->lvgl_ok = board.lvgl_ok;
    status->sd_ok = board.sd_ok;
    status->sd_total_mb = board.sd_total_mb;
    status->sd_free_mb = board.sd_free_mb;

    if (err != ESP_OK) {
        refresh_heap(status);
        ESP_LOGE(TAG, "Board bring-up failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_err_t rtc_err = rtc_manager_init();
    status->rtc_ok = (rtc_err == ESP_OK);
    if (status->rtc_ok) {
        refresh_rtc(status);
    } else {
        ESP_LOGW(TAG, "RTC init failed: %s", esp_err_to_name(rtc_err));
    }

    esp_err_t bus_err = modbus_rtu_init();
    status->rs485_ok = (bus_err == ESP_OK);
    if (bus_err != ESP_OK) {
        ESP_LOGE(TAG, "RS485/Modbus init failed: %s", esp_err_to_name(bus_err));
    } else {
        th_sensor_init();
        relay_modbus_init();
        /* Fieldbus poll deferred to runtime task — no Modbus timeouts in app_main. */
        status->relay_ok = false;
        status->hotair_ok = false;
        status->chamber_ok = false;
    }

    refresh_heap(status);
    ESP_LOGI(TAG, "Bring-up done rtc=%s rs485=%s heap=%u (fieldbus probe deferred)",
             status->rtc_ok ? "ok" : "fail",
             status->rs485_ok ? "ok" : "fail",
             (unsigned)status->free_heap);
    return ESP_OK;
}

void dryer_controller_refresh_heap(system_status_t *status)
{
    if (status == nullptr) {
        return;
    }
    refresh_heap(status);
    refresh_rtc(status);
}

void dryer_controller_poll_fieldbus(system_status_t *status)
{
    if (status == nullptr || !status->rs485_ok) {
        return;
    }

    /* Discover relay first so we can skip T/H polls that collide with its ID
     * (R4D8A08 with all address DIPs OFF answers as slave 1). */
    relay_state_t relays {};
    status->relay_ok = (relay_modbus_refresh(&relays) == ESP_OK);
    const uint8_t relay_addr = relay_modbus_address();

    th_reading_t hotair {};
    th_reading_t chamber {};
    if (relay_addr != 0 && relay_addr == TH_SENSOR_HOTAIR_ADDR) {
        status->hotair_ok = false;
        status->hotair_temp_c = 0.0f;
        status->hotair_rh = 0.0f;
    } else {
        status->hotair_ok = (th_sensor_read(TH_SENSOR_HOTAIR_ADDR, &hotair) == ESP_OK);
        status->hotair_temp_c = hotair.temperature_c;
        status->hotair_rh = hotair.humidity_rh;
    }
    if (relay_addr != 0 && relay_addr == TH_SENSOR_CHAMBER_ADDR) {
        status->chamber_ok = false;
        status->chamber_temp_c = 0.0f;
        status->chamber_rh = 0.0f;
    } else {
        status->chamber_ok = (th_sensor_read(TH_SENSOR_CHAMBER_ADDR, &chamber) == ESP_OK);
        status->chamber_temp_c = chamber.temperature_c;
        status->chamber_rh = chamber.humidity_rh;
    }
    status->relay_elevator = relays.elevator;
    status->relay_heater = relays.heater;
    status->relay_fan = relays.fan;
    status->relay_door_open = relays.door_open;
    status->relay_door_close = relays.door_close;
}
