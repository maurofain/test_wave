#include "cctalk.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#ifndef DNA_CCTALK
#define DNA_CCTALK 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_CCTALK == 0

#define CCTALK_UART_BUF_SIZE 512
#define CCTALK_FRAME_MAX_LEN 260

static const char *TAG = "CCTALK";
static int s_cctalk_uart_num = 1;
static bool s_cctalk_ready = false;
static SemaphoreHandle_t s_cctalk_lock = NULL;

extern void serial_test_push_monitor_entry(const char *label, const uint8_t *data, size_t len);
extern bool dump_cctalk_log;


/**
 * @brief Converte un timeout in millisecondi in ticks.
 * 
 * Questa funzione converte un timeout espresso in millisecondi in ticks utilizzando la frequenza del sistema.
 * 
 * @param [in] timeout_ms Il timeout in millisecondi da convertire.
 * @return Il timeout convertito in ticks.
 */
static TickType_t cctalk_ms_to_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        return 0;
    }
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if (ticks == 0) {
        ticks = 1;
    }
    return ticks;
}


/**
 * @brief Acquisisce un blocco sulla risorsa CCTalk.
 * 
 * Questa funzione tenta di acquisire un blocco sulla risorsa CCTalk utilizzando un timeout specifico.
 * 
 * @param timeout_ms Il timeout in millisecondi per l'acquisizione del blocco.
 * @return true Se il blocco è stato acquisito con successo.
 * @return false Se il blocco non è stato acquisito entro il timeout specificato.
 */
static bool cctalk_lock_take(uint32_t timeout_ms)
{
    if (!s_cctalk_lock) {
        return false;
    }
    return (xSemaphoreTake(s_cctalk_lock, cctalk_ms_to_ticks(timeout_ms)) == pdTRUE);
}


/**
 * @brief Rilascia il blocco del semaforo CCTalk.
 * 
 * Questa funzione rilascia il blocco del semaforo CCTalk, consentendo a altre
 * operazioni di accedere alla risorsa protetta.
 * 
 * @param [in/out] s_cctalk_lock Puntatore al semaforo CCTalk da rilasciare.
 * @return Niente.
 */
static void cctalk_lock_give(void)
{
    if (s_cctalk_lock) {
        xSemaphoreGive(s_cctalk_lock);
    }
}


/**
 * @brief Monitora un frame CCTalk.
 * 
 * Questa funzione monitora un frame CCTalk e gestisce eventuali errori.
 * 
 * @param [in] label Etichetta associata al frame.
 * @param [in] frame Puntatore al frame CCTalk da monitorare.
 * @return Nessun valore di ritorno.
 */
static void cctalk_monitor_frame(const char *label, const cctalk_frame_t *frame)
{
    if (!frame) {
        return;
    }

    uint16_t frame_len = (uint16_t)frame->data_len + 5U;
    if (frame_len > CCTALK_FRAME_MAX_LEN) {
        return;
    }

    uint8_t raw[CCTALK_FRAME_MAX_LEN] = {0};
    raw[0] = frame->destination;
    raw[1] = frame->data_len;
    raw[2] = frame->source;
    raw[3] = frame->header;
    if (frame->data_len > 0U) {
        memcpy(&raw[4], frame->data, frame->data_len);
    }
    raw[frame_len - 1U] = cctalk_checksum(raw, (uint8_t)(frame_len - 1U));

    serial_test_push_monitor_entry(label, raw, frame_len);
}

static void cctalk_log_hex_dump(const char *direction, const uint8_t *data, size_t len)
{
    if (!dump_cctalk_log) {
        return;
    }

    if (!direction || !data || len == 0U) {
        return;
    }

    const size_t bytes_per_line = 32U;
    char line[(32U * 3U)] = {0};

    for (size_t offset = 0; offset < len; offset += bytes_per_line) {
        size_t chunk = len - offset;
        if (chunk > bytes_per_line) {
            chunk = bytes_per_line;
        }

        size_t pos = 0;
        for (size_t i = 0; i < chunk; ++i) {
            size_t remaining = sizeof(line) - pos;
            if (remaining <= 1U) {
                break;
            }
            int wrote = snprintf(line + pos,
                                 remaining,
                                 "%02X%s",
                                 data[offset + i],
                                 (i + 1U < chunk) ? " " : "");
            if (wrote < 0) {
                break;
            }
            size_t wrote_size = (size_t)wrote;
            if (wrote_size >= remaining) {
                pos = sizeof(line) - 1U;
                break;
            }
            pos += wrote_size;
        }
        line[sizeof(line) - 1U] = '\0';

        ESP_LOGI(TAG,
                 "[C] CCTALK %s HEX %u/%u: %s",
                 direction,
                 (unsigned)(offset + chunk),
                 (unsigned)len,
                 line);
    }
}


/**
 * @brief Invia un frame CCTalk senza bloccare.
 * 
 * @param dest_addr [in] Indirizzo del destinatario.
 * @param src_addr [in] Indirizzo del mittente.
 * @param header [in] Header del frame.
 * @param data [in] Puntatore ai dati del frame.
 * @param len [in] Lunghezza dei dati.
 * @param monitor_tx [in] Flag per monitorare la trasmissione.
 * @return true Se la trasmissione è stata avviata con successo.
 * @return false Se la trasmissione non è stata avviata.
 */
