#include "relay_modbus.h"

#include "esp_log.h"
#include "modbus_rtu.h"

static const char *TAG = "relay";
static relay_state_t s_state;
static uint8_t s_addr;

/* N4D8B08 FC06 command words: high byte = cmd, low byte = delay. */
#define N4D_CMD_OPEN        0x0100
#define N4D_CMD_CLOSE       0x0200
#define N4D_CMD_CLOSE_ALL   0x0800
#define N4D_REG_ALL         0x0000
#define N4D_REG_CH1         0x0001
#define N4D_STATUS_ON       0x0001

static esp_err_t channel_cmd(uint16_t channel, uint16_t cmd)
{
    if (s_addr == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = modbus_write_single_reg(s_addr, channel, cmd);
    if (err != ESP_OK) {
        s_state.online = false;
        ESP_LOGW(TAG, "ch %u cmd 0x%04X failed: %s", channel, cmd, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t set_channel(uint16_t channel, bool on)
{
    return channel_cmd(channel, on ? N4D_CMD_OPEN : N4D_CMD_CLOSE);
}

static esp_err_t read_channels(uint8_t addr, uint16_t *regs8)
{
    return modbus_read_holding_regs(addr, N4D_REG_CH1, 8, regs8);
}

static esp_err_t try_probe_addr(uint8_t addr, uint16_t *regs8)
{
    /* Prefer a safe Close-all, then read status. */
    esp_err_t err = modbus_write_single_reg(addr, N4D_REG_ALL, N4D_CMD_CLOSE_ALL);
    if (err == ESP_OK) {
        if (regs8 != NULL && read_channels(addr, regs8) != ESP_OK) {
            for (int i = 0; i < 8; ++i) {
                regs8[i] = 0;
            }
        }
        return ESP_OK;
    }
    return read_channels(addr, regs8);
}

static esp_err_t discover_addr(uint16_t *regs8)
{
    if (s_addr != 0) {
        esp_err_t err = read_channels(s_addr, regs8);
        if (err == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "lost relay at addr %u — re-probing", s_addr);
        s_addr = 0;
    }

    /*
     * DIP A0–A5 are raw binary (A0=1 … A3=8).
     * A0 ON → 1 (factory). A1+A3 → 10 (product). A0+A3 → 9 (common mis-set).
     */
    static const uint8_t k_candidates[] = {RELAY_MODBUS_ADDR, 10, 9};
    for (size_t i = 0; i < sizeof(k_candidates); ++i) {
        esp_err_t err = try_probe_addr(k_candidates[i], regs8);
        if (err == ESP_OK) {
            s_addr = k_candidates[i];
            ESP_LOGI(TAG, "N4D8B08 found at Modbus addr %u (FC03/06)", s_addr);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static void apply_regs(const uint16_t *regs8)
{
    s_state.elevator = (regs8[RELAY_CH_ELEVATOR - 1] == N4D_STATUS_ON);
    s_state.heater = (regs8[RELAY_CH_HEATER - 1] == N4D_STATUS_ON);
    s_state.fan = (regs8[RELAY_CH_FAN - 1] == N4D_STATUS_ON);
    s_state.door_open = (regs8[RELAY_CH_DOOR_OPEN - 1] == N4D_STATUS_ON);
    s_state.door_close = (regs8[RELAY_CH_DOOR_CLOSE - 1] == N4D_STATUS_ON);
}

esp_err_t relay_modbus_init(void)
{
    s_state = (relay_state_t){0};
    s_addr = 0;
    ESP_LOGI(TAG, "N4D8B08 driver ready (expect DIP A0=ON → addr %u)", RELAY_MODBUS_ADDR);
    return ESP_OK;
}

uint8_t relay_modbus_address(void)
{
    return s_addr;
}

esp_err_t relay_modbus_refresh(relay_state_t *out_state)
{
    uint16_t regs[8] = {0};
    esp_err_t err = discover_addr(regs);
    if (err != ESP_OK) {
        s_state.online = false;
        if (out_state) {
            *out_state = s_state;
        }
        return err;
    }

    s_state.online = true;
    apply_regs(regs);

    if (s_state.door_open && s_state.door_close) {
        ESP_LOGE(TAG, "R4 and R5 both ON — forcing both OFF");
        set_channel(RELAY_CH_DOOR_OPEN, false);
        set_channel(RELAY_CH_DOOR_CLOSE, false);
        s_state.door_open = false;
        s_state.door_close = false;
    }

    if (out_state) {
        *out_state = s_state;
    }
    return ESP_OK;
}

esp_err_t relay_modbus_set_elevator(bool on)
{
    esp_err_t err = set_channel(RELAY_CH_ELEVATOR, on);
    if (err == ESP_OK) {
        s_state.elevator = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_set_heater(bool on)
{
    esp_err_t err = set_channel(RELAY_CH_HEATER, on);
    if (err == ESP_OK) {
        s_state.heater = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_set_fan(bool on)
{
    esp_err_t err = set_channel(RELAY_CH_FAN, on);
    if (err == ESP_OK) {
        s_state.fan = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_open_door(void)
{
    esp_err_t err = set_channel(RELAY_CH_DOOR_CLOSE, false);
    if (err != ESP_OK) {
        return err;
    }
    s_state.door_close = false;
    err = set_channel(RELAY_CH_DOOR_OPEN, true);
    if (err == ESP_OK) {
        s_state.door_open = true;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_close_door(void)
{
    esp_err_t err = set_channel(RELAY_CH_DOOR_OPEN, false);
    if (err != ESP_OK) {
        return err;
    }
    s_state.door_open = false;
    err = set_channel(RELAY_CH_DOOR_CLOSE, true);
    if (err == ESP_OK) {
        s_state.door_close = true;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_stop_door(void)
{
    esp_err_t e1 = set_channel(RELAY_CH_DOOR_OPEN, false);
    esp_err_t e2 = set_channel(RELAY_CH_DOOR_CLOSE, false);
    if (e1 == ESP_OK && e2 == ESP_OK) {
        s_state.door_open = false;
        s_state.door_close = false;
        s_state.online = true;
        return ESP_OK;
    }
    return e1 != ESP_OK ? e1 : e2;
}

esp_err_t relay_modbus_all_off(void)
{
    if (s_addr == 0) {
        uint16_t regs[8] = {0};
        if (discover_addr(regs) != ESP_OK) {
            s_state.online = false;
            return ESP_ERR_NOT_FOUND;
        }
    }

    esp_err_t last = channel_cmd(N4D_REG_ALL, N4D_CMD_CLOSE_ALL);
    if (last != ESP_OK) {
        for (uint16_t ch = 1; ch <= 8; ++ch) {
            esp_err_t err = set_channel(ch, false);
            if (err != ESP_OK) {
                last = err;
                break;
            }
        }
    }
    /* Preserve last known coil state when the write fails. */
    if (last == ESP_OK) {
        s_state.elevator = false;
        s_state.heater = false;
        s_state.fan = false;
        s_state.door_open = false;
        s_state.door_close = false;
        s_state.online = true;
    } else {
        s_state.online = false;
        ESP_LOGW(TAG, "all_off failed — keeping last known coil state");
    }
    return last;
}
