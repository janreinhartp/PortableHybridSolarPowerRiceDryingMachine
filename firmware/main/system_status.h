#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool display_ok;
    bool touch_ok;
    bool lvgl_ok;
    bool sd_ok;
    bool rtc_ok;
    bool rs485_ok;
    bool hotair_ok;
    bool chamber_ok;
    bool relay_ok;
    size_t sd_total_mb;
    size_t sd_free_mb;
    size_t free_heap;
    size_t free_internal_heap;
    uint16_t rtc_year;
    uint8_t rtc_month;
    uint8_t rtc_day;
    uint8_t rtc_hour;
    uint8_t rtc_minute;
    uint8_t rtc_second;
    float hotair_temp_c;
    float hotair_rh;
    float chamber_temp_c;
    float chamber_rh;
    bool relay_elevator;
    bool relay_heater;
    bool relay_fan;
    bool relay_door_open;
    bool relay_door_close;
} system_status_t;

#ifdef __cplusplus
}
#endif
