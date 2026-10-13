#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CIRCULATION_MODE_CONTINUOUS = 0,
    CIRCULATION_MODE_INTERVAL = 1,
} circulation_mode_t;

typedef struct {
    float target_moisture_pct;
    float drying_temperature_c;
    float temperature_hysteresis_c;
    float cooling_temperature_c;

    circulation_mode_t circulation_mode;
    uint32_t circulation_interval_s;
    uint32_t circulation_run_time_s;

    uint32_t discharge_time_s;
    uint32_t min_drying_time_s;
    uint32_t max_drying_time_s;

    /** Hot-air over-temperature trip (MASTERPLAN: over-temp input is hot-air). */
    float max_hot_air_temperature_c;
} dryer_settings_t;

esp_err_t settings_manager_init(void);

void settings_manager_get_defaults(dryer_settings_t *out);
void settings_manager_get(dryer_settings_t *out);

/** Validate without writing. Returns ESP_ERR_INVALID_ARG when rejected. */
esp_err_t settings_manager_validate(const dryer_settings_t *in);

/** Validate, persist to NVS, and replace the active settings. */
esp_err_t settings_manager_save(const dryer_settings_t *in);

/** Reload from NVS (or defaults if missing / corrupt). */
esp_err_t settings_manager_load(void);

#ifdef __cplusplus
}
#endif
