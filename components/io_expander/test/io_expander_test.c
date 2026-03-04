#include "io_expander_test.h"
#include "io_expander.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "IO_EXP_TEST";
static TaskHandle_t s_test_handle = NULL;
static bool s_run = false;


/**
 * @brief Funzione di test per l'espander I/O.
 * 
 * Questa funzione esegue i test per l'espander I/O.
 * 
 * @param arg Puntatore a dati di input per la funzione.
 * @return Nessun valore di ritorno.
 */
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


/**
 * @brief Avvia il test dell'espansore I/O.
 * 
 * Questa funzione inizia il processo di test per l'espansore I/O.
 * 
 * @return esp_err_t - Codice di errore che indica il successo o la fallita dell'operazione.
 */
esp_err_t io_expander_test_start(void) {
    if (s_test_handle) return ESP_ERR_INVALID_STATE;
    s_run = true;
    /* give the IO expander test a bit more stack in case logging
     * happens or the task is extended in future */
    xTaskCreate(io_expander_test_task, "ioexp_test", 4096, NULL, 5, &s_test_handle);
    return ESP_OK;
}


/**
 * @brief Arresta il test dell'espansore I/O.
 * 
 * Questa funzione si occupa di interrompere e pulire tutte le operazioni in corso
 * durante il test dell'espansore I/O, assicurandosi che tutte le risorse siano
 * rilasciate correttamente.
 * 
 * @return esp_err_t - Codice di errore che indica il successo o la fallita dell'operazione.
 */
esp_err_t io_expander_test_stop(void) {
    s_run = false;
    return ESP_OK;
}