static bool cctalk_send_frame_unlocked(uint8_t dest_addr,
                                       uint8_t src_addr,
                                       uint8_t header,
                                       const uint8_t *data,
                                       uint8_t len,
                                       bool monitor_tx)
{
    if (!s_cctalk_ready) {
        return false;
    }

    uint16_t frame_len = (uint16_t)len + 5U;
    uint8_t packet[CCTALK_FRAME_MAX_LEN] = {0};

    packet[0] = dest_addr;
    packet[1] = len;
    packet[2] = src_addr;
    packet[3] = header;
    if (len > 0U && data) {
        memcpy(&packet[4], data, len);
    }
    packet[frame_len - 1U] = cctalk_checksum(packet, (uint8_t)(frame_len - 1U));

    cctalk_log_hex_dump("TX", packet, frame_len);

    int sent = uart_write_bytes(s_cctalk_uart_num, (const char *)packet, (size_t)frame_len);
    if (sent != (int)frame_len) {
        return false;
    }

    (void)uart_wait_tx_done(s_cctalk_uart_num, pdMS_TO_TICKS(50));

    if (monitor_tx) {
        serial_test_push_monitor_entry("CCTALK_TX", packet, frame_len);
    }

    return true;
}


/**
 * @brief Riceve un frame CCTalk in modalità non bloccante.
 * 
 * @param [in/out] frame Puntatore al frame CCTalk da ricevere.
 * @param timeout_ms Timeout in millisecondi per la ricezione del frame.
 * @param monitor_rx Flag per attivare la monitorizzazione del ricevitore.
 * @return true Se il frame è stato ricevuto con successo.
 * @return false Se il frame non è stato ricevuto o si è verificato un errore.
 */
static bool cctalk_receive_frame_unlocked(cctalk_frame_t *frame, uint32_t timeout_ms, bool monitor_rx)
{
    if (!s_cctalk_ready || !frame) {
        return false;
    }

    uint8_t raw[CCTALK_FRAME_MAX_LEN] = {0};
    uint16_t idx = 0;
    uint16_t expected_len = 0;
    int64_t deadline_us = esp_timer_get_time() + ((int64_t)timeout_ms * 1000LL);

    while (esp_timer_get_time() < deadline_us) {
        uint8_t byte = 0;
        int read = uart_read_bytes(s_cctalk_uart_num, &byte, 1, pdMS_TO_TICKS(10));
        if (read != 1) {
            continue;
        }

        if (idx >= CCTALK_FRAME_MAX_LEN) {
            idx = 0;
            expected_len = 0;
        }

        raw[idx++] = byte;

        if (idx == 2U) {
            expected_len = (uint16_t)raw[1] + 5U;
            if (expected_len < 5U || expected_len > CCTALK_FRAME_MAX_LEN) {
                idx = 0;
                expected_len = 0;
            }
            continue;
        }

        if (expected_len > 0U && idx == expected_len) {
            uint8_t computed = cctalk_checksum(raw, (uint8_t)(expected_len - 1U));
            uint8_t received = raw[expected_len - 1U];
            if (computed != received) {
                idx = 0;
                expected_len = 0;
                continue;
            }

            cctalk_log_hex_dump("RX", raw, expected_len);

            frame->destination = raw[0];
            frame->data_len = raw[1];
            frame->source = raw[2];
            frame->header = raw[3];
            if (frame->data_len > 0U) {
                memcpy(frame->data, &raw[4], frame->data_len);
            }

            if (monitor_rx) {
                serial_test_push_monitor_entry("CCTALK_RX", raw, expected_len);
            }
            return true;
        }
    }

    return false;
}


/**
 * @brief Copia una risposta ASCII da un frame CCTalk in un buffer di output.
 * 
 * @param [in] response Puntatore al frame CCTalk contenente la risposta ASCII.
 * @param [out] out Buffer di output dove verrà copiata la risposta ASCII.
 * @param [in] out_len Dimensione del buffer di output.
 * @return true Se la copia è stata effettuata con successo.
 * @return false Se la copia non è stata effettuata a causa di parametri non validi o di una risposta vuota.
 */
static bool cctalk_copy_ascii_response(const cctalk_frame_t *response, char *out, size_t out_len)
{
    if (!response || !out || out_len == 0U || response->data_len == 0U) {
        return false;
    }

    size_t copy_len = response->data_len;
    if (copy_len >= out_len) {
        copy_len = out_len - 1U;
    }

    memcpy(out, response->data, copy_len);
    out[copy_len] = '\0';

    for (size_t i = 0; i < copy_len; ++i) {
        unsigned char ch = (unsigned char)out[i];
        if (ch < 32U || ch > 126U) {
            out[i] = '.';
        }
    }

    return true;
}


/**
 * @brief Controlla se la risposta contiene un ACK (Acknowledgment).
 *
 * Questa funzione verifica se la risposta ricevuta dal dispositivo CCTalk contiene un ACK.
 *
 * @param [in] response Puntatore alla struttura contenente la risposta ricevuta.
 * @return true se la risposta contiene un ACK, false altrimenti.
 */
static bool cctalk_expect_ack(const cctalk_frame_t *response)
{
    return (response && response->header == 0U);
}


/**
 * @brief Inizializza la libreria CCTalk.
 * 
 * @param [in] uart_num Numero del canale UART da utilizzare.
 * @param [in] tx_pin Numero del pin TX da utilizzare.
 * @param [in] rx_pin Numero del pin RX da utilizzare.
 * @param [in] baudrate Velocità di comunicazione in baud.
 * @return Nessun valore di ritorno.
 */
void cctalk_init(int uart_num, int tx_pin, int rx_pin, int baudrate)
{
    if (!s_cctalk_lock) {
        s_cctalk_lock = xSemaphoreCreateMutex();
    }

    if (s_cctalk_ready && s_cctalk_uart_num == uart_num) {
        ESP_LOGI(TAG, "[C] cctalk_init già attivo su UART=%d", uart_num);
        return;
    }

    if (s_cctalk_uart_num != uart_num && uart_is_driver_installed(s_cctalk_uart_num)) {
        (void)uart_driver_delete(s_cctalk_uart_num);
    }

    s_cctalk_uart_num = uart_num;

    if (uart_is_driver_installed(s_cctalk_uart_num)) {
        (void)uart_driver_delete(s_cctalk_uart_num);
    }

    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(s_cctalk_uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] uart_param_config fallita: %s", esp_err_to_name(err));
        s_cctalk_ready = false;
        return;
    }

    err = uart_set_pin(s_cctalk_uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] uart_set_pin fallita: %s", esp_err_to_name(err));
        s_cctalk_ready = false;
        return;
    }

    err = uart_driver_install(s_cctalk_uart_num, CCTALK_UART_BUF_SIZE, CCTALK_UART_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] uart_driver_install fallita: %s", esp_err_to_name(err));
        s_cctalk_ready = false;
        return;
    }

    (void)uart_flush_input(s_cctalk_uart_num);
    s_cctalk_ready = true;
}


/**
 * @brief Invia un frame CCTalk.
 * 
 * @param [in] dest_addr L'indirizzo del destinatario.
 * @param [in] src_addr L'indirizzo del mittente.
 * @param [in] header Il codice di intestazione del frame.
 * @param [in] data Un puntatore al buffer contenente i dati del frame.
 * @param [in] len La lunghezza del buffer dei dati.
 * @return true Se il frame è stato inviato con successo.
 * @return false Se il lock non è stato acquisito entro il timeout.
 */
bool cctalk_send_frame(uint8_t dest_addr,
                       uint8_t src_addr,
                       uint8_t header,
                       const uint8_t *data,
                       uint8_t len)
{
    if (!cctalk_lock_take(50)) {
        return false;
    }

    bool ok = cctalk_send_frame_unlocked(dest_addr, src_addr, header, data, len, true);
    cctalk_lock_give();
    return ok;
}


/**
 * @brief Riceve un frame CCTalk.
 * 
 * Questa funzione riceve un frame CCTalk dal bus CCTalk.
 * 
 * @param [in/out] frame Puntatore al buffer dove verrà memorizzato il frame ricevuto.
 * @param timeout_ms Timeout in millisecondi per l'acquisizione del lock.
 * @return true se il frame è stato ricevuto con successo, false altrimenti.
 */
bool cctalk_receive_frame(cctalk_frame_t *frame, uint32_t timeout_ms)
{
    if (!cctalk_lock_take(timeout_ms)) {
        return false;
    }

    bool ok = cctalk_receive_frame_unlocked(frame, timeout_ms, true);
    cctalk_lock_give();
    return ok;
}


/**
 * @brief Invia un comando CCTalk a un dispositivo.
 * 
 * @param [in] dest_addr L'indirizzo destinatario del comando.
 * @param [in] src_addr L'indirizzo sorgente del comando.
 * @param [in] header Il codice header del comando.
 * @param [in] data Un puntatore al buffer contenente i dati del comando.
 * @param [in] len La lunghezza del buffer dei dati.
 * @param [out] response Un puntatore al buffer dove verrà memorizzata la risposta del dispositivo.
 * @param [in] timeout_ms Il timeout in millisecondi per l'acquisizione del lock.
 * @return true Se il comando è stato inviato con successo.
 * @return false Se il comando non è stato inviato a causa di un timeout.
 */
