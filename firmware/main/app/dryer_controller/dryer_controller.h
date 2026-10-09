#pragma once

#include "esp_err.h"
#include "system_status.h"

/**
 * Board bring-up plus fieldbus (RS485/Modbus) coordination.
 */
esp_err_t dryer_controller_init(system_status_t *status);
void dryer_controller_refresh_heap(system_status_t *status);
void dryer_controller_poll_fieldbus(system_status_t *status);
