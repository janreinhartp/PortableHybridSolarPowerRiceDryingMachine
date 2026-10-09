#include "dryer_controller.h"

#include "board_hal.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "dryer_ctrl";

static void refresh_heap(system_status_t *status)
{
    status->free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    status->free_internal_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

esp_err_t dryer_controller_init(system_status_t *status)
{
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *status = {};
    refresh_heap(status);

    ESP_LOGI(TAG, "Phase 1 foundation bring-up starting");

    board_hal_status_t board {};
    esp_err_t err = board_hal_init(&board);

    status->display_ok = board.display_ok;
    status->touch_ok = board.touch_ok;
    status->lvgl_ok = board.lvgl_ok;
    status->sd_ok = board.sd_ok;
    status->sd_total_mb = board.sd_total_mb;
    status->sd_free_mb = board.sd_free_mb;

    refresh_heap(status);
    ESP_LOGI(TAG, "Bring-up done err=%s heap=%u internal=%u",
             esp_err_to_name(err),
             (unsigned)status->free_heap,
             (unsigned)status->free_internal_heap);
    return err;
}

void dryer_controller_refresh_heap(system_status_t *status)
{
    if (status == nullptr) {
        return;
    }
    refresh_heap(status);
}
