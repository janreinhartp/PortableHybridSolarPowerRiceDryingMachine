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
} system_status_t;

#ifdef __cplusplus
}
#endif
