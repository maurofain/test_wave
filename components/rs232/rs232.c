#include "rs232.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "device_config.h"

static const char *TAG __attribute__((unused)) = "RS232";

#ifndef DNA_RS232
#define DNA_RS232 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_RS232 == 0

/* [C] Stato init UART per get_status */
static bool s_rs232_initialized = false;

/**
 * @brief Inizializza la porta RS232.
 * 
 * Utilizza i parametri caricati dalla configurazione del dispositivo.
 * Nota sul Buffer TX: 
 * - Se impostato a 0, la trasmissione è bloccante e scrive direttamente nella FIFO hardware.
 * - Se > 0, il driver utilizza un buffer circolare in RAM per trasmissioni asincrone.
 */
esp_err_t rs232_init(void) {
    s_rs232_initialized = false;
    device_config_t *d_cfg = device_config_get();
    
    uart_config_t cfg = {
        .baud_rate = d_cfg->rs232.baud_rate,
        .data_bits = (d_cfg->rs232.data_bits == 7) ? UART_DATA_7_BITS : UART_DATA_8_BITS,
        .parity = (d_cfg->rs232.parity == 1) ? UART_PARITY_ODD : (d_cfg->rs232.parity == 2 ? UART_PARITY_EVEN : UART_PARITY_DISABLE),
        .stop_bits = (d_cfg->rs232.stop_bits == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t ret = uart_driver_install(CONFIG_APP_RS232_UART_PORT, d_cfg->rs232.rx_buf_size, d_cfg->rs232.tx_buf_size, 0, NULL, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;
    ret = uart_param_config(CONFIG_APP_RS232_UART_PORT, &cfg);
    if (ret != ESP_OK) return ret;
    ret = uart_set_pin(CONFIG_APP_RS232_UART_PORT, CONFIG_APP_RS232_TX_GPIO, CONFIG_APP_RS232_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return ret;
    
    // Imposta il timeout di ricezione (RX TOUT) a 10ms. 
    // Calcoliamo i simboli in base al baud rate: (baud * 10ms) / (10 bits * 1000ms) = baud / 1000
    int tout_symbols = d_cfg->rs232.baud_rate / 1000;
    if (tout_symbols < 1) tout_symbols = 1;
    ret = uart_set_rx_timeout(CONFIG_APP_RS232_UART_PORT, tout_symbols);
    if (ret == ESP_OK) s_rs232_initialized = true;
    return ret;
}

/**
 * @brief Riceve dati dalla porta RS232.
 * 
 * La funzione termina e restituisce i dati in tre scenari:
 * 1. Raggiungimento di 'max_len': se arrivano esattamente i byte richiesti, restituisce immediatamente.
 * 2. Timeout Hardware (Fine Trama): se vengono ricevuti dei byte ma la linea rimane inattiva per 10ms 
 *    (configurati nell'init), la funzione termina e restituisce i byte accumulati finora.
 * 3. Timeout Software: se non arriva alcun dato per il tempo 'timeout_ms', la funzione restituisce 0.
 * 
 * @param data Buffer di destinazione
 * @param max_len Numero massimo di byte da leggere
 * @param timeout_ms Tempo massimo di attesa software in millisecondi
 * @return int Numero di byte effettivamente letti, o -1 in caso di errore.
 */
int rs232_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms) {
    return uart_read_bytes(CONFIG_APP_RS232_UART_PORT, data, max_len, pdMS_TO_TICKS(timeout_ms));
}

/**
 * @brief Invia dati sulla porta RS232.
 * 
 * Scrive i byte passati nel buffer di trasmissione della UART.
 * La funzione è bloccante finché non c'è spazio sufficiente nel buffer TX hardware.
 * 
 * @param data Puntatore ai dati da inviare
 * @param len Lunghezza dei dati in byte
 * @return int Numero di byte effettivamente inviati, o -1 in caso di errore.
 */
int rs232_send(const uint8_t *data, size_t len) {
    return uart_write_bytes(CONFIG_APP_RS232_UART_PORT, data, len);
}

/**
 * @brief Deinizializza il driver UART RS232.
 * @return ESP_OK se il driver è stato eliminato correttamente.
 */
esp_err_t rs232_deinit(void)
{
    esp_err_t ret = uart_driver_delete(CONFIG_APP_RS232_UART_PORT);
    if (ret == ESP_ERR_INVALID_STATE) return ESP_OK;
    return ret;
}

/**
 * @brief Restituisce lo stato operativo del driver RS232.
 *
 * Poiché RS232 non ha un task attivo né ACK hardware, si considera:
 *   DISABLED : uart_driver non installato
 *   ONLINE   : uart_driver installato (la porta è operativa)
 */
hw_component_status_t rs232_get_status(void)
{
    return s_rs232_initialized ? HW_STATUS_ONLINE : HW_STATUS_DISABLED;
}

#endif /* DNA_RS232 == 0 */

/*
 * Mockup — nessuna UART RS232 reale.
 * Attiva quando DNA_RS232 == 1
 */
#if defined(DNA_RS232) && (DNA_RS232 == 1)


/**
 * @brief Inizializza la comunicazione RS232.
 *
 * Questa funzione inizializza la comunicazione RS232, configurando i parametri di comunicazione
 * come la velocità di trasmissione, il numero di dati, i bit di parità e i bit di stop.
 *
 * @return esp_err_t
 *         - ESP_OK: Inizializzazione avvenuta con successo.
 *         - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t rs232_deinit(void) { return ESP_OK; }
hw_component_status_t rs232_get_status(void) { return HW_STATUS_DISABLED; }

esp_err_t rs232_init(void)
{
    ESP_LOGI(TAG, "[C] [MOCK] rs232_init: porta RS232 simulata");
    return ESP_OK;
}


/**
 * @brief Riceve dati tramite interfaccia RS-232.
 *
 * @param [out] data Puntatore al buffer dove memorizzare i dati ricevuti.
 * @param max_len Lunghezza massima del buffer.
 * @param timeout_ms Timeout in millisecondi per l'attesa della ricezione.
 * @return Numero di byte ricevuti, o -1 in caso di errore.
 */
int rs232_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms)
{
    (void)data; (void)max_len; (void)timeout_ms;
    return 0; /* nessun dato disponibile */
}


/**
 * @brief Invia dati tramite interfaccia RS-232.
 *
 * @param [in] data Puntatore ai dati da inviare.
 * @param [in] len Lunghezza dei dati da inviare.
 * @return int Numero di byte inviati, o un valore negativo in caso di errore.
 */
int rs232_send(const uint8_t *data, size_t len)
{
    ESP_LOGI(TAG, "[C] [MOCK] rs232_send: %zu byte ignorati", len);
    (void)data;
    return (int)len; /* simulazione invio riuscito */
}

#endif /* DNA_RS232 == 1 */
