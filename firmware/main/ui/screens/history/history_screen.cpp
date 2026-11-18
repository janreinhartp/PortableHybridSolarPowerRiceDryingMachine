#include "history_screen.h"

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "logger.h"
#include "screen_common.h"
#include "ui_manager.h"
#include "ui_theme.h"

static bool s_alive = false;
static lv_obj_t *s_empty = nullptr;
static lv_obj_t *s_list = nullptr;
static lv_obj_t *s_cards[LOGGER_HISTORY_MAX] = {};
static size_t s_shown = SIZE_MAX;
static char s_lines[LOGGER_HISTORY_MAX][220] = {};

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
    s_list = nullptr;
    s_shown = SIZE_MAX;
    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        s_cards[i] = nullptr;
        s_lines[i][0] = '\0';
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

    s_list = lv_obj_create(parent);
    lv_obj_set_size(s_list, 992, 232);
    lv_obj_set_pos(s_list, 0, 96);
    /* Opaque bg — transparent scroll areas redraw whatever is behind and flicker. */
    lv_obj_set_style_bg_color(s_list, ui_theme_color(UI_COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_all(s_list, 0, 0);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scrollbar_mode(s_list, LV_SCROLLBAR_MODE_AUTO);

    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        s_cards[i] = ui_theme_make_card(s_list, 992, 88);
        lv_obj_set_pos(s_cards[i], 0, (lv_coord_t)((int)i * 96));
        lv_obj_add_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *lbl = ui_theme_make_label(s_cards[i], "", ui_font_body(), UI_COLOR_TEXT);
        lv_obj_set_width(lbl, 960);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_user_data(s_cards[i], lbl);
        s_lines[i][0] = '\0';
    }
    s_shown = SIZE_MAX;

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
    /* Lock busy: keep current widgets. Returning 0 used to hide every card. */
    if (n == SIZE_MAX) {
        return;
    }

    ui_label_set_text_if_changed(s_empty, n == 0 ? "No Completed Batches On The SD Card Yet."
                                                 : "");

    bool any_change = (n != s_shown);
    char lines[LOGGER_HISTORY_MAX][220] = {};
    for (size_t i = 0; i < n; ++i) {
        char dry[16];
        ui_format_hms(dry, sizeof(dry), rows[i].drying_s);
        snprintf(lines[i], sizeof(lines[i]),
                 "%.15s     %.23s     %.1f -> %.1f %%     %.15s  Dry %s",
                 rows[i].batch_id, rows[i].end_time, rows[i].initial_moisture,
                 rows[i].final_moisture, rows[i].completion, dry);
        if (strcmp(s_lines[i], lines[i]) != 0) {
            any_change = true;
        }
    }
    if (!any_change) {
        return;
    }

    for (size_t i = 0; i < LOGGER_HISTORY_MAX; ++i) {
        lv_obj_t *lbl = s_cards[i] != nullptr ? (lv_obj_t *)lv_obj_get_user_data(s_cards[i])
                                              : nullptr;
        if (lbl == nullptr) {
            continue;
        }
        if (i >= n) {
            if (!lv_obj_has_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
            }
            s_lines[i][0] = '\0';
            continue;
        }
        if (lv_obj_has_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_clear_flag(s_cards[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (strcmp(s_lines[i], lines[i]) != 0) {
            memcpy(s_lines[i], lines[i], sizeof(s_lines[i]));
            s_lines[i][sizeof(s_lines[i]) - 1] = '\0';
            lv_label_set_text(lbl, s_lines[i]);
            const uint32_t color =
                (rows[i].completion[0] == 'C') ? UI_COLOR_TEAL
                : (rows[i].completion[0] == 'F') ? UI_COLOR_RED
                                                 : UI_COLOR_AMBER;
            lv_obj_set_style_border_color(s_cards[i], ui_theme_color(color), 0);
            lv_obj_set_style_border_width(s_cards[i], 2, 0);
        }
    }
    s_shown = n;
}
