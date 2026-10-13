#pragma once

#include "lvgl.h"
#include "system_status.h"

void alarms_build(lv_obj_t *parent, const system_status_t *status);
void alarms_update(const system_status_t *status);
void alarms_detach(void);
