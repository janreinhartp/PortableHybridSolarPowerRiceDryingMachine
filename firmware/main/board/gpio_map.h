#pragma once

/**
 * Locked GPIO assignments for Waveshare ESP32-S3-Touch-LCD-7B.
 * Keep in sync with hardware/wiring/esp32-gpio-map.md
 *
 * Door travel limits are internal to the linear actuator (no ESP32 inputs).
 * Emergency stop is hard-wired to the panel main contactor (no ESP32 input).
 */

#include "driver/gpio.h"

/* Shared I2C (already used by Waveshare board bring-up) */
#define RICE_GPIO_I2C_SDA           GPIO_NUM_8
#define RICE_GPIO_I2C_SCL           GPIO_NUM_9

/* SDMMC (already used) */
#define RICE_GPIO_SD_CMD            GPIO_NUM_11
#define RICE_GPIO_SD_CLK            GPIO_NUM_12
#define RICE_GPIO_SD_D0             GPIO_NUM_13

/* Touch interrupt (already used) */
#define RICE_GPIO_TOUCH_INT         GPIO_NUM_4

/* Phase 3 — RS485 / Modbus UART1 */
#define RICE_GPIO_RS485_TX          GPIO_NUM_15
#define RICE_GPIO_RS485_RX          GPIO_NUM_16
#define RICE_GPIO_RS485_DE_RE       GPIO_NUM_26

/* I2C device addresses */
#define RICE_I2C_ADDR_IO_EXPANDER   0x24
#define RICE_I2C_ADDR_GT911         0x5D
#define RICE_I2C_ADDR_GT911_ALT     0x14
#define RICE_I2C_ADDR_RTC           0x68
#define RICE_I2C_ADDR_ADS1115       0x48
