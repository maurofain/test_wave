#include "mdb.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "device_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "MDB";

#ifndef DNA_MDB
#define DNA_MDB 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_MDB == 0

#define MDB_UART_PORT   CONFIG_APP_MDB_UART_PORT
#define MDB_TX_GPIO     CONFIG_APP_MDB_TX_GPIO
#define MDB_RX_GPIO     CONFIG_APP_MDB_RX_GPIO
#define MDB_COIN_SETUP_MAX_RETRIES 5

static mdb_status_t s_mdb_status = {0};
static uint8_t s_coin_setup_retries = 0;

const mdb_status_t* mdb_get_status(void) {
    return &s_mdb_status;
}

// Helper per calcolare la parità di un byte (ritorna true se dispari)

/**
 * @brief Calcola la parità di un byte.
 * 
 * Questa funzione calcola la parità di un byte fornito come input.
 * La parità è definita come il numero di bit 1 nel byte. Se il numero di bit 1 è pari, la parità è pari; se è dispari, la parità è dispari.
 * 
 * @param [in] data Byte di cui calcolare la parità.
 * @return true se la parità è pari, false se la parità è dispari.
 */
static bool get_byte_parity(uint8_t data) {
    return __builtin_parity(data);
}

// Helper: Invia ACK

/**
 * @brief Invia un ACK (Acknowledgment) al dispositivo.
 *
 * Questa funzione invia un ACK al dispositivo per confermare la ricezione di un pacchetto.
 *
 * @param [in/out] Nessun parametro specifico.
 * @return Nessun valore di ritorno.
 */
static void mdb_send_ack(void) {
    uint8_t ack = MDB_ACK;
    // ACK è un data byte, quindi 9° bit = 0
    // Per avere 9° bit = 0 con dato 0x00 (parità pari): usiamo EVEN
    uart_set_parity(MDB_UART_PORT, UART_PARITY_EVEN);
    uart_write_bytes(MDB_UART_PORT, &ack, 1);
    uart_wait_tx_done(MDB_UART_PORT, pdMS_TO_TICKS(10));
}

// Logica per la Macchina a Stati della Gettoniera (Coin Acceptor)

/**
 * @brief Gestisce lo stato di coin per il sistema di monete.
 * 
 * Questa funzione si occupa di gestire lo stato delle monete inserite nel sistema.
 * 
 * @param [in/out] coin_state Stato corrente delle monete.
 * @return void Nessun valore di ritorno.
 */
