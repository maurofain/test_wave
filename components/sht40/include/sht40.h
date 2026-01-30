#ifndef SHT40_H
#define SHT40_H

#include "esp_err.h"

/**
 * @brief Inizializza il sensore SHT40
 * @return ESP_OK se successo
 */
esp_err_t sht40_init(void);

/**
 * @brief Legge temperatura e umidità dal sensore
 * @param temp Puntatore dove salvare la temperatura (°C)
 * @param hum Puntatore dove salvare l'umidità relativa (%)
 * @return ESP_OK se successo
 */
esp_err_t sht40_read(float *temp, float *hum);

#endif
