#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "init.h"
#include "tasks.h"

static const char *TAG = "APP";

void app_main(void)
{
    ESP_ERROR_CHECK(init_run_factory());
    tasks_load_config("/spiffs/tasks.csv");
    tasks_start_all();
    ESP_LOGI(TAG, "[F] App factory pronta: endpoint HTTP /status e /ota disponibili");
    
    // Loop principale: segnala attività ogni minuto
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Attendi 60 secondi
        ESP_LOGI(TAG, "[F] Device in funzione ✓");
    }
}
