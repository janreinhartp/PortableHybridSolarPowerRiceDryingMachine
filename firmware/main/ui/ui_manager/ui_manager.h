#pragma once

#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Create the Phase 1 foundation status screen (call under lvgl_port_lock). */
void ui_manager_show_foundation_screen(const system_status_t *status);

/** Update live heap / touch feedback (call under lvgl_port_lock). */
void ui_manager_update_runtime(const system_status_t *status, unsigned touch_taps);

/** Bind a shared tap counter updated by the on-screen touch test button. */
void ui_manager_bind_tap_counter(unsigned *counter);

#ifdef __cplusplus
}
#endif
