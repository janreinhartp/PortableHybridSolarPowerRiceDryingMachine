#pragma once

#include "lvgl.h"
#include "system_status.h"

void discharge_build(lv_obj_t *parent, const system_status_t *status);
void discharge_update(const system_status_t *status);
void discharge_detach(void);
