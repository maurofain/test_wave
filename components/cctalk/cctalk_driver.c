#include "cctalk.h"
#include "device_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "fsm.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "CCTALK_DRV";

extern void serial_test_push_monitor_action(const char *tag, const char *message);
extern bool dump_cctalk_log;

#ifndef DNA_CCTALK
#define DNA_CCTALK 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_CCTALK == 0

#define CCTALK_CMD_TIMEOUT_MS      (700U)
#define CCTALK_POLL_INTERVAL_MS    (250U)
#define CCTALK_REINIT_INTERVAL_MS  (60000U)

static SemaphoreHandle_t s_cctalk_state_lock = NULL;
static bool s_driver_initialized = false;
static bool s_driver_init_failed = false;
static bool s_acceptor_enabled = false;
static bool s_acceptor_online = false;
static bool s_event_counter_valid = false;
static uint8_t s_last_event_counter = 0;
static uint32_t s_poll_error_streak = 0;
static char s_coin_id_cache[16][16] = {{0}};
static bool s_coin_id_cache_valid[16] = {0};

device_component_status_t cctalk_driver_get_component_status(void)
{
    const device_config_t *cfg = device_config_get();

    if (s_driver_init_failed) {
        return DEVICE_COMPONENT_STATUS_OFFLINE;
    }

    if (!cfg || !cfg->sensors.cctalk_enabled) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (!s_driver_initialized) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    if (cctalk_driver_is_acceptor_enabled() && cctalk_driver_is_acceptor_online()) {
        return DEVICE_COMPONENT_STATUS_ONLINE;
    }

    return DEVICE_COMPONENT_STATUS_OFFLINE;
}


/**
 * @brief Restituisce l'indirizzo CCtalk configurato per la gettoniera.
 *
 * @return uint8_t Indirizzo dispositivo (default 0x02 se non configurato).
 */
static uint8_t cctalk_get_acceptor_addr(void)
{
    uint8_t addr = CCTALK_DEFAULT_DEVICE_ADDR;
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->cctalk.address >= 1U) {
        addr = cfg->cctalk.address;
    }
    return addr;
}


/**
 * @brief Inizializza una volta lo stato CCTalk.
 * 
 * Questa funzione inizializza lo stato CCTalk una volta sola, controllando se la variabile di lock è già stata impostata.
 * 
 * @param [in/out] s_cctalk_state_lock Puntatore alla variabile di lock che indica se lo stato CCTalk è già stato inizializzato.
 * @return Nessun valore di ritorno.
 */
static void cctalk_state_init_once(void)
{
    if (!s_cctalk_state_lock) {
        s_cctalk_state_lock = xSemaphoreCreateMutex();
    }
}


/**
 * @brief Esegue lo stato di prelievo CCTalk.
 *
 * Questa funzione gestisce lo stato di prelievo CCTalk, attendendo fino al timeout specificato.
 *
 * @param [in] timeout_ms Il timeout in millisecondi per l'attesa dello stato di prelievo.
 * @return true se lo stato di prelievo è stato raggiunto entro il timeout, false altrimenti.
 */
static bool cctalk_state_take(uint32_t timeout_ms)
{
    cctalk_state_init_once();
    if (!s_cctalk_state_lock) {
        return false;
    }
    TickType_t ticks = (timeout_ms == 0U) ? 0 : pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms > 0U && ticks == 0) {
        ticks = 1;
    }
    return (xSemaphoreTake(s_cctalk_state_lock, ticks) == pdTRUE);
}


/**
 * @brief Gestisce lo stato di "give" del protocollo CCTalk.
 * 
 * Questa funzione controlla se lo stato di lock CCTalk è attivo.
 * 
 * @param [in/out] Nessun parametro di input/output.
 * @return Nessun valore di ritorno.
 */
static void cctalk_state_give(void)
{
    if (s_cctalk_state_lock) {
        xSemaphoreGive(s_cctalk_state_lock);
    }
}


/**
 * @brief Cancella la cache degli ID delle monete.
 * 
 * Questa funzione svuota la cache che contiene gli ID delle monete rilevate.
 * 
 * @param Nessun parametro.
 * @return Nessun valore di ritorno.
 */
static void cctalk_clear_coin_id_cache(void)
{
    if (!cctalk_state_take(20)) {
        return;
    }
    memset(s_coin_id_cache, 0, sizeof(s_coin_id_cache));
    memset(s_coin_id_cache_valid, 0, sizeof(s_coin_id_cache_valid));
    cctalk_state_give();
}

