#pragma once

#include "lvgl.h"
#include "system_status.h"

void complete_build(lv_obj_t *parent, const system_status_t *status);
void complete_update(const system_status_t *status);
void complete_detach(void);
