#pragma once

#include "esp_err.h"
#include "esp_netif.h"
#include "led_strip.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
	INIT_AGENT_ERR_NONE = 0,
	INIT_AGENT_ERR_NOT_EVALUATED = 1,
	INIT_AGENT_ERR_DISABLED_BY_CONFIG = 2,
	INIT_AGENT_ERR_INIT_FAILED = 3,
	INIT_AGENT_ERR_DEPENDENCY_FAILED = 4,
	INIT_AGENT_ERR_NETWORK_NO_IP = 5,
	INIT_AGENT_ERR_REMOTE_LOGIN_FAILED = 6,
	INIT_AGENT_ERR_RUNTIME_FAILED = 7,
	INIT_AGENT_ERR_NOT_AVAILABLE = 8,
} init_agent_error_code_t;

typedef struct {
	int32_t agn_value;
	int32_t state;
	int32_t error_code;
} init_agent_status_t;

esp_err_t init_run_factory(void);

/**
 * @brief Ottiene i netif per le varie interfacce
 */
void init_get_netifs(esp_netif_t **ap, esp_netif_t **sta, esp_netif_t **eth);

/**
 * @brief Ottiene l'handle della LED strip
 * @note La LED strip è inizializzata automaticamente in init_run_factory()
 *       tramite led_init(). Usa questa funzione per ottenerla.
 * @return Handle della LED strip, NULL se non inizializzato
 */
led_strip_handle_t init_get_ws2812_handle(void);

/**
 * @brief Forza la sincronizzazione NTP manuale
 * @return ESP_OK se la sincronizzazione è riuscita, ESP_FAIL altrimenti
 */
esp_err_t init_sync_ntp(void);

void init_i2c_and_io_expander(void); // Inizializza I2C e I/O Expander e controlla GPIO3

/**
 * @brief Ritorna true se il sistema è entrato in ERROR_LOCK per reboot consecutivi.
 */
bool init_is_error_lock_active(void);

/**
 * @brief Ritorna il numero di reboot consecutivi registrati.
 */
uint32_t init_get_consecutive_reboots(void);

/**
 * @brief Marca il boot come completato e azzera il contatore reboot consecutivi.
 */
esp_err_t init_mark_boot_completed(void);

/**
 * @brief Registra in NVS una richiesta di crash forzato da consumare al boot successivo.
 */
esp_err_t init_mark_forced_crash_request(void);

/**
 * @brief Inizializza display + LVGL senza touch, senza pannello emulatore.
 *
 * Va chiamata quando si vuole mostrare una schermata di errore (es. ERROR_LOCK)
 * prima che il boot normale sia completato. È no-op se il display è disabilitato
 * da config o se il DNA_LVGL mock è attivo.
 *
 * @return ESP_OK se il display è stato avviato, altrimenti il codice di errore.
 */
esp_err_t init_run_display_only(void);

/**
 * @brief Reinizializza la tabella globale stati agenti ai valori di default.
 */
void init_agent_status_reset_defaults(void);

/**
 * @brief Aggiorna stato e codice errore di un agente AGN.
 *
 * Convenzione stato: 0=fail, valori diversi da 0=non-fail.
 */
void init_agent_status_set(int32_t agn_value, int32_t state, init_agent_error_code_t error_code);

/**
 * @brief Restituisce la tabella globale stati agenti.
 *
 * Il primo elemento (AGN_ID_NONE) contiene nel campo `state` il numero totale
 * di device in errore (stato==0).
 */
const init_agent_status_t *init_agent_status_get_table(size_t *out_count);

/**
 * @brief Restituisce la descrizione testuale del codice errore agente.
 */
const char *init_agent_error_code_text(init_agent_error_code_t code);
