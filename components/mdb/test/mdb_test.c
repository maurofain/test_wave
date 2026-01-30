#include "mdb_test.h"
#include "mdb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MDB_TEST";
static TaskHandle_t s_mdb_test_handle = NULL;
static bool s_run = false;

static void mdb_test_task(void *arg) {
    ESP_LOGI(TAG, "Test MDB: Invio sequenziale RESET/POLL...");
    while(s_run) {
        // Test di basso livello: invio pacchetto RESET alla gettoniera (0x08)
        mdb_send_packet(0x08 | 0x00, NULL, 0); 
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Test POLL
        mdb_send_packet(0x08 | 0x03, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    s_mdb_test_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t mdb_test_start(void) {
    if (s_mdb_test_handle) return ESP_ERR_INVALID_STATE;
    mdb_init(); // Assicura che il driver UART sia installato
    s_run = true;
    xTaskCreate(mdb_test_task, "mdb_test", 4096, NULL, 5, &s_mdb_test_handle);
    return ESP_OK;
}

esp_err_t mdb_test_stop(void) {
    s_run = false;
    return ESP_OK;
}
