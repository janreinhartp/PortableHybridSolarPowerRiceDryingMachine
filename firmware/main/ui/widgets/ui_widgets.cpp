#include "ui_widgets.h"

#include <cstdio>
#include <cstring>

#include "screen_common.h"
#include "trend_buffer.h"
#include "ui_theme.h"

ui_gauge_t ui_gauge_create(lv_obj_t *parent, const char *title, const char *unit, int16_t min_v,
                           int16_t max_v, lv_coord_t size)
{
    ui_gauge_t g {};
    g.min_v = min_v;
    g.max_v = max_v;
    if (unit != nullptr) {
        snprintf(g.unit, sizeof(g.unit), "%s", unit);
    } else {
        g.unit[0] = '\0';
    }

    /* Extra bottom room for live value + Target line. */
    g.root = ui_theme_make_card(parent, size + 24, size + 92);
    g.title_lbl = ui_theme_make_label(g.root, title, ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_align(g.title_lbl, LV_ALIGN_TOP_MID, 0, 0);

    g.meter = lv_meter_create(g.root);
    lv_obj_set_size(g.meter, size, size);
    lv_obj_align(g.meter, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(g.meter, ui_theme_color(UI_COLOR_SURFACE_ALT), 0);
    lv_obj_set_style_border_width(g.meter, 0, 0);

    lv_meter_scale_t *scale = lv_meter_add_scale(g.meter);
    lv_meter_set_scale_ticks(g.meter, scale, 21, 2, 8, ui_theme_color(UI_COLOR_BORDER));
    lv_meter_set_scale_major_ticks(g.meter, scale, 5, 3, 12, ui_theme_color(UI_COLOR_MUTED), 12);
    lv_meter_set_scale_range(g.meter, scale, min_v, max_v, 270, 135);

    lv_meter_indicator_t *arc = lv_meter_add_arc(g.meter, scale, 6, ui_theme_color(UI_COLOR_ACCENT), 0);
    lv_meter_set_indicator_start_value(g.meter, arc, min_v);
    lv_meter_set_indicator_end_value(g.meter, arc, max_v);

    g.needle = lv_meter_add_needle_line(g.meter, scale, 3, ui_theme_color(UI_COLOR_AMBER), -8);

    g.value_lbl = ui_theme_make_label(g.root, "-", ui_font_kpi(), UI_COLOR_TEXT);
    lv_obj_align(g.value_lbl, LV_ALIGN_BOTTOM_MID, 0, -28);

    g.target_lbl = ui_theme_make_label(g.root, "", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_width(g.target_lbl, size + 4);
    lv_label_set_long_mode(g.target_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(g.target_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(g.target_lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_add_flag(g.target_lbl, LV_OBJ_FLAG_HIDDEN);
    return g;
}

void ui_gauge_set(ui_gauge_t *g, float value)
{
    if (g == nullptr || g->meter == nullptr) {
        return;
    }
    int16_t v = (int16_t)(value + (value >= 0 ? 0.5f : -0.5f));
    if (v < g->min_v) {
        v = g->min_v;
    }
    if (v > g->max_v) {
        v = g->max_v;
    }
    lv_meter_set_indicator_value(g->meter, g->needle, v);
    char buf[28];
    if (g->unit[0] != '\0') {
        snprintf(buf, sizeof(buf), "%.1f %s", value, g->unit);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", value);
    }
    lv_label_set_text(g->value_lbl, buf);
}

void ui_gauge_set_target(ui_gauge_t *g, bool show, float target)
{
    if (g == nullptr || g->target_lbl == nullptr) {
        return;
    }
    if (!show) {
        lv_obj_add_flag(g->target_lbl, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    char buf[36];
    if (g->unit[0] != '\0') {
        snprintf(buf, sizeof(buf), "Target %.1f %s", target, g->unit);
    } else {
        snprintf(buf, sizeof(buf), "Target %.1f", target);
    }
    lv_label_set_text(g->target_lbl, buf);
    lv_obj_clear_flag(g->target_lbl, LV_OBJ_FLAG_HIDDEN);
}

ui_kpi_t ui_kpi_create(lv_obj_t *parent, const char *title, lv_coord_t w, lv_coord_t h)
{
    ui_kpi_t k {};
    k.root = ui_theme_make_card(parent, w, h);
    k.title = ui_theme_make_label(k.root, title, ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_align(k.title, LV_ALIGN_TOP_LEFT, 0, 0);
    k.value = ui_theme_make_label(k.root, "-", ui_font_title(), UI_COLOR_TEXT);
    lv_obj_align(k.value, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return k;
}

void ui_kpi_set(ui_kpi_t *k, const char *value)
{
    if (k == nullptr || k->value == nullptr) {
        return;
    }
    lv_label_set_text(k->value, value != nullptr ? value : "-");
}

ui_trend_t ui_trend_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    ui_trend_t t {};
    t.root = ui_theme_make_card(parent, w, h);
    lv_obj_t *title = ui_theme_make_label(t.root, "Trend Moisture + Chamber", ui_font_caption(),
                                          UI_COLOR_MUTED);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    t.chart = lv_chart_create(t.root);
    lv_obj_set_size(t.chart, w - 28, h - 44);
    lv_obj_align(t.chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(t.chart, ui_theme_color(UI_COLOR_SURFACE_ALT), 0);
    lv_obj_set_style_border_width(t.chart, 0, 0);
    lv_chart_set_type(t.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(t.chart, TREND_POINTS);
    lv_chart_set_range(t.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 80);
    lv_chart_set_div_line_count(t.chart, 4, 6);
    lv_obj_set_style_line_color(t.chart, ui_theme_color(UI_COLOR_BORDER), LV_PART_MAIN);

    t.ser_moist = lv_chart_add_series(t.chart, ui_theme_color(UI_COLOR_ACCENT),
                                      LV_CHART_AXIS_PRIMARY_Y);
    t.ser_chamber = lv_chart_add_series(t.chart, ui_theme_color(UI_COLOR_AMBER),
                                        LV_CHART_AXIS_PRIMARY_Y);
    return t;
}

void ui_trend_refresh(ui_trend_t *t)
{
    if (t == nullptr || t->chart == nullptr) {
        return;
    }
    float moist[TREND_POINTS];
    float chamber[TREND_POINTS];
    const size_t n = trend_buffer_copy(moist, chamber, nullptr, TREND_POINTS);
    lv_chart_set_all_value(t->chart, t->ser_moist, LV_CHART_POINT_NONE);
    lv_chart_set_all_value(t->chart, t->ser_chamber, LV_CHART_POINT_NONE);
    for (size_t i = 0; i < n; ++i) {
        lv_chart_set_next_value(t->chart, t->ser_moist, (lv_coord_t)(moist[i] + 0.5f));
        lv_chart_set_next_value(t->chart, t->ser_chamber, (lv_coord_t)(chamber[i] + 0.5f));
    }
    lv_chart_refresh(t->chart);
}

ui_actuator_leds_t ui_leds_create(lv_obj_t *parent, lv_coord_t w)
{
    ui_actuator_leds_t leds {};
    leds.root = ui_theme_make_card(parent, w, 72);
    static const char *names[] = {"Heater", "Fan", "Elevator", "Door"};
    const lv_coord_t slot = w / 4;
    for (int i = 0; i < 4; ++i) {
        leds.leds[i] = lv_obj_create(leds.root);
        lv_obj_set_size(leds.leds[i], 18, 18);
        lv_obj_set_style_radius(leds.leds[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(leds.leds[i], 0, 0);
        lv_obj_set_style_bg_color(leds.leds[i], ui_theme_color(UI_COLOR_BORDER), 0);
        lv_obj_set_pos(leds.leds[i], (lv_coord_t)(i * slot + 8), 8);
        leds.labels[i] =
            ui_theme_make_label(leds.root, names[i], ui_font_caption(), UI_COLOR_MUTED);
        lv_obj_set_pos(leds.labels[i], (lv_coord_t)(i * slot + 8), 36);
    }
    return leds;
}

void ui_leds_set(ui_actuator_leds_t *leds, const system_status_t *status)
{
    if (leds == nullptr || status == nullptr) {
        return;
    }
    const bool on[4] = {
        status->relay_heater,
        status->relay_fan,
        status->relay_elevator,
        status->relay_door_open,
    };
    const uint32_t colors[4] = {UI_COLOR_HEATER, UI_COLOR_GREEN, UI_COLOR_ACCENT, UI_COLOR_ORANGE};
    for (int i = 0; i < 4; ++i) {
        lv_obj_set_style_bg_color(leds->leds[i],
                                  ui_theme_color(on[i] ? colors[i] : UI_COLOR_BORDER), 0);
    }
}

lv_obj_t *ui_state_chip_create(lv_obj_t *parent)
{
    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_set_size(chip, 140, 36);
    lv_obj_set_style_radius(chip, 18, 0);
    lv_obj_set_style_bg_color(chip, ui_theme_color(UI_COLOR_SURFACE_ALT), 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_pad_hor(chip, 12, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *lbl = ui_theme_make_label(chip, "Idle", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_center(lbl);
    lv_obj_set_user_data(chip, lbl);
    return chip;
}

void ui_state_chip_set(lv_obj_t *chip, uint8_t dryer_state)
{
    if (chip == nullptr) {
        return;
    }
    lv_obj_t *lbl = (lv_obj_t *)lv_obj_get_user_data(chip);
    if (lbl != nullptr) {
        lv_label_set_text(lbl, ui_state_title(dryer_state));
        lv_obj_set_style_text_color(lbl, ui_theme_color(UI_COLOR_ON_PRIMARY), 0);
    }
    lv_obj_set_style_bg_color(chip, ui_theme_state_color(dryer_state), 0);
}

typedef struct {
    ui_confirm_cb_t cb;
    void *user;
    lv_obj_t *overlay;
} confirm_ctx_t;

static void on_confirm_yes(lv_event_t *e)
{
    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_event_get_user_data(e);
    if (ctx == nullptr) {
        return;
    }
    ui_confirm_cb_t cb = ctx->cb;
    void *user = ctx->user;
    lv_obj_t *overlay = ctx->overlay;
    if (overlay != nullptr) {
        lv_obj_del(overlay);
    }
    if (cb != nullptr) {
        cb(user);
    }
}

static void on_confirm_no(lv_event_t *e)
{
    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_event_get_user_data(e);
    if (ctx != nullptr && ctx->overlay != nullptr) {
        lv_obj_del(ctx->overlay);
    }
}

static void on_confirm_overlay_delete(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_event_get_user_data(e);
    if (ctx != nullptr) {
        lv_mem_free(ctx);
    }
}

void ui_confirm_show(const char *title, const char *body, ui_confirm_cb_t on_yes, void *user)
{
    lv_obj_t *overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, ui_theme_color(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = ui_theme_make_card(overlay, 520, 220);
    lv_obj_center(card);

    confirm_ctx_t *ctx = (confirm_ctx_t *)lv_mem_alloc(sizeof(confirm_ctx_t));
    if (ctx == nullptr) {
        lv_obj_del(overlay);
        return;
    }
    ctx->cb = on_yes;
    ctx->user = user;
    ctx->overlay = overlay;

    lv_obj_t *t = ui_theme_make_label(card, title != nullptr ? title : "Confirm", ui_font_title(),
                                      UI_COLOR_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *b = ui_theme_make_label(card, body != nullptr ? body : "", ui_font_body(),
                                      UI_COLOR_MUTED);
    lv_obj_set_width(b, 480);
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
    lv_obj_align(b, LV_ALIGN_TOP_LEFT, 0, 44);

    lv_obj_t *yes = ui_theme_make_btn(card, "Confirm", on_confirm_yes, ctx, 180, 56, UI_COLOR_RED);
    lv_obj_align(yes, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *no =
        ui_theme_make_btn(card, "Cancel", on_confirm_no, ctx, 180, 56, UI_COLOR_SURFACE_ALT);
    lv_obj_align(no, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_add_event_cb(overlay, on_confirm_overlay_delete, LV_EVENT_DELETE, ctx);
}
