#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Planned product address (set board DIP/dial to this when T/H sensors use 1 & 2). */
#define RELAY_MODBUS_ADDR       10
#define RELAY_COIL_ELEVATOR     0  /* R1 */
#define RELAY_COIL_HEATER       1  /* R2 */
#define RELAY_COIL_FAN          2  /* R3 */
#define RELAY_COIL_DOOR_OPEN    3  /* R4 */
#define RELAY_COIL_DOOR_CLOSE   4  /* R5 */

typedef struct {
    bool online;
    bool elevator;
    bool heater;
    bool fan;
    bool door_open;
    bool door_close;
} relay_state_t;

esp_err_t relay_modbus_init(void);
/** Discovered Modbus slave ID, or 0 if not found yet. */
uint8_t relay_modbus_address(void);
esp_err_t relay_modbus_refresh(relay_state_t *out_state);
esp_err_t relay_modbus_set_elevator(bool on);
esp_err_t relay_modbus_set_heater(bool on);
esp_err_t relay_modbus_set_fan(bool on);
esp_err_t relay_modbus_open_door(void);
esp_err_t relay_modbus_close_door(void);
esp_err_t relay_modbus_stop_door(void);
esp_err_t relay_modbus_all_off(void);

#ifdef __cplusplus
}
#endif
