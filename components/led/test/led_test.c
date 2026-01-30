#include "led_test.h"
#include "led.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "LED_TEST";
static TaskHandle_t s_led_test_handle = NULL;
static bool s_run = false;
static uint8_t s_test_brightness = 100;

static void led_test_task(void *arg) {
    ESP_LOGI(TAG, "Avvio Test Striscia LED WS2812 (Rainbow)");
    int hue = 0;
    uint32_t count = led_get_count();
    if (count == 0) count = 1; // Protezione divisione per zero

    while(s_run) {
        for (int i = 0; i < (int)count; i++) {
            int h = (hue + (i * 360 / (int)count)) % 360;
            led_set_pixel_hsv(i, h, 255, s_test_brightness);
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

esp_err_t led_test_set_brightness(uint8_t brightness) {
    s_test_brightness = (brightness > 100) ? 100 : brightness;
    return ESP_OK;
}

esp_err_t led_test_set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    if (s_run) {
        s_run = false;
        vTaskDelay(pdMS_TO_TICKS(50)); // Attendi che il task rainbow si chiuda
    }
    
    s_test_brightness = (brightness > 100) ? 100 : brightness;
    
    // Applica luminosità (0-100%)
    uint8_t final_r = (r * s_test_brightness) / 100;
    uint8_t final_g = (g * s_test_brightness) / 100;
    uint8_t final_b = (b * s_test_brightness) / 100;

    led_fill_color(final_r, final_g, final_b);
    return ESP_OK;
}
