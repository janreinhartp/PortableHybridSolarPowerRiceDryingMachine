#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TH_SENSOR_HOTAIR_ADDR   1
#define TH_SENSOR_CHAMBER_ADDR  2

typedef struct {
    bool online;
    float temperature_c;
    float humidity_rh;
} th_reading_t;

esp_err_t th_sensor_init(void);
esp_err_t th_sensor_read(uint8_t slave_addr, th_reading_t *out);

#ifdef __cplusplus
}
#endif