bool cctalk_command(uint8_t dest_addr,
                    uint8_t src_addr,
                    uint8_t header,
                    const uint8_t *data,
                    uint8_t len,
                    cctalk_frame_t *response,
                    uint32_t timeout_ms)
{
    if (!cctalk_lock_take(timeout_ms)) {
        return false;
    }

    (void)uart_flush_input(s_cctalk_uart_num);

    bool monitor_traffic = (header != 229U);
    bool tx_ok = cctalk_send_frame_unlocked(dest_addr, src_addr, header, data, len, monitor_traffic);
    if (!tx_ok) {
        cctalk_lock_give();
        return false;
    }

    int64_t start_us = esp_timer_get_time();
    int64_t timeout_us = (int64_t)timeout_ms * 1000LL;

    while ((esp_timer_get_time() - start_us) < timeout_us) {
        int64_t elapsed_us = esp_timer_get_time() - start_us;
        uint32_t remaining_ms = (uint32_t)((timeout_us - elapsed_us) / 1000LL);
        if (remaining_ms == 0U) {
            remaining_ms = 1U;
        }

        cctalk_frame_t rx = {0};
        if (!cctalk_receive_frame_unlocked(&rx, remaining_ms, false)) {
            continue;
        }

        if (rx.source == src_addr && rx.destination == dest_addr) {
            continue;
        }

        if (rx.source != dest_addr || rx.destination != src_addr) {
            continue;
        }

        if (monitor_traffic) {
            cctalk_monitor_frame("CCTALK_RX", &rx);
        }

        if (response) {
            *response = rx;
        }

        cctalk_lock_give();
        return true;
    }

    cctalk_lock_give();
    return false;
}


/**
 * @brief Invia dati tramite la protocollo CCTalk.
 * 
 * @param [in] dest_addr L'indirizzo del destinatario.
 * @param [in] data Un puntatore ai dati da inviare.
 * @param [in] len La lunghezza dei dati da inviare.
 * @return true Se l'invio è stato avviato con successo.
 * @return false Se l'invio non è stato avviato, ad esempio perché i dati sono nulli o la lunghezza è zero.
 */
bool cctalk_send(uint8_t dest_addr, const uint8_t *data, uint8_t len)
{
    if (!data || len == 0U) {
        return false;
    }

    uint8_t header = data[0];
    const uint8_t *payload = (len > 1U) ? &data[1] : NULL;
    uint8_t payload_len = (len > 1U) ? (uint8_t)(len - 1U) : 0U;
    return cctalk_send_frame(dest_addr, CCTALK_MASTER_ADDRESS, header, payload, payload_len);
}


/**
 * @brief Riceve dati tramite il protocollo CCTalk.
 * 
 * @param [in] src_addr Puntatore all'indirizzo sorgente.
 * @param [out] data Puntatore al buffer dove memorizzare i dati ricevuti.
 * @param [out] len Puntatore alla lunghezza del buffer dei dati ricevuti.
 * @param timeout_ms Timeout in millisecondi per l'attesa della ricezione.
 * @return true Se la ricezione è stata completata con successo.
 * @return false Se la ricezione ha fallito o è stata interrotta.
 */
bool cctalk_receive(uint8_t *src_addr, uint8_t *data, uint8_t *len, uint32_t timeout_ms)
{
    if (!src_addr || !data || !len) {
        return false;
    }

    cctalk_frame_t frame = {0};
    if (!cctalk_receive_frame(&frame, timeout_ms)) {
        return false;
    }

    *src_addr = frame.source;
    *len = (uint8_t)(frame.data_len + 1U);
    data[0] = frame.header;
    if (frame.data_len > 0U) {
        memcpy(&data[1], frame.data, frame.data_len);
    }
    return true;
}


/**
 * @brief Calcola il checksum di un pacchetto CCTalk.
 *
 * Questa funzione calcola il checksum di un pacchetto CCTalk utilizzando l'algoritmo specifico.
 *
 * @param [in] packet Puntatore al pacchetto di dati per cui calcolare il checksum.
 * @param [in] len Lunghezza del pacchetto di dati.
 * @return Il valore del checksum calcolato.
 */
uint8_t cctalk_checksum(const uint8_t *packet, uint8_t len)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; ++i) {
        sum = (uint16_t)(sum + packet[i]);
    }
    return (uint8_t)(-sum);
}


/**
 * @brief Invia un comando di sondaggio all'indirizzo CCTalk specificato.
 *
 * @param [in] dest_addr Indirizzo CCTalk del dispositivo da sondare.
 * @param [in] timeout_ms Timeout in millisecondi per l'attesa della risposta.
 * @return true se la richiesta è stata inviata con successo, false altrimenti.
 */
bool cctalk_address_poll(uint8_t dest_addr, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 254U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}


/**
 * @brief Invia una richiesta di stato al dispositivo CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del dispositivo CCTalk.
 * @param [out] status Un puntatore a una variabile dove verrà memorizzato lo stato del dispositivo.
 * @param [in] timeout_ms Il timeout in millisecondi per l'attesa della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!status) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 248U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 1U) {
        return false;
    }
    *status = response.data[0];
    return true;
}


/**
 * @brief Invia una richiesta per ottenere l'ID del produttore tramite il protocollo CCTalk.
 *
 * @param dest_addr [in] Indirizzo destinatario del messaggio.
 * @param out [out] Buffer per memorizzare la risposta ricevuta.
 * @param out_len [in] Dimensione del buffer di output.
 * @param timeout_ms [in] Timeout in millisecondi per l'attesa della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_manufacturer_id(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 246U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}


/**
 * @brief Invia una richiesta di categoria di equipaggiamento CCTalk.
 *
 * @param dest_addr [in] Indirizzo destinatario del messaggio.
 * @param out [out] Buffer per la ricezione della risposta.
 * @param out_len [in] Dimensione del buffer di output.
 * @param timeout_ms [in] Timeout in millisecondi per la ricezione della risposta.
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_equipment_category(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 245U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}


/**
 * @brief Invia una richiesta di codice prodotto al dispositivo CCTalk.
 *
 * @param dest_addr [in] Indirizzo destinatario del dispositivo CCTalk.
 * @param out [out] Buffer per contenere il codice prodotto ricevuto.
 * @param out_len [in] Dimensione del buffer di output.
 * @param timeout_ms [in] Timeout in millisecondi per l'attesa della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false altrimenti.
 */
