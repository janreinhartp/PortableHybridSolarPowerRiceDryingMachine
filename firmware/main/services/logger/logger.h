#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOGGER_HISTORY_MAX 8
#define LOGGER_ALARM_MAX 8

typedef struct {
    char timestamp[24];
    char batch_id[16];
    char state[16];
    char fault[40];
    float moisture;
    float ec;
    float hot_air_temperature;
    float hot_air_humidity;
    float chamber_temperature;
    float chamber_humidity;
    uint8_t heater;
    uint8_t fan;
    uint8_t elevator;
    uint8_t door;
} logger_sample_t;

typedef struct {
    char batch_id[16];
    char start_time[24];
    char end_time[24];
    char completion[16];
    char fault[40];
    float initial_moisture;
    float final_moisture;
    float target_moisture;
    float max_temperature;
    float avg_temperature;
    float final_ec;
    uint32_t drying_s;
    uint32_t cooling_s;
    uint32_t discharge_s;
} logger_summary_t;

typedef struct {
    char timestamp[24];
    char state[16];
    char code[40];
    char detail[96];
} logger_alarm_t;

/** Start the SD writer task. sd_mounted is the board mount result. */
esp_err_t logger_init(bool sd_mounted);

/** Non-blocking. Drops the row if the queue is full. */
void logger_submit_sample(const logger_sample_t *sample);
void logger_submit_summary(const logger_summary_t *summary);
void logger_submit_alarm(const logger_alarm_t *alarm);

/** False after a failed batch CSV or summary write, or when no SD was mounted. */
bool logger_is_healthy(void);
uint32_t logger_dropped_count(void);

/** Newest entry first. Safe to call from the HMI task. */
size_t logger_copy_history(logger_summary_t *out, size_t max_entries);
size_t logger_copy_alarms(logger_alarm_t *out, size_t max_entries);

#ifdef __cplusplus
}
#endif
