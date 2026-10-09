#include "th_sensor.h"

#include "esp_log.h"
#include "modbus_rtu.h"

static const char *TAG = "th_sensor";

/**
 * Common RS485 Modbus T/H probe layout (confirm against your datasheet):
 * - Holding or input regs starting at 0x0000
 * - reg0 = humidity * 10
 * - reg1 = temperature * 10
 */
static esp_err_t read_scaled_pair(uint8_t slave, uint16_t *humidity_x10, uint16_t *temp_x10)
{
    uint16_t regs[2] = {0};

    /* Prefer input registers (FC04), fall back to holding (FC03). */
    esp_err_t err = modbus_read_input_regs(slave, 0x0000, 2, regs);
    if (err != ESP_OK) {
        err = modbus_read_holding_regs(slave, 0x0000, 2, regs);
    }
    if (err != ESP_OK) {
        return err;
    }

    *humidity_x10 = regs[0];
    *temp_x10 = regs[1];
    return ESP_OK;
}

esp_err_t th_sensor_init(void)
{
    ESP_LOGI(TAG, "T/H sensors expected at Modbus addrs %u (hot-air) and %u (chamber)",
             TH_SENSOR_HOTAIR_ADDR, TH_SENSOR_CHAMBER_ADDR);
    return ESP_OK;
}

esp_err_t th_sensor_read(uint8_t slave_addr, th_reading_t *out)
{
    if (out == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t humidity_x10 = 0;
    uint16_t temp_x10 = 0;
    esp_err_t err = read_scaled_pair(slave_addr, &humidity_x10, &temp_x10);
    if (err != ESP_OK) {
        out->online = false;
        out->temperature_c = 0.0f;
        out->humidity_rh = 0.0f;
        return err;
    }

    /* Handle signed temperature for values below 0 °C (two's complement in 16-bit). */
    int16_t temp_signed = (int16_t)temp_x10;
    out->online = true;
    out->temperature_c = temp_signed / 10.0f;
    out->humidity_rh = humidity_x10 / 10.0f;
    return ESP_OK;
}
