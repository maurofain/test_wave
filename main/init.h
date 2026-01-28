#pragma once

#include "esp_err.h"
#include "led_strip.h"

esp_err_t init_run_factory(void);

/**
 * @brief Ottiene l'handle della LED strip
 * @note La LED strip è inizializzata automaticamente in init_run_factory()
 *       tramite led_control_init(). Usa questa funzione per ottenerla.
 * @return Handle della LED strip, NULL se non inizializzato
 */
led_strip_handle_t init_get_ws2812_handle(void);
