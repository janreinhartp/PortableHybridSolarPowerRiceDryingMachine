#pragma once

#include "lvgl.h"
#include "system_status.h"

void batch_setup_build(lv_obj_t *parent, const system_status_t *status);
void batch_setup_update(const system_status_t *status);
void batch_setup_detach(void);
