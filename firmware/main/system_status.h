#pragma once

#include <stddef.h>
#include <stdbool.h>

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
    size_t free_heap;
    size_t free_internal_heap;
} system_status_t;

#ifdef __cplusplus
}
#endif
