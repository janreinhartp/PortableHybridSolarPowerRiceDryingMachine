#include "settings_manager.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "settings_mgr";
static const char *NVS_NS = "dryer_set";
/* Bump key when temporary bench timings change so old NVS values are not reused. */
static const char *NVS_KEY = "cfg_bench";

static dryer_settings_t s_settings {};

void settings_manager_get_defaults(dryer_settings_t *out)
{
    if (out == nullptr) {
        return;
    }
    *out = {};
    out->target_moisture_pct = 14.0f;
    out->drying_temperature_c = 50.0f;
    out->temperature_hysteresis_c = 2.0f;
    out->cooling_temperature_c = 35.0f;
    out->circulation_mode = CIRCULATION_MODE_INTERVAL;
    /* Temporary bench timing: 10 s period, elevator ON 3 s (was 120 / 10). */
    out->circulation_interval_s = 10;
    out->circulation_run_time_s = 3;
    out->discharge_time_s = 60;
    out->min_drying_time_s = 300;
    out->max_drying_time_s = 28800;
    out->max_hot_air_temperature_c = 70.0f;
}

esp_err_t settings_manager_validate(const dryer_settings_t *in)
{
    if (in == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (in->target_moisture_pct <= 0.0f || in->target_moisture_pct > 40.0f) {
        ESP_LOGW(TAG, "reject: target moisture out of range");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->drying_temperature_c < 20.0f || in->drying_temperature_c > 65.0f) {
        ESP_LOGW(TAG, "reject: drying temperature out of safe limits");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->temperature_hysteresis_c <= 0.0f || in->temperature_hysteresis_c > 20.0f) {
        ESP_LOGW(TAG, "reject: hysteresis invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->cooling_temperature_c < 10.0f || in->cooling_temperature_c > 60.0f) {
        ESP_LOGW(TAG, "reject: cooling temperature out of safe limits");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->cooling_temperature_c >= in->drying_temperature_c) {
        ESP_LOGW(TAG, "reject: cooling >= drying");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->max_hot_air_temperature_c <= in->drying_temperature_c ||
        in->max_hot_air_temperature_c > 90.0f) {
        ESP_LOGW(TAG, "reject: max hot-air temperature out of safe limits");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->circulation_mode != CIRCULATION_MODE_CONTINUOUS &&
        in->circulation_mode != CIRCULATION_MODE_INTERVAL) {
        ESP_LOGW(TAG, "reject: circulation mode invalid");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->circulation_interval_s == 0 || in->circulation_run_time_s == 0 ||
        in->discharge_time_s == 0 || in->min_drying_time_s == 0 ||
        in->max_drying_time_s == 0) {
        ESP_LOGW(TAG, "reject: timer <= 0");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->circulation_run_time_s >= in->circulation_interval_s) {
        ESP_LOGW(TAG, "reject: circulation run time >= interval");
        return ESP_ERR_INVALID_ARG;
    }
    if (in->max_drying_time_s <= in->min_drying_time_s) {
        ESP_LOGW(TAG, "reject: max drying time <= min drying time");
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

esp_err_t settings_manager_load(void)
{
    dryer_settings_t defaults {};
    settings_manager_get_defaults(&defaults);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_settings = defaults;
        ESP_LOGI(TAG, "No NVS settings — using defaults");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        s_settings = defaults;
        return err;
    }

    dryer_settings_t loaded {};
    size_t sz = sizeof(loaded);
    err = nvs_get_blob(h, NVS_KEY, &loaded, &sz);
    nvs_close(h);

    if (err != ESP_OK || sz != sizeof(loaded) ||
        settings_manager_validate(&loaded) != ESP_OK) {
        ESP_LOGW(TAG, "NVS settings missing/invalid — using defaults");
        s_settings = defaults;
        return ESP_OK;
    }

    s_settings = loaded;
    ESP_LOGI(TAG,
             "Loaded settings moist=%.1f dry=%.1f cool=%.1f circ=%u/%us mode=%d",
             s_settings.target_moisture_pct, s_settings.drying_temperature_c,
             s_settings.cooling_temperature_c,
             (unsigned)s_settings.circulation_run_time_s,
             (unsigned)s_settings.circulation_interval_s,
             (int)s_settings.circulation_mode);
    return ESP_OK;
}

esp_err_t settings_manager_save(const dryer_settings_t *in)
{
    esp_err_t err = settings_manager_validate(in);
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t h;
    err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(h, NVS_KEY, in, sizeof(*in));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        return err;
    }

    s_settings = *in;
    ESP_LOGI(TAG, "Settings saved to NVS");
    return ESP_OK;
}

void settings_manager_get(dryer_settings_t *out)
{
    if (out) {
        *out = s_settings;
    }
}

esp_err_t settings_manager_init(void)
{
    return settings_manager_load();
}
