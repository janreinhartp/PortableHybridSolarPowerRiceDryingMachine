#pragma once

#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_DASHBOARD = 0,
    UI_SCREEN_BATCH_SETUP,
    UI_SCREEN_DRYING,
    UI_SCREEN_COOLING,
    UI_SCREEN_DISCHARGE,
    UI_SCREEN_COMPLETE,
    UI_SCREEN_MANUAL,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_HISTORY,
    UI_SCREEN_ALARMS,
    UI_SCREEN_SYSTEM,
} ui_screen_t;

/** Build the Phase 7 shell (call under lvgl_port_lock). */
void ui_manager_show_hmi(const system_status_t *status);

/** Refresh the active screen from the control snapshot (call under lvgl_port_lock). */
void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps);

void ui_manager_bind_tap_counter(unsigned *counter);

/** Switch the content page. Safe from an LVGL event callback. */
void ui_manager_navigate(ui_screen_t screen);

void ui_manager_set_banner(const char *msg);

#ifdef __cplusplus
}
#endif
