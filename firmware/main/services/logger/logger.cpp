#include "logger.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "logger";

static const char *DIR_BATCHES = "/sdcard/batches";
static const char *DIR_SUMMARIES = "/sdcard/summaries";
static const char *DIR_LOGS = "/sdcard/logs";
static const char *DIR_CONFIG = "/sdcard/config";
static const char *PATH_INDEX = "/sdcard/summaries/index.csv";
static const char *PATH_ALARMS = "/sdcard/logs/alarms.csv";
static const char *PATH_SYSTEM = "/sdcard/logs/system.log";

typedef enum {
    LOG_KIND_SAMPLE = 1,
    LOG_KIND_SUMMARY = 2,
    LOG_KIND_ALARM = 3,
} log_kind_t;

typedef struct {
    log_kind_t kind;
    union {
        logger_sample_t sample;
        logger_summary_t summary;
        logger_alarm_t alarm;
    } data;
} log_queue_item_t;

static QueueHandle_t s_queue = nullptr;
static SemaphoreHandle_t s_mutex = nullptr;
static volatile bool s_batch_log_ok = false;
static uint32_t s_dropped = 0;

static FILE *s_batch_file = nullptr;
static char s_open_batch[16] = {};
static char s_last_summary_id[16] = {};

static logger_summary_t s_history[LOGGER_HISTORY_MAX] = {};
static size_t s_history_count = 0;
static logger_alarm_t s_alarms[LOGGER_ALARM_MAX] = {};
static size_t s_alarm_count = 0;

static void ensure_dir(const char *path)
{
    if (mkdir(path, 0775) != 0 && errno != EEXIST) {
        ESP_LOGW(TAG, "mkdir %s failed errno=%d", path, errno);
        s_batch_log_ok = false;
    }
}

static void close_batch_file(void)
{
    if (s_batch_file != nullptr) {
        fflush(s_batch_file);
        fclose(s_batch_file);
        s_batch_file = nullptr;
    }
    s_open_batch[0] = '\0';
}

static bool batch_id_ok(const char *batch_id)
{
    if (batch_id == nullptr || batch_id[0] == '\0') {
        return false;
    }
    return strchr(batch_id, '/') == nullptr && strchr(batch_id, '\\') == nullptr;
}

static esp_err_t open_batch_file(const char *batch_id)
{
    if (!batch_id_ok(batch_id)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_batch_file != nullptr && strcmp(s_open_batch, batch_id) == 0) {
        return ESP_OK;
    }

    close_batch_file();

    char path[64];
    snprintf(path, sizeof(path), "/sdcard/batches/%s.csv", batch_id);
    s_batch_file = fopen(path, "a");
    if (s_batch_file == nullptr) {
        ESP_LOGW(TAG, "open %s failed", path);
        s_batch_log_ok = false;
        return ESP_FAIL;
    }

    strncpy(s_open_batch, batch_id, sizeof(s_open_batch) - 1);
    if (fseek(s_batch_file, 0, SEEK_END) == 0 && ftell(s_batch_file) == 0) {
        fprintf(s_batch_file,
                "Timestamp,Batch ID,State,Moisture,EC,Hot Air Temperature,Hot Air Humidity,"
                "Chamber Temperature,Chamber Humidity,Heater,Fan,Elevator,Door,Fault Code\n");
    }
    return ESP_OK;
}

static void push_history_locked(const logger_summary_t *summary)
{
    if (s_history_count < LOGGER_HISTORY_MAX) {
        s_history_count++;
    }
    if (s_history_count > 1) {
        memmove(&s_history[1], &s_history[0],
                (s_history_count - 1) * sizeof(s_history[0]));
    }
    s_history[0] = *summary;
}

static void push_alarm_locked(const logger_alarm_t *alarm)
{
    if (s_alarm_count < LOGGER_ALARM_MAX) {
        s_alarm_count++;
    }
    if (s_alarm_count > 1) {
        memmove(&s_alarms[1], &s_alarms[0], (s_alarm_count - 1) * sizeof(s_alarms[0]));
    }
    s_alarms[0] = *alarm;
}

