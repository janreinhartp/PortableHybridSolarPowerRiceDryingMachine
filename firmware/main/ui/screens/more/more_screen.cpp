#include "more_screen.h"

#include "screen_common.h"
#include "ui_manager.h"

static bool s_alive = false;

static void go(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const int id = (int)(intptr_t)lv_event_get_user_data(e);
    ui_manager_navigate((ui_screen_t)id);
}

static void make_tile(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *title,
                      const char *subtitle, ui_screen_t screen, const char *symbol)
{
    lv_obj_t *card = ui_theme_make_card(parent, 488, 160);
    lv_obj_set_pos(card, x, y);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, go, LV_EVENT_CLICKED, (void *)(intptr_t)screen);

    lv_obj_t *icon = lv_obj_create(card);
    lv_obj_set_size(icon, 56, 56);
    lv_obj_set_style_bg_color(icon, ui_theme_color(UI_COLOR_SURFACE_ALT), 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_radius(icon, 8, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *sym = ui_theme_make_label(icon, symbol, ui_font_title(), UI_COLOR_TEAL);
    lv_obj_center(sym);
    lv_obj_clear_flag(sym, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *t = ui_theme_make_label(card, title, ui_font_title(), UI_COLOR_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 72, 36);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *s = ui_theme_make_label(card, subtitle, ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_width(s, 340);
    lv_label_set_long_mode(s, LV_LABEL_LONG_DOT);
    lv_obj_align(s, LV_ALIGN_TOP_LEFT, 72, 76);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *chev = ui_theme_make_label(card, LV_SYMBOL_RIGHT, ui_font_body(), UI_COLOR_DIM);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(chev, LV_OBJ_FLAG_CLICKABLE);
}

void more_detach(void)
{
    s_alive = false;
}

void more_build(lv_obj_t *parent, const system_status_t *status)
{
    (void)status;
    lv_obj_t *title = ui_theme_make_label(parent, "More", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 0, 0);

    make_tile(parent, 0, 48, "Settings", "Batch parameters and setpoints", UI_SCREEN_SETTINGS,
              LV_SYMBOL_SETTINGS);
    make_tile(parent, 504, 48, "History", "Completed batch summaries", UI_SCREEN_HISTORY,
              LV_SYMBOL_LIST);
    make_tile(parent, 0, 224, "Manual", "Technician actuator control", UI_SCREEN_MANUAL,
              LV_SYMBOL_EDIT);
    make_tile(parent, 504, 224, "System", "Health, network, and commission", UI_SCREEN_SYSTEM,
              LV_SYMBOL_GPS);

    s_alive = true;
}

void more_update(const system_status_t *status)
{
    (void)status;
    (void)s_alive;
}