static void mdb_coin_sm(void) {
    uint8_t rx[36];
    size_t rx_len;
    esp_err_t ret;

    switch (s_mdb_status.coin.state) {
        case MDB_STATE_INACTIVE:
            if (device_config_get()->mdb.coin_acceptor_en) s_mdb_status.coin.state = MDB_STATE_INIT_RESET;
            break;

        case MDB_STATE_INIT_RESET:
            ESP_LOGI(TAG, "Gettoniera MDB: Invio RESET...");
            mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_CMD_RESET, NULL, 0);
            vTaskDelay(pdMS_TO_TICKS(500)); // Aspetta reboot periferica
            s_coin_setup_retries = 0;
            s_mdb_status.coin.state = MDB_STATE_INIT_SETUP;
            break;

        case MDB_STATE_INIT_SETUP:
            ESP_LOGI(TAG, "Gettoniera MDB: Invio SETUP...");
            mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_CMD_SETUP, NULL, 0);
            if (mdb_receive_packet(rx, sizeof(rx), &rx_len, 100) == ESP_OK) {
                if (rx_len >= 23) {
                    s_coin_setup_retries = 0;
                    s_mdb_status.coin.feature_level = rx[0];
                    s_mdb_status.coin.currency_code = (rx[1] << 8) | rx[2];
                    s_mdb_status.coin.scaling_factor = rx[3];
                    s_mdb_status.coin.decimal_places = rx[4];
                    s_mdb_status.coin.coin_routing = (rx[5] << 8) | rx[6];
                    for (int i = 0; i < 16; i++) {
                        s_mdb_status.coin.coin_values[i] = rx[7 + i];
                    }
                    
                    ESP_LOGI(TAG, "Setup Gettoniera MDB: Livello %d, Valuta %04X, Fattore di Scala %d, Decimali %d", 
                             s_mdb_status.coin.feature_level, s_mdb_status.coin.currency_code,
                             s_mdb_status.coin.scaling_factor, s_mdb_status.coin.decimal_places);
                    
                    mdb_send_ack();
                    s_mdb_status.coin.state = MDB_STATE_INIT_ENABLE; // Passa alla fase di abilitazione
                    s_mdb_status.coin.is_online = true;
                } else {
                    ESP_LOGW(TAG, "Gettoniera MDB: Risposta di setup troppo breve (%zu)", rx_len);
                    s_coin_setup_retries++;
                }
            } else {
                s_coin_setup_retries++;
            }

            if (s_coin_setup_retries >= MDB_COIN_SETUP_MAX_RETRIES) {
                device_config_t *cfg = device_config_get();
                if (cfg) {
                    cfg->mdb.coin_acceptor_en = false;
                }
                s_mdb_status.coin.is_online = false;
                s_mdb_status.coin.state = MDB_STATE_INACTIVE;
                ESP_LOGE(TAG, "Gettoniera MDB: setup fallito %u volte, periferica disabilitata a runtime", (unsigned)s_coin_setup_retries);
            }
            break;

        case MDB_STATE_INIT_ENABLE:
            ESP_LOGI(TAG, "Gettoniera MDB: Abilitazione di tutti i tipi di monete...");
            {
                uint8_t enable_data[4] = {0xFF, 0xFF, 0xFF, 0xFF}; // Abilita tutto (Accept & Dispense)
                mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_COIN_CMD_COIN_TYPE, enable_data, 4);
            }
            if (mdb_receive_packet(rx, sizeof(rx), &rx_len, 50) == ESP_OK && rx[0] == MDB_ACK) {
                ESP_LOGI(TAG, "Gettoniera MDB: Abilitata!");
                s_mdb_status.coin.state = MDB_STATE_IDLE_POLLING;
            }
            break;

        case MDB_STATE_IDLE_POLLING:
            mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_CMD_POLL, NULL, 0);
            ret = mdb_receive_packet(rx, sizeof(rx), &rx_len, 20);
            if (ret == ESP_OK) {
                if (rx_len == 1 && rx[0] == MDB_ACK) {
                    // Tutto OK, nessun evento
                } else {
                    // Dati ricevuti (es. moneta inserita)
                    ESP_LOGI(TAG, "Evento Gettoniera MDB: Ricevuti %zu byte", rx_len);
                    mdb_send_ack();
                    
                    // Parsing eventi (solo i più importanti)
                    for (int i=0; i < (int)rx_len-1; i++) { // L'ultimo è il checksum
                        uint8_t b = rx[i];
                        if ((b & 0x80) == 0) { // Moneta accettata
                            uint8_t routing = (b >> 4) & 0x03; // 00: Cassetto, 01: Tubi, 02: Non Usato, 03: Rifiutata
                            uint8_t type = b & 0x0F;
                            uint16_t real_val = s_mdb_status.coin.coin_values[type] * s_mdb_status.coin.scaling_factor;
                            s_mdb_status.coin.credit_cents += real_val;
                            ESP_LOGI(TAG, "Gettoniera MDB: Moneta tipo %d accreditata (+%d unità), Routing: %d", type, real_val, routing);
                        } else if (b == 0x0B) { // Reset del Changer
                            ESP_LOGW(TAG, "Gettoniera MDB: Reset rilevato, ri-inizializzazione...");
                            s_mdb_status.coin.state = MDB_STATE_INIT_SETUP;
                        }
                    }
                }
                s_mdb_status.coin.is_online = true;
            } else {
                s_mdb_status.coin.is_online = false;
                ESP_LOGD(TAG, "Gettoniera MDB: Nessuna risposta");
            }
            break;
            
        default: break;
    }
}


/** @brief Avvia il motore di database.
 *  
 *  @param [in] arg Puntatore a dati di configurazione o contesto necessari per l'avvio del motore.
 *  
 *  @return Nessun valore di ritorno.
 */