static bool parse_summary_line(const char *line, logger_summary_t *out)
{
    unsigned drying = 0;
    unsigned cooling = 0;
    unsigned discharge = 0;
    logger_summary_t row {};
    const int n = sscanf(
        line,
        "%15[^,],%23[^,],%23[^,],%f,%f,%f,%f,%f,%f,%u,%u,%u,%15[^,],%39[^\r\n]",
        row.batch_id, row.start_time, row.end_time, &row.initial_moisture,
        &row.final_moisture, &row.target_moisture, &row.max_temperature,
        &row.avg_temperature, &row.final_ec, &drying, &cooling, &discharge,
        row.completion, row.fault);
    if (n != 14) {
        return false;
    }
    row.drying_s = drying;
    row.cooling_s = cooling;
    row.discharge_s = discharge;
    *out = row;
    return true;
}

static void load_history(void)
{
    FILE *f = fopen(PATH_INDEX, "r");
    if (f == nullptr) {
        return;
    }

    char line[256];
    /* Skip header. */
    if (fgets(line, sizeof(line), f) == nullptr) {
        fclose(f);
        return;
    }

    while (fgets(line, sizeof(line), f) != nullptr) {
        logger_summary_t row {};
        if (!parse_summary_line(line, &row)) {
            continue;
        }
        if (s_mutex != nullptr && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            push_history_locked(&row);
            xSemaphoreGive(s_mutex);
        }
    }
    fclose(f);
}

static void load_alarms(void)
{
    FILE *f = fopen(PATH_ALARMS, "r");
    if (f == nullptr) {
        return;
    }

    char line[256];
    if (fgets(line, sizeof(line), f) == nullptr) {
        fclose(f);
        return;
    }

    while (fgets(line, sizeof(line), f) != nullptr) {
        logger_alarm_t row {};
        const int n = sscanf(line, "%23[^,],%15[^,],%39[^,],%95[^\r\n]", row.timestamp,
                             row.state, row.code, row.detail);
        if (n != 4) {
            continue;
        }
        if (s_mutex != nullptr && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            push_alarm_locked(&row);
            xSemaphoreGive(s_mutex);
        }
    }
    fclose(f);
}

static void write_sample(const logger_sample_t *sample)
{
    if (open_batch_file(sample->batch_id) != ESP_OK) {
        return;
    }

    const int wrote = fprintf(
        s_batch_file,
        "%s,%s,%s,%.2f,%.3f,%.2f,%.2f,%.2f,%.2f,%u,%u,%u,%u,%s\n",
        sample->timestamp, sample->batch_id, sample->state, sample->moisture, sample->ec,
        sample->hot_air_temperature, sample->hot_air_humidity, sample->chamber_temperature,
        sample->chamber_humidity, (unsigned)sample->heater, (unsigned)sample->fan,
        (unsigned)sample->elevator, (unsigned)sample->door, sample->fault);
    if (wrote < 0 || fflush(s_batch_file) != 0) {
        ESP_LOGW(TAG, "batch CSV write failed");
        s_batch_log_ok = false;
        close_batch_file();
        return;
    }
    s_batch_log_ok = true;
}

