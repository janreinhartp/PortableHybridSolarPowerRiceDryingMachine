#include "sensor_manager.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ads1115.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "gpio_map.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "th_sensor.h"

static const char *TAG = "sensor_mgr";
static const char *NVS_NS = "sns_cal";

static sensor_data_t s_data {};
static sensor_moisture_cal_t s_moist_cal {0.0f, 0.0f, 2.5f, 50.0f, 5.0f, 100.0f};

void sensor_manager_get_default_moisture_cal(sensor_moisture_cal_t *out)
{
    if (out == nullptr) {
        return;
    }
    *out = {0.0f, 0.0f, 2.5f, 50.0f, 5.0f, 100.0f};
}

static float lerp(float v, float v_a, float y_a, float v_b, float y_b)
{
    const float dv = v_b - v_a;
    if (std::fabs(dv) < 1e-6f) {
        return y_a;
    }
    return y_a + (v - v_a) * (y_b - y_a) / dv;
}

static float apply_piecewise_3(const sensor_moisture_cal_t &cal, float voltage)
{
    /* Sort points by voltage into local copies. */
    float v[3] = {cal.v1, cal.v2, cal.v3};
    float y[3] = {cal.y1, cal.y2, cal.y3};
    for (int i = 0; i < 2; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            if (v[j] < v[i]) {
                const float tv = v[i];
                const float ty = y[i];
                v[i] = v[j];
                y[i] = y[j];
                v[j] = tv;
                y[j] = ty;
            }
        }
    }

    if (voltage <= v[1]) {
        return lerp(voltage, v[0], y[0], v[1], y[1]);
    }
    return lerp(voltage, v[1], y[1], v[2], y[2]);
}

static bool voltage_in_range(float volt)
{
    return volt >= -0.05f && volt <= 5.5f;
}

