#include "relay_modbus.h"

#include "esp_log.h"
#include "modbus_rtu.h"

static const char *TAG = "relay";
static relay_state_t s_state;
/* 0 = not discovered yet. R4D8A08 with all address DIPs OFF is usually ID 1. */
static uint8_t s_addr;

static esp_err_t set_coil(uint16_t coil, bool on)
{
    if (s_addr == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = modbus_write_single_coil(s_addr, coil, on);
    if (err != ESP_OK) {
        s_state.online = false;
        ESP_LOGW(TAG, "coil %u -> %d failed: %s", coil, (int)on, esp_err_to_name(err));
    }
    return err;
}

static esp_err_t try_read_coils(uint8_t addr, uint16_t *bits)
{
    return modbus_read_coils(addr, 0, 8, bits);
}

static esp_err_t discover_addr(uint16_t *bits)
{
    /* All address DIPs OFF on R4D8A08 → Modbus ID 1. Product target is 10. */
    static const uint8_t k_candidates[] = {1, RELAY_MODBUS_ADDR};

    if (s_addr != 0) {
        esp_err_t err = try_read_coils(s_addr, bits);
        if (err == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "lost relay at addr %u — re-scanning", s_addr);
        s_addr = 0;
    }

    for (size_t i = 0; i < sizeof(k_candidates); ++i) {
        uint8_t addr = k_candidates[i];
        if (i > 0 && addr == k_candidates[0]) {
            continue;
        }
        esp_err_t err = try_read_coils(addr, bits);
        if (err == ESP_OK) {
            s_addr = addr;
            ESP_LOGI(TAG, "Relay board found at Modbus addr %u", s_addr);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t relay_modbus_init(void)
{
    /* Probe deferred to runtime — avoids boot WDT when the bus is empty. */
    s_state = (relay_state_t){0};
    s_addr = 0;
    ESP_LOGI(TAG, "Relay driver ready (will probe addr 1 / %u)", RELAY_MODBUS_ADDR);
    return ESP_OK;
}

uint8_t relay_modbus_address(void)
{
    return s_addr;
}

esp_err_t relay_modbus_refresh(relay_state_t *out_state)
{
    uint16_t bits = 0;
    esp_err_t err = discover_addr(&bits);
    if (err != ESP_OK) {
        s_state.online = false;
        if (out_state) {
            *out_state = s_state;
        }
        return err;
    }

    s_state.online = true;
    s_state.elevator = (bits & (1u << RELAY_COIL_ELEVATOR)) != 0;
    s_state.heater = (bits & (1u << RELAY_COIL_HEATER)) != 0;
    s_state.fan = (bits & (1u << RELAY_COIL_FAN)) != 0;
    s_state.door_open = (bits & (1u << RELAY_COIL_DOOR_OPEN)) != 0;
    s_state.door_close = (bits & (1u << RELAY_COIL_DOOR_CLOSE)) != 0;

    /* Enforce mutual exclusion if board reports both. */
    if (s_state.door_open && s_state.door_close) {
        ESP_LOGE(TAG, "R4 and R5 both ON — forcing both OFF");
        set_coil(RELAY_COIL_DOOR_OPEN, false);
        set_coil(RELAY_COIL_DOOR_CLOSE, false);
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
    esp_err_t err = set_coil(RELAY_COIL_ELEVATOR, on);
    if (err == ESP_OK) {
        s_state.elevator = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_set_heater(bool on)
{
    esp_err_t err = set_coil(RELAY_COIL_HEATER, on);
    if (err == ESP_OK) {
        s_state.heater = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_set_fan(bool on)
{
    esp_err_t err = set_coil(RELAY_COIL_FAN, on);
    if (err == ESP_OK) {
        s_state.fan = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_open_door(void)
{
    esp_err_t err = set_coil(RELAY_COIL_DOOR_CLOSE, false);
    if (err != ESP_OK) {
        return err;
    }
    s_state.door_close = false;
    err = set_coil(RELAY_COIL_DOOR_OPEN, true);
    if (err == ESP_OK) {
        s_state.door_open = true;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_close_door(void)
{
    esp_err_t err = set_coil(RELAY_COIL_DOOR_OPEN, false);
    if (err != ESP_OK) {
        return err;
    }
    s_state.door_open = false;
    err = set_coil(RELAY_COIL_DOOR_CLOSE, true);
    if (err == ESP_OK) {
        s_state.door_close = true;
        s_state.online = true;
    }
    return err;
}

esp_err_t relay_modbus_stop_door(void)
{
    esp_err_t e1 = set_coil(RELAY_COIL_DOOR_OPEN, false);
    esp_err_t e2 = set_coil(RELAY_COIL_DOOR_CLOSE, false);
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
        uint16_t bits = 0;
        if (discover_addr(&bits) != ESP_OK) {
            s_state.online = false;
            return ESP_ERR_NOT_FOUND;
        }
    }

    esp_err_t last = ESP_OK;
    for (uint16_t coil = 0; coil < 8; ++coil) {
        esp_err_t err = set_coil(coil, false);
        if (err != ESP_OK) {
            last = err;
            break;
        }
    }
    s_state.elevator = false;
    s_state.heater = false;
    s_state.fan = false;
    s_state.door_open = false;
    s_state.door_close = false;
    s_state.online = (last == ESP_OK);
    return last;
}
