#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float moisture;
    float moisture_voltage;

    float hot_air_temperature;
    float hot_air_humidity;
    float chamber_temperature;
    float chamber_humidity;

    bool moisture_valid;
    bool hot_air_temp_valid;
    bool hot_air_humidity_valid;
    bool chamber_temp_valid;
    bool chamber_humidity_valid;
    bool ads1115_ok;

    uint32_t timestamp_ms;
} sensor_data_t;

/** Three-point piecewise-linear moisture calibration (voltage → %). */
typedef struct {
    float v1;
    float y1;
    float v2;
    float y2;
    float v3;
    float y3;
} sensor_moisture_cal_t;

esp_err_t sensor_manager_init(void);

/** Poll T/H + ADS1115 moisture and publish one validated snapshot. */
esp_err_t sensor_manager_poll(uint8_t relay_modbus_addr);

/** Copy the latest snapshot. */
void sensor_manager_get(sensor_data_t *out);

esp_err_t sensor_manager_load_calibration(void);
esp_err_t sensor_manager_save_moisture_cal(const sensor_moisture_cal_t *cal);
void sensor_manager_get_moisture_cal(sensor_moisture_cal_t *out);
void sensor_manager_get_default_moisture_cal(sensor_moisture_cal_t *out);

/** Validate three distinct voltages and y in 0–100 %. */
esp_err_t sensor_manager_validate_moisture_cal(const sensor_moisture_cal_t *cal);

#ifdef __cplusplus
}
#endif
