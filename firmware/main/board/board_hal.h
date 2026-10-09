#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool display_ok;
    bool touch_ok;
    bool lvgl_ok;
    bool sd_ok;
    size_t sd_total_mb;
    size_t sd_free_mb;
} board_hal_status_t;

esp_err_t board_hal_init(board_hal_status_t *out_status);
bool board_hal_lvgl_lock(int timeout_ms);
void board_hal_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif
