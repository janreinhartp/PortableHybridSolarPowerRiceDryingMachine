#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rs485_init(void);
esp_err_t rs485_transceive(const uint8_t *tx, size_t tx_len,
                           uint8_t *rx, size_t rx_capacity, size_t *rx_len,
                           uint32_t response_timeout_ms);

#ifdef __cplusplus
}
#endif
