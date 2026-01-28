#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Inizializza l'interfaccia per la EEPROM 24LC16
 * @return ESP_OK se riuscito
 */
esp_err_t eeprom_24lc16_init(void);

/**
 * @brief Legge dati dalla EEPROM
 * @param address Indirizzo di partenza (0-2047)
 * @param buffer Buffer dove memorizzare i dati
 * @param length Numero di byte da leggere
 * @return ESP_OK se riuscito
 */
esp_err_t eeprom_24lc16_read(uint16_t address, uint8_t *buffer, size_t length);

/**
 * @brief Scrive dati nella EEPROM (gestisce internamente le pagine da 16 byte)
 * @param address Indirizzo di partenza (0-2047)
 * @param buffer Dati da scrivere
 * @param length Numero di byte da scrivere
 * @return ESP_OK se riuscito
 */
esp_err_t eeprom_24lc16_write(uint16_t address, const uint8_t *buffer, size_t length);

/**
 * @brief Legge un singolo byte
 */
esp_err_t eeprom_24lc16_read_byte(uint16_t address, uint8_t *val);

/**
 * @brief Scrive un singolo byte
 */
esp_err_t eeprom_24lc16_write_byte(uint16_t address, uint8_t val);
