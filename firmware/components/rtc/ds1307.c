#include "ds1307.h"

#include "esp_log.h"
#include "i2c.h"

static const char *TAG = "ds1307";

#define DS1307_ADDR           0x68
#define DS1307_REG_SECONDS    0x00
#define DS1307_TIMEOUT_MS     500
/* DS1307 datasheet: standard-mode I2C only (max 100 kHz). */
#define DS1307_I2C_HZ         100000

static i2c_master_dev_handle_t s_dev = NULL;
static bool s_present = false;

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10) + (bcd & 0x0F));
}

static uint8_t bin_to_bcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10) << 4) | (bin % 10));
}

static esp_err_t ds1307_write(const uint8_t *data, size_t len)
{
    return i2c_master_transmit(s_dev, data, len, DS1307_TIMEOUT_MS);
}

static esp_err_t ds1307_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, DS1307_TIMEOUT_MS);
}

static void ds1307_scan_bus(void)
{
    i2c_master_bus_handle_t bus = DEV_I2C_Get_Bus();
    if (bus == NULL) {
        ESP_LOGW(TAG, "I2C scan skipped — bus not ready");
        return;
    }

    ESP_LOGI(TAG, "I2C scan (7-bit addresses):");
    int found = 0;
    for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
        esp_err_t err = i2c_master_probe(bus, addr, 100);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "  found 0x%02X%s", addr, (addr == DS1307_ADDR) ? "  <-- TinyRTC?" : "");
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "  no devices responded");
    }
}

esp_err_t ds1307_init(void)
{
    s_present = false;
    s_dev = NULL;

    if (DEV_I2C_Get_Bus() == NULL) {
        ESP_LOGE(TAG, "Shared I2C bus not initialized (touch bring-up must run first)");
        return ESP_ERR_INVALID_STATE;
    }

    ds1307_scan_bus();

    esp_err_t err = DEV_I2C_Add_Device(DS1307_ADDR, DS1307_I2C_HZ, &s_dev);
    if (err != ESP_OK || s_dev == NULL) {
        ESP_LOGE(TAG, "Failed to add DS1307 at 0x%02X / 100 kHz", DS1307_ADDR);
        return err != ESP_OK ? err : ESP_FAIL;
    }

    /* Probe with a simple write-read of seconds register. */
    uint8_t seconds = 0;
    err = ds1307_read_regs(DS1307_REG_SECONDS, &seconds, 1);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "TinyRTC no ACK at 0x%02X (%s). "
                 "Check wiring SDA=GPIO8 SCL=GPIO9 GND, power (DS1307 often needs 5V VCC), "
                 "and that module pull-ups are not forcing 5V onto the ESP32 bus.",
                 DS1307_ADDR, esp_err_to_name(err));
        return err;
    }

    s_present = true;

    /* Clear clock-halt bit if set so the oscillator can run. */
    if (seconds & 0x80) {
        uint8_t payload[2] = {DS1307_REG_SECONDS, (uint8_t)(seconds & 0x7F)};
        err = ds1307_write(payload, sizeof(payload));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Present but failed to clear CH bit: %s", esp_err_to_name(err));
            s_present = false;
            return err;
        }
        ESP_LOGI(TAG, "Cleared DS1307 clock-halt bit");
    }

    ESP_LOGI(TAG, "TinyRTC (DS1307) OK at 0x%02X @ 100 kHz", DS1307_ADDR);
    return ESP_OK;
}

bool ds1307_is_present(void)
{
    return s_present;
}

esp_err_t ds1307_get_time(ds1307_time_t *out_time)
{
    if (out_time == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_present || s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t raw[7] = {0};
    esp_err_t err = ds1307_read_regs(DS1307_REG_SECONDS, raw, sizeof(raw));
    if (err != ESP_OK) {
        s_present = false;
        ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(err));
        return err;
    }

    out_time->second = bcd_to_bin(raw[0] & 0x7F);
    out_time->minute = bcd_to_bin(raw[1] & 0x7F);
    /* Force 24h interpretation; clear 12h bit if a module left it set. */
    uint8_t hour_raw = raw[2];
    if (hour_raw & 0x40) {
        /* 12-hour mode in register — convert to 24h for UI. */
        uint8_t hour12 = bcd_to_bin(hour_raw & 0x1F);
        bool pm = (hour_raw & 0x20) != 0;
        if (hour12 == 12) {
            hour12 = 0;
        }
        out_time->hour = (uint8_t)(pm ? hour12 + 12 : hour12);
    } else {
        out_time->hour = bcd_to_bin(hour_raw & 0x3F);
    }
    out_time->day = bcd_to_bin(raw[4] & 0x3F);
    out_time->month = bcd_to_bin(raw[5] & 0x1F);
    out_time->year = (uint16_t)(2000 + bcd_to_bin(raw[6]));
    return ESP_OK;
}

esp_err_t ds1307_set_time(const ds1307_time_t *time)
{
    if (time == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_present || s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (time->year < 2000 || time->year > 2099 ||
        time->month < 1 || time->month > 12 ||
        time->day < 1 || time->day > 31 ||
        time->hour > 23 || time->minute > 59 || time->second > 59) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t payload[8] = {
        DS1307_REG_SECONDS,
        bin_to_bcd(time->second),          /* CH = 0 */
        bin_to_bcd(time->minute),
        bin_to_bcd(time->hour),            /* 24-hour mode */
        0x01,
        bin_to_bcd(time->day),
        bin_to_bcd(time->month),
        bin_to_bcd((uint8_t)(time->year - 2000)),
    };

    esp_err_t err = ds1307_write(payload, sizeof(payload));
    if (err != ESP_OK) {
        s_present = false;
        ESP_LOGE(TAG, "Write failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Time set to %04u-%02u-%02u %02u:%02u:%02u",
             time->year, time->month, time->day,
             time->hour, time->minute, time->second);
    return ESP_OK;
}
