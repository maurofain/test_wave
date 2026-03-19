#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DIGITAL_IO_LOCAL_OUTPUT_COUNT 8U
#define DIGITAL_IO_LOCAL_INPUT_COUNT 8U
#define DIGITAL_IO_MODBUS_OUTPUT_COUNT 8U
#define DIGITAL_IO_MODBUS_INPUT_COUNT 8U

#define DIGITAL_IO_OUTPUT_COUNT (DIGITAL_IO_LOCAL_OUTPUT_COUNT + DIGITAL_IO_MODBUS_OUTPUT_COUNT)
#define DIGITAL_IO_INPUT_COUNT (DIGITAL_IO_LOCAL_INPUT_COUNT + DIGITAL_IO_MODBUS_INPUT_COUNT)

#define DIGITAL_IO_FIRST_MODBUS_OUTPUT (DIGITAL_IO_LOCAL_OUTPUT_COUNT + 1U)
#define DIGITAL_IO_FIRST_MODBUS_INPUT (DIGITAL_IO_LOCAL_INPUT_COUNT + 1U)

#define DIGITAL_IO_ERR_BASE 0x7600
#define DIGITAL_IO_ERR_CONFIG_NOT_READY (DIGITAL_IO_ERR_BASE + 1)
#define DIGITAL_IO_ERR_LOCAL_IO_DISABLED (DIGITAL_IO_ERR_BASE + 2)
#define DIGITAL_IO_ERR_MODBUS_DISABLED (DIGITAL_IO_ERR_BASE + 3)

#define DIGITAL_IO_OUTPUT_WHITE_LED 1U
#define DIGITAL_IO_OUTPUT_BLUE_LED 2U
#define DIGITAL_IO_OUTPUT_RELAY3 3U
#define DIGITAL_IO_OUTPUT_RELAY4 4U
#define DIGITAL_IO_OUTPUT_RELAY2 5U
#define DIGITAL_IO_OUTPUT_RELAY1 6U
#define DIGITAL_IO_OUTPUT_RED_LED 7U
#define DIGITAL_IO_OUTPUT_HEATER1 8U

#define DIGITAL_IO_INPUT_DIP1 1U
#define DIGITAL_IO_INPUT_DIP2 2U
#define DIGITAL_IO_INPUT_DIP3 3U
#define DIGITAL_IO_INPUT_SERVICE_SWITCH 4U
#define DIGITAL_IO_INPUT_OPTO2 5U
#define DIGITAL_IO_INPUT_OPTO1 6U
#define DIGITAL_IO_INPUT_OPTO4 7U
#define DIGITAL_IO_INPUT_OPTO3 8U

typedef struct {
    uint16_t outputs_mask;
    uint16_t inputs_mask;
} digital_io_snapshot_t;

typedef struct {
    uint8_t input_id;
    bool available;
    bool is_local;
    bool touch_mappable;
    bool io_process_consumer;
    char code[24];
} digital_io_input_info_t;

/**
 * @brief Inizializza il layer unificato I/O digitale.
 *
 * L'inizializzazione è idempotente e può essere richiamata più volte.
 */
esp_err_t digital_io_init(void);

/**
 * @brief Imposta uno dei canali uscita digitali.
 *
 * Mapping:
 * - OUT01..OUT08: I/O locali su scheda
 * - OUT09..OUT16: scheda espansione Modbus
 */
esp_err_t digital_io_set_output(uint8_t output_id, bool value);

/**
 * @brief Legge lo stato di una uscita digitale.
 */
esp_err_t digital_io_get_output(uint8_t output_id, bool *out_value);

/**
 * @brief Legge lo stato di un ingresso digitale.
 *
 * Mapping:
 * - IN01..IN08: I/O locali su scheda
 * - IN09..IN16: scheda espansione Modbus
 */
esp_err_t digital_io_get_input(uint8_t input_id, bool *out_value);

/**
 * @brief Imposta in batch le uscite digitali usando una maschera bit.
 *
 * Il bit 0 corrisponde al canale 1.
 */
esp_err_t digital_io_set_outputs_mask(uint16_t outputs_mask);

/**
 * @brief Legge snapshot completo di uscite e ingressi digitali.
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

/**
 * @brief Verifica se un ingresso puo' essere mappato come tasto touch.
 */
bool digital_io_input_is_touch_mappable(uint8_t input_id);

/**
 * @brief Verifica se un ingresso e' di competenza del task io_process.
 */
bool digital_io_input_is_io_process_signal(uint8_t input_id);

/**
 * @brief Verifica se una uscita e' un relay pilotabile dai programmi.
 */
bool digital_io_output_is_program_relay(uint8_t output_id);

/**
 * @brief Verifica se una uscita e' di competenza del task io_process.
 */
bool digital_io_output_is_io_process_signal(uint8_t output_id);

/**
 * @brief Converte un relay logico programma nel corrispondente output fisico.
 */
esp_err_t digital_io_program_relay_to_output_id(uint8_t relay_number, uint8_t *out_output_id);

/**
 * @brief Restituisce il codice descrittivo di un ingresso.
 */
esp_err_t digital_io_input_get_code(uint8_t input_id, char *out_code, size_t out_code_len);

/**
 * @brief Restituisce il codice descrittivo di una uscita.
 */
esp_err_t digital_io_output_get_code(uint8_t output_id, char *out_code, size_t out_code_len);