static void write_summary(const logger_summary_t *summary)
{
    if (!batch_id_ok(summary->batch_id)) {
        s_batch_log_ok = false;
        return;
    }
    if (strcmp(s_last_summary_id, summary->batch_id) == 0) {
        return;
    }

    if (s_batch_file != nullptr && strcmp(s_open_batch, summary->batch_id) == 0) {
        close_batch_file();
    }

    char path[64];
    snprintf(path, sizeof(path), "/sdcard/summaries/%s.txt", summary->batch_id);
    FILE *txt = fopen(path, "w");
    if (txt == nullptr) {
        ESP_LOGW(TAG, "summary open failed %s", path);
        s_batch_log_ok = false;
        return;
    }

    const unsigned total =
        summary->drying_s + summary->cooling_s + summary->discharge_s;
    const int wrote = fprintf(
        txt,
        "Batch ID: %s\n"
        "Start Time: %s\n"
        "End Time: %s\n"
        "Initial Moisture: %.2f %%\n"
        "Final Moisture: %.2f %%\n"
        "Target Moisture: %.2f %%\n"
        "Maximum Temperature: %.2f C\n"
        "Average Temperature: %.2f C\n"
        "Drying Duration: %u s\n"
        "Cooling Duration: %u s\n"
        "Discharge Duration: %u s\n"
        "Total Batch Duration: %u s\n"
        "Final EC: %.3f\n"
        "Completion Status: %s\n"
        "Fault Status: %s\n",
        summary->batch_id, summary->start_time, summary->end_time,
        summary->initial_moisture, summary->final_moisture, summary->target_moisture,
        summary->max_temperature, summary->avg_temperature, (unsigned)summary->drying_s,
        (unsigned)summary->cooling_s, (unsigned)summary->discharge_s, total,
        summary->final_ec, summary->completion, summary->fault);
    const bool txt_ok = wrote > 0 && fflush(txt) == 0;
    fclose(txt);
    if (!txt_ok) {
        ESP_LOGW(TAG, "summary write failed %s", summary->batch_id);
        s_batch_log_ok = false;
        return;
    }

    FILE *index = fopen(PATH_INDEX, "a");
    if (index == nullptr) {
        s_batch_log_ok = false;
        return;
    }
    if (fseek(index, 0, SEEK_END) == 0 && ftell(index) == 0) {
        fprintf(index,
                "batch_id,start,end,initial_moisture,final_moisture,target_moisture,"
                "max_temp,avg_temp,final_ec,drying_s,cooling_s,discharge_s,completion,fault\n");
    }
    const int index_wrote = fprintf(
        index, "%s,%s,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,%u,%u,%u,%s,%s\n",
        summary->batch_id, summary->start_time, summary->end_time, summary->initial_moisture,
        summary->final_moisture, summary->target_moisture, summary->max_temperature,
        summary->avg_temperature, summary->final_ec, (unsigned)summary->drying_s,
        (unsigned)summary->cooling_s, (unsigned)summary->discharge_s, summary->completion,
        summary->fault);
    const bool index_ok = index_wrote > 0 && fflush(index) == 0;
    fclose(index);
    if (!index_ok) {
        s_batch_log_ok = false;
        return;
    }

    FILE *sys = fopen(PATH_SYSTEM, "a");
    if (sys != nullptr) {
        fprintf(sys, "%s %s %s moist %.2f->%.2f fault %s\n", summary->end_time,
                summary->batch_id, summary->completion, summary->initial_moisture,
                summary->final_moisture, summary->fault);
        fflush(sys);
        fclose(sys);
    }

    strncpy(s_last_summary_id, summary->batch_id, sizeof(s_last_summary_id) - 1);
    s_batch_log_ok = true;
    if (s_mutex != nullptr && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        push_history_locked(summary);
        xSemaphoreGive(s_mutex);
    }
    ESP_LOGI(TAG, "Summary stored %s (%s)", summary->batch_id, summary->completion);
}

static void write_alarm(const logger_alarm_t *alarm)
{
    FILE *f = fopen(PATH_ALARMS, "a");
    if (f == nullptr) {
        ESP_LOGW(TAG, "alarm log open failed");
    } else {
        if (fseek(f, 0, SEEK_END) == 0 && ftell(f) == 0) {
            fprintf(f, "timestamp,state,code,detail\n");
        }
        fprintf(f, "%s,%s,%s,%s\n", alarm->timestamp, alarm->state, alarm->code,
                alarm->detail);
        fflush(f);
        fclose(f);
    }

    if (s_mutex != nullptr && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        push_alarm_locked(alarm);
        xSemaphoreGive(s_mutex);
    }
}

