#pragma once

#include "esp_err.h"
#include "esp_netif.h"
#include "led_strip.h"

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
