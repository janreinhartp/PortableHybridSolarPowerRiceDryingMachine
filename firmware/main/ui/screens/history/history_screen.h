#pragma once

#include "lvgl.h"
#include "system_status.h"

void history_build(lv_obj_t *parent, const system_status_t *status);
void history_update(const system_status_t *status);
void history_detach(void);