static const char *cctalk_error_to_text(uint8_t code)
{
    switch (code) {
        case 0:   return "OK";
        case 1:   return "rifiutata";
        case 2:   return "inibita";
        case 8:   return "stringing";
        case 13:  return "inceppamento";
        case 254: return "errore_hw";
        default:  return "errore";
    }
}

/**
 * @brief Converte il label della moneta al valore in centesimi.
 * 
 * @param label [in] Label della moneta ricevuto dal comando 184 (es. "EU200A").
 * @param out_cents [out] Puntatore al valore in centesimi.
 * 
 * @return true se la conversione è riuscita, false altrimenti.
 */
static bool cctalk_coin_label_to_value(const char *label, int32_t *out_cents)
{
    if (!label || !out_cents) {
        return false;
    }

    /* Parse label format: "EU<value><letter>" where value is 3 digits
       Examples: EU050 = 50 cents, EU100 = 1 euro, EU200A = 2 euros */
    if (strlen(label) >= 5 && label[0] == 'E' && label[1] == 'U') {
        int val = atoi(&label[2]);
        if (val > 0) {
            *out_cents = val;
            return true;
        }
    }

    /* Fallback: unrecognized format defaults to 100 cents (1 euro) */
    ESP_LOGW(TAG, "[C] Moneta con label non riconosciuto: %s, defaulting a 1 euro", label);
    *out_cents = 100;
    return true;
}

static uint8_t cctalk_event_counter_delta(uint8_t previous_counter, uint8_t current_counter)
{
    return (uint8_t)(current_counter - previous_counter);
}

static void cctalk_log_buffered_credit_dump(const cctalk_buffer_t *buffer,
                                            bool previous_counter_valid,
                                            uint8_t previous_counter)
{
    if (!buffer || !dump_cctalk_log) {
        return;
    }

    char log_line[224] = {0};
    size_t offset = 0U;
    int written = snprintf(log_line + offset,
                           sizeof(log_line) - offset,
                           "[C] 229 ev=%u",
                           (unsigned)buffer->event_counter);
    if (written < 0) {
        return;
    }
    offset += (size_t)written;
    if (offset >= sizeof(log_line)) {
        log_line[sizeof(log_line) - 1U] = '\0';
        ESP_LOGI(TAG, "%s", log_line);
        return;
    }

    if (previous_counter_valid) {
        uint8_t delta = cctalk_event_counter_delta(previous_counter, buffer->event_counter);
        written = snprintf(log_line + offset,
                           sizeof(log_line) - offset,
                           " prev=%u delta=%u",
                           (unsigned)previous_counter,
                           (unsigned)delta);
    } else {
        written = snprintf(log_line + offset, sizeof(log_line) - offset, " prev=NA");
    }

    if (written < 0) {
        return;
    }
    offset += (size_t)written;
    if (offset >= sizeof(log_line)) {
        log_line[sizeof(log_line) - 1U] = '\0';
        ESP_LOGI(TAG, "%s", log_line);
        return;
    }

    for (size_t i = 0; i < 5 && offset < sizeof(log_line); ++i) {
        written = snprintf(log_line + offset,
                           sizeof(log_line) - offset,
                           " [%u:%u/%u]",
                           (unsigned)(i + 1U),
                           (unsigned)buffer->events[i].coin_id,
                           (unsigned)buffer->events[i].error_code);
        if (written < 0) {
            return;
        }

        offset += (size_t)written;
        if (offset >= sizeof(log_line)) {
            log_line[sizeof(log_line) - 1U] = '\0';
            break;
        }
    }

    ESP_LOGI(TAG, "%s", log_line);
}

static void cctalk_push_buffered_credit_hex_action(const cctalk_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }

    uint8_t payload[11] = {0};
    payload[0] = buffer->event_counter;
    for (size_t i = 0; i < 5U; ++i) {
        size_t base = 1U + (i * 2U);
        payload[base] = buffer->events[i].coin_id;
        payload[base + 1U] = buffer->events[i].error_code;
    }

    char msg[64] = {0};
    size_t offset = 0U;
    int written = snprintf(msg, sizeof(msg), "RX229 HEX");
    if (written < 0) {
        return;
    }
    offset = (size_t)written;

    for (size_t i = 0; i < sizeof(payload) && offset < sizeof(msg); ++i) {
        written = snprintf(msg + offset, sizeof(msg) - offset, " %02X", payload[i]);
        if (written < 0) {
            return;
        }
        size_t wrote_size = (size_t)written;
        if (wrote_size >= (sizeof(msg) - offset)) {
            msg[sizeof(msg) - 1U] = '\0';
            break;
        }
        offset += wrote_size;
    }

    serial_test_push_monitor_action("CCTALK", msg);
}


