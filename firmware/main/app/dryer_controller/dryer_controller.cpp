#include "dryer_controller.h"

#include "board_hal.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "rtc_manager.h"

static const char *TAG = "dryer_ctrl";

static void refresh_heap(system_status_t *status)
{
    status->free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    status->free_internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void refresh_rtc(system_status_t *status)
{
    rtc_datetime_t dt {};
    if (rtc_manager_get(&dt) == ESP_OK) {
        status->rtc_ok = true;
        status->rtc_year = dt.year;
        status->rtc_month = dt.month;
        status->rtc_day = dt.day;
        status->rtc_hour = dt.hour;
        status->rtc_minute = dt.minute;
        status->rtc_second = dt.second;
    } else {
        status->rtc_ok = false;
    }
}

esp_err_t dryer_controller_init(system_status_t *status)
{
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *status = {};
    refresh_heap(status);

    ESP_LOGI(TAG, "Phase 2 bring-up starting");

    board_hal_status_t board {};
    esp_err_t err = board_hal_init(&board);

    status->display_ok = board.display_ok;
    status->touch_ok = board.touch_ok;
    status->lvgl_ok = board.lvgl_ok;
    status->sd_ok = board.sd_ok;
    status->sd_total_mb = board.sd_total_mb;
    status->sd_free_mb = board.sd_free_mb;

    if (err != ESP_OK) {
        refresh_heap(status);
        ESP_LOGE(TAG, "Board bring-up failed: %s", esp_err_to_name(err));
        return err;
    }

    /* RTC is required for Phase 2 acceptance, but a missing module must be
     * reported on the HMI rather than blocking display bring-up. */
    esp_err_t rtc_err = rtc_manager_init();
    status->rtc_ok = (rtc_err == ESP_OK);
    if (status->rtc_ok) {
        refresh_rtc(status);
    } else {
        ESP_LOGW(TAG, "RTC init failed: %s", esp_err_to_name(rtc_err));
    }

    refresh_heap(status);
    ESP_LOGI(TAG, "Bring-up done board=%s rtc=%s heap=%u",
             esp_err_to_name(err),
             status->rtc_ok ? "ok" : "fail",
             (unsigned)status->free_heap);
    return ESP_OK;
}

void dryer_controller_refresh_heap(system_status_t *status)
{
    if (status == nullptr) {
        return;
    }
    refresh_heap(status);
    refresh_rtc(status);
}
