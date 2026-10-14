#include "process_controller.h"

#include <cstdio>
#include <cstring>

#include "actuator_manager.h"
#include "batch_manager.h"
#include "circulation_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "logger.h"
#include "rtc_manager.h"
#include "safety_manager.h"
#include "th_sensor.h"

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

static bool s_summary_pending = false;
static bool s_sample_queued_this_tick = false;
static char s_started[24] = {};
static char s_ended[24] = {};

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
static uint8_t s_discharge_phase_pub = 0;
static uint32_t s_discharge_remaining_pub = 0;

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

static bool logging_state(dryer_state_t state)
{
    return state == DRYER_STATE_DRYING || state == DRYER_STATE_COOLING ||
           state == DRYER_STATE_DISCHARGING;
}

static void format_now(const system_status_t *status, char *buf, size_t len)
{
    if (buf == nullptr || len == 0) {
        return;
    }
    if (status != nullptr && status->rtc_ok) {
        snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u",
                 (unsigned)(status->rtc_year % 10000u),
                 (unsigned)(status->rtc_month % 100u),
                 (unsigned)(status->rtc_day % 100u),
                 (unsigned)(status->rtc_hour % 100u),
                 (unsigned)(status->rtc_minute % 100u),
                 (unsigned)(status->rtc_second % 100u));
        return;
    }
    rtc_datetime_t dt {};
    if (rtc_manager_get(&dt) == ESP_OK) {
        snprintf(buf, len, "%04u-%02u-%02u %02u:%02u:%02u",
                 (unsigned)(dt.year % 10000u), (unsigned)(dt.month % 100u),
                 (unsigned)(dt.day % 100u), (unsigned)(dt.hour % 100u),
                 (unsigned)(dt.minute % 100u), (unsigned)(dt.second % 100u));
        return;
    }
    snprintf(buf, len, "NO-RTC");
}

static void copy_str(char *dst, size_t len, const char *src)
{
    if (dst == nullptr || len == 0) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, len - 1);
    dst[len - 1] = '\0';
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

static void note_temp(const sensor_data_t *sensors)
{
    if (sensors != nullptr && sensors->chamber_temp_valid) {
        batch_manager_note_chamber_temp(sensors->chamber_temperature);
    }
}

static void queue_sample(dryer_state_t state, const sensor_data_t *sensors,
                         const system_status_t *status)
{
    if (s_sample_queued_this_tick || !logging_state(state) || sensors == nullptr) {
        return;
    }

    batch_status_t batch {};
    batch_manager_get_status(&batch);
    if (batch.batch_id[0] == '\0') {
        return;
    }

    logger_sample_t sample {};
    format_now(status, sample.timestamp, sizeof(sample.timestamp));
    copy_str(sample.batch_id, sizeof(sample.batch_id), batch.batch_id);
    copy_str(sample.state, sizeof(sample.state), process_controller_state_name(state));
    copy_str(sample.fault, sizeof(sample.fault),
             safety_manager_fault_name(safety_manager_active_fault()));
    sample.moisture = sensors->moisture;
    sample.ec = sensors->ec;
    sample.hot_air_temperature = sensors->hot_air_temperature;
    sample.hot_air_humidity = sensors->hot_air_humidity;
    sample.chamber_temperature = sensors->chamber_temperature;
    sample.chamber_humidity = sensors->chamber_humidity;

    relay_state_t coils {};
    actuator_manager_get_state(&coils);
    sample.heater = coils.heater ? 1 : 0;
    sample.fan = coils.fan ? 1 : 0;
    sample.elevator = coils.elevator ? 1 : 0;
    sample.door = coils.door_open ? 1 : 0;

    logger_submit_sample(&sample);
    s_sample_queued_this_tick = true;
}

