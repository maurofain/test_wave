#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DIGITAL_IO_LOCAL_OUTPUT_COUNT 4U
#define DIGITAL_IO_LOCAL_INPUT_COUNT 4U
#define DIGITAL_IO_MODBUS_OUTPUT_COUNT 8U
#define DIGITAL_IO_MODBUS_INPUT_COUNT 8U

#define DIGITAL_IO_OUTPUT_COUNT (DIGITAL_IO_LOCAL_OUTPUT_COUNT + DIGITAL_IO_MODBUS_OUTPUT_COUNT)
#define DIGITAL_IO_INPUT_COUNT (DIGITAL_IO_LOCAL_INPUT_COUNT + DIGITAL_IO_MODBUS_INPUT_COUNT)

#define DIGITAL_IO_FIRST_MODBUS_OUTPUT (DIGITAL_IO_LOCAL_OUTPUT_COUNT + 1U)
#define DIGITAL_IO_FIRST_MODBUS_INPUT (DIGITAL_IO_LOCAL_INPUT_COUNT + 1U)

typedef struct {
    uint16_t outputs_mask;
    uint16_t inputs_mask;
} digital_io_snapshot_t;

typedef struct {
    uint8_t input_id;
    bool available;
    bool is_local;
} digital_io_input_info_t;

/**
 * @brief Inizializza il layer unificato I/O digitale.
 *
 * L'inizializzazione è idempotente e può essere richiamata più volte.
 */
esp_err_t digital_io_init(void);

/**
 * @brief Imposta uno dei canali uscita R01..R12.
 *
 * Mapping:
 * - R01..R04: I/O locali su scheda
 * - R05..R12: scheda espansione Modbus
 */
esp_err_t digital_io_set_output(uint8_t output_id, bool value);

/**
 * @brief Legge lo stato di una uscita R01..R12.
 */
esp_err_t digital_io_get_output(uint8_t output_id, bool *out_value);

/**
 * @brief Legge lo stato di un ingresso IN01..IN12.
 *
 * Mapping:
 * - IN01..IN04: I/O locali su scheda
 * - IN05..IN12: scheda espansione Modbus
 */
esp_err_t digital_io_get_input(uint8_t input_id, bool *out_value);

/**
 * @brief Imposta in batch le uscite R01..R12 usando una maschera bit.
 *
 * Il bit 0 corrisponde a R01, il bit 11 a R12.
 */
esp_err_t digital_io_set_outputs_mask(uint16_t outputs_mask);

/**
 * @brief Legge snapshot completo di uscite e ingressi (R01..R12, IN01..IN12).
 *
 * Nei campi mask il bit 0 corrisponde al canale 1.
 */
esp_err_t digital_io_get_snapshot(digital_io_snapshot_t *out_snapshot);

/**
 * @brief Restituisce la lista degli ingressi digitali disponibili per la UI.
 *
 * Se `out_list` è NULL, ritorna il numero totale di ingressi enumerabili.
 * In caso contrario popola fino a `max_items` elementi.
 *
 * @param out_list buffer output opzionale
 * @param max_items elementi massimi da scrivere in out_list
 * @return numero di elementi scritti (o totali se out_list==NULL)
 */
size_t digital_io_get_input_infos(digital_io_input_info_t *out_list, size_t max_items);
