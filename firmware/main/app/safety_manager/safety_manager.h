#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "sensor_manager.h"
#include "settings_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FAULT_NONE = 0,
    FAULT_F001_OVER_TEMPERATURE = 1,
    FAULT_F002_EMERGENCY_STOP = 2, /* electrical only — not ESP32-detectable */
    FAULT_F003_MOISTURE_SENSOR = 3,
    FAULT_F004_HOT_AIR_SENSOR = 4,
    FAULT_F005_CHAMBER_SENSOR = 5,
    FAULT_F006_MODBUS_COMMUNICATION = 6,
    FAULT_F007_ELEVATOR_TIMEOUT = 7,
    FAULT_F008_DOOR_TIMEOUT = 8,
    FAULT_F009_SD_CARD_ERROR = 9,
    FAULT_F010_MAX_DRYING_TIME = 10,
    FAULT_F011_RTC_ERROR = 11,
    FAULT_F012_INVALID_CONFIGURATION = 12,
} fault_code_t;

typedef struct {
    fault_code_t active_fault;
    bool heating_permitted;
    bool over_temperature;
    bool fan_forced_on;
    bool heater_forced_off;
    uint32_t fault_timestamp_ms;
} safety_status_t;

esp_err_t safety_manager_init(void);

/**
 * Evaluate sensors/settings and apply heater-off / fan-on authority.
 * Call after each sensor poll. relay_ok mirrors Modbus relay reachability.
 */
void safety_manager_evaluate(const sensor_data_t *sensors,
                             const dryer_settings_t *settings,
                             bool relay_ok,
                             bool rtc_ok);

void safety_manager_get_status(safety_status_t *out);
bool safety_manager_heating_permitted(void);
fault_code_t safety_manager_active_fault(void);

/** Clear latching faults after the condition is gone (operator acknowledge). */
esp_err_t safety_manager_acknowledge(void);

/**
 * Raise a process-originated fault (e.g. F010 max drying time).
 * Forces heater OFF. Does not override an active over-temperature latch.
 */
esp_err_t safety_manager_raise_fault(fault_code_t code);

/**
 * Request heater ON/OFF. ON is refused when heating is not permitted.
 * Safety still re-evaluates and can force OFF on the next tick.
 */
esp_err_t safety_manager_request_heater(bool on);

const char *safety_manager_fault_name(fault_code_t code);

#ifdef __cplusplus
}
#endif
