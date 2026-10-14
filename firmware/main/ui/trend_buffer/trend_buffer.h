#pragma once

#include <stddef.h>
#include <stdint.h>

#include "system_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TREND_POINTS 300 /* 5 minutes at 1 Hz */

typedef struct {
    float moisture[TREND_POINTS];
    float chamber_c[TREND_POINTS];
    float hotair_c[TREND_POINTS];
    size_t count;
    size_t head;
} trend_buffer_t;

void trend_buffer_init(void);
void trend_buffer_push(const system_status_t *status);
void trend_buffer_clear(void);

/** Newest samples last. out_n is number of valid points written (≤ max_n, ≤ TREND_POINTS). */
size_t trend_buffer_copy(float *moisture, float *chamber_c, float *hotair_c, size_t max_n);

const trend_buffer_t *trend_buffer_get(void);

#ifdef __cplusplus
}
#endif
