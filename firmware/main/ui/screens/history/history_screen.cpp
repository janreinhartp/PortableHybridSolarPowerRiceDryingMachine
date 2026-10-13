#include "history_screen.h"

#include <cstdio>

#include "logger.h"
#include "screen_common.h"

static bool s_alive = false;
static lv_obj_t *s_empty = nullptr;
static lv_obj_t *s_rows[LOGGER_HISTORY_MAX] = {};

void history_detach(void)
{
    s_alive = false;
    s_empty = nullptr;
    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        s_rows[i] = nullptr;
    }
}

void history_build(lv_obj_t *parent, const system_status_t *status)
{
    ui_label(parent, "BATCH HISTORY", 8, 0, &lv_font_montserrat_24, 0xE8EEF4);
    s_empty = ui_label(parent, "", 8, 44, &lv_font_montserrat_16, 0x9BB0C4);
    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        s_rows[i] = ui_label(parent, "", 8, (lv_coord_t)(80 + (int)i * 42),
                             &lv_font_montserrat_14, 0xE8EEF4);
        lv_obj_set_width(s_rows[i], 960);
        lv_label_set_long_mode(s_rows[i], LV_LABEL_LONG_WRAP);
    }
    s_alive = true;
    history_update(status);
}

void history_update(const system_status_t *status)
{
    (void)status;
    if (!s_alive) {
        return;
    }

    logger_summary_t rows[LOGGER_HISTORY_MAX] = {};
    const size_t n = logger_copy_history(rows, LOGGER_HISTORY_MAX);
    if (n == 0) {
        lv_label_set_text(s_empty, "No completed batches on the SD card yet.");
    } else {
        lv_label_set_text(s_empty, "Newest first. Files are under /sdcard/summaries.");
    }

    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        if (s_rows[i] == nullptr) {
            continue;
        }
        if (i >= n) {
            lv_label_set_text(s_rows[i], "");
            continue;
        }
        char dry[16];
        ui_format_hms(dry, sizeof(dry), rows[i].drying_s);
        char line[512];
        snprintf(line, sizeof(line),
                 "%.15s   %.15s   %.1f -> %.1f %%   dry %.8s   max %.1f C   %.23s",
                 rows[i].batch_id, rows[i].completion, rows[i].initial_moisture,
                 rows[i].final_moisture, dry, rows[i].max_temperature, rows[i].end_time);
        lv_label_set_text(s_rows[i], line);
    }
}
