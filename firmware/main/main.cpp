#include "board_hal.h"
#include "dryer_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "process_controller.h"
#include "ui_manager.h"

static const char *TAG = "main";

static system_status_t s_status {};
static unsigned s_touch_taps = 0;

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
    ESP_LOGI(TAG, "Hybrid Solar Power Rice Dryer firmware starting (Phase 6)");

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
    ESP_LOGI(TAG, "Phase 6 running (Process Controller / Batch Manager)");
}
