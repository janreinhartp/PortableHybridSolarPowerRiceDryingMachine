#include "history_screen.h"

#include <cstdio>

#include "logger.h"
#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;
static lv_obj_t *s_empty = nullptr;
static lv_obj_t *s_cards[LOGGER_HISTORY_MAX] = {};

static void on_back(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_MORE);
    }
}

static void on_home(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ui_manager_navigate(UI_SCREEN_DASHBOARD);
    }
}

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
    lv_obj_set_pos(title, 0, 0);

    lv_obj_t *hdr = ui_theme_make_card(parent, 992, 40);
    lv_obj_set_pos(hdr, 0, 48);
    lv_obj_t *h1 = ui_theme_make_label(hdr, "Batch", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_pos(h1, 0, 10);
    lv_obj_t *h2 = ui_theme_make_label(hdr, "Ended", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_pos(h2, 208, 10);
    lv_obj_t *h3 = ui_theme_make_label(hdr, "Moisture", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_pos(h3, 488, 10);
    lv_obj_t *h4 = ui_theme_make_label(hdr, "Result", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_pos(h4, 784, 10);

    s_empty = ui_theme_make_label(parent, "", ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_pos(s_empty, 0, 100);

    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, 992, 232);
    lv_obj_set_pos(list, 0, 96);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        s_cards[i] = ui_theme_make_card(list, 992, 88);
        lv_obj_set_pos(s_cards[i], 0, (lv_coord_t)((int)i * 96));
        lv_obj_t *lbl = ui_theme_make_label(s_cards[i], "", ui_font_body(), UI_COLOR_TEXT);
        lv_obj_set_width(lbl, 960);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_user_data(s_cards[i], lbl);
    }

    lv_obj_t *back =
        ui_theme_make_btn(parent, "Back To More", on_back, nullptr, 488, 64, UI_COLOR_SURFACE_ALT);
    lv_obj_set_pos(back, 0, 344);
    lv_obj_t *home =
        ui_theme_make_btn(parent, "Home", on_home, nullptr, 488, 64, UI_COLOR_TEAL);
    lv_obj_set_pos(home, 504, 344);

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
    lv_label_set_text(s_empty, n == 0 ? "No Completed Batches On The SD Card Yet." : "");
    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        lv_obj_t *lbl = s_cards[i] != nullptr ? (lv_obj_t *)lv_obj_get_user_data(s_cards[i])
                                              : nullptr;
        if (lbl == nullptr) {
            continue;
        }
        if (i >= n) {
            lv_label_set_text(lbl, "");
            lv_obj_add_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
        char dry[16];
        ui_format_hms(dry, sizeof(dry), rows[i].drying_s);
        char line[220];
        snprintf(line, sizeof(line),
                 "%.15s     %.23s     %.1f -> %.1f %%     %.15s  Dry %s",
                 rows[i].batch_id, rows[i].end_time, rows[i].initial_moisture,
                 rows[i].final_moisture, rows[i].completion, dry);
        lv_label_set_text(lbl, line);
        const uint32_t color =
            (rows[i].completion[0] == 'C') ? UI_COLOR_TEAL
            : (rows[i].completion[0] == 'F') ? UI_COLOR_RED
                                             : UI_COLOR_AMBER;
        lv_obj_set_style_border_color(s_cards[i], ui_theme_color(color), 0);
        lv_obj_set_style_border_width(s_cards[i], 2, 0);
    }
}
