#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "sensor_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Process states stored in NVS recovery (mirrors process_controller dryer_state_t). */
typedef enum {
    BATCH_STATE_IDLE = 0,
    BATCH_STATE_PRECHECK = 1,
    BATCH_STATE_DRYING = 2,
    BATCH_STATE_COOLING = 3,
    BATCH_STATE_DISCHARGING = 4,
    BATCH_STATE_COMPLETE = 5,
    BATCH_STATE_FAULT = 6,
} batch_process_state_t;

typedef struct {
    char batch_id[16];
    bool active;
    bool completed;
    bool interrupted_pending;

    batch_process_state_t last_state;
    uint32_t start_time_s; /* esp_timer seconds at create */
    uint32_t drying_elapsed_s;
    uint32_t cooling_elapsed_s;
    uint32_t discharge_elapsed_s;

    float initial_moisture;
    float initial_chamber_temp_c;
    float final_moisture;
    float final_chamber_temp_c;
    float max_chamber_temp_c;
    float avg_chamber_temp_c;

    /* Recovery snapshot (valid when interrupted_pending). */
    char interrupted_batch_id[16];
    batch_process_state_t interrupted_state;
    float interrupted_moisture;
    float interrupted_chamber_temp_c;
} batch_status_t;

esp_err_t batch_manager_init(void);

/** Load NVS interrupted-batch flag into RAM. Call once at boot. */
esp_err_t batch_manager_load_recovery(void);

bool batch_manager_interrupted_pending(void);
void batch_manager_get_status(batch_status_t *out);

/**
 * Allocate next BATCH-NNNN, record start sensors, clear completed flag.
 * Refuses if interrupted_pending (operator must ACK first).
 */
esp_err_t batch_manager_create(const sensor_data_t *sensors);

void batch_manager_set_state(batch_process_state_t state);
void batch_manager_update_elapsed(uint32_t drying_s, uint32_t cooling_s, uint32_t discharge_s);
void batch_manager_note_chamber_temp(float chamber_temp_c);

/** Persist interrupted recovery record (call on major transitions / each tick in active states). */
esp_err_t batch_manager_save_recovery(batch_process_state_t state,
                                      float moisture,
                                      float chamber_temp_c);

/** Finalize summary, clear NVS interrupted flag. */
esp_err_t batch_manager_complete(const sensor_data_t *sensors);

/** Operator discard of interrupted batch — clears NVS flag. */
esp_err_t batch_manager_acknowledge_interrupted(void);

/** Abort active batch without COMPLETE summary; clears recovery. */
esp_err_t batch_manager_abort(void);

#ifdef __cplusplus
}
#endif
