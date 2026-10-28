#pragma once

#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_SCREEN_DASHBOARD = 0,
    UI_SCREEN_BATCH_SETUP,
    UI_SCREEN_RUN,
    UI_SCREEN_COMPLETE,
    UI_SCREEN_MANUAL,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_HISTORY,
    UI_SCREEN_ALARMS,
    UI_SCREEN_SYSTEM,
    UI_SCREEN_MORE,
    UI_SCREEN_COMMISSION,
    UI_SCREEN_CALIBRATE_MOISTURE,
    UI_SCREEN_SET_CLOCK,
    /* Legacy aliases kept for any external callers */
    UI_SCREEN_DRYING = UI_SCREEN_RUN,
    UI_SCREEN_COOLING = UI_SCREEN_RUN,
    UI_SCREEN_DISCHARGE = UI_SCREEN_RUN,
} ui_screen_t;

void ui_manager_show_hmi(const system_status_t *status);
void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps);
void ui_manager_bind_tap_counter(unsigned *counter);
void ui_manager_navigate(ui_screen_t screen);
void ui_manager_set_banner(const char *msg);

#ifdef __cplusplus
}
#endif
