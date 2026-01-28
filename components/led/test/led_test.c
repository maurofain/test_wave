#include "led_test.h"
#include "led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_TEST";
static TaskHandle_t s_led_test_handle = NULL;
static bool s_run = false;

static void led_test_task(void *arg) {
    ESP_LOGI(TAG, "Avvio Test Striscia LED WS2812 (Rainbow)");
    int hue = 0;
    while(s_run) {
        for (int i = 0; i < 16; i++) { // Assumiamo 16 LED per test
            int h = (hue + (i * 360 / 16)) % 360;
            led_set_pixel_hsv(i, h, 255, 100);
        }
        led_refresh();
        hue = (hue + 5) % 360;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    led_clear();
    s_led_test_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t led_test_start(void) {
    if (s_led_test_handle) return ESP_ERR_INVALID_STATE;
    s_run = true;
    xTaskCreate(led_test_task, "led_test", 4096, NULL, 5, &s_led_test_handle);
    return ESP_OK;
}

esp_err_t led_test_stop(void) {
    s_run = false;
    return ESP_OK;
}
