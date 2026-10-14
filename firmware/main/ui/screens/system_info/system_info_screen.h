#pragma once

#include "lvgl.h"
#include "system_status.h"

void system_info_build(lv_obj_t *parent, const system_status_t *status, unsigned touch_taps);
void system_info_update(const system_status_t *status, unsigned touch_taps);
void system_info_detach(void);
void system_info_set_tap_counter(unsigned *counter);
