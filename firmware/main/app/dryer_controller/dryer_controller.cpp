#include "dryer_controller.h"

#include "actuator_manager.h"
#include "board_hal.h"
#include "circulation_manager.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "modbus_rtu.h"
#include "relay_modbus.h"
#include "rtc_manager.h"
#include "safety_manager.h"
#include "sensor_manager.h"
#include "settings_manager.h"
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

    ESP_LOGI(TAG, "Phase 5 bring-up starting");

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

    /* ADS1115 + calibration after shared I2C is up (touch bring-up). */
    sensor_manager_init();
    status->ads1115_ok = false;

    settings_manager_init();
    safety_manager_init();
    circulation_manager_init();
    {
        dryer_settings_t cfg {};
        settings_manager_get(&cfg);
        circulation_manager_configure(&cfg);
    }

    esp_err_t bus_err = modbus_rtu_init();
    status->rs485_ok = (bus_err == ESP_OK);
    if (bus_err != ESP_OK) {
        ESP_LOGE(TAG, "RS485/Modbus init failed: %s", esp_err_to_name(bus_err));
    } else {
        th_sensor_init();
        relay_modbus_init();
        actuator_manager_init();
        status->relay_ok = false;
        status->hotair_ok = false;
        status->chamber_ok = false;
    }

    refresh_heap(status);
    ESP_LOGI(TAG, "Bring-up done rtc=%s rs485=%s heap=%u",
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

    relay_state_t relays {};
    status->relay_ok = (actuator_manager_refresh(&relays) == ESP_OK);
    const uint8_t relay_addr = actuator_manager_relay_address();

    sensor_manager_poll(relay_addr);
    sensor_data_t sensors {};
    sensor_manager_get(&sensors);

    status->hotair_ok = sensors.hot_air_temp_valid;
    status->hotair_temp_c = sensors.hot_air_temperature;
    status->hotair_rh = sensors.hot_air_humidity;
    status->chamber_ok = sensors.chamber_temp_valid;
    status->chamber_temp_c = sensors.chamber_temperature;
    status->chamber_rh = sensors.chamber_humidity;
    status->ads1115_ok = sensors.ads1115_ok;
    status->moisture_ok = sensors.moisture_valid;
    status->ec_ok = sensors.ec_valid;
    status->moisture_pct = sensors.moisture;
    status->ec_value = sensors.ec;
    status->moisture_voltage = sensors.moisture_voltage;
    status->ec_voltage = sensors.ec_voltage;

    dryer_settings_t settings {};
    settings_manager_get(&settings);
    safety_manager_evaluate(&sensors, &settings, status->relay_ok, status->rtc_ok);

    if (circulation_manager_is_active()) {
        circulation_manager_tick();
    }

    /* Refresh coils after safety / circulation may have written. */
    if (status->relay_ok) {
        actuator_manager_refresh(&relays);
    } else {
        actuator_manager_get_state(&relays);
    }

    status->relay_elevator = relays.elevator;
    status->relay_heater = relays.heater;
    status->relay_fan = relays.fan;
    status->relay_door_open = relays.door_open;
    status->relay_door_close = relays.door_close;

    safety_status_t safety {};
    safety_manager_get_status(&safety);
    status->heating_permitted = safety.heating_permitted;
    status->safety_heater_forced_off = safety.heater_forced_off;
    status->safety_fan_forced_on = safety.fan_forced_on;
    status->active_fault = (uint8_t)safety.active_fault;

    circulation_status_t circ {};
    circulation_manager_get_status(&circ);
    status->circulation_active = circ.active;
    status->circulation_elevator_on = circ.elevator_on;
    status->circulation_mode = (uint8_t)circ.mode;
}
