#pragma once

#include "esp_err.h"

/**
 * @brief Invia un pacchetto MDB custom (Il primo byte è l'indirizzo+comando)
 */
esp_err_t test_mdb_send_custom(const char* hex_or_str);
