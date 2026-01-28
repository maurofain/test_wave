#pragma once

#include "esp_err.h"
#include "led_strip.h"

/**
 * @brief Inizializza il controller LED e crea la striscia
 * @return ESP_OK se riuscito
 */
esp_err_t led_control_init(void);

/**
 * @brief Ottiene l'handle della LED strip
 * @return Handle della LED strip, NULL se non inizializzato
 */
led_strip_handle_t led_control_get_handle(void);

/**
 * @brief Spegne tutti i LED
 * @return ESP_OK se riuscito
 */
esp_err_t led_control_clear(void);

/**
 * @brief Accende tutti i LED con colore specificato
 * @param red Valore rosso (0-255)
 * @param green Valore verde (0-255)
 * @param blue Valore blu (0-255)
 * @return ESP_OK se riuscito
 */
esp_err_t led_control_fill_color(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Imposta il colore di un LED specifico
 * @param index Indice del LED (0-based)
 * @param red Valore rosso (0-255)
 * @param green Valore verde (0-255)
 * @param blue Valore blu (0-255)
 * @return ESP_OK se riuscito
 */
esp_err_t led_control_set_pixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Esegui animazione di respirazione (pulsazione)
 * @param red Valore rosso (0-255)
 * @param green Valore verde (0-255)
 * @param blue Valore blu (0-255)
 * @param duration_ms Durata della pulsazione in millisecondi
 * @return ESP_OK se riuscito
 */
esp_err_t led_control_breathe(uint8_t red, uint8_t green, uint8_t blue, uint32_t duration_ms);

/**
 * @brief Esegui animazione a cascata (rainbow)
 * @param duration_ms Durata dell'animazione in millisecondi per ciclo
 * @return ESP_OK se riuscito
 */
esp_err_t led_control_rainbow(uint32_t duration_ms);

/**
 * @brief Esegui animazione di accensione progressiva
 * @param red Valore rosso (0-255)
 * @param green Valore verde (0-255)
 * @param blue Valore blu (0-255)
 * @param steps Numero di step di accensione
 * @param step_duration_ms Durata di ogni step in millisecondi
 * @return ESP_OK se riuscito
 */
esp_err_t led_control_fade_in(uint8_t red, uint8_t green, uint8_t blue, uint32_t steps, uint32_t step_duration_ms);

/**
 * @brief Esegui animazione di spegnimento progressivo
 * @param steps Numero di step di spegnimento
 * @param step_duration_ms Durata di ogni step in millisecondi
 * @return ESP_OK se riuscito
 */
esp_err_t led_control_fade_out(uint32_t steps, uint32_t step_duration_ms);
