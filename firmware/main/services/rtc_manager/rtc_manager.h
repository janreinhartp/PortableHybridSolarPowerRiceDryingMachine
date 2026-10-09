#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_datetime_t;

esp_err_t rtc_manager_init(void);
bool rtc_manager_is_ok(void);
esp_err_t rtc_manager_get(rtc_datetime_t *out_dt);
esp_err_t rtc_manager_set(const rtc_datetime_t *dt);
esp_err_t rtc_manager_adjust_minutes(int delta_minutes);
esp_err_t rtc_manager_set_build_time(void);

#ifdef __cplusplus
}
#endif