void mdb_engine_run(void *arg) {
    ESP_LOGI(TAG, "Motore di polling MDB avviato");
    while (1) {
        mdb_coin_sm();
        // mdb_bill_sm(); // futuro
        vTaskDelay(pdMS_TO_TICKS(500)); // Ciclo di polling
    }
}


/**
 * @brief Avvia il motore del database.
 * 
 * Questa funzione avvia il motore del database e prepara l'ambiente per l'accesso ai dati.
 * 
 * @return esp_err_t
 * - ESP_OK: Operazione riuscita.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t mdb_start_engine(void) {
    /* Il task mdb_engine è ora gestito da tasks.c tramite mdb_engine_run().
     * Questa funzione è mantenuta per retrocompatibilità ma non crea il task. */
    ESP_LOGI(TAG, "mdb_start_engine: task gestito da tasks.c (mdb_engine_run)");
    return ESP_OK;
}


/**
 * @brief Inizializza il database.
 *
 * Questa funzione inizializza il database e prepara tutti i componenti necessari per l'uso successivo.
 *
 * @return
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t mdb_init(void)
{
    device_config_t *d_cfg = device_config_get();
    ESP_LOGI(TAG, "Inizializzazione MDB su UART %d (TX:%d RX:%d)", MDB_UART_PORT, MDB_TX_GPIO, MDB_RX_GPIO);

    uart_config_t uart_config = {
        .baud_rate = d_cfg->mdb_serial.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, // Gestito manualmente per il 9° bit
        .stop_bits = (d_cfg->mdb_serial.stop_bits == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(MDB_UART_PORT, d_cfg->mdb_serial.rx_buf_size, d_cfg->mdb_serial.tx_buf_size, 0, NULL, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;
    
    ESP_ERROR_CHECK(uart_param_config(MDB_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(MDB_UART_PORT, MDB_TX_GPIO, MDB_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Usiamo modalità RS485 Half Duplex per controllo RTS se necessario, 
    // ma MDB è solitamente TTL. Usiamo UART_MODE_UART o RS485_APP_CTRL.
    ESP_ERROR_CHECK(uart_set_mode(MDB_UART_PORT, UART_MODE_UART));

    return ESP_OK;
}


/**
 * @brief Invia un byte tramite la comunicazione MDB.
 *
 * @param [in] data Byte da inviare.
 * @param [in] mode_bit Bit di modalità da inviare.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t mdb_send_byte(uint8_t data, bool mode_bit)
{
    // Simulazione 9° bit tramite parità
    bool data_parity = get_byte_parity(data);
    
    if (mode_bit) {
        // Vogliamo 9° bit = 1
        // Se data_parity è even (0), impostiamo ODD per avere parity bit = 1
        // Se data_parity è odd (1), impostiamo EVEN per avere parity bit = 1
        if (!data_parity) uart_set_parity(MDB_UART_PORT, UART_PARITY_ODD);
        else uart_set_parity(MDB_UART_PORT, UART_PARITY_EVEN);
    } else {
        // Vogliamo 9° bit = 0
        // Se data_parity è even (0), impostiamo EVEN per avere parity bit = 0
        // Se data_parity è odd (1), impostiamo ODD per avere parity bit = 0
        if (!data_parity) uart_set_parity(MDB_UART_PORT, UART_PARITY_EVEN);
        else uart_set_parity(MDB_UART_PORT, UART_PARITY_ODD);
    }
    
    uart_write_bytes(MDB_UART_PORT, &data, 1);
    
    // Attendiamo che il byte sia trasmesso prima di cambiare parità per il prossimo
    uart_wait_tx_done(MDB_UART_PORT, pdMS_TO_TICKS(100));
    
    return ESP_OK;
}

esp_err_t mdb_send_raw_byte(uint8_t data, bool mode_bit)
{
    return mdb_send_byte(data, mode_bit);
}


/**
 * @brief Invia un pacchetto tramite la libreria MDB.
 *
 * @param [in] address L'indirizzo del dispositivo a cui inviare il pacchetto.
 * @param [in] data Un puntatore al buffer contenente i dati del pacchetto.
 * @param [in] len La lunghezza del buffer dei dati.
 *
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t mdb_send_packet(uint8_t address, const uint8_t *data, size_t len)
{
    uint8_t checksum = address;
    
    ESP_LOGD(TAG, "Invio pacchetto MDB: Indirizzo 0x%02X, Lunghezza %zu", address, len);

    // Svuota buffer RX prima di inviare
    uart_flush_input(MDB_UART_PORT);

    // 1. Invia Indirizzo (9° bit = 1)
    mdb_send_byte(address, true);

    // 2. Invia Dati (9° bit = 0)
    for (size_t i = 0; i < len; i++) {
        mdb_send_byte(data[i], false);
        checksum += data[i];
    }

    // 3. Invia Checksum (9° bit = 0)
    mdb_send_byte(checksum, false);

    return ESP_OK;
}


/**
 * @brief Riceve un pacchetto da una coda di messaggi.
 *
 * Questa funzione riceve un pacchetto da una coda di messaggi e lo memorizza in un buffer fornito.
 *
 * @param [out] out_data Puntatore al buffer dove memorizzare il pacchetto ricevuto.
 * @param max_len Lunghezza massima del buffer out_data.
 * @param [out] out_len Puntatore alla variabile dove memorizzare la lunghezza effettiva del pacchetto ricevuto.
 * @param timeout_ms Timeout in millisecondi per l'attesa del pacchetto.
 *
 * @return
 * - ESP_OK: Operazione riuscita.
 * - ESP_ERR_INVALID_ARG: Argomento non valido.
 * - ESP_ERR_TIMEOUT: Timeout scaduto.
 * - ESP_FAIL: Operazione non riuscita per altri motivi.
 */