/**
 * @brief Ottiene etichetta del moneta da un canale CCTalk.
 * 
 * @param channel [in] Numero del canale CCTalk (1-16).
 * @param out [out] Buffer per la stringa dell'etichetta del moneta.
 * @param out_len [in] Dimensione del buffer di output.
 * 
 * @return true se l'operazione ha successo, false altrimenti.
 * 
 * @note La funzione controlla se il buffer di output è valido e se il numero del canale è compreso tra 1 e 16.
 */
static bool cctalk_fetch_coin_label(uint8_t channel, char *out, size_t out_len)
{
    if (!out || out_len == 0U || channel == 0U || channel > 16U) {
        return false;
    }

    out[0] = '\0';
    uint8_t index = (uint8_t)(channel - 1U);

    if (cctalk_state_take(20)) {
        if (s_coin_id_cache_valid[index]) {
            snprintf(out, out_len, "%s", s_coin_id_cache[index]);
            cctalk_state_give();
            return true;
        }
        cctalk_state_give();
    }

    char label[16] = {0};
    uint8_t acceptor_addr = cctalk_get_acceptor_addr();
    if (!cctalk_request_coin_id(acceptor_addr, channel, label, sizeof(label), CCTALK_CMD_TIMEOUT_MS)) {
        return false;
    }

    if (cctalk_state_take(20)) {
        snprintf(s_coin_id_cache[index], sizeof(s_coin_id_cache[index]), "%s", label);
        s_coin_id_cache_valid[index] = true;
        cctalk_state_give();
    }

    snprintf(out, out_len, "%s", label);
    return true;
}


/**
 * @brief Gestisce la registrazione delle informazioni di avvio della batteria.
 * 
 * Questa funzione registra le informazioni di avvio della batteria, inclusi
 * livelli di carica, temperatura e altre statistiche pertinenti.
 * 
 * @param [in] Nessun parametro di input.
 * @return Nessun valore di ritorno.
 */
