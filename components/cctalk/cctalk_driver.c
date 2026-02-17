#include "cctalk.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "CCTALK_DRV";

static void cctalk_task(void *arg)
{
    uint8_t src;
    uint8_t buf[256];
    uint8_t len;

    while (1) {
        if (cctalk_receive(&src, buf, &len, 500)) {
            ESP_LOGI(TAG, "RX from 0x%02X len=%d", src, len);
            // Push received bytes to serial monitor (sezione CCtalk)
            extern void serial_test_push_monitor_entry(const char*, const uint8_t*, size_t);
            serial_test_push_monitor_entry("CCTALK", buf, len);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t cctalk_driver_init(void)
{
    /* Force CCtalk to use dedicated pins: TX = GPIO20, RX = GPIO21 (per requirement)
       UART port is still taken from CONFIG_APP_RS232_UART_PORT */
    const int cctalk_tx_gpio = 20; /* TX */
    const int cctalk_rx_gpio = 21; /* RX */
    cctalk_init(CONFIG_APP_RS232_UART_PORT, cctalk_tx_gpio, cctalk_rx_gpio, 4800);

    if (xTaskCreate(cctalk_task, "cctalk_task", 2048, NULL, 5, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create cctalk_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "cctalk driver initialized on UART %d (tx=%d rx=%d)", CONFIG_APP_RS232_UART_PORT, cctalk_tx_gpio, cctalk_rx_gpio);
    return ESP_OK;
}
