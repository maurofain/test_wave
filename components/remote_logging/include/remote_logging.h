#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Inizializza il sistema di logging remoto
 *
 * @return ESP_OK se l'inizializzazione è riuscita, ESP_FAIL altrimenti
 */
esp_err_t remote_logging_init(void);

/**
 * @brief Invia un messaggio di log al server remoto
 *
 * @param level Livello del log (es. "INFO", "WARN", "ERROR")
 * @param tag Tag del componente che genera il log
 * @param message Messaggio di log
 * @return ESP_OK se l'invio è riuscito, ESP_FAIL altrimenti
 */
esp_err_t remote_logging_send(const char *level, const char *tag, const char *message);

/**
 * @brief Verifica se il logging remoto è abilitato
 *
 * @return true se abilitato, false altrimenti
 */
bool remote_logging_is_enabled(void);

/**
 * @brief Ferma il sistema di logging remoto
 */
void remote_logging_stop(void);