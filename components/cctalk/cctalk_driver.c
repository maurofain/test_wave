#include "cctalk.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "CCTALK_DRV";

#ifndef DNA_CCTALK
#define DNA_CCTALK 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_CCTALK == 0


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
    uint8_t src;
    uint8_t buf[256];
    uint8_t len;

    while (1) {
        if (cctalk_receive(&src, buf, &len, 500)) {
            ESP_LOGI(TAG, "RX from 0x%02X len=%d", src, len);
            // Push received bytes to serial monitor (sezione CCtalk)
            extern void serial_test_push_monitor_entry(const char*, const uint8_t*, size_t);
            serial_test_push_monitor_entry("CCTALK", buf, len);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
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
    /* Force CCtalk to use dedicated pins: TX = GPIO20, RX = GPIO21 (per requirement)
       UART port is still taken from CONFIG_APP_RS232_UART_PORT */
    const int cctalk_tx_gpio = 20; /* TX */
    const int cctalk_rx_gpio = 21; /* RX */
    cctalk_init(CONFIG_APP_RS232_UART_PORT, cctalk_tx_gpio, cctalk_rx_gpio, 9600);

    ESP_LOGI(TAG, "cctalk driver initialized on UART %d (tx=%d rx=%d, 9600 8N1) — task gestito da tasks.c", CONFIG_APP_RS232_UART_PORT, cctalk_tx_gpio, cctalk_rx_gpio);
    return ESP_OK;
}

#endif /* DNA_CCTALK == 0 */

/*
 * Mockup — nessuna UART CCtalk, nessun task.
 * Attiva quando DNA_CCTALK == 1
 */
#if defined(DNA_CCTALK) && (DNA_CCTALK == 1)


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
    /* Mockup: CCtalk disabilitato — task in attesa indefinita */
    while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); }
}

#endif /* DNA_CCTALK == 1 */
