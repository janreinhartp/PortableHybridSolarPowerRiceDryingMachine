#include "sensor_manager.h"

#include <cmath>
#include <cstring>

#include <cstdio>

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
static sensor_linear_cal_t s_moist_cal {0.0f, 0.0f, 5.0f, 100.0f};
static sensor_linear_cal_t s_ec_cal {0.0f, 0.0f, 5.0f, 5.0f};

static float apply_linear(const sensor_linear_cal_t &cal, float voltage)
{
    const float dv = cal.v2 - cal.v1;
    if (std::fabs(dv) < 1e-6f) {
        return cal.y1;
    }
    return cal.y1 + (voltage - cal.v1) * (cal.y2 - cal.y1) / dv;
}

static bool voltage_in_range(float v)
{
    /* Conditioned 0–5 V into ADS1115 ±6.144 V FSR; flag clearly impossible values. */
    return v >= -0.05f && v <= 5.5f;
}

static esp_err_t nvs_write_cal(const char *prefix, const sensor_linear_cal_t *cal)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    char key[16];
    snprintf(key, sizeof(key), "%s_v1", prefix);
    nvs_set_blob(h, key, &cal->v1, sizeof(float));
    snprintf(key, sizeof(key), "%s_y1", prefix);
    nvs_set_blob(h, key, &cal->y1, sizeof(float));
    snprintf(key, sizeof(key), "%s_v2", prefix);
    nvs_set_blob(h, key, &cal->v2, sizeof(float));
    snprintf(key, sizeof(key), "%s_y2", prefix);
    nvs_set_blob(h, key, &cal->y2, sizeof(float));
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static void nvs_read_cal(nvs_handle_t h, const char *prefix, sensor_linear_cal_t *cal)
{
    char key[16];
    size_t sz = sizeof(float);
    snprintf(key, sizeof(key), "%s_v1", prefix);
    nvs_get_blob(h, key, &cal->v1, &sz);
    sz = sizeof(float);
    snprintf(key, sizeof(key), "%s_y1", prefix);
    nvs_get_blob(h, key, &cal->y1, &sz);
    sz = sizeof(float);
    snprintf(key, sizeof(key), "%s_v2", prefix);
    nvs_get_blob(h, key, &cal->v2, &sz);
    sz = sizeof(float);
    snprintf(key, sizeof(key), "%s_y2", prefix);
    nvs_get_blob(h, key, &cal->y2, &sz);
}

esp_err_t sensor_manager_load_calibration(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No NVS calibration — using defaults 0V→0 / 5V→100%% moist, 0→0 / 5V→5 EC");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }
    nvs_read_cal(h, "m", &s_moist_cal);
    nvs_read_cal(h, "e", &s_ec_cal);
    nvs_close(h);
    ESP_LOGI(TAG, "Loaded calibration moist[%.2fV→%.1f .. %.2fV→%.1f] ec[%.2fV→%.2f .. %.2fV→%.2f]",
             s_moist_cal.v1, s_moist_cal.y1, s_moist_cal.v2, s_moist_cal.y2,
             s_ec_cal.v1, s_ec_cal.y1, s_ec_cal.v2, s_ec_cal.y2);
    return ESP_OK;
}

esp_err_t sensor_manager_save_moisture_cal(const sensor_linear_cal_t *cal)
{
    if (cal == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    s_moist_cal = *cal;
    return nvs_write_cal("m", &s_moist_cal);
}

esp_err_t sensor_manager_save_ec_cal(const sensor_linear_cal_t *cal)
{
    if (cal == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    s_ec_cal = *cal;
    return nvs_write_cal("e", &s_ec_cal);
}

void sensor_manager_get_moisture_cal(sensor_linear_cal_t *out)
{
    if (out) {
        *out = s_moist_cal;
    }
}

void sensor_manager_get_ec_cal(sensor_linear_cal_t *out)
{
    if (out) {
        *out = s_ec_cal;
    }
}

esp_err_t sensor_manager_init(void)
{
    s_data = {};
    sensor_manager_load_calibration();

    esp_err_t err = ads1115_init(RICE_I2C_ADDR_ADS1115);
    s_data.ads1115_ok = (err == ESP_OK);
    if (!s_data.ads1115_ok) {
        ESP_LOGW(TAG, "ADS1115 init soft-fail — moisture/EC marked invalid until present");
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
        float ev = 0.0f;
        if (ads1115_read_channel(ADS1115_CH_MOISTURE, nullptr, &mv) == ESP_OK) {
            next.moisture_voltage = mv;
            if (voltage_in_range(mv)) {
                next.moisture = apply_linear(s_moist_cal, mv);
                next.moisture_valid = true;
            }
        } else {
            next.ads1115_ok = false;
        }
        if (next.ads1115_ok && ads1115_read_channel(ADS1115_CH_EC, nullptr, &ev) == ESP_OK) {
            next.ec_voltage = ev;
            if (voltage_in_range(ev)) {
                next.ec = apply_linear(s_ec_cal, ev);
                next.ec_valid = true;
            }
        } else if (next.ads1115_ok) {
            next.ads1115_ok = ads1115_is_present();
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
