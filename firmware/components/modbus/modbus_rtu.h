#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MODBUS_MAX_REGS 16

esp_err_t modbus_rtu_init(void);

/** Read holding registers (FC 0x03). */
esp_err_t modbus_read_holding_regs(uint8_t slave, uint16_t start_addr,
                                   uint16_t count, uint16_t *out_regs);

/** Read input registers (FC 0x04). */
esp_err_t modbus_read_input_regs(uint8_t slave, uint16_t start_addr,
                                 uint16_t count, uint16_t *out_regs);

/** Write single coil (FC 0x05). coil_on true = 0xFF00. */
esp_err_t modbus_write_single_coil(uint8_t slave, uint16_t coil_addr, bool coil_on);

/** Read coils (FC 0x01), up to 16 coils packed into out_bits LSB-first. */
esp_err_t modbus_read_coils(uint8_t slave, uint16_t start_addr,
                            uint16_t count, uint16_t *out_bits);

#ifdef __cplusplus
}
#endif
