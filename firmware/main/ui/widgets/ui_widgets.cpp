#include "ui_widgets.h"

#include <cstdio>
#include <cstring>

#include "process_controller.h"
#include "screen_common.h"
#include "trend_buffer.h"
#include "ui_theme.h"

ui_metric_t ui_metric_create(lv_obj_t *parent, const char *title, lv_coord_t w, lv_coord_t h)
{
    ui_metric_t m {};
    m.root = ui_theme_make_card(parent, w, h);
    lv_obj_set_style_pad_all(m.root, 16, 0);

    m.title_lbl = ui_theme_make_label(m.root, title, ui_font_body(), UI_COLOR_MUTED);
    lv_obj_align(m.title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    m.value_lbl = ui_theme_make_label(m.root, "-", ui_font_kpi(), UI_COLOR_CREAM);
    lv_obj_align(m.value_lbl, LV_ALIGN_TOP_LEFT, 0, 36);

    m.unit_lbl = ui_theme_make_label(m.root, "", ui_font_body(), UI_COLOR_CREAM);
    lv_obj_align(m.unit_lbl, LV_ALIGN_TOP_LEFT, 0, 52);

    m.detail_lbl = ui_theme_make_label(m.root, "", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_set_width(m.detail_lbl, w - 32);
    lv_label_set_long_mode(m.detail_lbl, LV_LABEL_LONG_DOT);
    lv_obj_align(m.detail_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return m;
}

void ui_metric_set(ui_metric_t *m, const char *value, const char *unit, const char *detail)
{
    if (m == nullptr || m->value_lbl == nullptr) {
        return;
    }
    ui_label_set_text_if_changed(m->value_lbl, value != nullptr ? value : "-");
    if (m->unit_lbl != nullptr) {
        ui_label_set_text_if_changed(m->unit_lbl, unit != nullptr ? unit : "");
        if (unit != nullptr && unit[0] != '\0') {
            lv_obj_align_to(m->unit_lbl, m->value_lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, 0);
            lv_obj_clear_flag(m->unit_lbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m->unit_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (m->detail_lbl != nullptr) {
        ui_label_set_text_if_changed(m->detail_lbl, detail != nullptr ? detail : "");
    }
}

ui_bar_gauge_t ui_bar_gauge_create(lv_obj_t *parent, const char *title, const char *unit,
                                   int16_t min_v, int16_t max_v, lv_coord_t w, lv_coord_t h,
                                   uint32_t fill_hex)
{
    ui_bar_gauge_t g {};
    g.min_v = min_v;
    g.max_v = max_v;
    if (unit != nullptr) {
        snprintf(g.unit, sizeof(g.unit), "%s", unit);
    } else {
        g.unit[0] = '\0';
    }

    g.root = ui_theme_make_card(parent, w, h);
    g.title_lbl = ui_theme_make_label(g.root, title, ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(g.title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    g.value_lbl = ui_theme_make_label(g.root, "-", ui_font_kpi(), UI_COLOR_CREAM);
    lv_obj_align(g.value_lbl, LV_ALIGN_TOP_LEFT, 0, 28);

    g.target_lbl = ui_theme_make_label(g.root, "", ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_align(g.target_lbl, LV_ALIGN_TOP_RIGHT, 0, 40);
    lv_obj_add_flag(g.target_lbl, LV_OBJ_FLAG_HIDDEN);

    g.bar_w = (lv_coord_t)(w - 40);
    g.bar = lv_bar_create(g.root);
    lv_obj_set_size(g.bar, g.bar_w, 8);
    lv_obj_align(g.bar, LV_ALIGN_TOP_LEFT, 0, 80);
    lv_bar_set_range(g.bar, min_v, max_v);
    lv_obj_set_style_bg_color(g.bar, ui_theme_color(UI_COLOR_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g.bar, ui_theme_color(fill_hex), LV_PART_INDICATOR);
    lv_obj_set_style_radius(g.bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(g.bar, 4, LV_PART_INDICATOR);

    g.marker = lv_obj_create(g.root);
    lv_obj_set_size(g.marker, 2, 12);
    lv_obj_set_style_bg_color(g.marker, ui_theme_color(UI_COLOR_TEXT), 0);
    lv_obj_set_style_border_width(g.marker, 0, 0);
    lv_obj_set_style_radius(g.marker, 0, 0);
    lv_obj_clear_flag(g.marker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g.marker, LV_OBJ_FLAG_HIDDEN);

    char range[32];
    snprintf(range, sizeof(range), "%d %s", (int)min_v, g.unit);
    lv_obj_t *min_lbl = ui_theme_make_label(g.root, range, ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_align(min_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    snprintf(range, sizeof(range), "%d %s", (int)max_v, g.unit);
    lv_obj_t *max_lbl = ui_theme_make_label(g.root, range, ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_align(max_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    return g;
}

void ui_bar_gauge_set(ui_bar_gauge_t *g, float value)
{
    if (g == nullptr || g->bar == nullptr) {
        return;
    }
    int16_t v = (int16_t)(value + (value >= 0 ? 0.5f : -0.5f));
    if (v < g->min_v) {
        v = g->min_v;
    }
    if (v > g->max_v) {
        v = g->max_v;
    }
    if (lv_bar_get_value(g->bar) != v) {
        lv_bar_set_value(g->bar, v, LV_ANIM_OFF);
    }
    char buf[28];
    if (g->unit[0] != '\0') {
        snprintf(buf, sizeof(buf), "%.1f %s", value, g->unit);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", value);
    }
    ui_label_set_text_if_changed(g->value_lbl, buf);
}

void ui_bar_gauge_set_target(ui_bar_gauge_t *g, bool show, float target)
{
    if (g == nullptr || g->target_lbl == nullptr) {
        return;
    }
    if (!show) {
        lv_obj_add_flag(g->target_lbl, LV_OBJ_FLAG_HIDDEN);
        if (g->marker != nullptr) {
            lv_obj_add_flag(g->marker, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }
    char buf[36];
    if (g->unit[0] != '\0') {
        snprintf(buf, sizeof(buf), "Target %.1f %s", target, g->unit);
    } else {
        snprintf(buf, sizeof(buf), "Target %.1f", target);
    }
    ui_label_set_text_if_changed(g->target_lbl, buf);
    lv_obj_clear_flag(g->target_lbl, LV_OBJ_FLAG_HIDDEN);

    if (g->marker != nullptr && g->max_v > g->min_v) {
        float t = target;
        if (t < g->min_v) {
            t = (float)g->min_v;
        }
        if (t > g->max_v) {
            t = (float)g->max_v;
        }
        const float span = (float)(g->max_v - g->min_v);
        const lv_coord_t x =
            (lv_coord_t)(((t - (float)g->min_v) / span) * (float)g->bar_w);
        lv_obj_align(g->marker, LV_ALIGN_TOP_LEFT, x, 78);
        lv_obj_clear_flag(g->marker, LV_OBJ_FLAG_HIDDEN);
    }
}

ui_gauge_t ui_gauge_create(lv_obj_t *parent, const char *title, const char *unit, int16_t min_v,
                           int16_t max_v, lv_coord_t size)
{
    (void)size;
    return ui_bar_gauge_create(parent, title, unit, min_v, max_v, 328, 128, UI_COLOR_TEAL);
}

void ui_gauge_set(ui_gauge_t *g, float value)
{
    ui_bar_gauge_set(g, value);
}

void ui_gauge_set_target(ui_gauge_t *g, bool show, float target)
{
    ui_bar_gauge_set_target(g, show, target);
}

ui_kpi_t ui_kpi_create(lv_obj_t *parent, const char *title, lv_coord_t w, lv_coord_t h)
{
    ui_kpi_t k {};
    k.root = ui_theme_make_card(parent, w, h);
    k.title = ui_theme_make_label(k.root, title, ui_font_caption(), UI_COLOR_MUTED);
    lv_obj_align(k.title, LV_ALIGN_TOP_LEFT, 0, 0);
    k.value = ui_theme_make_label(k.root, "-", ui_font_title(), UI_COLOR_CREAM);
    lv_obj_align(k.value, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return k;
}

void ui_kpi_set(ui_kpi_t *k, const char *value)
{
    if (k == nullptr || k->value == nullptr) {
        return;
    }
    ui_label_set_text_if_changed(k->value, value != nullptr ? value : "-");
}

ui_trend_t ui_trend_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    ui_trend_t t {};
    t.root = ui_theme_make_card(parent, w, h);
    lv_obj_set_style_pad_all(t.root, 16, 0);

    lv_obj_t *title =
        ui_theme_make_label(t.root, "Live Trend - Last 5 Minutes", ui_font_body(), UI_COLOR_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_t *legend = ui_theme_make_label(t.root, "Moisture  /  Chamber", ui_font_caption(),
                                           UI_COLOR_MUTED);
    lv_obj_align(legend, LV_ALIGN_TOP_RIGHT, 0, 2);

    t.chart = lv_chart_create(t.root);
    lv_obj_set_size(t.chart, w - 48, h - 56);
    lv_obj_align(t.chart, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(t.chart, ui_theme_color(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_border_width(t.chart, 0, 0);
    lv_chart_set_type(t.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(t.chart, TREND_POINTS);
    lv_chart_set_range(t.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 80);
    lv_chart_set_div_line_count(t.chart, 3, 4);
    lv_obj_set_style_line_color(t.chart, ui_theme_color(UI_COLOR_BORDER), LV_PART_MAIN);

    t.ser_moist =
        lv_chart_add_series(t.chart, ui_theme_color(UI_COLOR_CREAM), LV_CHART_AXIS_PRIMARY_Y);
    t.ser_chamber =
        lv_chart_add_series(t.chart, ui_theme_color(UI_COLOR_AMBER), LV_CHART_AXIS_PRIMARY_Y);
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

static void fill_actuator_card(lv_obj_t *card, lv_obj_t **led, lv_obj_t **status,
                               lv_obj_t **label, const char *name, bool horizontal)
{
    *label = ui_theme_make_label(card, name, ui_font_body(),
                                 horizontal ? UI_COLOR_TEXT : UI_COLOR_MUTED);
    *led = lv_obj_create(card);
    lv_obj_set_size(*led, 12, 12);
    lv_obj_set_style_radius(*led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(*led, 0, 0);
    lv_obj_set_style_bg_color(*led, ui_theme_color(UI_COLOR_BORDER), 0);
    lv_obj_clear_flag(*led, LV_OBJ_FLAG_SCROLLABLE);
    *status = ui_theme_make_label(card, "OFF", ui_font_body(), UI_COLOR_DIM);

    if (horizontal) {
        lv_obj_align(*label, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_align(*status, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_align_to(*led, *status, LV_ALIGN_OUT_LEFT_MID, -8, 0);
    } else {
        lv_obj_align(*label, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_align(*led, LV_ALIGN_BOTTOM_LEFT, 0, -4);
        lv_obj_align_to(*status, *led, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    }
}

ui_actuator_leds_t ui_leds_create(lv_obj_t *parent, lv_coord_t w)
{
    return ui_actuator_row_create(parent, w, 112);
}

ui_actuator_leds_t ui_actuator_row_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h)
{
    ui_actuator_leds_t leds {};
    leds.root = lv_obj_create(parent);
    lv_obj_set_size(leds.root, w, h);
    lv_obj_set_style_bg_opa(leds.root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leds.root, 0, 0);
    lv_obj_set_style_pad_all(leds.root, 0, 0);
    lv_obj_clear_flag(leds.root, LV_OBJ_FLAG_SCROLLABLE);

    static const char *names[] = {"Heater", "Fan", "Elevator", "Door"};
    const bool tall = h >= 100;
    const lv_coord_t slot_w = (lv_coord_t)((w - 24) / 4);
    const lv_coord_t card_h = tall ? h : h;
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *card = ui_theme_make_card(leds.root, slot_w, card_h);
        lv_obj_set_pos(card, (lv_coord_t)(i * (slot_w + 8)), 0);
        fill_actuator_card(card, &leds.leds[i], &leds.status[i], &leds.labels[i], names[i], !tall);
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
    const char *text[4] = {
        on[0] ? "ON" : "OFF",
        on[1] ? "ON" : "OFF",
        on[2] ? "RUNNING" : "OFF",
        status->relay_door_open ? "OPEN" : "CLOSED",
    };
    const uint32_t colors[4] = {UI_COLOR_HEATER, UI_COLOR_TEAL_SOFT, UI_COLOR_TEAL_SOFT,
                                UI_COLOR_DIM};
    for (int i = 0; i < 4; ++i) {
        const bool active = (i == 3) ? status->relay_door_open : on[i];
        const uint32_t c = active ? colors[i] : (uint32_t)UI_COLOR_DIM;
        if (leds->leds[i] != nullptr) {
            lv_obj_set_style_bg_color(
                leds->leds[i],
                ui_theme_color(active ? colors[i] : (uint32_t)UI_COLOR_BORDER), 0);
        }
        if (leds->status[i] != nullptr) {
            ui_label_set_text_if_changed(leds->status[i], text[i]);
            lv_obj_set_style_text_color(leds->status[i], ui_theme_color(c), 0);
        }
    }
}

static const char *state_chip_text(uint8_t dryer_state)
{
    switch ((dryer_state_t)dryer_state) {
    case DRYER_STATE_PRECHECK:
        return "PRECHECK";
    case DRYER_STATE_DRYING:
        return "DRYING";
    case DRYER_STATE_COOLING:
        return "COOLING";
    case DRYER_STATE_DISCHARGING:
        return "DISCHARGE";
    case DRYER_STATE_COMPLETE:
        return "COMPLETE";
    case DRYER_STATE_FAULT:
        return "FAULT";
    case DRYER_STATE_IDLE:
    default:
        return "IDLE";
    }
}

lv_obj_t *ui_state_chip_create(lv_obj_t *parent)
{
    lv_obj_t *chip = lv_obj_create(parent);
    lv_obj_set_size(chip, 160, 40);
    lv_obj_set_style_radius(chip, 4, 0);
    lv_obj_set_style_bg_color(chip, ui_theme_color(UI_COLOR_SURFACE_ALT), 0);
    lv_obj_set_style_border_color(chip, ui_theme_color(UI_COLOR_BORDER), 0);
    lv_obj_set_style_border_width(chip, 1, 0);
    lv_obj_set_style_pad_hor(chip, 8, 0);
    lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dot = lv_obj_create(chip);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_bg_color(dot, ui_theme_color(UI_COLOR_DIM), 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 4, 0);

    lv_obj_t *lbl = ui_theme_make_label(chip, "IDLE", ui_font_body(), UI_COLOR_DIM);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 8, 0);
    lv_obj_set_user_data(chip, lbl);
    lv_obj_set_user_data(lbl, dot);
    return chip;
}

void ui_state_chip_set(lv_obj_t *chip, uint8_t dryer_state)
{
    if (chip == nullptr) {
        return;
    }
    /* user_data on chip is the label; stash last state in label's sibling via style cache. */
    static lv_obj_t *s_last_chip = nullptr;
    static uint8_t s_last_state = 0xFF;
    if (chip == s_last_chip && dryer_state == s_last_state) {
        return;
    }
    s_last_chip = chip;
    s_last_state = dryer_state;

    lv_obj_t *lbl = (lv_obj_t *)lv_obj_get_user_data(chip);
    const lv_color_t color = ui_theme_state_color(dryer_state);
    if (lbl != nullptr) {
        ui_label_set_text_if_changed(lbl, state_chip_text(dryer_state));
        lv_obj_set_style_text_color(lbl, color, 0);
        lv_obj_t *dot = (lv_obj_t *)lv_obj_get_user_data(lbl);
        if (dot != nullptr) {
            lv_obj_set_style_bg_color(dot, color, 0);
        }
    }
    lv_obj_set_style_border_color(chip, color, 0);
    lv_obj_set_style_bg_color(chip, ui_theme_color(UI_COLOR_SURFACE_ALT), 0);
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