static void cctalk_log_powerup_info(void)
{
    uint8_t acceptor_addr = cctalk_get_acceptor_addr();
    uint8_t status = 0;
    char text[64] = {0};

    if (cctalk_request_status(acceptor_addr, &status, CCTALK_CMD_TIMEOUT_MS)) {
        snprintf(text, sizeof(text), "STATUS=%u", (unsigned)status);
        serial_test_push_monitor_action("CCTALK", text);
    }

    if (cctalk_request_manufacturer_id(acceptor_addr, text, sizeof(text), CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "MANUF %s", text);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    if (cctalk_request_equipment_category(acceptor_addr, text, sizeof(text), CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "CAT %s", text);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    if (cctalk_request_product_code(acceptor_addr, text, sizeof(text), CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "MODEL %s", text);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    if (cctalk_request_build_code(acceptor_addr, text, sizeof(text), CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "FW %s", text);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    uint8_t serial[3] = {0};
    if (cctalk_request_serial_number(acceptor_addr, serial, CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "SER %02X%02X%02X", serial[0], serial[1], serial[2]);
        serial_test_push_monitor_action("CCTALK", msg);
    }
}

/*
* NOTA DA CONSIDERARE PER IMPLEMENTAZIONI FUTURE:
* La gettoniera accetta anche centesimi: quando raggiunge la quota di un euro la trasferisce a ecd
* e mantiene in memoria eventiali centesimi rimasti. Al momento dell'utilizzo dell'ultimo ecd
* azzera il credito rimasto (< 1.00 euro)
*/

/**
 * @brief Elabora i crediti bufferizzati ricevuti dal comando 229.
 * 
 * Questa funzione gestisce gli eventi di inserimento monete e i relativi errori.
 * 
 * 📌 IMPORTANTE - Monitor CCTalk:
 *    I messaggi di moneta/evento vengono SEMPRE pushati al monitor della sezione /test,
 *    indipendentemente dal valore di dump_cctalk_log:
 *    - dump_cctalk_log = true:  Mostra frame HEX completi (TX/RX) + messaggi monete
 *    - dump_cctalk_log = false: Mostra SOLO messaggi monete senza frame HEX
 *    
 *    Questo permette di seguire il flusso di inserimento monete anche in modalità
 *    "senza commenti" (cioè senza visualizzare i dettagli tecnici dei frame).
 */
static void cctalk_handle_buffered_credit(const cctalk_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }

    bool baseline = false;
    bool changed = false;
    bool previous_counter_valid = false;
    uint8_t previous_counter = 0;
    uint8_t new_event_count = 0;

    if (!cctalk_state_take(20)) {
        return;
    }

    if (!s_event_counter_valid) {
        s_event_counter_valid = true;
        s_last_event_counter = buffer->event_counter;
        baseline = true;
    } else {
        previous_counter = s_last_event_counter;
        previous_counter_valid = true;
        if (s_last_event_counter != buffer->event_counter) {
            s_last_event_counter = buffer->event_counter;
            changed = true;
            new_event_count = cctalk_event_counter_delta(previous_counter, buffer->event_counter);
        }
    }

    cctalk_state_give();
    
    // Log tecnico su seriale (solo se dump_cctalk_log = true)
    cctalk_log_buffered_credit_dump(buffer, previous_counter_valid, previous_counter);

    if (baseline) {
        char msg[64] = {0};
        snprintf(msg, sizeof(msg), "BUFFER baseline ev=%u", (unsigned)buffer->event_counter);
        // Push al monitor: baseline iniziale
        serial_test_push_monitor_action("CCTALK", msg);
        return;
    }

    if (!changed) {
        return;
    }

    {
        char msg[64] = {0};
        snprintf(msg,
                 sizeof(msg),
                 "BUFFER ev %u->%u (+%u)",
                 (unsigned)previous_counter,
                 (unsigned)buffer->event_counter,
                 (unsigned)new_event_count);
        // Push al monitor: contatore eventi cambiato
        serial_test_push_monitor_action("CCTALK", msg);
    }

    // Mostra il payload HEX ricevuto dal comando 229
    // (questo è un messaggio di "RX229 HEX ..." che è sempre pushato)
    cctalk_push_buffered_credit_hex_action(buffer);

    if (new_event_count > 5U) {
        char msg[72] = {0};
        snprintf(msg,
                 sizeof(msg),
                 "BUFFER overflow: +%u eventi, elaboro ultimi 5",
                 (unsigned)new_event_count);
        serial_test_push_monitor_action("CCTALK", msg);
        ESP_LOGW(TAG, "[C] buffered credit overflow: delta=%u, elaboro ultimi 5", (unsigned)new_event_count);
        new_event_count = 5U;
    }

    // Elabora ogni evento di moneta/errore
    // ✅ NB: I messaggi di moneta vengono SEMPRE pushati al monitor
    //    anche quando dump_cctalk_log = false
    //    Indipendentemente dalla modalità di debug
    for (size_t i = 0; i < (size_t)new_event_count; ++i) {
        uint8_t channel = buffer->events[i].coin_id;
        uint8_t event_data2 = buffer->events[i].error_code; // Se channel != 0 è il sorter path, se channel == 0 è l'error code

        if (channel == 0U && event_data2 == 0U) {
            continue;
        }

        char msg[96] = {0};
        if (channel > 0U) {
            // Moneta inserita correttamente
            char coin_label[16] = {0};
            if (cctalk_fetch_coin_label(channel, coin_label, sizeof(coin_label)) && coin_label[0] != '\0') {
                snprintf(msg, sizeof(msg), "MONETA CH%u %s", (unsigned)channel, coin_label);
                
                /* Converte label a valore e pubblica evento FSM per credito */
                int32_t coin_cents = 0;
                if (cctalk_coin_label_to_value(coin_label, &coin_cents)) {
                    fsm_input_event_t ev = {
                        .from = AGN_ID_CCTALK,
                        .to = {AGN_ID_FSM},
                        .action = ACTION_ID_PAYMENT_ACCEPTED,
                        .type = FSM_INPUT_EVENT_TOKEN,
                        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
                        .value_i32 = coin_cents,
                        .value_u32 = 0,
                        .aux_u32 = 0,
                        .text = {0},
                    };
                    snprintf(ev.text, sizeof(ev.text), "cctalk_coin_%s", coin_label);
                    if (!fsm_event_publish(&ev, pdMS_TO_TICKS(50))) {
                        ESP_LOGW(TAG, "[C] Coda FSM piena per moneta %s", coin_label);
                    }
                }
            } else {
                snprintf(msg, sizeof(msg), "MONETA CH%u", (unsigned)channel);
            }
        } else {
            // Errore (channel == 0)
            snprintf(msg,
                     sizeof(msg),
                     "EVENTO ERR%u %s",
                     (unsigned)event_data2,
                     cctalk_error_to_text(event_data2));
        }

        // Push al monitor: questo messaggio appare nel monitor /test
        // indipendentemente da dump_cctalk_log
        serial_test_push_monitor_action("CCTALK", msg);
    }
}


/**
 * @brief Esegue una singola iterazione del polling CCTalk.
 *
 * Questa funzione esegue una singola iterazione del polling CCTalk, gestendo le comunicazioni
 * con dispositivi CCTalk collegati.
 *
 * @return Nessun valore di ritorno.
 */
static void cctalk_poll_once(void)
{
    uint8_t acceptor_addr = cctalk_get_acceptor_addr();
    cctalk_buffer_t buffer = {0};
    if (!cctalk_read_buffered_credit(acceptor_addr, &buffer, CCTALK_CMD_TIMEOUT_MS)) {
        s_poll_error_streak++;
        if (s_poll_error_streak == 1U || (s_poll_error_streak % 20U) == 0U) {
            serial_test_push_monitor_action("CCTALK", "POLL buffered credit timeout/errore");
        }
        if (s_poll_error_streak >= 20U) {
            if (cctalk_state_take(20)) {
                if (s_acceptor_online) {
                    s_acceptor_online = false;
                    serial_test_push_monitor_action("CCTALK", "GETTONIERA offline (errore poll ripetuto)");
                }
                cctalk_state_give();
            }
        }
        return;
    }

    if (cctalk_state_take(20)) {
        if (!s_acceptor_online) {
            s_acceptor_online = true;
            serial_test_push_monitor_action("CCTALK", "GETTONIERA tornata online");
        }
        s_poll_error_streak = 0U;
        cctalk_state_give();
    }
    cctalk_handle_buffered_credit(&buffer);
}


/**
 * @brief Esegue il task CCTalk.
 *
 * Questa funzione gestisce il task principale per la comunicazione CCTalk.
 * Implementa il Passo 4 della sequenza di inizializzazione:
 * 
 *   Passo 4: Read Credit (229) - Ciclo infinito di lettura incasso
 *   ├─ Poll ogni 250 ms il buffer crediti (comando 229)
 *   ├─ Se nuovo evento → elabora moneta rilevata (coin_id, error_code)
 *   └─ Log monete e relativi stati di errore
 *
 * @param arg Puntatore a dati di input utilizzati dal task.
 * @return Nessun valore di ritorno.
 */
void cctalk_task_run(void *arg)
{
    (void)arg;

    /* Ensure mailbox is ready for receiving control events */
    if (!fsm_event_queue_init(0)) {
        ESP_LOGW(TAG, "[C] fsm_event_queue_init failed or not ready; continuing");
    }

    while (1) {
        /* Non‑blocking check for control messages directed to CCTALK */
        fsm_input_event_t ev;
        if (fsm_event_receive(&ev, AGN_ID_CCTALK, 0)) {
            if (ev.action == ACTION_ID_CCTALK_START) {
                serial_test_push_monitor_action("CCTALK", "START event received");
                esp_err_t start_err = cctalk_driver_start_acceptor();
                if (start_err != ESP_OK) {
                    char msg[80];
                    snprintf(msg, sizeof(msg), "START failed: %s", esp_err_to_name(start_err));
                    serial_test_push_monitor_action("CCTALK", msg);
                } else {
                    serial_test_push_monitor_action("CCTALK", "START ok");
                }
            } else if (ev.action == ACTION_ID_CCTALK_STOP) {
                serial_test_push_monitor_action("CCTALK", "STOP event received");
                esp_err_t stop_err = cctalk_driver_stop_acceptor();
                if (stop_err != ESP_OK) {
                    char msg[80];
                    snprintf(msg, sizeof(msg), "STOP failed: %s", esp_err_to_name(stop_err));
                    serial_test_push_monitor_action("CCTALK", msg);
                } else {
                    serial_test_push_monitor_action("CCTALK", "STOP ok");
                }
            } else if (ev.action == ACTION_ID_CCTALK_MASK) {
                uint32_t mask = ev.value_u32;
                uint8_t mask_low = (uint8_t)(mask & 0xFFU);
                uint8_t mask_high = (uint8_t)((mask >> 8) & 0xFFU);
                uint8_t addr = (ev.value_i32 != 0) ? (uint8_t)ev.value_i32 : cctalk_get_acceptor_addr();
                char msg[96];
                snprintf(msg, sizeof(msg), "MASK event addr=0x%02X low=0x%02X high=0x%02X", (unsigned)addr, (unsigned)mask_low, (unsigned)mask_high);
                serial_test_push_monitor_action("CCTALK", msg);

                /* Ensure driver initialized in driver context before sending commands */
                esp_err_t init_err = cctalk_driver_init();
                if (init_err != ESP_OK) {
                    char failmsg[80];
                    snprintf(failmsg, sizeof(failmsg), "MASK init fail: %s", esp_err_to_name(init_err));
                    serial_test_push_monitor_action("CCTALK", failmsg);
                } else {
                    /* Try to set master inhibit (standard) to enabled before applying mask */
                    if (!cctalk_modify_master_inhibit_std(addr, true, CCTALK_CMD_TIMEOUT_MS)) {
                        serial_test_push_monitor_action("CCTALK", "MASK master_inhibit_std fail");
                    }

                    if (!cctalk_modify_inhibit_status(addr, mask_low, mask_high, CCTALK_CMD_TIMEOUT_MS)) {
                        char failmsg[80];
                        snprintf(failmsg, sizeof(failmsg), "MASK failed addr=0x%02X", (unsigned)addr);
                        serial_test_push_monitor_action("CCTALK", failmsg);
                    } else {
                        serial_test_push_monitor_action("CCTALK", "MASK ok");
                    }
                }
            }
        }

        if (cctalk_driver_is_acceptor_enabled()) {
            // Passo 4: Ciclo di lettura crediti (comando 229 - Read Buffered Credit)
            cctalk_poll_once();

            static uint32_t s_reinit_timer_ms = 0U;
            if (!cctalk_driver_is_acceptor_online()) {
                s_reinit_timer_ms += CCTALK_POLL_INTERVAL_MS;
                if (s_reinit_timer_ms >= CCTALK_REINIT_INTERVAL_MS) {
                    s_reinit_timer_ms = 0U;
                    serial_test_push_monitor_action("CCTALK", "GETTONIERA offline; tentativo reinit");
                    esp_err_t start_err = cctalk_driver_start_acceptor();
                    if (start_err != ESP_OK) {
                        char msg[96];
                        snprintf(msg, sizeof(msg), "REINIT failed: %s", esp_err_to_name(start_err));
                        serial_test_push_monitor_action("CCTALK", msg);
                    } else {
                        serial_test_push_monitor_action("CCTALK", "REINIT ok");
                    }
                }
            } else {
                s_reinit_timer_ms = 0U;
            }

            vTaskDelay(pdMS_TO_TICKS(CCTALK_POLL_INTERVAL_MS));  // 250 ms tra letture
        } else {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}


/**
 * @brief Inizializza il driver CCTalk.
 *
 * Questa funzione inizializza il driver CCTalk, preparando il sistema per la comunicazione.
 *
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t cctalk_driver_init(void)
{
    cctalk_state_init_once();
    s_driver_init_failed = false;

    if (!cctalk_state_take(200)) {
        ESP_LOGE(TAG, "[C] driver init lock timeout");
        s_driver_init_failed = true;
        return ESP_FAIL;
    }

    if (s_driver_initialized) {
        cctalk_state_give();
        return ESP_OK;
    }

    const int cctalk_tx_gpio = 20; /* TX */
    const int cctalk_rx_gpio = 21; /* RX */
    cctalk_init(CONFIG_APP_RS232_UART_PORT, cctalk_tx_gpio, cctalk_rx_gpio, 9600);
    s_driver_initialized = true;
    cctalk_state_give();

    ESP_LOGI(TAG,
             "[C] driver init UART=%d tx=%d rx=%d (single-wire adattato su RX/TX MH1001)",
             CONFIG_APP_RS232_UART_PORT,
             cctalk_tx_gpio,
             cctalk_rx_gpio);
    return ESP_OK;
}


/**
 * @brief Avvia il driver per l'acettore CCTalk.
 *
 * Questa funzione inizia il processo di accettazione per l'acettore CCTalk.
 * Se il driver non è stato inizializzato, la funzione restituirà un errore.
 *
 * @return esp_err_t - Codice di errore che indica lo stato dell'operazione.
 * @retval ESP_OK - Operazione completata con successo.
 * @retval ESP_FAIL - Operazione fallita.
 * @retval ESP_ERR_INVALID_STATE - Driver non inizializzato.
 */
esp_err_t cctalk_driver_start_acceptor(void)
{
    esp_err_t init_ret = cctalk_driver_init();
    if (init_ret != ESP_OK) {
        return init_ret;
    }

    serial_test_push_monitor_action("CCTALK", "GETTONIERA start sequenza");

    uint8_t acceptor_addr = cctalk_get_acceptor_addr();

    // Passo 1: Address Poll - Verifica che la gettoniera sia presente e risponda
    // Invia comando 254 (0xFE) senza dati, aspetta ACK
    // Se risponde, la gettoniera è attiva e comunica
    // 02 00 01 FE FF → Address Poll
    if (!cctalk_address_poll(acceptor_addr, CCTALK_CMD_TIMEOUT_MS)) {
        char msg[64] = {0};
        snprintf(msg, sizeof(msg), "Address Poll FAIL (addr 0x%02X)", (unsigned)acceptor_addr);
        serial_test_push_monitor_action("CCTALK", msg);
        return ESP_ERR_NOT_FOUND;
    }

    {
        char msg[64] = {0};
        snprintf(msg, sizeof(msg), "Address Poll OK (addr 0x%02X)", (unsigned)acceptor_addr);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    // Delay 100ms tra comandi per stabilità comunicazione
    vTaskDelay(pdMS_TO_TICKS(100));

    // Passo 2: Modify Inhibit Status - Abilita tutti i 16 canali
    // Comando 231 (0xE7) con 2 byte: 0xFF 0xFF
    // Apre tutti i canali per le monete
    // 02 02 01 E7 FF FF XX → Tutti 16 canali ABILITATI
    if (!cctalk_modify_inhibit_status(acceptor_addr, 0xFFU, 0xFFU, CCTALK_CMD_TIMEOUT_MS)) {
        serial_test_push_monitor_action("CCTALK", "Inhibit mask FF FF FAIL");
        return ESP_FAIL;
    }

    // Delay 100ms
    vTaskDelay(pdMS_TO_TICKS(100));

    // Passo 3: Master Inhibit standard (comando 228)
    // Comando 228 secondo CCTalk spec ufficiale
    // 02 01 01 E4 01 XX → Master Inhibit standard
    if (!cctalk_modify_master_inhibit_std(acceptor_addr, true, CCTALK_CMD_TIMEOUT_MS)) {
        serial_test_push_monitor_action("CCTALK", "Master Inhibit STD (228) FAIL");
        return ESP_FAIL;
    }

    // Delay 100ms
    vTaskDelay(pdMS_TO_TICKS(100));

    cctalk_clear_coin_id_cache();
    if (cctalk_state_take(20)) {
        s_acceptor_enabled = true;
        s_acceptor_online = true;
        s_event_counter_valid = false;
        s_last_event_counter = 0;
        s_poll_error_streak = 0;
        cctalk_state_give();
    }

    serial_test_push_monitor_action("CCTALK", "GETTONIERA abilitata (accettazione ON)");
    return ESP_OK;
}


/**
 * @brief Arresta l'acquisizione dei dati dal driver CCTalk.
 * 
 * Questa funzione si occupa di arrestare l'acquisizione dei dati dal driver CCTalk.
 * Se il driver non è stato inizializzato, la funzione non ha alcun effetto.
 * 
 * @return esp_err_t - Codice di errore che indica il successo o la fallita dell'operazione.
 * @param [in/out] - Non applicabile in questo caso.
 */
esp_err_t cctalk_driver_stop_acceptor(void)
{
    if (!s_driver_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t acceptor_addr = cctalk_get_acceptor_addr();
    bool inhibit_ok = cctalk_modify_master_inhibit(acceptor_addr, false, CCTALK_CMD_TIMEOUT_MS);

    if (cctalk_state_take(20)) {
        s_acceptor_enabled = false;
        s_acceptor_online = false;
        s_event_counter_valid = false;
        s_last_event_counter = 0;
        s_poll_error_streak = 0;
        cctalk_state_give();
    }

    if (!inhibit_ok) {
        serial_test_push_monitor_action("CCTALK", "Master Inhibit=0 (inibito) FAIL");
        return ESP_FAIL;
    }

    serial_test_push_monitor_action("CCTALK", "GETTONIERA disabilitata");
    return ESP_OK;
}


/**
 * @brief Controlla se l'acettore CCTalk è abilitato.
 *
 * Questa funzione verifica lo stato dell'acettore CCTalk e restituisce
 * un valore booleano che indica se l'acettore è attualmente abilitato.
 *
 * @return true se l'acettore CCTalk è abilitato, false altrimenti.
 */
bool cctalk_driver_is_acceptor_enabled(void)
{
    bool enabled = false;
    if (cctalk_state_take(5)) {
        enabled = s_acceptor_enabled;
        cctalk_state_give();
    }
    return enabled;
}

bool cctalk_driver_is_acceptor_online(void)
{
    bool online = false;
    if (cctalk_state_take(5)) {
        online = s_acceptor_online;
        cctalk_state_give();
    }
    return online;
}

#endif /* DNA_CCTALK == 0 */

/*
 * Mockup — nessuna UART CCtalk, nessun task.
 * Attiva quando DNA_CCTALK == 1
 */
#if defined(DNA_CCTALK) && (DNA_CCTALK == 1)

static bool s_mock_acceptor_enabled = false;
static bool s_mock_driver_initialized = false;

device_component_status_t cctalk_driver_get_component_status(void)
{
    const device_config_t *cfg = device_config_get();

    if (!cfg || !cfg->sensors.cctalk_enabled) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (!s_mock_driver_initialized) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    return s_mock_acceptor_enabled ? DEVICE_COMPONENT_STATUS_ONLINE
                                   : DEVICE_COMPONENT_STATUS_OFFLINE;
}


/**
 * @brief Inizializza il driver CCTalk.
 *
 * Questa funzione inizializza il driver CCTalk, preparando il sistema per la comunicazione.
 *
 * @return esp_err_t
 *         - ESP_OK: Inizializzazione avvenuta con successo.
 *         - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t cctalk_driver_init(void)
{
    ESP_LOGI(TAG, "[C] [MOCK] cctalk_driver_init: CCtalk disabilitato");
    s_mock_driver_initialized = true;
    return ESP_OK;
}


/**
 * @brief Avvia il driver del rifiutatore CCTalk.
 *
 * Questa funzione inizia il processo di accettazione del rifiutatore CCTalk.
 *
 * @return esp_err_t - Codice di errore che indica lo stato dell'operazione.
 *         - ESP_OK: Operazione completata con successo.
 *         - ESP_FAIL: Operazione fallita.
 */
esp_err_t cctalk_driver_start_acceptor(void)
{
    s_mock_acceptor_enabled = true;
    ESP_LOGI(TAG, "[C] [MOCK] cctalk_driver_start_acceptor");
    return ESP_OK;
}


/**
 * @brief Arresta l'acettatore CCTalk.
 *
 * Questa funzione interrompe il processo di accettazione CCTalk e libera eventuali risorse allocate.
 *
 * @return
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t cctalk_driver_stop_acceptor(void)
{
    s_mock_acceptor_enabled = false;
    ESP_LOGI(TAG, "[C] [MOCK] cctalk_driver_stop_acceptor");
    return ESP_OK;
}


/**
 * @brief Controlla se l'acettore CCTalk è abilitato.
 *
 * Questa funzione verifica lo stato dell'acettore CCTalk e restituisce
 * un valore booleano che indica se l'acettore è attualmente abilitato.
 *
 * @return true se l'acettore CCTalk è abilitato, false altrimenti.
 */
bool cctalk_driver_is_acceptor_enabled(void)
{
    return s_mock_acceptor_enabled;
}


/**
 * @brief Esegue il task principale per la gestione del protocollo CCtalk.
 * 
 * Questa funzione gestisce il ciclo di vita del task principale per la comunicazione
 * con dispositivi che utilizzano il protocollo CCtalk. Il task si trova in un ciclo
 * infinito, attendendo comandi e gestendo le risposte.
 * 
 * @param arg Puntatore a dati aggiuntivi che possono essere passati al task.
 *            In questo mockup, il parametro non viene utilizzato.
 * @return Non applicabile (void).
 */
void cctalk_task_run(void *arg)
{
    (void)arg;

    /* Ensure mailbox is ready for receiving control events */
    if (!fsm_event_queue_init(0)) {
        ESP_LOGW(TAG, "[C] fsm_event_queue_init failed or not ready (mock); continuing");
    }

    while (1) {
        /* Non‑blocking check for control messages directed to CCTALK */
        fsm_input_event_t ev;
        if (fsm_event_receive(&ev, AGN_ID_CCTALK, 0)) {
            if (ev.action == ACTION_ID_CCTALK_START) {
                serial_test_push_monitor_action("CCTALK", "[MOCK] START event received");
                (void)cctalk_driver_start_acceptor();
                serial_test_push_monitor_action("CCTALK", "[MOCK] START ok");
            } else if (ev.action == ACTION_ID_CCTALK_STOP) {
                serial_test_push_monitor_action("CCTALK", "[MOCK] STOP event received");
                (void)cctalk_driver_stop_acceptor();
                serial_test_push_monitor_action("CCTALK", "[MOCK] STOP ok");
            } else if (ev.action == ACTION_ID_CCTALK_MASK) {
                uint32_t mask = ev.value_u32;
                uint8_t mask_low = (uint8_t)(mask & 0xFFU);
                uint8_t mask_high = (uint8_t)((mask >> 8) & 0xFFU);
                char msg[96];
                snprintf(msg, sizeof(msg), "[MOCK] MASK event low=0x%02X high=0x%02X", (unsigned)mask_low, (unsigned)mask_high);
                serial_test_push_monitor_action("CCTALK", msg);
                /* Mock: do not call hardware functions, just acknowledge */
                serial_test_push_monitor_action("CCTALK", "[MOCK] MASK ok");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#endif /* DNA_CCTALK == 1 */