bool cctalk_request_product_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 244U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}


/**
 * @brief Invia una richiesta per ottenere il numero di serie di un dispositivo CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del dispositivo CCTalk.
 * @param [out] serial Un array di 3 byte che riceverà il numero di serie del dispositivo.
 * @param [in] timeout_ms Il timeout in millisecondi per l'attesa della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_serial_number(uint8_t dest_addr, uint8_t serial[3], uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!serial) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 242U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 3U) {
        return false;
    }
    serial[0] = response.data[0];
    serial[1] = response.data[1];
    serial[2] = response.data[2];
    return true;
}


/**
 * @brief Costruisce il codice di richiesta CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario della richiesta.
 * @param [out] out Buffer in cui memorizzare il codice di richiesta.
 * @param [in] out_len Dimensione del buffer di output.
 * @param [in] timeout_ms Timeout in millisecondi per la richiesta.
 * @return true se la richiesta è stata costruita con successo, false altrimenti.
 */
bool cctalk_request_build_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 192U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}


/**
 * @brief Abilita o disabilita l'inibizione del dispositivo master.
 *
 * @param [in] dest_addr L'indirizzo del dispositivo destinatario.
 * @param [in] enable Flag booleano per abilitare (true) o disabilitare (false) l'inibizione.
 * @param [in] timeout_ms Il timeout in millisecondi per l'operazione.
 * @return true se l'operazione ha successo, false in caso di errore.
 */
bool cctalk_modify_master_inhibit(uint8_t dest_addr, bool enable, uint32_t timeout_ms)
{
    uint8_t data[2];
    /* invia maschera a 2 byte: low = CH1..8, high = CH9..16 */
    if (enable) {
        /* abilitare tutti i canali => maschera 0x00 0x00 */
        data[0] = 0x00U;
        data[1] = 0x00U;
    } else {
        /* inibire tutti i canali => maschera 0xFF 0xFF */
        data[0] = 0xFFU;
        data[1] = 0xFFU;
    }
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 231U, data, 2U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}


/**
 * @brief Imposta la maschera di inibizione canali della gettoniera.
 *
 * @param [in] dest_addr Indirizzo del dispositivo destinatario.
 * @param [in] mask_low Maschera bit canali 1..8.
 * @param [in] mask_high Maschera bit canali 9..16.
 * @param [in] timeout_ms Timeout in millisecondi per l'operazione.
 * @return true se il comando è stato accettato, false altrimenti.
 */
bool cctalk_modify_inhibit_status(uint8_t dest_addr, uint8_t mask_low, uint8_t mask_high, uint32_t timeout_ms)
{
    uint8_t data[2] = {mask_low, mask_high};
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 231U, data, 2U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}


/**
 * @brief Richiede lo stato di inibizione CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario della richiesta.
 * @param [out] mask_low Un puntatore al buffer dove verrà memorizzato il byte basso della maschera.
 * @param [out] mask_high Un puntatore al buffer dove verrà memorizzato il byte alto della maschera.
 * @param [in] timeout_ms Il timeout in millisecondi per la richiesta.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_inhibit_status(uint8_t dest_addr, uint8_t *mask_low, uint8_t *mask_high, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!mask_low || !mask_high) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 230U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 2U) {
        return false;
    }
    *mask_low = response.data[0];
    *mask_high = response.data[1];
    return true;
}


/**
 * @brief Invia una richiesta per ottenere l'ID della moneta utilizzando il protocollo CCTalk.
 *
 * @param dest_addr [in] L'indirizzo destinatario del dispositivo CCTalk.
 * @param channel [in] Il canale su cui inviare la richiesta.
 * @param out [out] Un buffer per contenere la risposta ricevuta.
 * @param out_len [in] La dimensione del buffer di output.
 * @param timeout_ms [in] Il tempo massimo di attesa per una risposta in millisecondi.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_coin_id(uint8_t dest_addr, uint8_t channel, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    uint8_t data = channel;
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 184U, &data, 1U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}


/**
 * @brief Modifica i percorsi del sorter tramite il protocollo CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del comando.
 * @param [in] paths Un puntatore al buffer contenente i percorsi da modificare.
 * @param [in] len La lunghezza del buffer dei percorsi.
 * @param [in] timeout_ms Il timeout in millisecondi per l'attesa della risposta.
 * @return true se la modifica è stata avvenuta con successo, false altrimenti.
 */
