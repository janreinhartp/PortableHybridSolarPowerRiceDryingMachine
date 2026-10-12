#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Eletechsup N4D8B08 (FC03/FC06 holding registers).
 * DIP A0–A5 = raw binary address (A0=1, A1=2, A2=4, A3=8, A4=16, A5=32).
 * Factory: A0 ON → ID 1. Product: A1+A3 ON → ID 10 (sensors use 1 and 2).
 */
#define RELAY_MODBUS_ADDR       1

/* Channel registers are 1-based (0x0001–0x0008). */
#define RELAY_CH_ELEVATOR       1  /* R1 */
#define RELAY_CH_HEATER         2  /* R2 */
#define RELAY_CH_FAN            3  /* R3 */
#define RELAY_CH_DOOR_OPEN      4  /* R4 */
#define RELAY_CH_DOOR_CLOSE     5  /* R5 */

/* Legacy aliases used by older call sites / comments. */
#define RELAY_COIL_ELEVATOR     RELAY_CH_ELEVATOR
#define RELAY_COIL_HEATER       RELAY_CH_HEATER
#define RELAY_COIL_FAN          RELAY_CH_FAN
#define RELAY_COIL_DOOR_OPEN    RELAY_CH_DOOR_OPEN
#define RELAY_COIL_DOOR_CLOSE   RELAY_CH_DOOR_CLOSE

typedef struct {
    bool online;
    bool elevator;
    bool heater;
    bool fan;
    bool door_open;
    bool door_close;
} relay_state_t;

esp_err_t relay_modbus_init(void);
/** Active Modbus slave ID, or 0 if not found yet. */
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
