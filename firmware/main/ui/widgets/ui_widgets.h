#pragma once

#include "lvgl.h"
#include "system_status.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *title_lbl;
    lv_obj_t *value_lbl;
    lv_obj_t *unit_lbl;
    lv_obj_t *detail_lbl;
} ui_metric_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *bar;
    lv_obj_t *marker;
    lv_obj_t *value_lbl;
    lv_obj_t *target_lbl;
    lv_obj_t *title_lbl;
    char unit[8];
    int16_t min_v;
    int16_t max_v;
    lv_coord_t bar_w;
} ui_bar_gauge_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *title;
    lv_obj_t *value;
} ui_kpi_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *chart;
    lv_chart_series_t *ser_moist;
    lv_chart_series_t *ser_chamber;
} ui_trend_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *leds[4];
    lv_obj_t *status[4];
    lv_obj_t *labels[4];
} ui_actuator_leds_t;

typedef void (*ui_confirm_cb_t)(void *user);

ui_metric_t ui_metric_create(lv_obj_t *parent, const char *title, lv_coord_t w, lv_coord_t h);
void ui_metric_set(ui_metric_t *m, const char *value, const char *unit, const char *detail);

ui_bar_gauge_t ui_bar_gauge_create(lv_obj_t *parent, const char *title, const char *unit,
                                   int16_t min_v, int16_t max_v, lv_coord_t w, lv_coord_t h,
                                   uint32_t fill_hex);
void ui_bar_gauge_set(ui_bar_gauge_t *g, float value);
void ui_bar_gauge_set_target(ui_bar_gauge_t *g, bool show, float target);

/** Legacy meter API kept for compatibility; creates a bar gauge of comparable size. */
typedef ui_bar_gauge_t ui_gauge_t;
ui_gauge_t ui_gauge_create(lv_obj_t *parent, const char *title, const char *unit, int16_t min_v,
                           int16_t max_v, lv_coord_t size);
void ui_gauge_set(ui_gauge_t *g, float value);
void ui_gauge_set_target(ui_gauge_t *g, bool show, float target);

ui_kpi_t ui_kpi_create(lv_obj_t *parent, const char *title, lv_coord_t w, lv_coord_t h);
void ui_kpi_set(ui_kpi_t *k, const char *value);

ui_trend_t ui_trend_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);
void ui_trend_refresh(ui_trend_t *t);

ui_actuator_leds_t ui_leds_create(lv_obj_t *parent, lv_coord_t w);
/** Compact row of 4 actuator status cards (Home / Run). */
ui_actuator_leds_t ui_actuator_row_create(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);
void ui_leds_set(ui_actuator_leds_t *leds, const system_status_t *status);

lv_obj_t *ui_state_chip_create(lv_obj_t *parent);
void ui_state_chip_set(lv_obj_t *chip, uint8_t dryer_state);

void ui_confirm_show(const char *title, const char *body, ui_confirm_cb_t on_yes, void *user);

#ifdef __cplusplus
}
#endif
