#pragma once

#include "lvgl.h"
#include "system_status.h"

void run_build(lv_obj_t *parent, const system_status_t *status);
void run_update(const system_status_t *status);
void run_detach(void);