esp_err_t mdb_receive_packet(uint8_t *out_data, size_t max_len, size_t *out_len, uint32_t timeout_ms)
{
    if (!out_data || !out_len) return ESP_ERR_INVALID_ARG;

    size_t received = 0;
    uint8_t byte;
    uint8_t checksum = 0;
    TickType_t start_tick = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(timeout_ms)) {
        if (uart_read_bytes(MDB_UART_PORT, &byte, 1, pdMS_TO_TICKS(timeout_ms)) > 0) {
            out_data[received++] = byte;
            
            while (received < max_len) {
                if (uart_read_bytes(MDB_UART_PORT, &byte, 1, pdMS_TO_TICKS(5)) > 0) {
                    out_data[received++] = byte;
                } else {
                    break; 
                }
            }
            break; 
        }
    }

    if (received == 0) return ESP_ERR_TIMEOUT;
    *out_len = received;

    if (received == 1) return ESP_OK;

    for (size_t i = 0; i < received - 1; i++) checksum += out_data[i];
    
    if (checksum != out_data[received - 1]) {
        ESP_LOGW(TAG, "Errore Checksum MDB: Calcolato 0x%02X != Ricevuto 0x%02X", checksum, out_data[received-1]);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

/**
 * @brief Segnala la fermata del Polling Engine MDB.
 *
 * Attualmente il task non supporta stop in-flight: questa funzione
 * rappresenta un hook futuro per inviare un segnale di stop al task.
 */
esp_err_t mdb_stop_engine(void)
{
    /* [C] TODO: implementare segnale di stop al task mdb_engine_run */
    return ESP_OK;
}

/**
 * @brief Restituisce lo stato operativo del driver MDB.
 *
 * Mappa is_online di coin/bill nell'enum condiviso hw_component_status_t.
 */
hw_component_status_t mdb_get_hw_status(void)
{
    const mdb_status_t *st = mdb_get_status();
    if (!st) return HW_STATUS_DISABLED;
    /* Se coin o bill è online il bus è operativo */
    if (st->coin.is_online || st->bill.is_online) return HW_STATUS_ONLINE;
    /* Se lo stato coin non è INACTIVE il driver è stato avviato ma offline */
    if (st->coin.state != MDB_STATE_INACTIVE) return HW_STATUS_OFFLINE;
    if (st->bill.state != MDB_STATE_INACTIVE) return HW_STATUS_OFFLINE;
    /* init chiamato ma engine non ancora avviato */
    return HW_STATUS_ENABLED;
}

#endif /* DNA_MDB == 0 */

/*
 * Mockup — nessuna UART MDB reale, nessuna periferica.
 * Attiva quando DNA_MDB == 1
 */
#if defined(DNA_MDB) && (DNA_MDB == 1)

static mdb_status_t s_mock_mdb_status = {0};

const mdb_status_t *mdb_get_status(void)
{
    return &s_mock_mdb_status;
}

/* [C] Mockup: stop_engine e get_hw_status simulati */
esp_err_t mdb_stop_engine(void) { return ESP_OK; }
hw_component_status_t mdb_get_hw_status(void) { return HW_STATUS_DISABLED; }


/**
 * @brief Inizializza il database.
 *
 * Questa funzione inizializza il database e prepara tutti i componenti necessari per l'uso successivo.
 *
 * @return
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t mdb_init(void)
{
    ESP_LOGI(TAG, "[C] [MOCK] mdb_init: bus MDB simulato");
    return ESP_OK;
}


/**
 * @brief Avvia il motore di database.
 *
 * Questa funzione avvia il motore di database e prepara l'ambiente per l'accesso ai dati.
 *
 * @return esp_err_t
 * - ESP_OK: Operazione riuscita.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t mdb_start_engine(void)
{
    ESP_LOGI(TAG, "[C] [MOCK] mdb_start_engine: polling disabilitato");
    return ESP_OK;
}


/**
 * @brief Avvia il motore del database.
 * 
 * Questa funzione avvia il motore del database e gestisce il suo ciclo di vita.
 * 
 * @param arg Puntatore a dati aggiuntivi necessari per l'avvio del motore.
 * @return Nessun valore di ritorno.
 */
void mdb_engine_run(void *arg)
{
    /* Mockup: MDB disabilitato — task in attesa indefinita */
    while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); }
}


