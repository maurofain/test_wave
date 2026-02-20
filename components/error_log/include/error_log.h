#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief inizializza il modulo di logging errori SD
 *        monta la SD se necessaria e installa il vprintf wrapper
 */
void error_log_init(void);

/**
 * @brief scrive un messaggio di errore nel log corrente (interno)
 *        utilizza sd_card_write_file e genera un nuovo file se necessario
 */
void error_log_write_msg(const char *msg);

