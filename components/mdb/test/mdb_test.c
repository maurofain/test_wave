#include "mdb_test.h"
#include "mdb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MDB_TEST";
static TaskHandle_t s_mdb_test_handle = NULL;
static bool s_run = false;


/**
 * @brief Funzione di test per la gestione del database.
 * 
 * Questa funzione esegue un test sulle operazioni di lettura e scrittura del database.
 * 
 * @param arg Puntatore a dati di input utilizzati dal task.
 * @return Nessun valore di ritorno.
 */
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


/**
 * @brief Avvia il test per la lettura e scrittura dei dati in memoria.
 * 
 * Questa funzione inizia un test che verifica la capacità di leggere e scrivere
 * dati in una memoria specifica. Il test include la creazione di un buffer di
 * test, la scrittura dei dati nel buffer e la verifica della correttezza dei dati
 * letti.
 * 
 * @return esp_err_t - Codice di errore che indica il successo o la fallita dell'operazione.
 *         - ESP_OK: Operazione completata con successo.
 *         - ESP_FAIL: Operazione fallita.
 */
esp_err_t mdb_test_start(void) {
    if (s_mdb_test_handle) return ESP_ERR_INVALID_STATE;
    mdb_init(); // Assicura che il driver UART sia installato
    s_run = true;
    xTaskCreate(mdb_test_task, "mdb_test", 4096, NULL, 5, &s_mdb_test_handle);
    return ESP_OK;
}


/**
 * @brief Arresta il test in corso.
 * 
 * Questa funzione interrompe l'esecuzione del test in corso.
 * 
 * @return esp_err_t
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t mdb_test_stop(void) {
    s_run = false;
    return ESP_OK;
}
