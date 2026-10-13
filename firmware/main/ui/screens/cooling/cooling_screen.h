#pragma once

#include "lvgl.h"
#include "system_status.h"

void cooling_build(lv_obj_t *parent, const system_status_t *status);
void cooling_update(const system_status_t *status);
void cooling_detach(void);
