#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "init.h"
#include "tasks.h"
#include "app_version.h"
#include "device_config.h"

static const char *TAG = "APP";

void app_main(void)
{
    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "  MODO: %s", device_config_get_running_app_name());
    ESP_LOGI(TAG, "  App Test Wave - Versione: %s", APP_VERSION);
    ESP_LOGI(TAG, "  Data Compilazione: %s", APP_DATE);
    ESP_LOGI(TAG, "==========================================");

    ESP_ERROR_CHECK(init_run_factory());
    tasks_load_config("/spiffs/tasks.csv");
    tasks_start_all();
    ESP_LOGI(TAG, "[M] App factory pronta: endpoint HTTP /status e /ota disponibili");
    
    // Loop principale: segnala attività periodicamente
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Attesa
        ESP_LOGI(TAG, "[M] Device in funzione ✓");
    }
}
