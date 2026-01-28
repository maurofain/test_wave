#include "io_expander_test.h"
#include "io_expander.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "IO_EXP_TEST";
static TaskHandle_t s_test_handle = NULL;
static bool s_run = false;

static void io_expander_test_task(void *arg) {
    ESP_LOGI(TAG, "Avvio Test IO Expander (lampeggio 1Hz)");
    while(s_run) {
        io_set_port(0xFF);
        vTaskDelay(pdMS_TO_TICKS(500));
        io_set_port(0x00);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    s_test_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t io_expander_test_start(void) {
    if (s_test_handle) return ESP_ERR_INVALID_STATE;
    s_run = true;
    xTaskCreate(io_expander_test_task, "ioexp_test", 2048, NULL, 5, &s_test_handle);
    return ESP_OK;
}

esp_err_t io_expander_test_stop(void) {
    s_run = false;
    return ESP_OK;
}
