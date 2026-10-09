#include "rtc_manager.h"

#include <cstdio>
#include <cstring>

#include "ds1307.h"
#include "esp_log.h"

static const char *TAG = "rtc_mgr";
static bool s_ok = false;

static int month_from_build_str(const char *mon)
{
    static const char *names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    for (int i = 0; i < 12; ++i) {
        if (strncmp(mon, names[i], 3) == 0) {
            return i + 1;
        }
    }
    return 1;
}

esp_err_t rtc_manager_init(void)
{
    esp_err_t err = ds1307_init();
    s_ok = (err == ESP_OK) && ds1307_is_present();
    if (!s_ok) {
        ESP_LOGW(TAG, "RTC unavailable — timestamps will be invalid until fixed");
        return err == ESP_OK ? ESP_FAIL : err;
    }
    ESP_LOGI(TAG, "RTC manager ready");
    return ESP_OK;
}

bool rtc_manager_is_ok(void)
{
    return s_ok && ds1307_is_present();
}

esp_err_t rtc_manager_get(rtc_datetime_t *out_dt)
{
    if (out_dt == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    ds1307_time_t t {};
    esp_err_t err = ds1307_get_time(&t);
    if (err != ESP_OK) {
        s_ok = false;
        return err;
    }
    out_dt->year = t.year;
    out_dt->month = t.month;
    out_dt->day = t.day;
    out_dt->hour = t.hour;
    out_dt->minute = t.minute;
    out_dt->second = t.second;
    s_ok = true;
    return ESP_OK;
}

esp_err_t rtc_manager_set(const rtc_datetime_t *dt)
{
    if (dt == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    ds1307_time_t t {
        .year = dt->year,
        .month = dt->month,
        .day = dt->day,
        .hour = dt->hour,
        .minute = dt->minute,
        .second = dt->second,
    };
    esp_err_t err = ds1307_set_time(&t);
    s_ok = (err == ESP_OK);
    return err;
}

esp_err_t rtc_manager_adjust_minutes(int delta_minutes)
{
    rtc_datetime_t dt {};
    esp_err_t err = rtc_manager_get(&dt);
    if (err != ESP_OK) {
        return err;
    }

    int total = (int)dt.hour * 60 + (int)dt.minute + delta_minutes;
    while (total < 0) {
        total += 24 * 60;
        /* Date rollback kept simple for Phase 2 HMI nudges. */
    }
    while (total >= 24 * 60) {
        total -= 24 * 60;
    }
    dt.hour = (uint8_t)(total / 60);
    dt.minute = (uint8_t)(total % 60);
    dt.second = 0;
    return rtc_manager_set(&dt);
}

esp_err_t rtc_manager_set_build_time(void)
{
    /* __DATE__ = "Mmm dd yyyy", __TIME__ = "hh:mm:ss" */
    char mon[4] = {0};
    int day = 1;
    int year = 2026;
    int hour = 0;
    int minute = 0;
    int second = 0;

    if (sscanf(__DATE__, "%3s %d %d", mon, &day, &year) != 3) {
        ESP_LOGE(TAG, "Failed to parse __DATE__");
        return ESP_FAIL;
    }
    if (sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second) != 3) {
        ESP_LOGE(TAG, "Failed to parse __TIME__");
        return ESP_FAIL;
    }

    rtc_datetime_t dt {
        .year = (uint16_t)year,
        .month = (uint8_t)month_from_build_str(mon),
        .day = (uint8_t)day,
        .hour = (uint8_t)hour,
        .minute = (uint8_t)minute,
        .second = (uint8_t)second,
    };
    return rtc_manager_set(&dt);
}
