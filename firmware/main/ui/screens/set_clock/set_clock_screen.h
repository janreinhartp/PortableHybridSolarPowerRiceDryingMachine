#pragma once

#include "lvgl.h"
#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

void set_clock_detach(void);
void set_clock_build(lv_obj_t *parent, const system_status_t *status);
void set_clock_update(const system_status_t *status);

#ifdef __cplusplus
}
#endif
