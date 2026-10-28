#pragma once

#include "system_status.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void calibrate_moisture_build(lv_obj_t *parent, const system_status_t *status);
void calibrate_moisture_update(const system_status_t *status);
void calibrate_moisture_detach(void);

#ifdef __cplusplus
}
#endif
