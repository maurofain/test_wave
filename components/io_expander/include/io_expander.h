#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include "hw_common.h"

extern uint8_t io_output_state;
extern uint8_t io_input_state;

/**
 * @brief Inizializza gli I/O Expander (FXL6408)
 * @return ESP_OK in caso di successo
 */
esp_err_t io_expander_init(void);

/**
 * @brief Imposta il flag di abilitazione dell'IO expander basato sulla configurazione
 * @param enabled True se abilitato, false se disabilitato
 */
void io_expander_set_config_enabled(bool enabled);

/**
 * @brief Imposta un pin di uscita specifico
 * @param pin Numero del pin (0-7)
 * @param value 0 o 1
 */
void io_set_pin(int pin, int value);

/**
 * @brief Imposta tutti i pin di uscita contemporaneamente
 * @param val Valore a 8 bit da impostare
 */
void io_set_port(uint8_t val);

/**
 * @brief Legge un pin di ingresso specifico
 * @param pin Numero del pin (0-7)
 * @return true se alto, false se basso
 */
bool io_get_pin(int pin);

/**
 * @brief Legge tutti i pin di ingresso contemporaneamente
 * @return Valore a 8 bit della porta di ingresso
 */
uint8_t io_get(void);
hw_component_status_t io_expander_get_status(void);