bool cctalk_modify_sorter_paths(uint8_t dest_addr, const uint8_t *paths, uint8_t len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (len > 0U && !paths) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 210U, paths, len, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}


/**
 * @brief Legge dati dal buffer CCTalk in modo buffered.
 *
 * @param [in] dest_addr L'indirizzo del dispositivo di destinazione.
 * @param [out] buffer Puntatore al buffer in cui memorizzare i dati letti.
 * @param [in] timeout_ms Il timeout in millisecondi per l'operazione di lettura.
 *
 * @return true se la lettura è stata completata con successo, false in caso di errore.
 */
bool cctalk_read_buffered_credit(uint8_t dest_addr, cctalk_buffer_t *buffer, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!buffer) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 229U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 11U) {
        return false;
    }

    buffer->event_counter = response.data[0];
    for (size_t i = 0; i < 5; ++i) {
        size_t base = 1U + (i * 2U);
        buffer->events[i].coin_id = response.data[base];
        buffer->events[i].error_code = response.data[base + 1U];
    }
    return true;
}


/**
 * @brief Richiede lo stato di inserimento alla scheda CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario della richiesta.
 * @param [out] status Un puntatore a una variabile dove verrà memorizzato lo stato di inserimento.
 * @param [in] timeout_ms Il timeout in millisecondi per la ricezione della risposta.
 * @return true se la richiesta è stata inviata con successo, false altrimenti.
 */
bool cctalk_request_insertion_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!status) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 226U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 1U) {
        return false;
    }
    *status = response.data[0];
    return true;
}


/**
 * @brief Resetta il dispositivo CCTalk.
 *
 * @param [in] dest_addr L'indirizzo del dispositivo da resettare.
 * @param [in] timeout_ms Il timeout in millisecondi per l'operazione di reset.
 * @return true se il reset è stato avviato con successo, false altrimenti.
 */
bool cctalk_reset_device(uint8_t dest_addr, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 1U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}

#endif /* DNA_CCTALK == 0 */

/*
 * Mockup — nessuna UART CCtalk reale.
 * Attiva quando DNA_CCTALK == 1
 */
#if defined(DNA_CCTALK) && (DNA_CCTALK == 1)


/**
 * @brief Inizializza la comunicazione CCTalk.
 *
 * @param [in] uart_num Numero del canale UART da utilizzare.
 * @param [in] tx_pin Numero del pin TX per la comunicazione UART.
 * @param [in] rx_pin Numero del pin RX per la comunicazione UART.
 * @param [in] baudrate Velocità di comunicazione in baud.
 *
 * @return Nessun valore di ritorno.
 */
void cctalk_init(int uart_num, int tx_pin, int rx_pin, int baudrate)
{
    (void)uart_num; (void)tx_pin; (void)rx_pin; (void)baudrate;
}


/**
 * @brief Invia dati tramite la protocollo CCTalk.
 *
 * @param [in] dest_addr L'indirizzo del destinatario.
 * @param [in] data Un puntatore ai dati da inviare.
 * @param [in] len La lunghezza dei dati da inviare.
 * @return true Se l'invio è stato completato con successo.
 * @return false Se l'invio è fallito.
 */
bool cctalk_send(uint8_t dest_addr, const uint8_t *data, uint8_t len)
{
    (void)dest_addr; (void)data; (void)len;
    return true;
}


/**
 * @brief Riceve dati tramite la protocollo CCTalk.
 *
 * @param [in] src_addr Indirizzo sorgente del messaggio.
 * @param [out] data Buffer per memorizzare i dati ricevuti.
 * @param [in/out] len Puntatore al numero di byte da ricevere e dimensione del buffer.
 * @param timeout_ms Timeout in millisecondi per l'attesa della ricezione.
 *
 * @return true se la ricezione è stata completata con successo, false altrimenti.
 */
bool cctalk_receive(uint8_t *src_addr, uint8_t *data, uint8_t *len, uint32_t timeout_ms)
{
    (void)src_addr; (void)data; (void)len; (void)timeout_ms;
    return false;
}


/**
 * @brief Calcola il checksum di un pacchetto CCTalk.
 *
 * Questa funzione calcola il checksum di un pacchetto CCTalk utilizzando l'algoritmo specifico.
 *
 * @param [in] packet Puntatore al pacchetto di dati per cui calcolare il checksum.
 * @param [in] len Lunghezza del pacchetto di dati.
 * @return Il valore del checksum calcolato.
 */
uint8_t cctalk_checksum(const uint8_t *packet, uint8_t len)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; ++i) {
        sum = (uint16_t)(sum + packet[i]);
    }
    return (uint8_t)(-sum);
}


/**
 * @brief Invia un frame CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del frame.
 * @param [in] src_addr L'indirizzo sorgente del frame.
 * @param [in] header Il codice header del frame.
 * @param [in] data Un puntatore al buffer contenente i dati del frame.
 * @param [in] len La lunghezza del buffer dei dati.
 * @return true Se il frame è stato inviato con successo.
 * @return false In caso di errore durante l'invio del frame.
 */
