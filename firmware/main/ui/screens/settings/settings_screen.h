#pragma once

#include "lvgl.h"
#include "system_status.h"

void settings_build(lv_obj_t *parent, const system_status_t *status);
void settings_update(const system_status_t *status);
void settings_detach(void);
