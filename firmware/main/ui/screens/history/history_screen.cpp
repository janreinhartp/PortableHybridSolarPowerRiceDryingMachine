#include "history_screen.h"

#include <cstdio>

#include "logger.h"
#include "screen_common.h"

static bool s_alive = false;
static lv_obj_t *s_empty = nullptr;
static lv_obj_t *s_cards[LOGGER_HISTORY_MAX] = {};

void history_detach(void)
{
    s_alive = false;
    s_empty = nullptr;
    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        s_cards[i] = nullptr;
    }
}

void history_build(lv_obj_t *parent, const system_status_t *status)
{
    lv_obj_t *title =
        ui_theme_make_label(parent, "Batch History", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 8, 4);
    s_empty = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_pos(s_empty, 8, 40);
    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        s_cards[i] = ui_theme_make_card(parent, 980, 40);
        lv_obj_set_pos(s_cards[i], 8, (lv_coord_t)(72 + (int)i * 44));
        lv_obj_t *lbl = ui_theme_make_label(s_cards[i], "", ui_font_body(), UI_COLOR_TEXT);
        lv_obj_set_width(lbl, 940);
        lv_obj_set_user_data(s_cards[i], lbl);
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
    lv_label_set_text(s_empty, n == 0 ? "No Completed Batches On The SD Card Yet."
                                      : "Newest First. Files Under /sdcard/summaries.");
    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        lv_obj_t *lbl = s_cards[i] != nullptr ? (lv_obj_t *)lv_obj_get_user_data(s_cards[i])
                                              : nullptr;
        if (lbl == nullptr) {
            continue;
        }
        if (i >= n) {
            lv_label_set_text(lbl, "");
            continue;
        }
        char dry[16];
        ui_format_hms(dry, sizeof(dry), rows[i].drying_s);
        char line[220];
        snprintf(line, sizeof(line),
                 "%.15s   %.15s   %.1f -> %.1f %%   Dry %.8s   Max %.1f C   %.23s",
                 rows[i].batch_id, rows[i].completion, rows[i].initial_moisture,
                 rows[i].final_moisture, dry, rows[i].max_temperature, rows[i].end_time);
        lv_label_set_text(lbl, line);
        const uint32_t color =
            (rows[i].completion[0] == 'C') ? UI_COLOR_TEAL
            : (rows[i].completion[0] == 'F') ? UI_COLOR_RED
                                             : UI_COLOR_AMBER;
        lv_obj_set_style_border_color(s_cards[i], ui_theme_color(color), 0);
        lv_obj_set_style_border_width(s_cards[i], 2, 0);
    }
}
