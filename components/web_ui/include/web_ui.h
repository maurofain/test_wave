#pragma once

#include "esp_http_server.h"

/**
 * @brief Inizializza e avvia il server HTTP con tutti i relativi handler
 * @return ESP_OK se avviato correttamente
 */
esp_err_t web_ui_init(void);

/**
 * @brief Ferma il server HTTP
 */
void web_ui_stop(void);
