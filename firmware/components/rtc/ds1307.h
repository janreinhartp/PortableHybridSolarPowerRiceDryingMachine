#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;   /* 2000–2099 */
    uint8_t month;   /* 1–12 */
    uint8_t day;     /* 1–31 */
    uint8_t hour;    /* 0–23 */
    uint8_t minute;  /* 0–59 */
    uint8_t second;  /* 0–59 */
} ds1307_time_t;

esp_err_t ds1307_init(void);
bool ds1307_is_present(void);
esp_err_t ds1307_get_time(ds1307_time_t *out_time);
esp_err_t ds1307_set_time(const ds1307_time_t *time);

#ifdef __cplusplus
}
#endif
