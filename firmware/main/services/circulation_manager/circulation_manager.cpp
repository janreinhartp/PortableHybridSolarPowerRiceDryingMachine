#include "circulation_manager.h"

#include "actuator_manager.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "circ_mgr";

static circulation_status_t s_status {};
static int64_t s_phase_start_us = 0;
static bool s_in_run_phase = false; /* interval mode: true while elevator should be ON */

esp_err_t circulation_manager_init(void)
{
    s_status = {};
    s_status.mode = CIRCULATION_MODE_INTERVAL;
    s_status.interval_s = 120;
    s_status.run_time_s = 10;
    s_phase_start_us = 0;
    s_in_run_phase = false;
    ESP_LOGI(TAG, "Circulation Manager ready");
    return ESP_OK;
}

void circulation_manager_configure(const dryer_settings_t *settings)
{
    if (settings == nullptr) {
        return;
    }
    s_status.mode = settings->circulation_mode;
    s_status.interval_s = settings->circulation_interval_s;
    s_status.run_time_s = settings->circulation_run_time_s;
}

esp_err_t circulation_manager_start(void)
{
    if (s_status.interval_s == 0 || s_status.run_time_s == 0 ||
        s_status.run_time_s >= s_status.interval_s) {
        if (s_status.mode == CIRCULATION_MODE_INTERVAL) {
            ESP_LOGW(TAG, "Cannot start — invalid interval timing");
            return ESP_ERR_INVALID_STATE;
        }
    }

    s_status.active = true;
    s_phase_start_us = esp_timer_get_time();
    s_status.phase_elapsed_ms = 0;

    if (s_status.mode == CIRCULATION_MODE_CONTINUOUS) {
        s_in_run_phase = true;
        actuator_manager_set_elevator(true);
        s_status.elevator_on = true;
        ESP_LOGI(TAG, "Circulation START continuous");
    } else {
        /* Interval: begin in OFF dwell for (interval - run), then ON for run.
         * Spec example: OFF until 02:00, ON 10s, OFF ... — start OFF. */
        s_in_run_phase = false;
        actuator_manager_set_elevator(false);
        s_status.elevator_on = false;
        ESP_LOGI(TAG, "Circulation START interval run=%us period=%us",
                 (unsigned)s_status.run_time_s, (unsigned)s_status.interval_s);
    }
    return ESP_OK;
}

esp_err_t circulation_manager_stop(void)
{
    s_status.active = false;
    s_in_run_phase = false;
    s_status.phase_elapsed_ms = 0;
    actuator_manager_set_elevator(false);
    s_status.elevator_on = false;
    ESP_LOGI(TAG, "Circulation STOP");
    return ESP_OK;
}

void circulation_manager_tick(void)
{
    if (!s_status.active) {
        return;
    }

    const int64_t now = esp_timer_get_time();
    const uint32_t elapsed_ms =
        (uint32_t)((now - s_phase_start_us) / 1000LL);
    s_status.phase_elapsed_ms = elapsed_ms;

    if (s_status.mode == CIRCULATION_MODE_CONTINUOUS) {
        if (!s_status.elevator_on) {
            actuator_manager_set_elevator(true);
            s_status.elevator_on = true;
        }
        return;
    }

    /* Interval mode — non-blocking phase machine. */
    const uint32_t dwell_ms =
        (s_status.interval_s > s_status.run_time_s)
            ? (s_status.interval_s - s_status.run_time_s) * 1000U
            : 0;
    const uint32_t run_ms = s_status.run_time_s * 1000U;

    if (!s_in_run_phase) {
        if (elapsed_ms >= dwell_ms) {
            s_in_run_phase = true;
            s_phase_start_us = now;
            s_status.phase_elapsed_ms = 0;
            actuator_manager_set_elevator(true);
            s_status.elevator_on = true;
            ESP_LOGI(TAG, "Interval: elevator ON for %us",
                     (unsigned)s_status.run_time_s);
        } else if (s_status.elevator_on) {
            actuator_manager_set_elevator(false);
            s_status.elevator_on = false;
        }
    } else {
        if (elapsed_ms >= run_ms) {
            s_in_run_phase = false;
            s_phase_start_us = now;
            s_status.phase_elapsed_ms = 0;
            actuator_manager_set_elevator(false);
            s_status.elevator_on = false;
            ESP_LOGI(TAG, "Interval: elevator OFF for %us",
                     (unsigned)(s_status.interval_s - s_status.run_time_s));
        } else if (!s_status.elevator_on) {
            actuator_manager_set_elevator(true);
            s_status.elevator_on = true;
        }
    }
}

bool circulation_manager_is_active(void)
{
    return s_status.active;
}

void circulation_manager_get_status(circulation_status_t *out)
{
    if (out) {
        *out = s_status;
    }
}
