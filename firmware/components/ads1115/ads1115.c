#include "ads1115.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c.h"

static const char *TAG = "ads1115";

#define ADS1115_REG_CONVERSION  0x00
#define ADS1115_REG_CONFIG      0x01
#define ADS1115_I2C_HZ          400000
#define ADS1115_TIMEOUT_MS      200
#define ADS1115_FSR_V           6.144f

static i2c_master_dev_handle_t s_dev = NULL;
static bool s_present = false;
static uint8_t s_addr = ADS1115_I2C_ADDR_DEFAULT;

static esp_err_t write_reg16(uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = {
        reg,
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)(value & 0xFF),
    };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), ADS1115_TIMEOUT_MS);
}

static esp_err_t read_reg16(uint8_t reg, uint16_t *out)
{
    uint8_t raw[2] = {0};
    esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, raw, sizeof(raw), ADS1115_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    *out = (uint16_t)((raw[0] << 8) | raw[1]);
    return ESP_OK;
}

esp_err_t ads1115_init(uint8_t i2c_addr)
{
    s_present = false;
    s_dev = NULL;
    s_addr = (i2c_addr == 0) ? ADS1115_I2C_ADDR_DEFAULT : i2c_addr;

    if (DEV_I2C_Get_Bus() == NULL) {
        ESP_LOGE(TAG, "Shared I2C bus not ready");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = DEV_I2C_Add_Device(s_addr, ADS1115_I2C_HZ, &s_dev);
    if (err != ESP_OK || s_dev == NULL) {
        ESP_LOGE(TAG, "Failed to add ADS1115 at 0x%02X", s_addr);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    uint16_t config = 0;
    err = read_reg16(ADS1115_REG_CONFIG, &config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADS1115 no ACK at 0x%02X (%s) — moisture/EC offline until wired",
                 s_addr, esp_err_to_name(err));
        return err;
    }

    s_present = true;
    ESP_LOGI(TAG, "ADS1115 OK at 0x%02X (config=0x%04X, FSR=±%.3f V)", s_addr, config, ADS1115_FSR_V);
    return ESP_OK;
}

bool ads1115_is_present(void)
{
    return s_present;
}

esp_err_t ads1115_read_channel(ads1115_mux_t mux, int16_t *out_raw, float *out_volts)
{
    if (!s_present || s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (mux > ADS1115_MUX_AIN3_GND) {
        return ESP_ERR_INVALID_ARG;
    }

    /* OS=1 start, MUX single-ended, PGA=±6.144V, MODE=single, DR=128SPS, COMP disable */
    uint16_t config = 0x8000;
    config |= (uint16_t)((0x04 + (uint16_t)mux) << 12); /* 100..111 */
    config |= (0x0 << 9);  /* PGA ±6.144 V */
    config |= (0x1 << 8);  /* single-shot */
    config |= (0x4 << 5);  /* 128 SPS */
    config |= 0x3;         /* disable comparator */

    esp_err_t err = write_reg16(ADS1115_REG_CONFIG, config);
    if (err != ESP_OK) {
        s_present = false;
        ESP_LOGW(TAG, "config write failed: %s", esp_err_to_name(err));
        return err;
    }

    /* 128 SPS ≈ 7.8 ms; wait with margin */
    vTaskDelay(pdMS_TO_TICKS(10));

    for (int i = 0; i < 10; ++i) {
        uint16_t cfg = 0;
        err = read_reg16(ADS1115_REG_CONFIG, &cfg);
        if (err != ESP_OK) {
            s_present = false;
            return err;
        }
        if (cfg & 0x8000) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    uint16_t raw_u = 0;
    err = read_reg16(ADS1115_REG_CONVERSION, &raw_u);
    if (err != ESP_OK) {
        s_present = false;
        return err;
    }

    int16_t raw = (int16_t)raw_u;
    float volts = ((float)raw / 32768.0f) * ADS1115_FSR_V;

    if (out_raw) {
        *out_raw = raw;
    }
    if (out_volts) {
        *out_volts = volts;
    }
    return ESP_OK;
}
