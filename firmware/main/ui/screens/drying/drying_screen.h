#pragma once

#include "lvgl.h"
#include "system_status.h"

void drying_build(lv_obj_t *parent, const system_status_t *status);
void drying_update(const system_status_t *status);
void drying_detach(void);
