#pragma once

#include "lvgl.h"
#include "system_status.h"

void manual_build(lv_obj_t *parent, const system_status_t *status);
void manual_update(const system_status_t *status);
void manual_detach(void);
