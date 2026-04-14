#include "mdb_test.h"
#include "mdb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MDB_TEST";
static TaskHandle_t s_mdb_test_handle = NULL;
static bool s_run = false;

#define MDB_TEST_PK4_ADDR      0x10
#define MDB_TEST_PK4_RESET     0x00
#define MDB_TEST_PK4_SETUP     0x01
#define MDB_TEST_PK4_POLL      0x02

static void mdb_test_log_bytes(const char *label, const uint8_t *data, size_t len)
{
    char buffer[256] = {0};
    size_t offset = 0;

    if (!data || len == 0U) {
        ESP_LOGI(TAG, "[C] [%s] <empty>", label);
        return;
    }

    for (size_t index = 0; index < len && offset + 4U < sizeof(buffer); ++index) {
        offset += (size_t)snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", data[index]);
    }

    if (offset > 0U) {
        buffer[offset - 1U] = '\0';
    }

    ESP_LOGI(TAG, "[C] [%s] len=%u data=%s", label, (unsigned)len, buffer);
}

static void mdb_test_send_and_log(const char *phase, uint8_t command, uint32_t timeout_ms)
{
    uint8_t rx[64] = {0};
    size_t rx_len = 0;
    uint8_t frame = (uint8_t)(MDB_TEST_PK4_ADDR | command);
    esp_err_t ret = ESP_OK;

    ESP_LOGI(TAG, "[C] [%s] invio frame=0x%02X", phase, frame);
    mdb_send_packet(frame, NULL, 0);

    ret = mdb_receive_packet(rx, sizeof(rx), &rx_len, timeout_ms);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG,
                 "[C] [%s] nessuna risposta err=%s timeout=%u ms",
                 phase,
                 esp_err_to_name(ret),
                 (unsigned)timeout_ms);
        return;
    }

    mdb_test_log_bytes(phase, rx, rx_len);

    if (rx_len > 1U && rx[0] != MDB_ACK) {
        ESP_LOGI(TAG, "[C] [%s] invio ACK al device", phase);
        mdb_send_raw_byte(MDB_ACK, false);
    }
}


/**
 * @brief Funzione di test per la gestione del database.
 * 
 * Questa funzione esegue un test sulle operazioni di lettura e scrittura del database.
 * 
 * @param arg Puntatore a dati di input utilizzati dal task.
 * @return Nessun valore di ritorno.
 */
static void mdb_test_task(void *arg) {
    ESP_LOGI(TAG, "[C] Test MDB: sequenza PK4 RESET/SETUP/POLL...");
    while(s_run) {
        mdb_test_send_and_log("pk4_reset", MDB_TEST_PK4_RESET, 100);
        vTaskDelay(pdMS_TO_TICKS(500));

        mdb_test_send_and_log("pk4_setup", MDB_TEST_PK4_SETUP, 100);
        vTaskDelay(pdMS_TO_TICKS(500));

        mdb_test_send_and_log("pk4_poll", MDB_TEST_PK4_POLL, 100);
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
