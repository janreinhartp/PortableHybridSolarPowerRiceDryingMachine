#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "relay_modbus.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Only software path that may write the Modbus relay board.
 * UI / process / safety must call these APIs — not relay_modbus_* directly.
 */
esp_err_t actuator_manager_init(void);

/** Refresh coil state (includes address discovery). */
esp_err_t actuator_manager_refresh(relay_state_t *out_state);

uint8_t actuator_manager_relay_address(void);

esp_err_t actuator_manager_set_fan(bool on);
esp_err_t actuator_manager_set_elevator(bool on);
esp_err_t actuator_manager_set_heater(bool on);
/** Force heater control line OFF (software authority for safe state). */
esp_err_t actuator_manager_force_heater_off(void);

esp_err_t actuator_manager_open_door(void);
esp_err_t actuator_manager_close_door(void);
esp_err_t actuator_manager_stop_door(void);
esp_err_t actuator_manager_all_off(void);

void actuator_manager_get_state(relay_state_t *out_state);

#ifdef __cplusplus
}
#endif
