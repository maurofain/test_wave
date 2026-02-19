#pragma once

#include "esp_http_server.h"
#include <stdbool.h>

/**
 * @brief Inizializza e avvia il server HTTP con tutti i relativi handler
 * @return ESP_OK se avviato correttamente
 */
esp_err_t web_ui_init(void);

/**
 * @brief Ferma il server HTTP
 */
void web_ui_stop(void);

/**
 * @brief Ritorna true se il server HTTP Web UI è attivo
 */
bool web_ui_is_running(void);

/**
 * @brief Aggiunge un log internamente (per uso da altri componenti)
 * @param level Livello del log (INFO, ERROR, etc.)
 * @param tag Tag del componente che genera il log
 * @param message Messaggio del log
 */
void web_ui_add_log(const char *level, const char *tag, const char *message);
