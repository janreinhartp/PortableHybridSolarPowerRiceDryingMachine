#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "settings_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool active;
    bool elevator_on;
    circulation_mode_t mode;
    uint32_t interval_s;
    uint32_t run_time_s;
    uint32_t phase_elapsed_ms;
} circulation_status_t;

esp_err_t circulation_manager_init(void);

/** Apply mode/interval/run-time from settings (safe while stopped or running). */
void circulation_manager_configure(const dryer_settings_t *settings);

esp_err_t circulation_manager_start(void);
esp_err_t circulation_manager_stop(void);

/**
 * Non-blocking tick. Call from the control / runtime loop (~1 Hz is fine).
 * Drives the elevator via Actuator Manager when active.
 */
void circulation_manager_tick(void);

bool circulation_manager_is_active(void);
void circulation_manager_get_status(circulation_status_t *out);

#ifdef __cplusplus
}
#endif
