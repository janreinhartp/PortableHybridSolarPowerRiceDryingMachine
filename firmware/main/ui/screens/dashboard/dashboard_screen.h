#pragma once

#include "lvgl.h"
#include "system_status.h"

void dashboard_build(lv_obj_t *parent, const system_status_t *status);
void dashboard_update(const system_status_t *status);
void dashboard_detach(void);
