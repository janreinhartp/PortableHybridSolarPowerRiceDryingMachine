#pragma once

#include "lvgl.h"
#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

void pid_cal_build(lv_obj_t *parent, const system_status_t *status);
void pid_cal_update(const system_status_t *status);
void pid_cal_detach(void);

#ifdef __cplusplus
}
#endif
