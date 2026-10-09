#include "rs485.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/* Waveshare ESP32-S3-Touch-LCD-7B onboard SP3485:
 *   GPIO16 = UART TX -> transceiver DI
 *   GPIO15 = UART RX <- transceiver RO
 * Direction is automatic — do NOT drive a DE/RE GPIO. */
#define RS485_UART_NUM   UART_NUM_1
#define RS485_TX_GPIO    GPIO_NUM_16
#define RS485_RX_GPIO    GPIO_NUM_15
#define RS485_BAUD       9600
#define RS485_RX_BUF     512
#define RS485_TX_BUF     256

static const char *TAG = "rs485";
static SemaphoreHandle_t s_bus_mutex;
static bool s_ready = false;

esp_err_t rs485_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "RS485 init starting (UART1 TX=%d RX=%d, auto-DE)",
             RS485_TX_GPIO, RS485_RX_GPIO);

    s_bus_mutex = xSemaphoreCreateMutex();
    if (s_bus_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    uart_config_t uart_cfg = {
        .baud_rate = RS485_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(RS485_UART_NUM, RS485_RX_BUF, RS485_TX_BUF, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }
    err = uart_param_config(RS485_UART_NUM, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
        uart_driver_delete(RS485_UART_NUM);
        return err;
    }
    err = uart_set_pin(RS485_UART_NUM, RS485_TX_GPIO, RS485_RX_GPIO,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
        uart_driver_delete(RS485_UART_NUM);
        return err;
    }

    s_ready = true;
    ESP_LOGI(TAG, "UART1 RS485 ready TX=%d RX=%d @ %d 8N1 (onboard auto-DE)",
             RS485_TX_GPIO, RS485_RX_GPIO, RS485_BAUD);
    return ESP_OK;
}

esp_err_t rs485_transceive(const uint8_t *tx, size_t tx_len,
                           uint8_t *rx, size_t rx_capacity, size_t *rx_len,
                           uint32_t response_timeout_ms)
{
    if (!s_ready || tx == NULL || tx_len == 0 || rx == NULL || rx_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_bus_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    *rx_len = 0;
    uart_flush_input(RS485_UART_NUM);

    int written = uart_write_bytes(RS485_UART_NUM, tx, tx_len);
    uart_wait_tx_done(RS485_UART_NUM, pdMS_TO_TICKS(100));
    /* Allow auto-DE turnaround before listening. */
    vTaskDelay(1);

    if (written != (int)tx_len) {
        xSemaphoreGive(s_bus_mutex);
        return ESP_FAIL;
    }

    size_t got = 0;
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(response_timeout_ms);
    while (got < rx_capacity) {
        TickType_t now = xTaskGetTickCount();
        if (now >= deadline) {
            break;
        }
        int n = uart_read_bytes(RS485_UART_NUM, rx + got, rx_capacity - got,
                                deadline - now);
        if (n > 0) {
            got += (size_t)n;
            vTaskDelay(pdMS_TO_TICKS(5));
            size_t pending = 0;
            uart_get_buffered_data_len(RS485_UART_NUM, &pending);
            if (pending == 0) {
                break;
            }
        } else {
            break;
        }
    }

    *rx_len = got;
    xSemaphoreGive(s_bus_mutex);

    if (got == 0) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
