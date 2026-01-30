#ifndef AUX_GPIO_H
#define AUX_GPIO_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inizializza i GPIO ausiliari basandosi sulla configurazione salvata.
 * 
 * @return esp_err_t 
 */
esp_err_t aux_gpio_init(void);

/**
 * @brief Ottiene lo stato corrente di un GPIO ausiliario in formato JSON.
 * 
 * @param json_buffer Buffer dove scrivere il JSON
 * @param max_len Lunghezza massima del buffer
 * @return esp_err_t 
 */
esp_err_t aux_gpio_get_all_json(char *json_buffer, size_t max_len);

/**
 * @brief Imposta il livello di un GPIO ausiliario.
 * 
 * @param pin Numero del pin (es. 33)
 * @param level Livello (0 o 1)
 * @return esp_err_t 
 */
esp_err_t aux_gpio_set_level(int pin, int level);

#endif // AUX_GPIO_H
