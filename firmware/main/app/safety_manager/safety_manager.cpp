#include "safety_manager.h"

#include "actuator_manager.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "safety_mgr";
static safety_status_t s_status {};
static bool s_overtemp_latched = false;

const char *safety_manager_fault_name(fault_code_t code)
{
    switch (code) {
    case FAULT_NONE:
        return "NONE";
    case FAULT_F001_OVER_TEMPERATURE:
        return "F001 OVER_TEMPERATURE";
    case FAULT_F002_EMERGENCY_STOP:
        return "F002 EMERGENCY_STOP";
    case FAULT_F003_MOISTURE_SENSOR:
        return "F003 MOISTURE_SENSOR";
    case FAULT_F004_HOT_AIR_SENSOR:
        return "F004 HOT_AIR_SENSOR";
    case FAULT_F005_CHAMBER_SENSOR:
        return "F005 CHAMBER_SENSOR";
    case FAULT_F006_MODBUS_COMMUNICATION:
        return "F006 MODBUS_COMMUNICATION";
    case FAULT_F007_ELEVATOR_TIMEOUT:
        return "F007 ELEVATOR_TIMEOUT";
    case FAULT_F008_DOOR_TIMEOUT:
        return "F008 DOOR_TIMEOUT";
    case FAULT_F009_SD_CARD_ERROR:
        return "F009 SD_CARD_ERROR";
    case FAULT_F010_MAX_DRYING_TIME:
        return "F010 MAX_DRYING_TIME";
    case FAULT_F011_RTC_ERROR:
        return "F011 RTC_ERROR";
    case FAULT_F012_INVALID_CONFIGURATION:
        return "F012 INVALID_CONFIGURATION";
    default:
        return "UNKNOWN";
    }
}

static void set_fault(fault_code_t code)
{
    if (s_status.active_fault != code) {
        ESP_LOGW(TAG, "Fault: %s", safety_manager_fault_name(code));
        s_status.active_fault = code;
        s_status.fault_timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    }
}

esp_err_t safety_manager_init(void)
{
    s_status = {};
    s_status.heating_permitted = false;
    s_overtemp_latched = false;
    ESP_LOGI(TAG, "Safety Manager ready (heater-off authority)");
    return ESP_OK;
}

void safety_manager_evaluate(const sensor_data_t *sensors,
                             const dryer_settings_t *settings,
                             bool relay_ok,
                             bool rtc_ok)
{
    if (sensors == nullptr || settings == nullptr) {
        return;
    }

    (void)rtc_ok;

    s_status.over_temperature = false;
    s_status.fan_forced_on = false;
    s_status.heater_forced_off = false;

    /* Highest priority: hot-air over-temperature → heater OFF, fan ON (latched). */
    if (sensors->hot_air_temp_valid &&
        sensors->hot_air_temperature >= settings->max_hot_air_temperature_c) {
        s_status.over_temperature = true;
        s_overtemp_latched = true;
        set_fault(FAULT_F001_OVER_TEMPERATURE);
        actuator_manager_force_heater_off();
        actuator_manager_set_fan(true);
        s_status.heater_forced_off = true;
        s_status.fan_forced_on = true;
        s_status.heating_permitted = false;
        return;
    }

    if (s_overtemp_latched) {
        /* Stay in F001 until operator acknowledge; keep heater forced off. */
        set_fault(FAULT_F001_OVER_TEMPERATURE);
        actuator_manager_force_heater_off();
        actuator_manager_set_fan(true);
        s_status.heater_forced_off = true;
        s_status.fan_forced_on = true;
        s_status.heating_permitted = false;
        return;
    }

    /*
     * Heating permission (MASTERPLAN): both chamber and hot-air must be valid.
     * Moisture / RTC faults are reported but do not alone block heating.
     */
    const bool temps_ok =
        sensors->hot_air_temp_valid && sensors->chamber_temp_valid;

    if (!relay_ok) {
        set_fault(FAULT_F006_MODBUS_COMMUNICATION);
    } else if (!sensors->hot_air_temp_valid) {
        set_fault(FAULT_F004_HOT_AIR_SENSOR);
    } else if (!sensors->chamber_temp_valid) {
        set_fault(FAULT_F005_CHAMBER_SENSOR);
    } else if (!sensors->moisture_valid) {
        set_fault(FAULT_F003_MOISTURE_SENSOR);
    } else if (!rtc_ok) {
        set_fault(FAULT_F011_RTC_ERROR);
    } else {
        s_status.active_fault = FAULT_NONE;
        s_status.fault_timestamp_ms = 0;
    }

    s_status.heating_permitted = temps_ok && relay_ok;

    if (!s_status.heating_permitted) {
        actuator_manager_force_heater_off();
        s_status.heater_forced_off = true;
    }
}

void safety_manager_get_status(safety_status_t *out)
{
    if (out) {
        *out = s_status;
    }
}

bool safety_manager_heating_permitted(void)
{
    return s_status.heating_permitted;
}

fault_code_t safety_manager_active_fault(void)
{
    return s_status.active_fault;
}

esp_err_t safety_manager_acknowledge(void)
{
    if (s_status.over_temperature) {
        ESP_LOGW(TAG, "Cannot ACK while over-temperature is active");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "Fault acknowledged (was %s)",
             safety_manager_fault_name(s_status.active_fault));
    s_overtemp_latched = false;
    s_status.active_fault = FAULT_NONE;
    s_status.fault_timestamp_ms = 0;
    return ESP_OK;
}

esp_err_t safety_manager_request_heater(bool on)
{
    if (on && !s_status.heating_permitted) {
        ESP_LOGW(TAG, "Heater ON refused — heating not permitted (%s)",
                 safety_manager_fault_name(s_status.active_fault));
        actuator_manager_force_heater_off();
        return ESP_ERR_INVALID_STATE;
    }
    if (!on) {
        return actuator_manager_force_heater_off();
    }
    return actuator_manager_set_heater(true);
}
