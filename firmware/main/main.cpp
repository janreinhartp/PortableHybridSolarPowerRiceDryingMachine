#include "board_hal.h"
#include "boot_screen.h"
#include "dryer_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "process_controller.h"
#include "ui_manager.h"
#include "web_server.h"

#include <cstring>

static const char *TAG = "main";

static system_status_t s_status {};
static unsigned s_touch_taps = 0;

static boot_step_t step_from_id(const char *id)
{
    if (id == nullptr) {
        return BOOT_STEP_COUNT;
    }
    if (strcmp(id, "display") == 0) {
        return BOOT_STEP_DISPLAY;
    }
    if (strcmp(id, "sd") == 0) {
        return BOOT_STEP_SD;
    }
    if (strcmp(id, "rtc") == 0) {
        return BOOT_STEP_RTC;
    }
    if (strcmp(id, "moisture") == 0) {
        return BOOT_STEP_MOISTURE;
    }
    if (strcmp(id, "core") == 0) {
        return BOOT_STEP_CORE;
    }
    if (strcmp(id, "logger") == 0) {
        return BOOT_STEP_LOGGER;
    }
    if (strcmp(id, "rs485") == 0) {
        return BOOT_STEP_RS485;
    }
    if (strcmp(id, "relay") == 0) {
        return BOOT_STEP_RELAY;
    }
    if (strcmp(id, "wifi") == 0) {
        return BOOT_STEP_WIFI;
    }
    return BOOT_STEP_COUNT;
}

static void on_init_progress(const char *step_id, bool ok, void *user)
{
    (void)user;
    const boot_step_t step = step_from_id(step_id);
    if (step >= BOOT_STEP_COUNT) {
        return;
    }
    if (board_hal_lvgl_lock(200)) {
        if (!boot_screen_is_active() &&
            (step == BOOT_STEP_DISPLAY || step == BOOT_STEP_SD)) {
            /* Board just came up — show splash before remaining steps. */
            if (s_status.lvgl_ok) {
                boot_screen_show();
            }
        }
        if (boot_screen_is_active()) {
            boot_screen_set(step, ok ? BOOT_STATUS_OK : BOOT_STATUS_FAIL);
        }
        board_hal_lvgl_unlock();
    }
}

static void runtime_status_task(void *arg)
{
    (void)arg;

    while (true) {
        dryer_controller_refresh_heap(&s_status);
        dryer_controller_poll_fieldbus(&s_status);

        ESP_LOGI(TAG,
                 "state=%s batch=%s hotair=%s/%.1fC chamber=%s/%.1fC "
                 "moist=%s/%.1f%% dry=%us cool=%us disc=%us "
                 "heat=%d fan=%d elev=%d fault=%u int=%d",
                 process_controller_state_name(
                     (dryer_state_t)s_status.dryer_state),
                 s_status.batch_id[0] ? s_status.batch_id : "-",
                 s_status.hotair_ok ? "ok" : "fail", s_status.hotair_temp_c,
                 s_status.chamber_ok ? "ok" : "fail", s_status.chamber_temp_c,
                 s_status.moisture_ok ? "ok" : "fail", s_status.moisture_pct,
                 (unsigned)s_status.drying_elapsed_s,
                 (unsigned)s_status.cooling_elapsed_s,
                 (unsigned)s_status.discharge_elapsed_s,
                 (int)s_status.relay_heater, (int)s_status.relay_fan,
                 (int)s_status.relay_elevator,
                 (unsigned)s_status.active_fault,
                 (int)s_status.batch_interrupted);

        if (board_hal_lvgl_lock(100)) {
            ui_manager_update_runtime(&s_status, s_touch_taps);
            board_hal_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hybrid Solar Power Rice Dryer firmware starting");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = dryer_controller_init(&s_status, on_init_progress, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Foundation bring-up failed: %s", esp_err_to_name(err));
        return;
    }

    /* Soft-fail: dryer still runs if SoftAP/HTTP cannot start. */
    if (web_server_start(&s_status) == ESP_OK) {
        s_status.wifi_ok = true;
        web_server_copy_ssid(s_status.wifi_ssid, sizeof(s_status.wifi_ssid));
        if (board_hal_lvgl_lock(-1)) {
            if (boot_screen_is_active()) {
                boot_screen_set(BOOT_STEP_WIFI, BOOT_STATUS_OK);
            }
            board_hal_lvgl_unlock();
        }
    } else {
        s_status.wifi_ok = false;
        s_status.wifi_ssid[0] = '\0';
        ESP_LOGW(TAG, "Web dashboard unavailable");
        if (board_hal_lvgl_lock(-1)) {
            if (boot_screen_is_active()) {
                boot_screen_set(BOOT_STEP_WIFI, BOOT_STATUS_FAIL);
            }
            board_hal_lvgl_unlock();
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (board_hal_lvgl_lock(-1)) {
        boot_screen_destroy();
        ui_manager_bind_tap_counter(&s_touch_taps);
        ui_manager_show_hmi(&s_status);
        board_hal_lvgl_unlock();
    }

    xTaskCreate(runtime_status_task, "runtime_status", 8192, nullptr, 3, nullptr);
    ESP_LOGI(TAG, "Running (logger + HMI + local web)");
}