/**
 * @brief Invia un pacchetto tramite la libreria MDB.
 *
 * @param [in] address L'indirizzo del dispositivo a cui inviare il pacchetto.
 * @param [in] data Un puntatore al buffer contenente i dati del pacchetto.
 * @param [in] len La lunghezza del buffer dei dati.
 *
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t mdb_send_packet(uint8_t address, const uint8_t *data, size_t len)
{
    ESP_LOGI(TAG, "[C] [MOCK] mdb_send_packet: addr=0x%02X len=%zu ignorato", address, len);
    (void)data;
    return ESP_OK;
}

esp_err_t mdb_send_raw_byte(uint8_t data, bool mode_bit)
{
    ESP_LOGI(TAG, "[C] [MOCK] mdb_send_raw_byte: data=0x%02X b9=%d ignorato", data, (int)mode_bit);
    return ESP_OK;
}


/**
 * @brief Riceve un pacchetto da una coda di messaggi.
 *
 * Questa funzione riceve un pacchetto da una coda di messaggi e lo memorizza in un buffer fornito.
 *
 * @param [out] out_data Puntatore al buffer dove memorizzare il pacchetto ricevuto.
 * @param max_len Lunghezza massima del buffer di destinazione.
 * @param [out] out_len Puntatore alla variabile dove memorizzare la lunghezza effettiva del pacchetto ricevuto.
 * @param timeout_ms Timeout in millisecondi per l'attesa del pacchetto.
 *
 * @return
 * - ESP_OK: Operazione riuscita.
 * - ESP_ERR_INVALID_ARG: Argomento non valido.
 * - ESP_ERR_TIMEOUT: Timeout scaduto.
 * - ESP_FAIL: Operazione non riuscita per altri motivi.
 */
esp_err_t mdb_receive_packet(uint8_t *out_data, size_t max_len, size_t *out_len, uint32_t timeout_ms)
{
    (void)out_data; (void)max_len; (void)timeout_ms;
    if (out_len) *out_len = 0;
    return ESP_ERR_TIMEOUT;
}

#endif /* DNA_MDB == 1 */
