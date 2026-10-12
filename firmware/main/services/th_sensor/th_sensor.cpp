#include "th_sensor.h"

#include "esp_log.h"
#include "modbus_rtu.h"

static const char *TAG = "th_sensor";

esp_err_t th_sensor_init(void)
{
    ESP_LOGI(TAG,
             "XY-MD03 ready: hot-air@%u chamber@%u (FC04 T@0x0001 H@0x0002; addr reg 0x0101)",
             TH_SENSOR_HOTAIR_ADDR, TH_SENSOR_CHAMBER_ADDR);
    return ESP_OK;
}

esp_err_t th_sensor_read(uint8_t slave_addr, th_reading_t *out)
{
    if (out == nullptr || slave_addr == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t regs[2] = {0};
    /* XY-MD03: input regs 0x0001=temp×10, 0x0002=humidity×10. */
    esp_err_t err = modbus_read_input_regs(slave_addr, TH_REG_TEMP, 2, regs);
    if (err != ESP_OK) {
        /* Some clones answer on holding regs with the same map. */
        err = modbus_read_holding_regs(slave_addr, TH_REG_TEMP, 2, regs);
    }
    if (err != ESP_OK) {
        out->online = false;
        out->temperature_c = 0.0f;
        out->humidity_rh = 0.0f;
        return err;
    }

    int16_t temp_signed = (int16_t)regs[0];
    out->online = true;
    out->temperature_c = temp_signed / 10.0f;
    out->humidity_rh = regs[1] / 10.0f;
    return ESP_OK;
}

esp_err_t th_sensor_set_address(uint8_t current_addr, uint8_t new_addr)
{
    if (current_addr == 0 || new_addr < 1 || new_addr > 247) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Changing XY-MD03 slave %u → %u (holding 0x%04X)",
             current_addr, new_addr, TH_REG_DEVICE_ADDR);
    esp_err_t err = modbus_write_single_reg(current_addr, TH_REG_DEVICE_ADDR, new_addr);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "set_address failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Address write OK — power-cycle probe, then poll at %u", new_addr);
    return ESP_OK;
}

esp_err_t th_sensor_get_address(uint8_t current_addr, uint16_t *out_addr)
{
    if (out_addr == nullptr || current_addr == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return modbus_read_holding_regs(current_addr, TH_REG_DEVICE_ADDR, 1, out_addr);
}
