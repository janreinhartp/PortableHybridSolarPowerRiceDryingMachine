#include "modbus_rtu.h"

#include <string.h>

#include "esp_log.h"
#include "rs485.h"

static const char *TAG = "modbus";

#define MODBUS_TIMEOUT_MS   150
#define MODBUS_RETRIES      2

static uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x0001) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static esp_err_t modbus_transaction(const uint8_t *req, size_t req_len,
                                    uint8_t *resp, size_t resp_cap, size_t *resp_len,
                                    uint8_t expect_fc)
{
    esp_err_t last = ESP_FAIL;
    for (int attempt = 0; attempt < MODBUS_RETRIES; ++attempt) {
        size_t got = 0;
        last = rs485_transceive(req, req_len, resp, resp_cap, &got, MODBUS_TIMEOUT_MS);
        if (last == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "timeout slave=%u fc=0x%02X try=%d", req[0], expect_fc, attempt + 1);
            continue;
        }
        if (last != ESP_OK) {
            continue;
        }
        if (got < 5) {
            last = ESP_ERR_INVALID_SIZE;
            continue;
        }

        uint16_t rx_crc = (uint16_t)(resp[got - 2] | (resp[got - 1] << 8));
        uint16_t calc = modbus_crc16(resp, got - 2);
        if (rx_crc != calc) {
            ESP_LOGW(TAG, "CRC mismatch slave=%u (rx=%04X calc=%04X)", req[0], rx_crc, calc);
            last = ESP_ERR_INVALID_CRC;
            continue;
        }

        if (resp[0] != req[0]) {
            last = ESP_FAIL;
            continue;
        }

        if (resp[1] & 0x80) {
            ESP_LOGW(TAG, "exception slave=%u code=%u", resp[0], resp[2]);
            last = ESP_ERR_INVALID_STATE;
            continue;
        }

        if (resp[1] != expect_fc) {
            last = ESP_FAIL;
            continue;
        }

        *resp_len = got;
        return ESP_OK;
    }
    return last;
}

esp_err_t modbus_rtu_init(void)
{
    return rs485_init();
}

static esp_err_t modbus_read_regs(uint8_t fc, uint8_t slave, uint16_t start_addr,
                                  uint16_t count, uint16_t *out_regs)
{
    if (out_regs == NULL || count == 0 || count > MODBUS_MAX_REGS) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t req[8];
    req[0] = slave;
    req[1] = fc;
    req[2] = (uint8_t)(start_addr >> 8);
    req[3] = (uint8_t)(start_addr & 0xFF);
    req[4] = (uint8_t)(count >> 8);
    req[5] = (uint8_t)(count & 0xFF);
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    uint8_t resp[5 + MODBUS_MAX_REGS * 2 + 2];
    size_t resp_len = 0;
    esp_err_t err = modbus_transaction(req, sizeof(req), resp, sizeof(resp), &resp_len, fc);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t byte_count = resp[2];
    if (byte_count != count * 2 || resp_len < (size_t)(3 + byte_count + 2)) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (uint16_t i = 0; i < count; ++i) {
        out_regs[i] = (uint16_t)((resp[3 + i * 2] << 8) | resp[4 + i * 2]);
    }
    return ESP_OK;
}

esp_err_t modbus_read_holding_regs(uint8_t slave, uint16_t start_addr,
                                   uint16_t count, uint16_t *out_regs)
{
    return modbus_read_regs(0x03, slave, start_addr, count, out_regs);
}

esp_err_t modbus_read_input_regs(uint8_t slave, uint16_t start_addr,
                                 uint16_t count, uint16_t *out_regs)
{
    return modbus_read_regs(0x04, slave, start_addr, count, out_regs);
}

esp_err_t modbus_write_single_coil(uint8_t slave, uint16_t coil_addr, bool coil_on)
{
    uint8_t req[8];
    req[0] = slave;
    req[1] = 0x05;
    req[2] = (uint8_t)(coil_addr >> 8);
    req[3] = (uint8_t)(coil_addr & 0xFF);
    req[4] = coil_on ? 0xFF : 0x00;
    req[5] = 0x00;
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    uint8_t resp[8];
    size_t resp_len = 0;
    esp_err_t err = modbus_transaction(req, sizeof(req), resp, sizeof(resp), &resp_len, 0x05);
    if (err != ESP_OK) {
        return err;
    }
    if (resp_len < 8) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t modbus_read_coils(uint8_t slave, uint16_t start_addr,
                            uint16_t count, uint16_t *out_bits)
{
    if (out_bits == NULL || count == 0 || count > 16) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t req[8];
    req[0] = slave;
    req[1] = 0x01;
    req[2] = (uint8_t)(start_addr >> 8);
    req[3] = (uint8_t)(start_addr & 0xFF);
    req[4] = (uint8_t)(count >> 8);
    req[5] = (uint8_t)(count & 0xFF);
    uint16_t crc = modbus_crc16(req, 6);
    req[6] = (uint8_t)(crc & 0xFF);
    req[7] = (uint8_t)(crc >> 8);

    uint8_t resp[8];
    size_t resp_len = 0;
    esp_err_t err = modbus_transaction(req, sizeof(req), resp, sizeof(resp), &resp_len, 0x01);
    if (err != ESP_OK) {
        return err;
    }
    if (resp_len < 6) {
        return ESP_ERR_INVALID_SIZE;
    }
    *out_bits = resp[3];
    if (resp[2] >= 2) {
        *out_bits |= (uint16_t)(resp[4] << 8);
    }
    return ESP_OK;
}
