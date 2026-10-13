#include "batch_manager.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "batch_mgr";
static const char *NVS_NS = "batch_mgr";
static const char *NVS_KEY_SEQ = "seq";
static const char *NVS_KEY_RECOVERY = "recovery";

typedef struct {
    uint8_t magic; /* 0xB6 */
    uint8_t state;
    char batch_id[16];
    float moisture;
    float chamber_temp_c;
    uint32_t saved_at_s;
} batch_recovery_blob_t;

static batch_status_t s_status {};

static uint32_t now_s(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

static esp_err_t nvs_write_seq(uint32_t seq)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_u32(h, NVS_KEY_SEQ, seq);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

static uint32_t nvs_read_seq(void)
{
    nvs_handle_t h;
    uint32_t seq = 0;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return 0;
    }
    nvs_get_u32(h, NVS_KEY_SEQ, &seq);
    nvs_close(h);
    return seq;
}

static esp_err_t nvs_clear_recovery(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_erase_key(h, NVS_KEY_RECOVERY);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t batch_manager_init(void)
{
    s_status = {};
    ESP_LOGI(TAG, "Batch Manager ready");
    return batch_manager_load_recovery();
}

esp_err_t batch_manager_load_recovery(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_status.interrupted_pending = false;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    batch_recovery_blob_t blob {};
    size_t sz = sizeof(blob);
    err = nvs_get_blob(h, NVS_KEY_RECOVERY, &blob, &sz);
    nvs_close(h);

    if (err != ESP_OK || sz != sizeof(blob) || blob.magic != 0xB6) {
        s_status.interrupted_pending = false;
        return ESP_OK;
    }

    s_status.interrupted_pending = true;
    strncpy(s_status.interrupted_batch_id, blob.batch_id,
            sizeof(s_status.interrupted_batch_id) - 1);
    s_status.interrupted_state = (batch_process_state_t)blob.state;
    s_status.interrupted_moisture = blob.moisture;
    s_status.interrupted_chamber_temp_c = blob.chamber_temp_c;

    ESP_LOGW(TAG, "Interrupted batch %s state=%u moist=%.1f chamber=%.1f",
             s_status.interrupted_batch_id, (unsigned)blob.state, blob.moisture,
             blob.chamber_temp_c);
    return ESP_OK;
}

bool batch_manager_interrupted_pending(void)
{
    return s_status.interrupted_pending;
}

void batch_manager_get_status(batch_status_t *out)
{
    if (out) {
        *out = s_status;
    }
}

esp_err_t batch_manager_create(const sensor_data_t *sensors)
{
    if (sensors == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_status.interrupted_pending) {
        ESP_LOGW(TAG, "Cannot create batch — interrupted pending ACK");
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t seq = nvs_read_seq() + 1;
    if (nvs_write_seq(seq) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to persist batch sequence");
    }

    s_status = {};
    snprintf(s_status.batch_id, sizeof(s_status.batch_id), "BATCH-%04u",
             (unsigned)(seq % 10000u));
    s_status.active = true;
    s_status.completed = false;
    s_status.last_state = BATCH_STATE_PRECHECK;
    s_status.start_time_s = now_s();
    s_status.initial_moisture = sensors->moisture;
    s_status.initial_chamber_temp_c = sensors->chamber_temperature;
    s_status.max_chamber_temp_c = sensors->chamber_temperature;

    ESP_LOGI(TAG, "Created %s moist=%.1f chamber=%.1f", s_status.batch_id,
             s_status.initial_moisture, s_status.initial_chamber_temp_c);
    return ESP_OK;
}

void batch_manager_set_state(batch_process_state_t state)
{
    s_status.last_state = state;
}

void batch_manager_update_elapsed(uint32_t drying_s, uint32_t cooling_s, uint32_t discharge_s)
{
    s_status.drying_elapsed_s = drying_s;
    s_status.cooling_elapsed_s = cooling_s;
    s_status.discharge_elapsed_s = discharge_s;
}

void batch_manager_note_chamber_temp(float chamber_temp_c)
{
    if (chamber_temp_c > s_status.max_chamber_temp_c) {
        s_status.max_chamber_temp_c = chamber_temp_c;
    }
}

esp_err_t batch_manager_save_recovery(batch_process_state_t state,
                                      float moisture,
                                      float chamber_temp_c)
{
    if (!s_status.active || s_status.batch_id[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    batch_recovery_blob_t blob {};
    blob.magic = 0xB6;
    blob.state = (uint8_t)state;
    strncpy(blob.batch_id, s_status.batch_id, sizeof(blob.batch_id) - 1);
    blob.moisture = moisture;
    blob.chamber_temp_c = chamber_temp_c;
    blob.saved_at_s = now_s();

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(h, NVS_KEY_RECOVERY, &blob, sizeof(blob));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);

    s_status.interrupted_pending = false; /* active run owns the flag until reboot */
    return err;
}

esp_err_t batch_manager_complete(const sensor_data_t *sensors)
{
    if (!s_status.active) {
        return ESP_ERR_INVALID_STATE;
    }
    if (sensors) {
        s_status.final_moisture = sensors->moisture;
        s_status.final_ec = sensors->ec;
        s_status.final_chamber_temp_c = sensors->chamber_temperature;
        batch_manager_note_chamber_temp(sensors->chamber_temperature);
    }

    s_status.active = false;
    s_status.completed = true;
    s_status.last_state = BATCH_STATE_COMPLETE;
    nvs_clear_recovery();
    s_status.interrupted_pending = false;

    ESP_LOGI(TAG,
             "Complete %s dry=%us cool=%us discharge=%us moist=%.1f->%.1f maxT=%.1f",
             s_status.batch_id, (unsigned)s_status.drying_elapsed_s,
             (unsigned)s_status.cooling_elapsed_s,
             (unsigned)s_status.discharge_elapsed_s, s_status.initial_moisture,
             s_status.final_moisture, s_status.max_chamber_temp_c);
    return ESP_OK;
}

esp_err_t batch_manager_acknowledge_interrupted(void)
{
    if (!s_status.interrupted_pending) {
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Interrupted batch %s acknowledged/discarded",
             s_status.interrupted_batch_id);
    nvs_clear_recovery();
    s_status.interrupted_pending = false;
    memset(s_status.interrupted_batch_id, 0, sizeof(s_status.interrupted_batch_id));
    return ESP_OK;
}

esp_err_t batch_manager_abort(void)
{
    if (s_status.active) {
        ESP_LOGW(TAG, "Aborting active batch %s", s_status.batch_id);
        s_status.active = false;
        s_status.completed = false;
        s_status.last_state = BATCH_STATE_IDLE;
    }
    nvs_clear_recovery();
    s_status.interrupted_pending = false;
    return ESP_OK;
}
