#include "board_hal.h"
#include "dryer_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "ui_manager.h"

static const char *TAG = "main";

static system_status_t s_status {};
static unsigned s_touch_taps = 0;

static void runtime_status_task(void *arg)
{
    (void)arg;
    while (true) {
        dryer_controller_refresh_heap(&s_status);

        if (s_status.rtc_ok) {
            ESP_LOGI(TAG, "rtc=%04u-%02u-%02u %02u:%02u:%02u heap=%u taps=%u",
                     s_status.rtc_year, s_status.rtc_month, s_status.rtc_day,
                     s_status.rtc_hour, s_status.rtc_minute, s_status.rtc_second,
                     (unsigned)s_status.free_heap, s_touch_taps);
        } else {
            ESP_LOGW(TAG, "rtc=FAIL heap=%u taps=%u sd=%s",
                     (unsigned)s_status.free_heap, s_touch_taps,
                     s_status.sd_ok ? "ok" : "missing");
        }

        if (board_hal_lvgl_lock(100)) {
            ui_manager_update_runtime(&s_status, s_touch_taps);
            board_hal_lvgl_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Hybrid Solar Power Rice Dryer firmware starting (Phase 2)");

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

    xTaskCreate(runtime_status_task, "runtime_status", 4096, nullptr, 3, nullptr);
    ESP_LOGI(TAG, "Phase 2 running");
}
