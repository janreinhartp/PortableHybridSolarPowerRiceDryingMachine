#include "process_controller.h"

#include <cstring>

#include "actuator_manager.h"
#include "batch_manager.h"
#include "circulation_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "safety_manager.h"

static const char *TAG = "process_ctrl";

static dryer_state_t s_state = DRYER_STATE_IDLE;

static int64_t s_state_enter_us = 0;
static int64_t s_drying_start_us = 0;
static int64_t s_cooling_start_us = 0;
static int64_t s_discharge_start_us = 0;

static uint32_t s_drying_elapsed_s = 0;
static uint32_t s_cooling_elapsed_s = 0;
static uint32_t s_discharge_elapsed_s = 0;

static bool s_heater_on = false;
static int64_t s_last_recovery_us = 0;
static const int64_t k_recovery_interval_us = 30LL * 1000000LL;

/* DISCHARGING sub-steps */
typedef enum {
    DISCHARGE_OPEN_DOOR = 0,
    DISCHARGE_SETTLE,
    DISCHARGE_ELEVATOR,
    DISCHARGE_CLOSE_DOOR,
} discharge_step_t;

static discharge_step_t s_discharge_step = DISCHARGE_OPEN_DOOR;
static int64_t s_discharge_step_us = 0;
static const uint32_t k_door_settle_s = 5;

static uint32_t elapsed_s(int64_t start_us)
{
    if (start_us <= 0) {
        return 0;
    }
    return (uint32_t)((esp_timer_get_time() - start_us) / 1000000LL);
}

