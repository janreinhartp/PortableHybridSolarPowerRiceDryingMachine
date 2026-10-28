#include "boot_screen.h"

#include "lvgl.h"
#include "ui_theme.h"

static lv_obj_t *s_scr = nullptr;
static lv_obj_t *s_status_lbl[BOOT_STEP_COUNT] {};
static lv_obj_t *s_bar = nullptr;
static boot_status_t s_state[BOOT_STEP_COUNT] {};

static const char *k_labels[BOOT_STEP_COUNT] = {
    "Display / Touch / LVGL",
    "SD Card",
    "RTC",
    "Moisture ADS1115",
    "Settings / Safety / Process",
    "Logger",
    "RS485 / Modbus",
    "Relay Board",
    "Wi-Fi SoftAP",
};

static const char *status_text(boot_status_t st)
{
    switch (st) {
    case BOOT_STATUS_OK:
        return "Ok";
    case BOOT_STATUS_FAIL:
        return "Fail";
    default:
        return "…";
    }
}

static uint32_t status_color(boot_status_t st)
{
    switch (st) {
    case BOOT_STATUS_OK:
        return UI_COLOR_GREEN;
    case BOOT_STATUS_FAIL:
        return UI_COLOR_RED;
    default:
        return UI_COLOR_DIM;
    }
}

static void refresh_bar(void)
{
    if (s_bar == nullptr) {
        return;
    }
    int done = 0;
    for (int i = 0; i < BOOT_STEP_COUNT; ++i) {
        if (s_state[i] != BOOT_STATUS_PENDING) {
            ++done;
        }
    }
    lv_bar_set_value(s_bar, (done * 100) / BOOT_STEP_COUNT, LV_ANIM_OFF);
}

void boot_screen_show(void)
{
    if (s_scr != nullptr) {
        return;
    }

    for (int i = 0; i < BOOT_STEP_COUNT; ++i) {
        s_state[i] = BOOT_STATUS_PENDING;
        s_status_lbl[i] = nullptr;
    }

    s_scr = lv_obj_create(nullptr);
    ui_theme_apply_screen(s_scr);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *brand =
        ui_theme_make_label(s_scr, "SOLAR RICE DRYER", ui_font_title(), UI_COLOR_CREAM);
    lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 48);

    lv_obj_t *sub =
        ui_theme_make_label(s_scr, "Starting…", ui_font_body(), UI_COLOR_MUTED);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 96);

    s_bar = lv_bar_create(s_scr);
    lv_obj_set_size(s_bar, 720, 12);
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_set_style_bg_color(s_bar, ui_theme_color(UI_COLOR_SURFACE_ALT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, ui_theme_color(UI_COLOR_TEAL), LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, 4, LV_PART_INDICATOR);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);

    lv_obj_t *card = ui_theme_make_card(s_scr, 720, 360);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 180);

    for (int i = 0; i < BOOT_STEP_COUNT; ++i) {
        lv_obj_t *row = lv_obj_create(card);
        lv_obj_set_size(row, 680, 32);
        lv_obj_set_pos(row, 8, 8 + i * 36);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name =
            ui_theme_make_label(row, k_labels[i], ui_font_body(), UI_COLOR_TEXT);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);

        s_status_lbl[i] =
            ui_theme_make_label(row, status_text(BOOT_STATUS_PENDING), ui_font_body(),
                                status_color(BOOT_STATUS_PENDING));
        lv_obj_align(s_status_lbl[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }

    lv_scr_load(s_scr);
}

void boot_screen_set(boot_step_t step, boot_status_t status)
{
    if (step < 0 || step >= BOOT_STEP_COUNT) {
        return;
    }
    s_state[step] = status;
    if (s_status_lbl[step] != nullptr) {
        lv_label_set_text(s_status_lbl[step], status_text(status));
        lv_obj_set_style_text_color(s_status_lbl[step], ui_theme_color(status_color(status)),
                                    0);
    }
    refresh_bar();
}

void boot_screen_destroy(void)
{
    if (s_scr == nullptr) {
        return;
    }
    lv_obj_del(s_scr);
    s_scr = nullptr;
    s_bar = nullptr;
    for (int i = 0; i < BOOT_STEP_COUNT; ++i) {
        s_status_lbl[i] = nullptr;
        s_state[i] = BOOT_STATUS_PENDING;
    }
}

bool boot_screen_is_active(void)
{
    return s_scr != nullptr;
}
