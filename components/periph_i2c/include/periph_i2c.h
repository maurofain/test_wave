#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inizializza il bus I2C delle periferiche applicative (GPIO27/GPIO26, NUM=0).
 *        Idempotente: chiamate successive restituiscono ESP_OK senza reinizializzare.
 *
 * @return ESP_OK oppure codice di errore ESP-IDF.
 */
esp_err_t periph_i2c_init(void);

/**
 * @brief Restituisce l'handle del bus I2C periferiche.
 *        Ritorna NULL se periph_i2c_init() non è stata ancora chiamata.
 */
i2c_master_bus_handle_t periph_i2c_get_handle(void);

#ifdef __cplusplus
}
#endif
