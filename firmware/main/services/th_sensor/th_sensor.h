#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Product Modbus IDs for XY-MD03 probes (set via holding reg 0x0101). */
#define TH_SENSOR_HOTAIR_ADDR   1
#define TH_SENSOR_CHAMBER_ADDR  2

#define TH_REG_TEMP             0x0001
#define TH_REG_HUMIDITY         0x0002
#define TH_REG_DEVICE_ADDR      0x0101
#define TH_REG_BAUD             0x0102

typedef struct {
    bool online;
    float temperature_c;
    float humidity_rh;
} th_reading_t;

esp_err_t th_sensor_init(void);

/** Read T/H from an XY-MD03 at slave_addr (FC04 @ 0x0001, count 2). */
esp_err_t th_sensor_read(uint8_t slave_addr, th_reading_t *out);

/**
 * Change Modbus slave ID (FC06 write holding 0x0101).
 * Connect only one factory-default probe (usually ID 1) when assigning.
 * New address takes effect after the probe accepts the write (often needs power cycle).
 */
esp_err_t th_sensor_set_address(uint8_t current_addr, uint8_t new_addr);

/** Read back device address register from a probe that still answers at current_addr. */
esp_err_t th_sensor_get_address(uint8_t current_addr, uint16_t *out_addr);

#ifdef __cplusplus
}
#endif