esp_err_t sensor_manager_validate_moisture_cal(const sensor_moisture_cal_t *cal)
{
    if (cal == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    const float vs[3] = {cal->v1, cal->v2, cal->v3};
    const float ys[3] = {cal->y1, cal->y2, cal->y3};
    for (int i = 0; i < 3; ++i) {
        if (!voltage_in_range(vs[i])) {
            return ESP_ERR_INVALID_ARG;
        }
        if (ys[i] < 0.0f || ys[i] > 100.0f) {
            return ESP_ERR_INVALID_ARG;
        }
        for (int j = i + 1; j < 3; ++j) {
            if (std::fabs(vs[i] - vs[j]) < 1e-3f) {
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    return ESP_OK;
}

static esp_err_t nvs_write_moisture_cal(const sensor_moisture_cal_t *cal)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    char key[16];
    const float *vals[] = {&cal->v1, &cal->y1, &cal->v2, &cal->y2, &cal->v3, &cal->y3};
    const char *names[] = {"m_v1", "m_y1", "m_v2", "m_y2", "m_v3", "m_y3"};
    for (int i = 0; i < 6; ++i) {
        snprintf(key, sizeof(key), "%s", names[i]);
        nvs_set_blob(h, key, vals[i], sizeof(float));
    }
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static bool nvs_get_f(nvs_handle_t h, const char *key, float *out)
{
    size_t sz = sizeof(float);
    return nvs_get_blob(h, key, out, &sz) == ESP_OK && sz == sizeof(float);
}

esp_err_t sensor_manager_load_calibration(void)
{
    sensor_manager_get_default_moisture_cal(&s_moist_cal);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No NVS calibration — defaults 0→0 / 2.5→50 / 5→100 %%");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    float v1 = s_moist_cal.v1, y1 = s_moist_cal.y1;
    float v2 = s_moist_cal.v2, y2 = s_moist_cal.y2;
    float v3 = s_moist_cal.v3, y3 = s_moist_cal.y3;
    const bool has_v1 = nvs_get_f(h, "m_v1", &v1);
    const bool has_y1 = nvs_get_f(h, "m_y1", &y1);
    const bool has_v2 = nvs_get_f(h, "m_v2", &v2);
    const bool has_y2 = nvs_get_f(h, "m_y2", &y2);
    const bool has_v3 = nvs_get_f(h, "m_v3", &v3);
    const bool has_y3 = nvs_get_f(h, "m_y3", &y3);
    nvs_close(h);

    if (has_v1 && has_y1 && has_v2 && has_y2) {
        s_moist_cal.v1 = v1;
        s_moist_cal.y1 = y1;
        s_moist_cal.v2 = v2;
        s_moist_cal.y2 = y2;
        if (has_v3 && has_y3) {
            s_moist_cal.v3 = v3;
            s_moist_cal.y3 = y3;
        } else {
            /* Migrate old 2-point: synthesize mid. */
            s_moist_cal.v3 = v2;
            s_moist_cal.y3 = y2;
            s_moist_cal.v2 = 0.5f * (v1 + v2);
            s_moist_cal.y2 = 0.5f * (y1 + y2);
            ESP_LOGI(TAG, "Migrated 2-point moisture cal → synthesized mid");
        }
    }

    if (sensor_manager_validate_moisture_cal(&s_moist_cal) != ESP_OK) {
        ESP_LOGW(TAG, "Invalid NVS moisture cal — using defaults");
        sensor_manager_get_default_moisture_cal(&s_moist_cal);
    }

    ESP_LOGI(TAG, "Moisture cal [%.2fV→%.1f, %.2fV→%.1f, %.2fV→%.1f]",
             s_moist_cal.v1, s_moist_cal.y1, s_moist_cal.v2, s_moist_cal.y2,
             s_moist_cal.v3, s_moist_cal.y3);
    return ESP_OK;
}

esp_err_t sensor_manager_save_moisture_cal(const sensor_moisture_cal_t *cal)
{
    if (sensor_manager_validate_moisture_cal(cal) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }
    s_moist_cal = *cal;
    return nvs_write_moisture_cal(&s_moist_cal);
}

void sensor_manager_get_moisture_cal(sensor_moisture_cal_t *out)
{
    if (out) {
        *out = s_moist_cal;
    }
}

esp_err_t sensor_manager_init(void)
{
    s_data = {};
    sensor_manager_load_calibration();

    esp_err_t err = ads1115_init(RICE_I2C_ADDR_ADS1115);
    s_data.ads1115_ok = (err == ESP_OK);
    if (!s_data.ads1115_ok) {
        ESP_LOGW(TAG, "ADS1115 init soft-fail — moisture marked invalid until present");
    }
    return ESP_OK;
}

esp_err_t sensor_manager_poll(uint8_t relay_modbus_addr)
{
    sensor_data_t next {};
    next.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    next.ads1115_ok = ads1115_is_present();

    th_reading_t hotair {};
    th_reading_t chamber {};

    if (relay_modbus_addr != 0 && relay_modbus_addr == TH_SENSOR_HOTAIR_ADDR) {
        next.hot_air_temp_valid = false;
        next.hot_air_humidity_valid = false;
    } else if (th_sensor_read(TH_SENSOR_HOTAIR_ADDR, &hotair) == ESP_OK) {
        next.hot_air_temperature = hotair.temperature_c;
        next.hot_air_humidity = hotair.humidity_rh;
        next.hot_air_temp_valid = true;
        next.hot_air_humidity_valid = true;
    }

    if (relay_modbus_addr != 0 && relay_modbus_addr == TH_SENSOR_CHAMBER_ADDR) {
        next.chamber_temp_valid = false;
        next.chamber_humidity_valid = false;
    } else if (th_sensor_read(TH_SENSOR_CHAMBER_ADDR, &chamber) == ESP_OK) {
        next.chamber_temperature = chamber.temperature_c;
        next.chamber_humidity = chamber.humidity_rh;
        next.chamber_temp_valid = true;
        next.chamber_humidity_valid = true;
    }

    if (next.ads1115_ok) {
        float mv = 0.0f;
        if (ads1115_read_channel(ADS1115_CH_MOISTURE, nullptr, &mv) == ESP_OK) {
            next.moisture_voltage = mv;
            if (voltage_in_range(mv)) {
                next.moisture = apply_piecewise_3(s_moist_cal, mv);
                next.moisture_valid = true;
            }
        } else {
            next.ads1115_ok = false;
        }
    }

    s_data = next;
    return ESP_OK;
}

void sensor_manager_get(sensor_data_t *out)
{
    if (out) {
        *out = s_data;
    }
}
