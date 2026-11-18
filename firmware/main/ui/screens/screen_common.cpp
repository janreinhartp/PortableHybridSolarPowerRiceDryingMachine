#include "screen_common.h"

#include <cstdio>

#include "settings_manager.h"

void ui_format_hms(char *buf, size_t len, uint32_t seconds)
{
    if (buf == nullptr || len == 0) {
        return;
    }
    snprintf(buf, len, "%02u:%02u:%02u", (unsigned)(seconds / 3600u),
             (unsigned)((seconds / 60u) % 60u), (unsigned)(seconds % 60u));
}

void ui_format_header_date(char *buf, size_t len, uint16_t year, uint8_t month, uint8_t day)
{
    if (buf == nullptr || len == 0) {
        return;
    }
    static const char *k_months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const unsigned m = (month >= 1 && month <= 12) ? (unsigned)month : 1u;
    snprintf(buf, len, "%s %u, %u", k_months[m - 1u], (unsigned)day, (unsigned)year);
}

void ui_format_header_time(char *buf, size_t len, uint8_t hour, uint8_t minute)
{
    if (buf == nullptr || len == 0) {
        return;
    }
    const unsigned h24 = (unsigned)(hour % 24u);
    const unsigned h12 = (h24 % 12u == 0u) ? 12u : (h24 % 12u);
    const char *ampm = (h24 >= 12u) ? "PM" : "AM";
    snprintf(buf, len, "%u:%02u %s", h12, (unsigned)(minute % 100u), ampm);
}

const char *ui_door_text(bool door_open)
{
    return door_open ? "Open" : "Closed";
}

const char *ui_circ_mode_name(uint8_t mode)
{
    return mode == (uint8_t)CIRCULATION_MODE_CONTINUOUS ? "Continuous" : "Interval";
}
