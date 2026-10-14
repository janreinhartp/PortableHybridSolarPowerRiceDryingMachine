#include "trend_buffer.h"

#include <cstring>

static trend_buffer_t s_buf {};

void trend_buffer_init(void)
{
    s_buf = {};
}

void trend_buffer_clear(void)
{
    s_buf = {};
}

void trend_buffer_push(const system_status_t *status)
{
    if (status == nullptr) {
        return;
    }

    const size_t i = s_buf.head;
    s_buf.moisture[i] = status->moisture_ok ? status->moisture_pct : 0.0f;
    s_buf.chamber_c[i] = status->chamber_ok ? status->chamber_temp_c : 0.0f;
    s_buf.hotair_c[i] = status->hotair_ok ? status->hotair_temp_c : 0.0f;
    s_buf.head = (i + 1) % TREND_POINTS;
    if (s_buf.count < TREND_POINTS) {
        s_buf.count++;
    }
}

size_t trend_buffer_copy(float *moisture, float *chamber_c, float *hotair_c, size_t max_n)
{
    if (max_n == 0 || s_buf.count == 0) {
        return 0;
    }
    const size_t n = s_buf.count < max_n ? s_buf.count : max_n;
    const size_t start =
        (s_buf.count < TREND_POINTS) ? 0 : s_buf.head;
    for (size_t i = 0; i < n; ++i) {
        const size_t idx = (start + (s_buf.count - n) + i) % TREND_POINTS;
        if (moisture != nullptr) {
            moisture[i] = s_buf.moisture[idx];
        }
        if (chamber_c != nullptr) {
            chamber_c[i] = s_buf.chamber_c[idx];
        }
        if (hotair_c != nullptr) {
            hotair_c[i] = s_buf.hotair_c[idx];
        }
    }
    return n;
}

const trend_buffer_t *trend_buffer_get(void)
{
    return &s_buf;
}
