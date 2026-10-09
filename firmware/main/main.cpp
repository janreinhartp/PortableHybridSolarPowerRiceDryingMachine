#include "board_hal.h"
#include "dryer_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "relay_modbus.h"
#include "ui_manager.h"

static const char *TAG = "main";

static system_status_t s_status {};
static unsigned s_touch_taps = 0;

static void runtime_status_task(void *arg)
{
    (void)arg;
    bool safe_off_attempted = false;

    while (true) {
        dryer_controller_refresh_heap(&s_status);
        dryer_controller_poll_fieldbus(&s_status);

        /* Once the relay answers, force a safe all-OFF (fail-soft if it drops). */
        if (s_status.relay_ok && !safe_off_attempted) {
            safe_off_attempted = true;
            ESP_LOGI(TAG, "Relay online — commanding all coils OFF");
            relay_modbus_all_off();
            dryer_controller_poll_fieldbus(&s_status);
        }

        ESP_LOGI(TAG,
                 "hotair=%s/%.1fC chamber=%s/%.1fC relay=%s@%u elev=%d heat=%d fan=%d door=%d/%d",
                 s_status.hotair_ok ? "ok" : "fail", s_status.hotair_temp_c,
                 s_status.chamber_ok ? "ok" : "fail", s_status.chamber_temp_c,
                 s_status.relay_ok ? "ok" : "fail", (unsigned)relay_modbus_address(),
                 (int)s_status.relay_elevator, (int)s_status.relay_heater,
                 (int)s_status.relay_fan,
                 (int)s_status.relay_door_open, (int)s_status.relay_door_close);

        if (board_hal_lvgl_lock(100)) {
            ui_manager_update_runtime(&s_status, s_touch_taps);
            board_hal_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hybrid Solar Power Rice Dryer firmware starting (Phase 3)");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = dryer_controller_init(&s_status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Foundation bring-up failed: %s", esp_err_to_name(err));
        return;
    }

    if (board_hal_lvgl_lock(-1)) {
        ui_manager_bind_tap_counter(&s_touch_taps);
        ui_manager_show_foundation_screen(&s_status);
        board_hal_lvgl_unlock();
    }

    xTaskCreate(runtime_status_task, "runtime_status", 6144, nullptr, 3, nullptr);
    ESP_LOGI(TAG, "Phase 3 running");
}
