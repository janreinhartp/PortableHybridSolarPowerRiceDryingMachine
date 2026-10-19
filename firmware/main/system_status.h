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
    bool ads1115_ok;
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
    float moisture_pct;
    float moisture_voltage;
    bool moisture_ok;
    bool relay_elevator;
    bool relay_heater;
    bool relay_fan;
    bool relay_door_open;
    bool relay_door_close;

    /* Phase 5 — safety / settings / circulation summary for HMI */
    bool heating_permitted;
    bool safety_heater_forced_off;
    bool safety_fan_forced_on;
    uint8_t active_fault; /* fault_code_t */
    bool circulation_active;
    bool circulation_elevator_on;
    uint8_t circulation_mode;

    /* Phase 6 — process / batch summary for HMI */
    uint8_t dryer_state; /* dryer_state_t */
    char batch_id[16];
    bool batch_interrupted;
    uint32_t drying_elapsed_s;
    uint32_t cooling_elapsed_s;
    uint32_t discharge_elapsed_s;
    float target_moisture_pct;
    float drying_temperature_c;
    float cooling_temperature_c;

    /* Phase 7 — settings snapshot, batch result, logger, and alarm context */
    float pid_kp;
    float pid_ki;
    float pid_kd;
    uint32_t pid_window_s;
    uint32_t circulation_interval_s;
    uint32_t circulation_run_time_s;
    uint32_t discharge_time_s;
    uint32_t min_drying_time_s;
    uint32_t max_drying_time_s;
    float max_hot_air_temperature_c;
    uint8_t discharge_phase;
    uint32_t discharge_remaining_s;
    bool precheck_ready;
    bool logger_ok;
    uint32_t logger_dropped;
    float initial_moisture_pct;
    float final_moisture_pct;
    float max_chamber_temp_c;
    float avg_chamber_temp_c;
    float heater_duty_pct; /* 0–100 during DRYING */
    char batch_started[24];
    char batch_ended[24];
    char fault_time[24];
    char fault_state[16];

    /* Phase 8 — local Wi-Fi SoftAP dashboard */
    bool wifi_ok;
    char wifi_ssid[32];
} system_status_t;

#ifdef __cplusplus
}
#endif