const char *process_controller_state_name(dryer_state_t state)
{
    switch (state) {
    case DRYER_STATE_IDLE:
        return "IDLE";
    case DRYER_STATE_PRECHECK:
        return "PRECHECK";
    case DRYER_STATE_DRYING:
        return "DRYING";
    case DRYER_STATE_COOLING:
        return "COOLING";
    case DRYER_STATE_DISCHARGING:
        return "DISCHARGING";
    case DRYER_STATE_COMPLETE:
        return "COMPLETE";
    case DRYER_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

static batch_process_state_t to_batch_state(dryer_state_t state)
{
    return (batch_process_state_t)state;
}

static void enter_safe_idle_outputs(void)
{
    if (circulation_manager_is_active()) {
        circulation_manager_stop();
    }
    safety_manager_request_heater(false);
    s_heater_on = false;
    actuator_manager_set_fan(false);
    actuator_manager_set_elevator(false);
    actuator_manager_close_door();
}

static void save_recovery_throttled(const sensor_data_t *sensors, bool force)
{
    const int64_t now = esp_timer_get_time();
    if (!force && (now - s_last_recovery_us) < k_recovery_interval_us) {
        return;
    }
    s_last_recovery_us = now;
    batch_manager_save_recovery(to_batch_state(s_state), sensors->moisture,
                                sensors->chamber_temperature);
}

static void transition_to(dryer_state_t next)
{
    if (s_state == next) {
        return;
    }
    ESP_LOGI(TAG, "State %s -> %s", process_controller_state_name(s_state),
             process_controller_state_name(next));
    s_state = next;
    s_state_enter_us = esp_timer_get_time();
    batch_manager_set_state(to_batch_state(next));

    if (next == DRYER_STATE_IDLE || next == DRYER_STATE_COMPLETE) {
        enter_safe_idle_outputs();
    }
}

static void enter_fault(fault_code_t code)
{
    if (code != FAULT_NONE) {
        safety_manager_raise_fault(code);
    }
    if (circulation_manager_is_active()) {
        circulation_manager_stop();
    }
    safety_manager_request_heater(false);
    s_heater_on = false;
    transition_to(DRYER_STATE_FAULT);
}

static void apply_heater_hysteresis(const sensor_data_t *sensors,
                                    const dryer_settings_t *settings)
{
    if (!sensors->chamber_temp_valid) {
        safety_manager_request_heater(false);
        s_heater_on = false;
        return;
    }

    const float t = sensors->chamber_temperature;
    const float target = settings->drying_temperature_c;
    const float h = settings->temperature_hysteresis_c;

    if (t <= target - h) {
        if (safety_manager_request_heater(true) == ESP_OK) {
            s_heater_on = true;
        } else {
            s_heater_on = false;
        }
    } else if (t >= target + h) {
        safety_manager_request_heater(false);
        s_heater_on = false;
    } else if (s_heater_on) {
        /* Maintain previous ON — re-request in case safety blocked. */
        if (safety_manager_request_heater(true) != ESP_OK) {
            s_heater_on = false;
        }
    }
}

static bool precheck_ok(const sensor_data_t *sensors,
                        const dryer_settings_t *settings,
                        const system_status_t *status,
                        fault_code_t *out_fault)
{
    *out_fault = FAULT_NONE;

    if (settings_manager_validate(settings) != ESP_OK) {
        *out_fault = FAULT_F012_INVALID_CONFIGURATION;
        return false;
    }
    if (safety_manager_active_fault() != FAULT_NONE) {
        *out_fault = safety_manager_active_fault();
        return false;
    }
    if (!status->relay_ok) {
        *out_fault = FAULT_F006_MODBUS_COMMUNICATION;
        return false;
    }
    if (!status->rtc_ok) {
        *out_fault = FAULT_F011_RTC_ERROR;
        return false;
    }
    if (!status->sd_ok) {
        *out_fault = FAULT_F009_SD_CARD_ERROR;
        return false;
    }
    if (!sensors->hot_air_temp_valid) {
        *out_fault = FAULT_F004_HOT_AIR_SENSOR;
        return false;
    }
    if (!sensors->chamber_temp_valid) {
        *out_fault = FAULT_F005_CHAMBER_SENSOR;
        return false;
    }
    if (!sensors->moisture_valid) {
        *out_fault = FAULT_F003_MOISTURE_SENSOR;
        return false;
    }
    return true;
}

static void mirror_to_status(system_status_t *status)
{
    if (status == nullptr) {
        return;
    }
    status->dryer_state = (uint8_t)s_state;
    status->drying_elapsed_s = s_drying_elapsed_s;
    status->cooling_elapsed_s = s_cooling_elapsed_s;
    status->discharge_elapsed_s = s_discharge_elapsed_s;
    status->batch_interrupted = batch_manager_interrupted_pending();

    batch_status_t batch {};
    batch_manager_get_status(&batch);
    strncpy(status->batch_id, batch.batch_id, sizeof(status->batch_id) - 1);
    status->batch_id[sizeof(status->batch_id) - 1] = '\0';
    if (status->batch_interrupted && status->batch_id[0] == '\0') {
        strncpy(status->batch_id, batch.interrupted_batch_id,
                sizeof(status->batch_id) - 1);
    }

    dryer_settings_t cfg {};
    settings_manager_get(&cfg);
    status->target_moisture_pct = cfg.target_moisture_pct;
    status->drying_temperature_c = cfg.drying_temperature_c;
    status->cooling_temperature_c = cfg.cooling_temperature_c;
}

esp_err_t process_controller_init(void)
{
    s_state = DRYER_STATE_IDLE;
    s_state_enter_us = esp_timer_get_time();
    s_drying_start_us = 0;
    s_cooling_start_us = 0;
    s_discharge_start_us = 0;
    s_drying_elapsed_s = 0;
    s_cooling_elapsed_s = 0;
    s_discharge_elapsed_s = 0;
    s_heater_on = false;
    s_discharge_step = DISCHARGE_OPEN_DOOR;

    enter_safe_idle_outputs();

    if (batch_manager_interrupted_pending()) {
        ESP_LOGW(TAG, "Boot: interrupted batch pending — IDLE until ACK");
    }

    ESP_LOGI(TAG, "Process Controller ready (state=IDLE)");
    return ESP_OK;
}

esp_err_t process_controller_start(void)
{
    if (batch_manager_interrupted_pending()) {
        ESP_LOGW(TAG, "START refused — acknowledge interrupted batch first");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_state != DRYER_STATE_IDLE && s_state != DRYER_STATE_COMPLETE) {
        ESP_LOGW(TAG, "START refused — state=%s",
                 process_controller_state_name(s_state));
        return ESP_ERR_INVALID_STATE;
    }
    if (safety_manager_active_fault() != FAULT_NONE) {
        ESP_LOGW(TAG, "START refused — active fault");
        return ESP_ERR_INVALID_STATE;
    }

    s_drying_elapsed_s = 0;
    s_cooling_elapsed_s = 0;
    s_discharge_elapsed_s = 0;
    s_heater_on = false;
    transition_to(DRYER_STATE_PRECHECK);
    return ESP_OK;
}

esp_err_t process_controller_acknowledge_interrupted(void)
{
    return batch_manager_acknowledge_interrupted();
}

esp_err_t process_controller_abort(void)
{
    batch_manager_abort();
    enter_safe_idle_outputs();
    s_drying_elapsed_s = 0;
    s_cooling_elapsed_s = 0;
    s_discharge_elapsed_s = 0;
    transition_to(DRYER_STATE_IDLE);
    return ESP_OK;
}

bool process_controller_manual_control_allowed(void)
{
    return s_state == DRYER_STATE_IDLE || s_state == DRYER_STATE_COMPLETE;
}

dryer_state_t process_controller_get_state(void)
{
    return s_state;
}

void process_controller_tick(const sensor_data_t *sensors,
                             const dryer_settings_t *settings,
                             system_status_t *status)
{
    if (sensors == nullptr || settings == nullptr || status == nullptr) {
        return;
    }

    /* Active-run fault takeover (Safety has priority). */
    if (s_state != DRYER_STATE_IDLE && s_state != DRYER_STATE_COMPLETE &&
        s_state != DRYER_STATE_FAULT) {
        fault_code_t fault = safety_manager_active_fault();
        if (fault != FAULT_NONE) {
            enter_fault(FAULT_NONE); /* already raised by Safety */
            mirror_to_status(status);
            return;
        }
    }

    switch (s_state) {
    case DRYER_STATE_IDLE:
        /* Safe outputs applied on entry; do not re-pulse door every tick. */
        break;

    case DRYER_STATE_PRECHECK: {
        fault_code_t fault = FAULT_NONE;
        if (!precheck_ok(sensors, settings, status, &fault)) {
            enter_fault(fault);
            break;
        }

        if (batch_manager_create(sensors) != ESP_OK) {
            enter_fault(FAULT_F012_INVALID_CONFIGURATION);
            break;
        }

        circulation_manager_configure(settings);
        actuator_manager_close_door();
        actuator_manager_set_fan(true);
        safety_manager_request_heater(false);
        s_heater_on = false;

        if (circulation_manager_start() != ESP_OK) {
            enter_fault(FAULT_F012_INVALID_CONFIGURATION);
            break;
        }

        s_drying_start_us = esp_timer_get_time();
        s_drying_elapsed_s = 0;
        transition_to(DRYER_STATE_DRYING);
        save_recovery_throttled(sensors, true);
        break;
    }

    case DRYER_STATE_DRYING: {
        s_drying_elapsed_s = elapsed_s(s_drying_start_us);
        batch_manager_update_elapsed(s_drying_elapsed_s, s_cooling_elapsed_s,
                                     s_discharge_elapsed_s);
        batch_manager_note_chamber_temp(sensors->chamber_temperature);

        actuator_manager_set_fan(true);
        actuator_manager_close_door();
        apply_heater_hysteresis(sensors, settings);
        save_recovery_throttled(sensors, false);

        if (s_drying_elapsed_s >= settings->max_drying_time_s) {
            enter_fault(FAULT_F010_MAX_DRYING_TIME);
            break;
        }

        const bool moisture_done =
            sensors->moisture_valid &&
            sensors->moisture <= settings->target_moisture_pct;
        const bool min_time_done =
            s_drying_elapsed_s >= settings->min_drying_time_s;

        if (moisture_done && min_time_done) {
            safety_manager_request_heater(false);
            s_heater_on = false;
            if (circulation_manager_is_active()) {
                circulation_manager_stop();
            }
            actuator_manager_set_elevator(false);
            actuator_manager_set_fan(true);
            actuator_manager_close_door();

            s_cooling_start_us = esp_timer_get_time();
            s_cooling_elapsed_s = 0;
            transition_to(DRYER_STATE_COOLING);
            save_recovery_throttled(sensors, true);
        }
        break;
    }

    case DRYER_STATE_COOLING: {
        s_cooling_elapsed_s = elapsed_s(s_cooling_start_us);
        batch_manager_update_elapsed(s_drying_elapsed_s, s_cooling_elapsed_s,
                                     s_discharge_elapsed_s);

        safety_manager_request_heater(false);
        s_heater_on = false;
        if (circulation_manager_is_active()) {
            circulation_manager_stop();
        }
        actuator_manager_set_elevator(false);
        actuator_manager_set_fan(true);
        actuator_manager_close_door();
        save_recovery_throttled(sensors, false);

        if (sensors->chamber_temp_valid &&
            sensors->chamber_temperature <= settings->cooling_temperature_c) {
            actuator_manager_set_fan(false);
            s_discharge_start_us = esp_timer_get_time();
            s_discharge_elapsed_s = 0;
            s_discharge_step = DISCHARGE_OPEN_DOOR;
            s_discharge_step_us = esp_timer_get_time();
            transition_to(DRYER_STATE_DISCHARGING);
            save_recovery_throttled(sensors, true);
        }
        break;
    }

    case DRYER_STATE_DISCHARGING: {
        s_discharge_elapsed_s = elapsed_s(s_discharge_start_us);
        batch_manager_update_elapsed(s_drying_elapsed_s, s_cooling_elapsed_s,
                                     s_discharge_elapsed_s);

        safety_manager_request_heater(false);
        s_heater_on = false;
        actuator_manager_set_fan(false);

        const uint32_t step_s = elapsed_s(s_discharge_step_us);

        switch (s_discharge_step) {
        case DISCHARGE_OPEN_DOOR:
            actuator_manager_open_door();
            actuator_manager_set_elevator(false);
            s_discharge_step = DISCHARGE_SETTLE;
            s_discharge_step_us = esp_timer_get_time();
            break;

        case DISCHARGE_SETTLE:
            actuator_manager_open_door();
            if (step_s >= k_door_settle_s) {
                actuator_manager_stop_door();
                actuator_manager_set_elevator(true);
                s_discharge_step = DISCHARGE_ELEVATOR;
                s_discharge_step_us = esp_timer_get_time();
            }
            break;

        case DISCHARGE_ELEVATOR:
            actuator_manager_set_elevator(true);
            if (step_s >= settings->discharge_time_s) {
                actuator_manager_set_elevator(false);
                actuator_manager_close_door();
                s_discharge_step = DISCHARGE_CLOSE_DOOR;
                s_discharge_step_us = esp_timer_get_time();
            }
            break;

        case DISCHARGE_CLOSE_DOOR:
            actuator_manager_set_elevator(false);
            actuator_manager_close_door();
            if (step_s >= k_door_settle_s) {
                actuator_manager_stop_door();
                enter_safe_idle_outputs();
                batch_manager_update_elapsed(s_drying_elapsed_s, s_cooling_elapsed_s,
                                             s_discharge_elapsed_s);
                batch_manager_complete(sensors);
                transition_to(DRYER_STATE_COMPLETE);
            }
            break;
        }

        if (s_state == DRYER_STATE_DISCHARGING) {
            save_recovery_throttled(sensors, false);
        }
        break;
    }

    case DRYER_STATE_COMPLETE:
        break;

    case DRYER_STATE_FAULT:
        if (circulation_manager_is_active()) {
            circulation_manager_stop();
        }
        safety_manager_request_heater(false);
        s_heater_on = false;
        /* Leave fan to Safety (may force ON for over-temp). */
        if (safety_manager_active_fault() == FAULT_NONE) {
            batch_manager_abort();
            transition_to(DRYER_STATE_IDLE);
        }
        break;
    }

    mirror_to_status(status);
}