bool cctalk_send_frame(uint8_t dest_addr,
                       uint8_t src_addr,
                       uint8_t header,
                       const uint8_t *data,
                       uint8_t len)
{
    (void)dest_addr; (void)src_addr; (void)header; (void)data; (void)len;
    return true;
}


/**
 * @brief Riceve un frame CCTalk.
 *
 * Questa funzione riceve un frame CCTalk dal bus di comunicazione.
 *
 * @param [in/out] frame Puntatore al buffer dove viene memorizzato il frame ricevuto.
 * @param timeout_ms Timeout in millisecondi per l'attesa del frame.
 * @return true se il frame è stato ricevuto con successo, false altrimenti.
 */
bool cctalk_receive_frame(cctalk_frame_t *frame, uint32_t timeout_ms)
{
    (void)frame; (void)timeout_ms;
    return false;
}


/**
 * @brief Invia un comando CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del comando.
 * @param [in] src_addr L'indirizzo sorgente del comando.
 * @param [in] header Il codice header del comando.
 * @param [in] data Un puntatore al buffer contenente i dati del comando.
 * @param [in] len La lunghezza del buffer dei dati.
 * @param [out] response Un puntatore al buffer dove verrà memorizzata la risposta del dispositivo.
 * @param [in] timeout_ms Il timeout in millisecondi per la ricezione della risposta.
 * @return true se il comando è stato inviato con successo, false altrimenti.
 */
bool cctalk_command(uint8_t dest_addr,
                    uint8_t src_addr,
                    uint8_t header,
                    const uint8_t *data,
                    uint8_t len,
                    cctalk_frame_t *response,
                    uint32_t timeout_ms)
{
    (void)dest_addr; (void)src_addr; (void)header;
    (void)data; (void)len; (void)response; (void)timeout_ms;
    return false;
}


/**
 * @brief Invia un comando di polling all'indirizzo CCTalk specificato.
 *
 * @param [in] dest_addr Indirizzo CCTalk del dispositivo di destinazione.
 * @param [in] timeout_ms Timeout in millisecondi per l'attesa della risposta.
 * @return true se il polling è stato completato con successo, false altrimenti.
 */
bool cctalk_address_poll(uint8_t dest_addr, uint32_t timeout_ms)
{
    (void)dest_addr; (void)timeout_ms;
    return false;
}


/**
 * @brief Invia una richiesta di stato al dispositivo CCTalk.
 *
 * @param dest_addr [in] Indirizzo del dispositivo destinatario.
 * @param status [out] Puntatore al buffer dove verrà memorizzato lo stato del dispositivo.
 * @param timeout_ms [in] Timeout in millisecondi per l'attesa della risposta.
 * @return true se la richiesta è stata inviata con successo, false altrimenti.
 */
bool cctalk_request_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms)
{
    (void)dest_addr; (void)status; (void)timeout_ms;
    return false;
}


/**
 * @brief Invia una richiesta per ottenere l'ID del produttore tramite il protocollo CCTalk.
 *
 * @param dest_addr [in] Indirizzo destinatario del messaggio.
 * @param out [out] Buffer per memorizzare la risposta ricevuta.
 * @param out_len [in] Dimensione del buffer di output.
 * @param timeout_ms [in] Timeout in millisecondi per l'attesa della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_manufacturer_id(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}


/**
 * @brief Invia una richiesta di categoria di equipaggiamento al dispositivo CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del dispositivo CCTalk.
 * @param [out] out Buffer per memorizzare la risposta del dispositivo.
 * @param [in] out_len Dimensione del buffer di output.
 * @param [in] timeout_ms Timeout in millisecondi per la ricezione della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_equipment_category(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}


/**
 * @brief Invia una richiesta di codice prodotto al dispositivo CCTalk.
 *
 * @param dest_addr [in] Indirizzo destinatario del dispositivo CCTalk.
 * @param out [out] Buffer per memorizzare il codice prodotto ricevuto.
 * @param out_len [in] Dimensione del buffer di output.
 * @param timeout_ms [in] Timeout in millisecondi per l'attesa della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false altrimenti.
 */
bool cctalk_request_product_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}


/**
 * @brief Invia una richiesta per ottenere il numero di serie di un dispositivo CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del dispositivo CCTalk.
 * @param [out] serial Un array di 3 byte che riceverà il numero di serie del dispositivo.
 * @param [in] timeout_ms Il timeout in millisecondi per l'attesa della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_serial_number(uint8_t dest_addr, uint8_t serial[3], uint32_t timeout_ms)
{
    (void)dest_addr; (void)serial; (void)timeout_ms;
    return false;
}


/**
 * @brief Costruisce il codice di richiesta CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario della richiesta.
 * @param [out] out Buffer per memorizzare il codice di richiesta generato.
 * @param [in] out_len Dimensione massima del buffer di output.
 * @param [in] timeout_ms Timeout in millisecondi per l'attesa della risposta.
 * @return true se la generazione del codice di richiesta è stata completata con successo, false altrimenti.
 */
bool cctalk_request_build_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}


