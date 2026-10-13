#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float moisture;
    float ec;
    float moisture_voltage;
    float ec_voltage;

    float hot_air_temperature;
    float hot_air_humidity;
    float chamber_temperature;
    float chamber_humidity;

    bool moisture_valid;
    bool ec_valid;
    bool hot_air_temp_valid;
    bool hot_air_humidity_valid;
    bool chamber_temp_valid;
    bool chamber_humidity_valid;
    bool ads1115_ok;

    uint32_t timestamp_ms;
} sensor_data_t;

typedef struct {
    float v1;
    float y1;
    float v2;
    float y2;
} sensor_linear_cal_t;

esp_err_t sensor_manager_init(void);

/** Poll T/H + ADS1115 and publish one validated snapshot. */
esp_err_t sensor_manager_poll(uint8_t relay_modbus_addr);

/** Copy the latest snapshot. */
void sensor_manager_get(sensor_data_t *out);

esp_err_t sensor_manager_load_calibration(void);
esp_err_t sensor_manager_save_moisture_cal(const sensor_linear_cal_t *cal);
esp_err_t sensor_manager_save_ec_cal(const sensor_linear_cal_t *cal);
void sensor_manager_get_moisture_cal(sensor_linear_cal_t *out);
void sensor_manager_get_ec_cal(sensor_linear_cal_t *out);

#ifdef __cplusplus
}
#endif
