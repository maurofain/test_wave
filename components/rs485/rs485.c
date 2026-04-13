#include "rs485.h"
#include "device_config.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG __attribute__((unused)) = "RS485";

#ifndef DNA_RS485
#define DNA_RS485 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_RS485 == 0

/* [C] Stato init UART per get_status */
static bool s_rs485_initialized = false;

/**
 * @brief Inizializza la porta RS485 in modalità Half-Duplex.
 *
 * Utilizza i parametri della configurazione device.
 * Nota sul Buffer TX:
 * - Per RS485 è caldamente consigliato il valore 0 (modalità bloccante).
 * - Questo assicura che il bus venga liberato correttamente per la ricezione
 * subito dopo l'invio.
 */
esp_err_t rs485_init(void) {
  s_rs485_initialized = false;
  device_config_t *d_cfg = device_config_get();

  uart_config_t cfg = {
      .baud_rate = d_cfg->rs485.baud_rate,
      .data_bits =
          (d_cfg->rs485.data_bits == 7) ? UART_DATA_7_BITS : UART_DATA_8_BITS,
      .parity = (d_cfg->rs485.parity == 1)
                    ? UART_PARITY_ODD
                    : (d_cfg->rs485.parity == 2 ? UART_PARITY_EVEN
                                                : UART_PARITY_DISABLE),
      .stop_bits =
          (d_cfg->rs485.stop_bits == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_DEFAULT,
  };
  esp_err_t ret =
      uart_driver_install(CONFIG_APP_RS485_UART_PORT, d_cfg->rs485.rx_buf_size,
                          d_cfg->rs485.tx_buf_size, 0, NULL, 0);
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    return ret;
  ret = uart_param_config(CONFIG_APP_RS485_UART_PORT, &cfg);
  if (ret != ESP_OK)
    return ret;
  ret = uart_set_pin(CONFIG_APP_RS485_UART_PORT, CONFIG_APP_RS485_TX_GPIO,
                     CONFIG_APP_RS485_RX_GPIO, CONFIG_APP_RS485_DE_GPIO,
                     UART_PIN_NO_CHANGE);
  if (ret != ESP_OK)
    return ret;
  ret = uart_set_mode(CONFIG_APP_RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX);
  if (ret != ESP_OK)
    return ret;

  // Imposta il timeout di ricezione (RX TOUT) a 10ms.
  // Calcoliamo i simboli in base al baud rate: (baud * 10ms) / (10 bits *
  // 1000ms) = baud / 1000
  int tout_symbols = d_cfg->rs485.baud_rate / 1000;
  if (tout_symbols < 1)
    tout_symbols = 1;
  ret = uart_set_rx_timeout(CONFIG_APP_RS485_UART_PORT, tout_symbols);
  if (ret != ESP_OK)
    return ret;

//   gpio_set_direction(CONFIG_APP_RS485_DE_GPIO, GPIO_MODE_OUTPUT);
//   gpio_set_level(CONFIG_APP_RS485_DE_GPIO, 0);
  s_rs485_initialized = true;
  return ESP_OK;
}

esp_err_t rs485_deinit(void) {
  esp_err_t ret = uart_driver_delete(CONFIG_APP_RS485_UART_PORT);
  if (ret == ESP_ERR_INVALID_STATE) {
    return ESP_OK;
  }
  return ret;
}

/**
 * @brief Riceve dati dalla porta RS485.
 *
 * La funzione termina e restituisce i dati in tre scenari:
 * 1. Raggiungimento di 'max_len': se arrivano esattamente i byte richiesti,
 * restituisce immediatamente.
 * 2. Timeout Hardware (Fine Trama): se vengono ricevuti dei byte ma la linea
 * rimane inattiva per 10ms (configurati nell'init), la funzione termina e
 * restituisce i byte accumulati finora.
 * 3. Timeout Software: se non arriva alcun dato per il tempo 'timeout_ms', la
 * funzione restituisce 0.
 *
 * @param data Buffer di destinazione
 * @param max_len Numero massimo di byte da leggere
 * @param timeout_ms Tempo massimo di attesa software in millisecondi
 * @return int Numero di byte effettivamente letti, o -1 in caso di errore.
 */
int rs485_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms) {
  return uart_read_bytes(CONFIG_APP_RS485_UART_PORT, data, max_len,
                         pdMS_TO_TICKS(timeout_ms));
}

/**
 * @brief Invia dati sulla porta RS485.
 *
 * Scrive i byte passati nel buffer di trasmissione della UART.
 * Essendo configurata in modalità RS485 Half Duplex, il driver gestisce
 * automaticamente il segnale DE (Data Enable) per commutare tra TX e RX.
 *
 * @param data Puntatore ai dati da inviare
 * @param len Lunghezza dei dati in byte
 * @return int Numero di byte effettivamente inviati, o -1 in caso di errore.
 */
int rs485_send(const uint8_t *data, size_t len) {
  return uart_write_bytes(CONFIG_APP_RS485_UART_PORT, data, len);
}

/**
 * @brief Restituisce lo stato operativo del driver RS485.
 *
 * DISABLED : uart_driver non installato
 * ONLINE   : uart_driver installato e porta operativa
 */
hw_component_status_t rs485_get_status(void)
{
    return s_rs485_initialized ? HW_STATUS_ONLINE : HW_STATUS_DISABLED;
}

#endif /* DNA_RS485 == 0 */

/*
 * Mockup — nessuna UART RS485 reale.
 * Attiva quando DNA_RS485 == 1
 */
#if defined(DNA_RS485) && (DNA_RS485 == 1)

/**
 * @brief Inizializza la comunicazione RS485.
 *
 * Questa funzione configura e inizializza il periferico RS485 per la
 * comunicazione.
 *
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t rs485_init(void) {
  ESP_LOGI(TAG, "[C] [MOCK] rs485_init: bus RS485 simulato");
  return ESP_OK;
}

esp_err_t rs485_deinit(void) {
  ESP_LOGI(TAG, "[C] [MOCK] rs485_deinit: bus RS485 simulato");
  return ESP_OK;
}

/**
 * @brief Riceve dati tramite RS485.
 *
 * Questa funzione riceve dati tramite la linea RS485 e li memorizza nel buffer
 * fornito.
 *
 * @param [in/out] data Puntatore al buffer dove memorizzare i dati ricevuti.
 * @param max_len Lunghezza massima del buffer.
 * @param timeout_ms Timeout in millisecondi per la ricezione dei dati.
 * @return Numero di byte ricevuti, o -1 in caso di errore.
 */
int rs485_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms) {
  (void)data;
  (void)max_len;
  (void)timeout_ms;
  return 0; /* nessun dato sul bus */
}

/**
 * @brief Invia dati tramite RS485.
 *
 * Questa funzione invia un buffer di dati tramite la linea RS485.
 *
 * @param [in] data Puntatore al buffer di dati da inviare.
 * @param [in] len Lunghezza del buffer di dati.
 * @return Numero di byte inviati, o un valore negativo in caso di errore.
 */
int rs485_send(const uint8_t *data, size_t len) {
  ESP_LOGI(TAG, "[C] [MOCK] rs485_send: %zu byte ignorati", len);
  (void)data;
  return (int)len; /* simulazione invio riuscito */
}

/* [C] Mockup: get_status restituisce DISABLED (nessun HW reale) */
hw_component_status_t rs485_get_status(void) { return HW_STATUS_DISABLED; }

#endif /* DNA_RS485 == 1 */