static void logger_task(void *arg)
{
    (void)arg;
    ensure_dir(DIR_BATCHES);
    ensure_dir(DIR_SUMMARIES);
    ensure_dir(DIR_LOGS);
    ensure_dir(DIR_CONFIG);
    load_history();
    load_alarms();

    log_queue_item_t item {};
    while (true) {
        if (xQueueReceive(s_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        switch (item.kind) {
        case LOG_KIND_SAMPLE:
            write_sample(&item.data.sample);
            break;
        case LOG_KIND_SUMMARY:
            write_summary(&item.data.summary);
            break;
        case LOG_KIND_ALARM:
            write_alarm(&item.data.alarm);
            break;
        }
    }
}

static void enqueue(const log_queue_item_t *item)
{
    if (s_queue == nullptr || item == nullptr) {
        return;
    }
    if (xQueueSend(s_queue, item, 0) != pdTRUE) {
        s_dropped++;
        if ((s_dropped % 10u) == 1u) {
            ESP_LOGW(TAG, "log queue full, dropped=%u", (unsigned)s_dropped);
        }
    }
}

esp_err_t logger_init(bool sd_mounted)
{
    static bool s_started = false;
    if (s_started) {
        return ESP_OK;
    }

    s_batch_log_ok = sd_mounted;
    s_dropped = 0;
    s_history_count = 0;
    s_alarm_count = 0;
    s_last_summary_id[0] = '\0';

    if (s_queue == nullptr) {
        s_queue = xQueueCreate(16, sizeof(log_queue_item_t));
    }
    if (s_mutex == nullptr) {
        s_mutex = xSemaphoreCreateMutex();
    }
    if (s_queue == nullptr || s_mutex == nullptr) {
        ESP_LOGE(TAG, "logger queue/mutex alloc failed");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(logger_task, "logger", 8192, nullptr, 2, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "logger task create failed");
        s_batch_log_ok = false;
        return ESP_FAIL;
    }

    s_started = true;
    ESP_LOGI(TAG, "Logger task started sd=%s", sd_mounted ? "mounted" : "absent");
    return ESP_OK;
}

void logger_submit_sample(const logger_sample_t *sample)
{
    if (sample == nullptr) {
        return;
    }
    log_queue_item_t item {};
    item.kind = LOG_KIND_SAMPLE;
    item.data.sample = *sample;
    enqueue(&item);
}

void logger_submit_summary(const logger_summary_t *summary)
{
    if (summary == nullptr) {
        return;
    }
    log_queue_item_t item {};
    item.kind = LOG_KIND_SUMMARY;
    item.data.summary = *summary;
    enqueue(&item);
}

void logger_submit_alarm(const logger_alarm_t *alarm)
{
    if (alarm == nullptr) {
        return;
    }
    log_queue_item_t item {};
    item.kind = LOG_KIND_ALARM;
    item.data.alarm = *alarm;
    enqueue(&item);
}

bool logger_is_healthy(void)
{
    return s_batch_log_ok;
}

uint32_t logger_dropped_count(void)
{
    return s_dropped;
}

size_t logger_copy_history(logger_summary_t *out, size_t max_entries)
{
    if (out == nullptr || max_entries == 0 || s_mutex == nullptr) {
        return 0;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return 0;
    }
    size_t n = s_history_count;
    if (n > max_entries) {
        n = max_entries;
    }
    if (n > 0) {
        memcpy(out, s_history, n * sizeof(*out));
    }
    xSemaphoreGive(s_mutex);
    return n;
}

size_t logger_copy_alarms(logger_alarm_t *out, size_t max_entries)
{
    if (out == nullptr || max_entries == 0 || s_mutex == nullptr) {
        return 0;
    }
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return 0;
    }
    size_t n = s_alarm_count;
    if (n > max_entries) {
        n = max_entries;
    }
    if (n > 0) {
        memcpy(out, s_alarms, n * sizeof(*out));
    }
    xSemaphoreGive(s_mutex);
    return n;
}
