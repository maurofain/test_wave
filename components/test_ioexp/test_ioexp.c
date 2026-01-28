#include "test_ioexp.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "TEST_IOEXP";

void test_ioexp_task(void *arg) {
    ESP_LOGI(TAG, "I/O Expander Test: Avvio");
    while(1) {
        uint8_t data_on[] = {0x02, 0xFF, 0xFF};
        i2c_master_write_to_device(CONFIG_APP_I2C_PORT, 0x20, data_on, 3, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(500));
        uint8_t data_off[] = {0x02, 0x00, 0x00};
        i2c_master_write_to_device(CONFIG_APP_I2C_PORT, 0x20, data_off, 3, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
