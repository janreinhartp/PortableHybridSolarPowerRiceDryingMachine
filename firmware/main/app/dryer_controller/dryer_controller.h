#pragma once

#include "esp_err.h"
#include "system_status.h"

/**
 * Phase 1 dryer controller: board bring-up and status reporting only.
 * Later phases add process/safety/batch coordination here.
 */
esp_err_t dryer_controller_init(system_status_t *status);
void dryer_controller_refresh_heap(system_status_t *status);