static void enqueue_summary(const char *completion, const sensor_data_t *sensors,
                            const system_status_t *status)
{
    if (!s_summary_pending) {
        return;
    }

    batch_status_t batch {};
    batch_manager_get_status(&batch);
    if (batch.batch_id[0] == '\0') {
        s_summary_pending = false;
        return;
    }

    s_summary_pending = false;
    format_now(status, s_ended, sizeof(s_ended));

    dryer_settings_t cfg {};
    settings_manager_get(&cfg);

    logger_summary_t summary {};
    copy_str(summary.batch_id, sizeof(summary.batch_id), batch.batch_id);
    copy_str(summary.start_time, sizeof(summary.start_time), s_started);
    copy_str(summary.end_time, sizeof(summary.end_time), s_ended);
    copy_str(summary.completion, sizeof(summary.completion), completion);
    copy_str(summary.fault, sizeof(summary.fault),
             safety_manager_fault_name(safety_manager_active_fault()));
    summary.initial_moisture = batch.initial_moisture;
    summary.target_moisture = cfg.target_moisture_pct;
    summary.max_temperature = batch.max_chamber_temp_c;
    summary.avg_temperature = batch.avg_chamber_temp_c;
    summary.drying_s = batch.drying_elapsed_s;
    summary.cooling_s = batch.cooling_elapsed_s;
    summary.discharge_s = batch.discharge_elapsed_s;
    if (batch.completed) {
        summary.final_moisture = batch.final_moisture;
        summary.final_ec = batch.final_ec;
    } else if (sensors != nullptr) {
        summary.final_moisture = sensors->moisture;
        summary.final_ec = sensors->ec;
    }
    logger_submit_summary(&summary);
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

static void enter_fault(fault_code_t code, const sensor_data_t *sensors,
                        const system_status_t *status)
{
    if (code != FAULT_NONE) {
        safety_manager_raise_fault(code);
    }
    if (circulation_manager_is_active()) {
        circulation_manager_stop();
    }
    safety_manager_request_heater(false);
    s_heater_on = false;
    queue_sample(s_state, sensors, status);
    enqueue_summary("FAULT", sensors, status);
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
    if (!status->sd_ok || !logger_is_healthy()) {
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

void process_controller_fill_status(system_status_t *status)
{
    if (status == nullptr) {
        return;
    }

    status->dryer_state = (uint8_t)s_state;
    status->drying_elapsed_s = s_drying_elapsed_s;
    status->cooling_elapsed_s = s_cooling_elapsed_s;
    status->discharge_elapsed_s = s_discharge_elapsed_s;
    status->batch_interrupted = batch_manager_interrupted_pending();
    status->discharge_phase = s_discharge_phase_pub;
    status->discharge_remaining_s = s_discharge_remaining_pub;
    copy_str(status->batch_started, sizeof(status->batch_started), s_started);
    copy_str(status->batch_ended, sizeof(status->batch_ended), s_ended);

    batch_status_t batch {};
    batch_manager_get_status(&batch);
    copy_str(status->batch_id, sizeof(status->batch_id), batch.batch_id);
    if (status->batch_interrupted && status->batch_id[0] == '\0') {
        copy_str(status->batch_id, sizeof(status->batch_id), batch.interrupted_batch_id);
    }
    status->initial_moisture_pct = batch.initial_moisture;
    status->final_moisture_pct = batch.final_moisture;
    status->final_ec = batch.final_ec;
    status->max_chamber_temp_c = batch.max_chamber_temp_c;
    status->avg_chamber_temp_c = batch.avg_chamber_temp_c;

    dryer_settings_t cfg {};
    settings_manager_get(&cfg);
    status->target_moisture_pct = cfg.target_moisture_pct;
    status->drying_temperature_c = cfg.drying_temperature_c;
    status->cooling_temperature_c = cfg.cooling_temperature_c;
    status->temperature_hysteresis_c = cfg.temperature_hysteresis_c;
    status->circulation_interval_s = cfg.circulation_interval_s;
    status->circulation_run_time_s = cfg.circulation_run_time_s;
    status->discharge_time_s = cfg.discharge_time_s;
    status->min_drying_time_s = cfg.min_drying_time_s;
    status->max_drying_time_s = cfg.max_drying_time_s;
    status->max_hot_air_temperature_c = cfg.max_hot_air_temperature_c;
    status->circulation_mode = (uint8_t)cfg.circulation_mode;
}

static void publish(const sensor_data_t *sensors, const dryer_settings_t *settings,
                    system_status_t *status)
{
    process_controller_fill_status(status);
    fault_code_t ignored = FAULT_NONE;
    status->precheck_ready =
        sensors != nullptr && settings != nullptr &&
        precheck_ok(sensors, settings, status, &ignored) &&
        !batch_manager_interrupted_pending() &&
        (s_state == DRYER_STATE_IDLE || s_state == DRYER_STATE_COMPLETE);
}

static bool require_manual(void)
{
    if (!process_controller_manual_control_allowed()) {
        ESP_LOGW(TAG, "Manual command refused — state=%s",
                 process_controller_state_name(s_state));
        return false;
    }
    return true;
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
    s_discharge_phase_pub = 0;
    s_discharge_remaining_pub = 0;
    s_summary_pending = false;
    s_sample_queued_this_tick = false;
    s_started[0] = '\0';
    s_ended[0] = '\0';

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
    s_ended[0] = '\0';
    transition_to(DRYER_STATE_PRECHECK);
    return ESP_OK;
}

esp_err_t process_controller_acknowledge_interrupted(void)
{
    return batch_manager_acknowledge_interrupted();
}

esp_err_t process_controller_abort(void)
{
    sensor_data_t sensors {};
    sensor_manager_get(&sensors);
    enqueue_summary("ABORTED", &sensors, nullptr);
    batch_manager_abort();
    enter_safe_idle_outputs();
    s_drying_elapsed_s = 0;
    s_cooling_elapsed_s = 0;
    s_discharge_elapsed_s = 0;
    s_discharge_phase_pub = 0;
    s_discharge_remaining_pub = 0;
    transition_to(DRYER_STATE_IDLE);
    return ESP_OK;
}

esp_err_t process_controller_acknowledge_fault(void)
{
    return safety_manager_acknowledge();
}

esp_err_t process_controller_save_settings(const dryer_settings_t *settings)
{
    if (!process_controller_settings_editable()) {
        ESP_LOGW(TAG, "Settings save refused — state=%s",
                 process_controller_state_name(s_state));
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = settings_manager_save(settings);
    if (err == ESP_OK && settings != nullptr) {
        circulation_manager_configure(settings);
    }
    return err;
}

esp_err_t process_controller_manual_toggle_fan(void)
{
    if (!require_manual()) {
        return ESP_ERR_INVALID_STATE;
    }
    relay_state_t st {};
    actuator_manager_get_state(&st);
    return actuator_manager_set_fan(!st.fan);
}

esp_err_t process_controller_manual_toggle_elevator(void)
{
    if (!require_manual()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (circulation_manager_is_active()) {
        circulation_manager_stop();
    }
    relay_state_t st {};
    actuator_manager_get_state(&st);
    return actuator_manager_set_elevator(!st.elevator);
}

esp_err_t process_controller_manual_toggle_heater(void)
{
    if (!require_manual()) {
        return ESP_ERR_INVALID_STATE;
    }
    relay_state_t st {};
    actuator_manager_get_state(&st);
    return safety_manager_request_heater(!st.heater);
}

esp_err_t process_controller_manual_all_off(void)
{
    if (!require_manual()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (circulation_manager_is_active()) {
        circulation_manager_stop();
    }
    return actuator_manager_all_off();
}

esp_err_t process_controller_manual_toggle_circulation(void)
{
    if (!require_manual()) {
        return ESP_ERR_INVALID_STATE;
    }
    dryer_settings_t cfg {};
    settings_manager_get(&cfg);
    circulation_manager_configure(&cfg);
    if (circulation_manager_is_active()) {
        return circulation_manager_stop();
    }
    return circulation_manager_start();
}

esp_err_t process_controller_manual_door_open(void)
{
    if (!require_manual()) {
        return ESP_ERR_INVALID_STATE;
    }
    return actuator_manager_open_door();
}

esp_err_t process_controller_manual_door_close(void)
{
    if (!require_manual()) {
        return ESP_ERR_INVALID_STATE;
    }
    return actuator_manager_close_door();
}

esp_err_t process_controller_manual_door_stop(void)
{
    if (!require_manual()) {
        return ESP_ERR_INVALID_STATE;
    }
    return actuator_manager_stop_door();
}

esp_err_t process_controller_assign_sensor(uint8_t new_addr, const char *role,
                                           char *msg, size_t msg_len)
{
    if (msg == nullptr || msg_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!require_manual()) {
        snprintf(msg, msg_len, "Sensor setup blocked while a batch is active");
        return ESP_ERR_INVALID_STATE;
    }

    static const uint8_t k_try[] = {1, 2, 3, 4, 5, 8, 10};
    uint8_t found = 0;
    th_reading_t reading {};
    for (size_t i = 0; i < sizeof(k_try); ++i) {
        if (th_sensor_read(k_try[i], &reading) == ESP_OK) {
            found = k_try[i];
            break;
        }
    }

    if (found == 0) {
        snprintf(msg, msg_len, "Sensor setup: no XY-MD03 answering (wire ONE probe only)");
        ESP_LOGI(TAG, "%s", msg);
        return ESP_ERR_NOT_FOUND;
    }
    if (found == new_addr) {
        snprintf(msg, msg_len, "Sensor setup: already %s @%u (%.1f C)",
                 role != nullptr ? role : "probe", (unsigned)found, reading.temperature_c);
        ESP_LOGI(TAG, "%s", msg);
        return ESP_OK;
    }

    esp_err_t err = th_sensor_set_address(found, new_addr);
    if (err == ESP_OK) {
        snprintf(msg, msg_len, "Sensor setup: %s wrote %u->%u OK — power-cycle probe",
                 role != nullptr ? role : "probe", (unsigned)found, (unsigned)new_addr);
    } else {
        snprintf(msg, msg_len, "Sensor setup: %s %u->%u FAILED (%s)",
                 role != nullptr ? role : "probe", (unsigned)found, (unsigned)new_addr,
                 esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "%s", msg);
    return err;
}

bool process_controller_manual_control_allowed(void)
{
    return s_state == DRYER_STATE_IDLE || s_state == DRYER_STATE_COMPLETE;
}

bool process_controller_settings_editable(void)
{
    return s_state == DRYER_STATE_IDLE || s_state == DRYER_STATE_COMPLETE ||
           s_state == DRYER_STATE_FAULT;
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

    s_sample_queued_this_tick = false;

    if (logging_state(s_state) && !logger_is_healthy()) {
        enter_fault(FAULT_F009_SD_CARD_ERROR, sensors, status);
        publish(sensors, settings, status);
        return;
    }

    /* Active-run fault takeover (Safety has priority). */
    if (s_state != DRYER_STATE_IDLE && s_state != DRYER_STATE_COMPLETE &&
        s_state != DRYER_STATE_FAULT) {
        fault_code_t fault = safety_manager_active_fault();
        if (fault != FAULT_NONE) {
            enter_fault(FAULT_NONE, sensors, status); /* already raised by Safety */
            publish(sensors, settings, status);
            return;
        }
    }

    switch (s_state) {
    case DRYER_STATE_IDLE:
        break;

    case DRYER_STATE_PRECHECK: {
        fault_code_t fault = FAULT_NONE;
        if (!precheck_ok(sensors, settings, status, &fault)) {
            enter_fault(fault, sensors, status);
            break;
        }

        if (batch_manager_create(sensors) != ESP_OK) {
            enter_fault(FAULT_F012_INVALID_CONFIGURATION, sensors, status);
            break;
        }

        format_now(status, s_started, sizeof(s_started));
        s_ended[0] = '\0';
        s_summary_pending = true;

        circulation_manager_configure(settings);
        actuator_manager_close_door();
        actuator_manager_set_fan(true);
        safety_manager_request_heater(false);
        s_heater_on = false;

        if (circulation_manager_start() != ESP_OK) {
            enter_fault(FAULT_F012_INVALID_CONFIGURATION, sensors, status);
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
        note_temp(sensors);

        actuator_manager_set_fan(true);
        actuator_manager_close_door();
        apply_heater_hysteresis(sensors, settings);
        save_recovery_throttled(sensors, false);

        if (s_drying_elapsed_s >= settings->max_drying_time_s) {
            enter_fault(FAULT_F010_MAX_DRYING_TIME, sensors, status);
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
        note_temp(sensors);

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
        note_temp(sensors);

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
                batch_manager_update_elapsed(s_drying_elapsed_s, s_cooling_elapsed_s,
                                             s_discharge_elapsed_s);
                queue_sample(s_state, sensors, status);
                batch_manager_complete(sensors);
                enqueue_summary("COMPLETE", sensors, status);
                enter_safe_idle_outputs();
                transition_to(DRYER_STATE_COMPLETE);
            }
            break;
        }

        if (s_state == DRYER_STATE_DISCHARGING) {
            const uint32_t phase_elapsed = elapsed_s(s_discharge_step_us);
            uint32_t limit = k_door_settle_s;
            if (s_discharge_step == DISCHARGE_ELEVATOR) {
                limit = settings->discharge_time_s;
            } else if (s_discharge_step == DISCHARGE_OPEN_DOOR) {
                limit = 0;
            }
            s_discharge_phase_pub = (uint8_t)s_discharge_step;
            s_discharge_remaining_pub =
                phase_elapsed >= limit ? 0 : (limit - phase_elapsed);
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
        if (safety_manager_active_fault() == FAULT_NONE) {
            batch_manager_abort();
            transition_to(DRYER_STATE_IDLE);
        }
        break;
    }

    if (s_state != DRYER_STATE_DISCHARGING) {
        s_discharge_phase_pub = 0;
        s_discharge_remaining_pub = 0;
    }

    if (logging_state(s_state)) {
        queue_sample(s_state, sensors, status);
    }

    publish(sensors, settings, status);
}
