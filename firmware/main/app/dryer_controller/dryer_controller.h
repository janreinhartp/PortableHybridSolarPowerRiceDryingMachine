#pragma once

#include "esp_err.h"
#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Optional bring-up progress callback (id string + ok).
 * Called after each soft-fail-capable step; board hard-fail returns before later steps.
 */
typedef void (*dryer_init_progress_fn)(const char *step_id, bool ok, void *user);

/**
 * Board bring-up plus Sensor/Actuator Manager and fieldbus coordination.
 * @param progress optional; may be NULL
 */
esp_err_t dryer_controller_init(system_status_t *status, dryer_init_progress_fn progress,
                                void *progress_user);
void dryer_controller_refresh_heap(system_status_t *status);
void dryer_controller_poll_fieldbus(system_status_t *status);

#ifdef __cplusplus
}
#endif
