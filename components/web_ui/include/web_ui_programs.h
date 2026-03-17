#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/**
 * @file web_ui_programs.h
 * @brief Tipi e interfaccia per gestione programmi emulatori
 */

#define WEB_UI_PROGRAM_MAX 10
#define WEB_UI_PROGRAM_NAME_MAX 32
#define WEB_UI_VIRTUAL_RELAY_MAX 10

/**
 * @brief Rappresenta un singolo programma configurabile
 */
typedef struct {
    uint8_t program_id;
    char name[WEB_UI_PROGRAM_NAME_MAX];
    bool enabled;
    uint16_t price_units;
    uint16_t duration_sec;
    uint16_t pause_max_suspend_sec;
    uint16_t relay_mask;
} web_ui_program_entry_t;

typedef struct {
    uint8_t count;
    web_ui_program_entry_t programs[WEB_UI_PROGRAM_MAX];
} web_ui_program_table_t;

typedef struct {
    bool status;
    uint32_t duration_ms;
} web_ui_virtual_relay_state_t;

/**
 * @brief Ottiene la tabella programmi corrente
 */
const web_ui_program_table_t *web_ui_program_table_get(void);
esp_err_t web_ui_program_table_init(void);
#/**
 * @brief Serializza la tabella programmi in JSON (allocato)
 */
char *web_ui_program_table_to_json(void);
/**
 * @brief Aggiorna la tabella a partire da JSON
 */
esp_err_t web_ui_program_table_update_from_json(const char *json_payload, size_t len, char *err_msg, size_t err_msg_len);

esp_err_t web_ui_virtual_relay_control(uint8_t relay_number, bool status, uint32_t duration_ms);
bool web_ui_virtual_relay_get(uint8_t relay_number, web_ui_virtual_relay_state_t *state_out);
char *web_ui_virtual_relays_to_json(void);
