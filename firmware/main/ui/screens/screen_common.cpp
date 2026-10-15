#include "screen_common.h"

#include <cstdio>

#include "process_controller.h"
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

void ui_format_header_datetime(char *buf, size_t len, uint16_t year, uint8_t month, uint8_t day,
                               uint8_t hour, uint8_t minute)
{
    if (buf == nullptr || len == 0) {
        return;
    }
    char date[24];
    char time[16];
    ui_format_header_date(date, sizeof(date), year, month, day);
    ui_format_header_time(time, sizeof(time), hour, minute);
    snprintf(buf, len, "%s  %s", date, time);
}

const char *ui_on_off(bool on)
{
    return on ? "On" : "Off";
}

const char *ui_door_text(bool door_open, bool door_close)
{
    if (door_open) {
        return "Open";
    }
    if (door_close) {
        return "Closing";
    }
    return "Closed";
}

const char *ui_circ_mode_name(uint8_t mode)
{
    return mode == (uint8_t)CIRCULATION_MODE_CONTINUOUS ? "Continuous" : "Interval";
}

const char *ui_state_title(uint8_t dryer_state)
{
    switch ((dryer_state_t)dryer_state) {
    case DRYER_STATE_PRECHECK:
        return "Precheck";
    case DRYER_STATE_DRYING:
        return "Drying";
    case DRYER_STATE_COOLING:
        return "Cooling";
    case DRYER_STATE_DISCHARGING:
        return "Discharging";
    case DRYER_STATE_COMPLETE:
        return "Complete";
    case DRYER_STATE_FAULT:
        return "Fault";
    case DRYER_STATE_IDLE:
    default:
        return "Idle";
    }
}
