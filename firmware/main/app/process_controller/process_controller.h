#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "sensor_manager.h"
#include "settings_manager.h"
#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRYER_STATE_IDLE = 0,
    DRYER_STATE_PRECHECK = 1,
    DRYER_STATE_DRYING = 2,
    DRYER_STATE_COOLING = 3,
    DRYER_STATE_DISCHARGING = 4,
    DRYER_STATE_COMPLETE = 5,
    DRYER_STATE_FAULT = 6,
} dryer_state_t;

esp_err_t process_controller_init(void);

/** Start a batch from IDLE or COMPLETE. Refuses if interrupted pending. */
esp_err_t process_controller_start(void);

/** Discard interrupted-batch NVS flag so a new START is allowed. */
esp_err_t process_controller_acknowledge_interrupted(void);

/**
 * Operator-confirmed resume of interrupted batch into saved phase
 * (DRYING / COOLING / DISCHARGING) after sensor/safety gate.
 */
esp_err_t process_controller_resume_interrupted(void);

/** Abort active batch → IDLE (safe outputs). */
esp_err_t process_controller_abort(void);

/**
 * Advance the state machine. Call once per control tick after safety_manager_evaluate.
 * board_ok fields come from system_status (rtc/sd/relay).
 */
void process_controller_tick(const sensor_data_t *sensors,
                             const dryer_settings_t *settings,
                             system_status_t *status);

dryer_state_t process_controller_get_state(void);
const char *process_controller_state_name(dryer_state_t state);

/** True when manual actuator buttons may be used (IDLE / COMPLETE). */
bool process_controller_manual_control_allowed(void);

/** True in IDLE, COMPLETE, and FAULT — not while a batch is in progress. */
bool process_controller_settings_editable(void);

/**
 * Copy process, batch, and settings fields into the HMI snapshot.
 * Does not run the state machine.
 */
void process_controller_fill_status(system_status_t *status);

/** Persist settings. Refused while PRECHECK / DRYING / COOLING / DISCHARGING. */
esp_err_t process_controller_save_settings(const dryer_settings_t *settings);

/** Operator fault acknowledge. Does not clear an active over-temperature trip. */
esp_err_t process_controller_acknowledge_fault(void);

esp_err_t process_controller_manual_toggle_fan(void);
esp_err_t process_controller_manual_toggle_elevator(void);
esp_err_t process_controller_manual_toggle_heater(void);
esp_err_t process_controller_manual_all_off(void);
esp_err_t process_controller_manual_toggle_circulation(void);
esp_err_t process_controller_manual_door_open(void);
esp_err_t process_controller_manual_door_close(void);
esp_err_t process_controller_manual_door_stop(void);

/**
 * Commissioning: scan the bus and assign one XY-MD03.
 * new_addr is the product id (hot-air 1, chamber 2). Refused during a batch.
 * msg receives a short result string.
 */
esp_err_t process_controller_assign_sensor(uint8_t new_addr, const char *role,
                                           char *msg, size_t msg_len);

#ifdef __cplusplus
}
#endif
