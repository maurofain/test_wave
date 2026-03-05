#include "cctalk.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "CCTALK_DRV";

extern void serial_test_push_monitor_action(const char *tag, const char *message);

#ifndef DNA_CCTALK
#define DNA_CCTALK 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_CCTALK == 0

#define CCTALK_COIN_ACCEPTOR_ADDR  (0x02U)
#define CCTALK_CMD_TIMEOUT_MS      (700U)
#define CCTALK_POLL_INTERVAL_MS    (250U)

static SemaphoreHandle_t s_cctalk_state_lock = NULL;
static bool s_driver_initialized = false;
static bool s_acceptor_enabled = false;
static bool s_event_counter_valid = false;
static uint8_t s_last_event_counter = 0;
static uint32_t s_poll_error_streak = 0;
static char s_coin_id_cache[16][16] = {{0}};
static bool s_coin_id_cache_valid[16] = {0};

static void cctalk_state_init_once(void)
{
    if (!s_cctalk_state_lock) {
        s_cctalk_state_lock = xSemaphoreCreateMutex();
    }
}

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

static void cctalk_state_give(void)
{
    if (s_cctalk_state_lock) {
        xSemaphoreGive(s_cctalk_state_lock);
    }
}

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
    if (!cctalk_request_coin_id(CCTALK_COIN_ACCEPTOR_ADDR, channel, label, sizeof(label), CCTALK_CMD_TIMEOUT_MS)) {
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

static void cctalk_log_powerup_info(void)
{
    uint8_t status = 0;
    char text[64] = {0};

    if (cctalk_request_status(CCTALK_COIN_ACCEPTOR_ADDR, &status, CCTALK_CMD_TIMEOUT_MS)) {
        snprintf(text, sizeof(text), "STATUS=%u", (unsigned)status);
        serial_test_push_monitor_action("CCTALK", text);
    }

    if (cctalk_request_manufacturer_id(CCTALK_COIN_ACCEPTOR_ADDR, text, sizeof(text), CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "MANUF %s", text);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    if (cctalk_request_equipment_category(CCTALK_COIN_ACCEPTOR_ADDR, text, sizeof(text), CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "CAT %s", text);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    if (cctalk_request_product_code(CCTALK_COIN_ACCEPTOR_ADDR, text, sizeof(text), CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "MODEL %s", text);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    if (cctalk_request_build_code(CCTALK_COIN_ACCEPTOR_ADDR, text, sizeof(text), CCTALK_CMD_TIMEOUT_MS)) {
        char msg[80] = {0};
        snprintf(msg, sizeof(msg), "FW %s", text);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    uint8_t serial[3] = {0};
    if (cctalk_request_serial_number(CCTALK_COIN_ACCEPTOR_ADDR, serial, CCTALK_CMD_TIMEOUT_MS)) {
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
static void cctalk_handle_buffered_credit(const cctalk_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }

    bool baseline = false;
    bool changed = false;
    uint8_t previous_counter = 0;

    if (!cctalk_state_take(20)) {
        return;
    }

    if (!s_event_counter_valid) {
        s_event_counter_valid = true;
        s_last_event_counter = buffer->event_counter;
        baseline = true;
    } else if (s_last_event_counter != buffer->event_counter) {
        previous_counter = s_last_event_counter;
        s_last_event_counter = buffer->event_counter;
        changed = true;
    }

    cctalk_state_give();

    if (baseline) {
        char msg[64] = {0};
        snprintf(msg, sizeof(msg), "BUFFER baseline ev=%u", (unsigned)buffer->event_counter);
        serial_test_push_monitor_action("CCTALK", msg);
        return;
    }

    if (!changed) {
        return;
    }

    {
        char msg[64] = {0};
        snprintf(msg, sizeof(msg), "BUFFER ev %u->%u", (unsigned)previous_counter, (unsigned)buffer->event_counter);
        serial_test_push_monitor_action("CCTALK", msg);
    }

    for (size_t i = 0; i < 5; ++i) {
        uint8_t channel = buffer->events[i].coin_id;
        uint8_t error = buffer->events[i].error_code;

        if (channel == 0U && error == 0U) {
            continue;
        }

        char msg[96] = {0};
        if (error == 0U) {
            char coin_label[16] = {0};
            if (cctalk_fetch_coin_label(channel, coin_label, sizeof(coin_label)) && coin_label[0] != '\0') {
                snprintf(msg, sizeof(msg), "MONETA CH%u %s", (unsigned)channel, coin_label);
            } else {
                snprintf(msg, sizeof(msg), "MONETA CH%u", (unsigned)channel);
            }
        } else {
            snprintf(msg,
                     sizeof(msg),
                     "EVENTO CH%u ERR%u %s",
                     (unsigned)channel,
                     (unsigned)error,
                     cctalk_error_to_text(error));
        }

        serial_test_push_monitor_action("CCTALK", msg);
    }
}

static void cctalk_poll_once(void)
{
    cctalk_buffer_t buffer = {0};
    if (!cctalk_read_buffered_credit(CCTALK_COIN_ACCEPTOR_ADDR, &buffer, CCTALK_CMD_TIMEOUT_MS)) {
        s_poll_error_streak++;
        if (s_poll_error_streak == 1U || (s_poll_error_streak % 20U) == 0U) {
            serial_test_push_monitor_action("CCTALK", "POLL buffered credit timeout/errore");
        }
        return;
    }

    s_poll_error_streak = 0U;
    cctalk_handle_buffered_credit(&buffer);
}


/**
 * @brief Esegue il task CCTalk.
 *
 * Questa funzione gestisce il task principale per la comunicazione CCTalk.
 *
 * @param arg Puntatore a dati di input utilizzati dal task.
 * @return Nessun valore di ritorno.
 */
void cctalk_task_run(void *arg)
{
    (void)arg;
    while (1) {
        if (cctalk_driver_is_acceptor_enabled()) {
            cctalk_poll_once();
            vTaskDelay(pdMS_TO_TICKS(CCTALK_POLL_INTERVAL_MS));
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

    const int cctalk_tx_gpio = 20; /* TX */
    const int cctalk_rx_gpio = 21; /* RX */
    cctalk_init(CONFIG_APP_RS232_UART_PORT, cctalk_tx_gpio, cctalk_rx_gpio, 9600);

    if (cctalk_state_take(20)) {
        s_driver_initialized = true;
        cctalk_state_give();
    }

    ESP_LOGI(TAG,
             "[C] driver init UART=%d tx=%d rx=%d (single-wire adattato su RX/TX MH1001)",
             CONFIG_APP_RS232_UART_PORT,
             cctalk_tx_gpio,
             cctalk_rx_gpio);
    return ESP_OK;
}

esp_err_t cctalk_driver_start_acceptor(void)
{
    if (!s_driver_initialized) {
        esp_err_t init_ret = cctalk_driver_init();
        if (init_ret != ESP_OK) {
            return init_ret;
        }
    }

    serial_test_push_monitor_action("CCTALK", "GETTONIERA start sequenza");

    if (!cctalk_address_poll(CCTALK_COIN_ACCEPTOR_ADDR, CCTALK_CMD_TIMEOUT_MS)) {
        serial_test_push_monitor_action("CCTALK", "Address Poll FAIL (addr 0x02)");
        return ESP_ERR_NOT_FOUND;
    }

    serial_test_push_monitor_action("CCTALK", "Address Poll OK (addr 0x02)");
    cctalk_log_powerup_info();

    if (!cctalk_modify_master_inhibit(CCTALK_COIN_ACCEPTOR_ADDR, true, CCTALK_CMD_TIMEOUT_MS)) {
        serial_test_push_monitor_action("CCTALK", "Master Inhibit ON FAIL");
        return ESP_FAIL;
    }

    {
        uint8_t mask_low = 0;
        uint8_t mask_high = 0;
        if (cctalk_request_inhibit_status(CCTALK_COIN_ACCEPTOR_ADDR, &mask_low, &mask_high, CCTALK_CMD_TIMEOUT_MS)) {
            char msg[64] = {0};
            snprintf(msg, sizeof(msg), "Inhibit mask %02X %02X", mask_low, mask_high);
            serial_test_push_monitor_action("CCTALK", msg);
        }
    }

    cctalk_clear_coin_id_cache();
    if (cctalk_state_take(20)) {
        s_acceptor_enabled = true;
        s_event_counter_valid = false;
        s_last_event_counter = 0;
        s_poll_error_streak = 0;
        cctalk_state_give();
    }

    serial_test_push_monitor_action("CCTALK", "GETTONIERA abilitata");
    return ESP_OK;
}

esp_err_t cctalk_driver_stop_acceptor(void)
{
    if (!s_driver_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    bool inhibit_ok = cctalk_modify_master_inhibit(CCTALK_COIN_ACCEPTOR_ADDR, false, CCTALK_CMD_TIMEOUT_MS);

    if (cctalk_state_take(20)) {
        s_acceptor_enabled = false;
        s_event_counter_valid = false;
        s_last_event_counter = 0;
        s_poll_error_streak = 0;
        cctalk_state_give();
    }

    if (!inhibit_ok) {
        serial_test_push_monitor_action("CCTALK", "Master Inhibit OFF FAIL");
        return ESP_FAIL;
    }

    serial_test_push_monitor_action("CCTALK", "GETTONIERA disabilitata");
    return ESP_OK;
}

bool cctalk_driver_is_acceptor_enabled(void)
{
    bool enabled = false;
    if (cctalk_state_take(5)) {
        enabled = s_acceptor_enabled;
        cctalk_state_give();
    }
    return enabled;
}

#endif /* DNA_CCTALK == 0 */

/*
 * Mockup — nessuna UART CCtalk, nessun task.
 * Attiva quando DNA_CCTALK == 1
 */
#if defined(DNA_CCTALK) && (DNA_CCTALK == 1)

static bool s_mock_acceptor_enabled = false;


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
    return ESP_OK;
}

esp_err_t cctalk_driver_start_acceptor(void)
{
    s_mock_acceptor_enabled = true;
    ESP_LOGI(TAG, "[C] [MOCK] cctalk_driver_start_acceptor");
    return ESP_OK;
}

esp_err_t cctalk_driver_stop_acceptor(void)
{
    s_mock_acceptor_enabled = false;
    ESP_LOGI(TAG, "[C] [MOCK] cctalk_driver_stop_acceptor");
    return ESP_OK;
}

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
    /* Mockup: CCtalk disabilitato — task in attesa indefinita */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#endif /* DNA_CCTALK == 1 */
