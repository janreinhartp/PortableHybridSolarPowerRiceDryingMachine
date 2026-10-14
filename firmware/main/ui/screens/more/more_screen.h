#pragma once

#include "lvgl.h"
#include "system_status.h"

void more_build(lv_obj_t *parent, const system_status_t *status);
void more_update(const system_status_t *status);
void more_detach(void);
