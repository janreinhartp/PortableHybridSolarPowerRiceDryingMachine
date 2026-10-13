#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default 7-bit address when ADDR is tied to GND. */
#define ADS1115_I2C_ADDR_DEFAULT  0x48

typedef enum {
    ADS1115_MUX_AIN0_GND = 0,
    ADS1115_MUX_AIN1_GND = 1,
    ADS1115_MUX_AIN2_GND = 2,
    ADS1115_MUX_AIN3_GND = 3,
} ads1115_mux_t;

/**
 * Product channel map (as wired).
 * AIN2 = moisture, AIN3 = EC. Do not treat one 0–5 V line as both.
 */
#define ADS1115_CH_MOISTURE  ADS1115_MUX_AIN2_GND
#define ADS1115_CH_EC        ADS1115_MUX_AIN3_GND

esp_err_t ads1115_init(uint8_t i2c_addr);
bool ads1115_is_present(void);

/**
 * Single-shot read of one single-ended channel.
 * Uses ±6.144 V FSR so a conditioned 0–5 V signal stays in range.
 * @param out_raw   signed conversion code (optional)
 * @param out_volts converted voltage (optional)
 */
esp_err_t ads1115_read_channel(ads1115_mux_t mux, int16_t *out_raw, float *out_volts);

#ifdef __cplusplus
}
#endif
