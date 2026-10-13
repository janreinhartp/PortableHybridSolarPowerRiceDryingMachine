#include "actuator_manager.h"

#include "esp_log.h"

static const char *TAG = "actuator_mgr";
static relay_state_t s_state {};

static esp_err_t ensure_online(void)
{
    esp_err_t err = relay_modbus_refresh(&s_state);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "relay not online: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t actuator_manager_init(void)
{
    s_state = {};
    ESP_LOGI(TAG, "Actuator Manager ready (sole relay writer)");
    return ESP_OK;
}

esp_err_t actuator_manager_refresh(relay_state_t *out_state)
{
    esp_err_t err = relay_modbus_refresh(&s_state);
    if (out_state) {
        *out_state = s_state;
    }
    return err;
}

uint8_t actuator_manager_relay_address(void)
{
    return relay_modbus_address();
}

void actuator_manager_get_state(relay_state_t *out_state)
{
    if (out_state) {
        *out_state = s_state;
    }
}

esp_err_t actuator_manager_set_fan(bool on)
{
    if (ensure_online() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = relay_modbus_set_fan(on);
    if (err == ESP_OK) {
        s_state.fan = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t actuator_manager_set_elevator(bool on)
{
    if (ensure_online() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = relay_modbus_set_elevator(on);
    if (err == ESP_OK) {
        s_state.elevator = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t actuator_manager_set_heater(bool on)
{
    if (ensure_online() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = relay_modbus_set_heater(on);
    if (err == ESP_OK) {
        s_state.heater = on;
        s_state.online = true;
    }
    return err;
}

esp_err_t actuator_manager_force_heater_off(void)
{
    return actuator_manager_set_heater(false);
}

esp_err_t actuator_manager_open_door(void)
{
    if (ensure_online() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = relay_modbus_open_door();
    if (err == ESP_OK) {
        s_state.door_open = true;
        s_state.door_close = false;
        s_state.online = true;
    }
    return err;
}

esp_err_t actuator_manager_close_door(void)
{
    if (ensure_online() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = relay_modbus_close_door();
    if (err == ESP_OK) {
        s_state.door_close = true;
        s_state.door_open = false;
        s_state.online = true;
    }
    return err;
}

esp_err_t actuator_manager_stop_door(void)
{
    if (ensure_online() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = relay_modbus_stop_door();
    if (err == ESP_OK) {
        s_state.door_open = false;
        s_state.door_close = false;
        s_state.online = true;
    }
    return err;
}

esp_err_t actuator_manager_all_off(void)
{
    if (ensure_online() != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Keep last known coil state if the Modbus write fails. */
    const relay_state_t prior = s_state;
    esp_err_t err = relay_modbus_all_off();
    if (err == ESP_OK) {
        s_state.elevator = false;
        s_state.heater = false;
        s_state.fan = false;
        s_state.door_open = false;
        s_state.door_close = false;
        s_state.online = true;
    } else {
        s_state = prior;
        s_state.online = false;
        ESP_LOGW(TAG, "all_off failed — preserving last known coil state");
    }
    return err;
}
