#include "board_hal.h"

#include "esp_log.h"
#include "gt911.h"
#include "lvgl_port.h"
#include "rgb_lcd_port.h"
#include "sd.h"

static const char *TAG = "board_hal";

esp_err_t board_hal_init(board_hal_status_t *out_status)
{
    if (out_status == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_status = (board_hal_status_t){0};

    esp_lcd_touch_handle_t tp_handle = touch_gt911_init();
    out_status->touch_ok = (tp_handle != NULL);
    if (!out_status->touch_ok) {
        ESP_LOGE(TAG, "Touch (GT911) init failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Touch OK");

    esp_lcd_panel_handle_t panel_handle = waveshare_esp32_s3_rgb_lcd_init();
    out_status->display_ok = (panel_handle != NULL);
    if (!out_status->display_ok) {
        ESP_LOGE(TAG, "RGB LCD init failed");
        return ESP_FAIL;
    }
    wavesahre_rgb_lcd_bl_on();
    ESP_LOGI(TAG, "Display OK (1024x600), backlight on");

    esp_err_t err = lvgl_port_init(panel_handle, tp_handle);
    out_status->lvgl_ok = (err == ESP_OK);
    if (!out_status->lvgl_ok) {
        ESP_LOGE(TAG, "LVGL port init failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "LVGL OK");

    err = sd_mmc_init();
    out_status->sd_ok = (err == ESP_OK);
    if (out_status->sd_ok) {
        size_t total_kb = 0;
        size_t free_kb = 0;
        if (read_sd_capacity(&total_kb, &free_kb) == ESP_OK) {
            out_status->sd_total_mb = total_kb / 1024;
            out_status->sd_free_mb = free_kb / 1024;
        }
        sd_card_print_info();
        ESP_LOGI(TAG, "SD mounted at /sdcard (%u MB free / %u MB total)",
                 (unsigned)out_status->sd_free_mb, (unsigned)out_status->sd_total_mb);
    } else {
        ESP_LOGW(TAG, "SD mount failed (card missing or error): %s", esp_err_to_name(err));
    }

    return ESP_OK;
}

bool board_hal_lvgl_lock(int timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void board_hal_lvgl_unlock(void)
{
    lvgl_port_unlock();
}