/**
 * @brief Abilita o disabilita l'inibizione del maestro CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del comando.
 * @param [in] enable Flag booleano per abilitare (true) o disabilitare (false) l'inibizione.
 * @param [in] timeout_ms Il timeout in millisecondi per l'operazione.
 * @return true se l'operazione è stata completata con successo, false altrimenti.
 */
bool cctalk_modify_master_inhibit(uint8_t dest_addr, bool enable, uint32_t timeout_ms)
{
    (void)dest_addr; (void)enable; (void)timeout_ms;
    return true;
}


/**
 * @brief Imposta la maschera di inibizione canali della gettoniera.
 *
 * @param [in] dest_addr Indirizzo del dispositivo destinatario.
 * @param [in] mask_low Maschera bit canali 1..8.
 * @param [in] mask_high Maschera bit canali 9..16.
 * @param [in] timeout_ms Timeout in millisecondi per l'operazione.
 * @return true se il comando è stato accettato, false altrimenti.
 */
bool cctalk_modify_inhibit_status(uint8_t dest_addr, uint8_t mask_low, uint8_t mask_high, uint32_t timeout_ms)
{
    (void)dest_addr; (void)mask_low; (void)mask_high; (void)timeout_ms;
    return true;
}


/**
 * @brief Invia una richiesta di stato di inibizione CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario della richiesta.
 * @param [out] mask_low Un puntatore al buffer dove verrà memorizzata la maschera bassa dello stato.
 * @param [out] mask_high Un puntatore al buffer dove verrà memorizzata la maschera alta dello stato.
 * @param [in] timeout_ms Il timeout in millisecondi per la ricezione della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false altrimenti.
 */
bool cctalk_request_inhibit_status(uint8_t dest_addr, uint8_t *mask_low, uint8_t *mask_high, uint32_t timeout_ms)
{
    (void)dest_addr; (void)mask_low; (void)mask_high; (void)timeout_ms;
    return false;
}


/**
 * @brief Invia una richiesta per ottenere l'ID della moneta all'indirizzo destinatario specificato.
 *
 * @param dest_addr Indirizzo destinatario del dispositivo.
 * @param channel Canale su cui inviare la richiesta.
 * @param out Buffer per memorizzare la risposta ricevuta.
 * @param out_len Dimensione del buffer di output.
 * @param timeout_ms Timeout massimo in millisecondi per la ricezione della risposta.
 * @return true se la richiesta è stata inviata con successo, false altrimenti.
 */
bool cctalk_request_coin_id(uint8_t dest_addr, uint8_t channel, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)channel; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}


/**
 * @brief Modifica i percorsi del sorter tramite il protocollo CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario del comando.
 * @param [in] paths Un puntatore al buffer contenente i percorsi da modificare.
 * @param [in] len La lunghezza del buffer paths.
 * @param [in] timeout_ms Il timeout in millisecondi per l'attesa della risposta.
 * @return true se la modifica è stata avvenuta con successo, false altrimenti.
 */
bool cctalk_modify_sorter_paths(uint8_t dest_addr, const uint8_t *paths, uint8_t len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)paths; (void)len; (void)timeout_ms;
    return true;
}


/**
 * @brief Legge i dati dal buffer CCTalk in modo buffered.
 *
 * @param [in] dest_addr L'indirizzo del dispositivo di destinazione.
 * @param [out] buffer Puntatore al buffer in cui memorizzare i dati letti.
 * @param [in] timeout_ms Il timeout in millisecondi per l'attesa della risposta.
 *
 * @return true se la lettura è stata completata con successo, false altrimenti.
 */
bool cctalk_read_buffered_credit(uint8_t dest_addr, cctalk_buffer_t *buffer, uint32_t timeout_ms)
{
    (void)dest_addr; (void)buffer; (void)timeout_ms;
    return false;
}


/**
 * @brief Richiede lo stato di inserimento alla scheda CCTalk.
 *
 * @param [in] dest_addr L'indirizzo destinatario della richiesta.
 * @param [out] status Un puntatore a una variabile dove verrà memorizzato lo stato di inserimento.
 * @param [in] timeout_ms Il timeout in millisecondi per la ricezione della risposta.
 *
 * @return true se la richiesta è stata inviata con successo, false in caso di errore.
 */
bool cctalk_request_insertion_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms)
{
    (void)dest_addr; (void)status; (void)timeout_ms;
    return false;
}


/**
 * @brief Resetta il dispositivo CCTalk.
 *
 * Questa funzione invia un comando di reset al dispositivo CCTalk specificato
 * tramite l'indirizzo di destinazione e attende una risposta entro il timeout
 * specificato.
 *
 * @param [in] dest_addr Indirizzo di destinazione del dispositivo CCTalk.
 * @param [in] timeout_ms Timeout in millisecondi per l'attesa della risposta.
 * @return true se il reset è stato avviato con successo, false altrimenti.
 */
bool cctalk_reset_device(uint8_t dest_addr, uint32_t timeout_ms)
{
    (void)dest_addr; (void)timeout_ms;
    return true;
}

#endif /* DNA_CCTALK == 1 */
