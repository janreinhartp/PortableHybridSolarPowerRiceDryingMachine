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

void more_detach(void)
{
    s_alive = false;
}

void more_build(lv_obj_t *parent, const system_status_t *status)
{
    (void)status;
    lv_obj_t *title =
        ui_theme_make_label(parent, "More", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_set_pos(title, 8, 4);
    lv_obj_t *sub = ui_theme_make_label(parent, "Technician And Maintenance Pages",
                                        ui_font_body(), UI_COLOR_MUTED);
    lv_obj_set_pos(sub, 8, 40);

    struct {
        const char *text;
        ui_screen_t screen;
        uint32_t color;
        lv_coord_t x;
        lv_coord_t y;
    } items[] = {
        {"Settings", UI_SCREEN_SETTINGS, UI_COLOR_ACCENT, 8, 100},
        {"History", UI_SCREEN_HISTORY, UI_COLOR_TEAL, 260, 100},
        {"Manual", UI_SCREEN_MANUAL, UI_COLOR_ORANGE, 512, 100},
        {"System", UI_SCREEN_SYSTEM, UI_COLOR_SURFACE_ALT, 764, 100},
    };
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        lv_obj_t *btn = ui_theme_make_btn(parent, items[i].text, go,
                                          (void *)(intptr_t)items[i].screen, 230, 120,
                                          items[i].color);
        lv_obj_set_pos(btn, items[i].x, items[i].y);
    }
    s_alive = true;
}

void more_update(const system_status_t *status)
{
    (void)status;
    (void)s_alive;
}
