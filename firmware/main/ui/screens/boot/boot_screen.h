#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BOOT_STEP_DISPLAY = 0,
    BOOT_STEP_SD,
    BOOT_STEP_RTC,
    BOOT_STEP_MOISTURE,
    BOOT_STEP_CORE,
    BOOT_STEP_LOGGER,
    BOOT_STEP_RS485,
    BOOT_STEP_RELAY,
    BOOT_STEP_WIFI,
    BOOT_STEP_COUNT
} boot_step_t;

typedef enum {
    BOOT_STATUS_PENDING = 0,
    BOOT_STATUS_OK,
    BOOT_STATUS_FAIL,
} boot_status_t;

/** Full-screen splash (call under lvgl lock). */
void boot_screen_show(void);
void boot_screen_set(boot_step_t step, boot_status_t status);
void boot_screen_destroy(void);
bool boot_screen_is_active(void);

#ifdef __cplusplus
}
#endif
