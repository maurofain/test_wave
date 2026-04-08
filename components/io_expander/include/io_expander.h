#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

extern uint8_t io_output_state;
extern uint8_t io_input_state;

/**
 * @brief Snapshot atomico dello stato degli I/O expander
 */
typedef struct {
    uint8_t output_state;  /* Stato dell'ultima scrittura sui pin di uscita (0-7) */
    uint8_t input_state;   /* Stato dell'ultima lettura dai pin di ingresso (0-7) */
} io_expander_snapshot_t;

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

/**
 * @brief [C] Ritorna uno snapshot atomico dello stato degli I/O expander
 * @note Questa funzione è thread-safe e non richiede I2C; legge i valori cached
 * @return Snapshot contenente gli stati di output e input
 */
io_expander_snapshot_t io_expander_get_snapshot(void);
