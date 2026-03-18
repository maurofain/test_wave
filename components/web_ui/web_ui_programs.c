#include "web_ui_programs.h"
#include "fsm.h"
#include "device_config.h"
#include "tasks.h"

#include "cJSON.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * @file web_ui_programs.c
 * @brief Gestione della tabella programmi e dei relè virtuali
 *
 * Questo modulo mantiene in RAM la configurazione dei "programmi"
 * (usati in emulatori e nella macchina reale) e fornisce API per serializzare
 * su JSON, caricare/salvare da file e modificare stati dei relè virtuali.
 */

#define TAG "WEB_UI_PROGRAMS"
#define PROGRAMS_STORAGE_PATH "/spiffs/programs.json"
#define PROGRAM_NAME_KEY_MAX 32

static web_ui_program_table_t s_program_table;
static web_ui_virtual_relay_state_t s_virtual_relays[WEB_UI_VIRTUAL_RELAY_MAX + 1];
static bool s_initialized = false;
static bool s_storage_loaded = false;

static char *programs_table_to_json_internal(bool include_display_name);

static void program_name_build_key(uint8_t program_id, char *key_out, size_t key_out_len)
{
    if (!key_out || key_out_len == 0) {
        return;
    }

    if (program_id == 0) {
        key_out[0] = '\0';
        return;
    }

    snprintf(key_out, key_out_len, "program_name_%02u", (unsigned)program_id);
}

static void program_name_build_fallback(uint8_t program_id, char *fallback_out, size_t fallback_out_len)
{
    if (!fallback_out || fallback_out_len == 0) {
        return;
    }

    snprintf(fallback_out, fallback_out_len, "Programma %u", (unsigned)program_id);
}

static void program_entry_refresh_name(web_ui_program_entry_t *entry)
{
    if (!entry) {
        return;
    }

    char key[PROGRAM_NAME_KEY_MAX] = {0};
    char fallback[WEB_UI_PROGRAM_NAME_MAX] = {0};
    char localized[WEB_UI_PROGRAM_NAME_MAX] = {0};

    program_name_build_key(entry->program_id, key, sizeof(key));
    program_name_build_fallback(entry->program_id, fallback, sizeof(fallback));

    if (key[0] != '\0' &&
        device_config_get_ui_text_scoped("lvgl", key, fallback, localized, sizeof(localized)) == ESP_OK &&
        localized[0] != '\0') {
        snprintf(entry->name, sizeof(entry->name), "%s", localized);
        return;
    }

    snprintf(entry->name, sizeof(entry->name), "%s", fallback);
}

static void programs_refresh_names_from_i18n(void)
{
    for (uint8_t index = 0; index < s_program_table.count; ++index) {
        program_entry_refresh_name(&s_program_table.programs[index]);
    }
}

/**
 * @brief Salva la tabella dei programmi su SPIFFS
 *
 * Converte la tabella corrente in JSON ed esegue fwrite sul percorso
 * definito da PROGRAMS_STORAGE_PATH. Usa malloc/free internamente.
 *
 * @return ESP_OK se la scrittura ha scritto tutti i byte, ESP_FAIL altrimenti
 */
static esp_err_t programs_save_to_storage(void)
{
    char *json = programs_table_to_json_internal(false);
    if (!json) {
        return ESP_FAIL;
    }

    FILE *file = fopen(PROGRAMS_STORAGE_PATH, "w");
    if (!file) {
        free(json);
        return ESP_FAIL;
    }

    size_t len = strlen(json);
    size_t written = fwrite(json, 1, len, file);
    fclose(file);
    free(json);

    return (written == len) ? ESP_OK : ESP_FAIL;
}

/**
 * @brief Tenta di caricare la tabella programmi da file
 *
 * Se il file è assente o malformato viene mantenuta la configurazione
 * di default (generata da programs_init_defaults()).
 */
static void programs_try_load_from_storage(void)
{
    FILE *file = fopen(PROGRAMS_STORAGE_PATH, "r");
    if (!file) {
        ESP_LOGW(TAG, "Nessun file programmi in %s, uso default", PROGRAMS_STORAGE_PATH);
        return;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return;
    }
    long size = ftell(file);
    if (size <= 0 || size > 16384) {
        fclose(file);
        ESP_LOGW(TAG, "Dimensione file programmi non valida: %ld", size);
        return;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return;
    }

    char *payload = calloc(1, (size_t)size + 1);
    if (!payload) {
        fclose(file);
        return;
    }

    size_t read = fread(payload, 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size) {
        free(payload);
        ESP_LOGW(TAG, "Lettura file programmi incompleta");
        return;
    }

    char err_msg[128] = {0};
    esp_err_t err = web_ui_program_table_update_from_json(payload, (size_t)size, err_msg, sizeof(err_msg));
    free(payload);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Tabella programmi caricata da %s", PROGRAMS_STORAGE_PATH);
    } else {
        ESP_LOGW(TAG, "Errore caricamento tabella programmi da file: %s", err_msg[0] ? err_msg : "unknown");
    }
}

/**
 * @brief Inizializza la tabella programmi con valori predefiniti
 *
 * La funzione è eseguita in modo pigro la prima volta che serve, e
 * successivamente carica eventuali dati salvati su memoria permanente.
 */
static void programs_init_defaults(void)
{
    if (s_initialized) {
        return;
    }

    memset(&s_program_table, 0, sizeof(s_program_table));
    s_program_table.count = 10;
    for (uint8_t index = 0; index < s_program_table.count; ++index) {
        web_ui_program_entry_t *entry = &s_program_table.programs[index];
        entry->program_id = (uint8_t)(index + 1);
        entry->name[0] = '\0';
        entry->enabled = true;
        entry->price_units = 10;
        entry->duration_sec = 60;
        entry->pause_max_suspend_sec = 60;
        entry->relay_mask = (uint16_t)(1U << index);
    }

    programs_refresh_names_from_i18n();

    memset(s_virtual_relays, 0, sizeof(s_virtual_relays));
    s_initialized = true;
}

esp_err_t web_ui_program_table_init(void)
{
    programs_init_defaults();
    if (!s_storage_loaded) {
        programs_try_load_from_storage();
        s_storage_loaded = true;
    }
    return ESP_OK;
}

/**
 * @brief Restituisce puntatore alla tabella programmi attuale
 *
 * Garantisce l'inizializzazione lazy se necessario.
 *
 * @return puntatore const a una struttura static
 */
const web_ui_program_table_t *web_ui_program_table_get(void)
{
    programs_init_defaults();
    programs_refresh_names_from_i18n();
    return &s_program_table;
}

static char *programs_table_to_json_internal(bool include_display_name)
{
    programs_init_defaults();
    programs_refresh_names_from_i18n();

    cJSON *root = cJSON_CreateObject();
    cJSON *programs = cJSON_CreateArray();
    if (!root || !programs) {
        cJSON_Delete(root);
        cJSON_Delete(programs);
        return NULL;
    }

    for (uint8_t index = 0; index < s_program_table.count; ++index) {
        const web_ui_program_entry_t *entry = &s_program_table.programs[index];
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            continue;
        }

        cJSON_AddNumberToObject(item, "program_id", entry->program_id);
        if (include_display_name) {
            cJSON_AddStringToObject(item, "name", entry->name);
        }
        cJSON_AddBoolToObject(item, "enabled", entry->enabled);
        cJSON_AddNumberToObject(item, "price_units", entry->price_units);
        cJSON_AddNumberToObject(item, "duration_sec", entry->duration_sec);
        cJSON_AddNumberToObject(item, "pause_max_suspend_sec", entry->pause_max_suspend_sec);
        cJSON_AddNumberToObject(item, "relay_mask", entry->relay_mask);
        cJSON_AddItemToArray(programs, item);
    }

    cJSON_AddItemToObject(root, "programs", programs);
    cJSON_AddNumberToObject(root, "count", s_program_table.count);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

/**
 * @brief Converte la tabella programmi in stringa JSON minificata
 *
 * L'utente deve liberare la memoria restituita con free().
 *
 * @return stringa JSON allocata oppure NULL in caso di errore
 */
char *web_ui_program_table_to_json(void)
{
    return programs_table_to_json_internal(true);
}

/**
 * @brief Aggiorna la tabella programmi a partire da un payload JSON
 *
 * Il JSON viene validato, le eventuali inconsistenze ritornano
 * ESP_ERR_INVALID_ARG e un messaggio esplicativo viene copiato in
 * `err_msg` se non NULL.
 *
 * @param json_payload buffer contenente JSON
 * @param len lunghezza del buffer
 * @param err_msg output per messaggio d'errore (opzionale)
 * @param err_msg_len dimensione di err_msg
 * @return ESP_OK se aggiornato con successo, altrimenti codice errore
 */
esp_err_t web_ui_program_table_update_from_json(const char *json_payload, size_t len, char *err_msg, size_t err_msg_len)
{
    programs_init_defaults();

    if (!json_payload || len == 0) {
        if (err_msg && err_msg_len > 0) {
            snprintf(err_msg, err_msg_len, "payload vuoto");
        }
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_ParseWithLength(json_payload, len);
    if (!root) {
        if (err_msg && err_msg_len > 0) {
            snprintf(err_msg, err_msg_len, "JSON non valido");
        }
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *programs = NULL;
    if (cJSON_IsArray(root)) {
        programs = root;
    } else {
        programs = cJSON_GetObjectItem(root, "programs");
    }

    if (!programs || !cJSON_IsArray(programs)) {
        cJSON_Delete(root);
        if (err_msg && err_msg_len > 0) {
            snprintf(err_msg, err_msg_len, "campo programs mancante");
        }
        return ESP_ERR_INVALID_ARG;
    }

    int count = cJSON_GetArraySize(programs);
    if (count < 0 || count > WEB_UI_PROGRAM_MAX) {
        cJSON_Delete(root);
        if (err_msg && err_msg_len > 0) {
            snprintf(err_msg, err_msg_len, "count programmi non valido (max %d)", WEB_UI_PROGRAM_MAX);
        }
        return ESP_ERR_INVALID_SIZE;
    }

    web_ui_program_table_t next = {0};
    next.count = (uint8_t)count;

    for (int index = 0; index < count; ++index) {
        cJSON *item = cJSON_GetArrayItem(programs, index);
        if (!cJSON_IsObject(item)) {
            cJSON_Delete(root);
            if (err_msg && err_msg_len > 0) {
                snprintf(err_msg, err_msg_len, "entry %d non valida", index);
            }
            return ESP_ERR_INVALID_ARG;
        }

        web_ui_program_entry_t *entry = &next.programs[index];

        cJSON *program_id = cJSON_GetObjectItem(item, "program_id");
        cJSON *enabled = cJSON_GetObjectItem(item, "enabled");
        cJSON *price_units = cJSON_GetObjectItem(item, "price_units");
        cJSON *duration_sec = cJSON_GetObjectItem(item, "duration_sec");
        cJSON *pause_max_suspend_sec = cJSON_GetObjectItem(item, "pause_max_suspend_sec");
        cJSON *relay_mask = cJSON_GetObjectItem(item, "relay_mask");

        if (!cJSON_IsNumber(program_id) || !cJSON_IsBool(enabled) ||
            !cJSON_IsNumber(price_units) || !cJSON_IsNumber(duration_sec) || !cJSON_IsNumber(relay_mask)) {
            cJSON_Delete(root);
            if (err_msg && err_msg_len > 0) {
                snprintf(err_msg, err_msg_len, "campi mancanti/non validi entry %d", index);
            }
            return ESP_ERR_INVALID_ARG;
        }

        int pid = program_id->valueint;
        int price = price_units->valueint;
        int duration = duration_sec->valueint;
        int pause_max = cJSON_IsNumber(pause_max_suspend_sec) ? pause_max_suspend_sec->valueint : 60;
        int mask = relay_mask->valueint;

        if (pid <= 0 || pid > 255 || price < 0 || price > 65535 || duration < 0 || duration > 65535 ||
            pause_max < 0 || pause_max > 65535 || mask < 0 || mask > 0xFFFF) {
            cJSON_Delete(root);
            if (err_msg && err_msg_len > 0) {
                snprintf(err_msg, err_msg_len, "valori fuori range entry %d", index);
            }
            return ESP_ERR_INVALID_ARG;
        }

        entry->program_id = (uint8_t)pid;
        entry->enabled = cJSON_IsTrue(enabled);
        entry->price_units = (uint16_t)price;
        entry->duration_sec = (uint16_t)duration;
        entry->pause_max_suspend_sec = (uint16_t)pause_max;
        entry->relay_mask = (uint16_t)mask;

        program_entry_refresh_name(entry);
    }

    s_program_table = next;
    cJSON_Delete(root);

    if (programs_save_to_storage() != ESP_OK) {
        if (err_msg && err_msg_len > 0) {
            snprintf(err_msg, err_msg_len, "tabella aggiornata ma salvataggio su file fallito");
        }
        ESP_LOGE(TAG, "Salvataggio tabella programmi su %s fallito", PROGRAMS_STORAGE_PATH);
        return ESP_FAIL;
    }

    if (err_msg && err_msg_len > 0) {
        snprintf(err_msg, err_msg_len, "ok");
    }
    ESP_LOGI(TAG, "Tabella programmi aggiornata: %u programmi", (unsigned)s_program_table.count);
    return ESP_OK;
}

/**
 * @brief Controlla uno dei relè logici R01..R12 tramite layer I/O unificato
 *
 * L'API mantiene lo stato richiesto nel registro locale e inoltra il comando
 * al backend hardware corretto (I/O locale o Modbus) in base al numero relè.
 *
 * @param relay_number indice relè (1..WEB_UI_VIRTUAL_RELAY_MAX)
 * @param status true per attivare, false per disattivare
 * @param duration_ms durata della segnalazione in millisecondi
 * @return ESP_OK se accettato, errore driver in caso contrario
 */
esp_err_t web_ui_virtual_relay_control(uint8_t relay_number, bool status, uint32_t duration_ms)
{
    programs_init_defaults();

    if (relay_number == 0 || relay_number > WEB_UI_VIRTUAL_RELAY_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t io_err = tasks_digital_io_set_output_via_agent(relay_number,
                                                              status,
                                                              pdMS_TO_TICKS(250));
    if (io_err != ESP_OK) {
        ESP_LOGW(TAG,
                 "[C] Relay R%u non aggiornato su hardware: %s",
                 (unsigned)relay_number,
                 esp_err_to_name(io_err));
        return io_err;
    }

    s_virtual_relays[relay_number].status = status;
    s_virtual_relays[relay_number].duration_ms = duration_ms;

    ESP_LOGI(TAG, "[C] relay %u => status=%s duration_ms=%lu",
             (unsigned)relay_number,
             status ? "ON" : "OFF",
             (unsigned long)duration_ms);

    char msg[FSM_EVENT_TEXT_MAX_LEN] = {0};
    snprintf(msg, sizeof(msg), "Relay R%u %s (dur=%lu)",
             (unsigned)relay_number,
             status ? "ON" : "OFF",
             (unsigned long)duration_ms);
    fsm_append_message(msg);

    return ESP_OK;
}

/**
 * @brief Legge lo stato di un relè virtuale
 *
 * Copia i dati nel buffer fornito. Restituisce false se l'indice non è valido
 * o il puntatore di output è NULL.
 */
bool web_ui_virtual_relay_get(uint8_t relay_number, web_ui_virtual_relay_state_t *state_out)
{
    programs_init_defaults();

    if (relay_number == 0 || relay_number > WEB_UI_VIRTUAL_RELAY_MAX || !state_out) {
        return false;
    }

    bool hw_status = false;
    esp_err_t io_err = tasks_digital_io_get_output_via_agent(relay_number,
                                                              &hw_status,
                                                              pdMS_TO_TICKS(250));
    if (io_err == ESP_OK) {
        s_virtual_relays[relay_number].status = hw_status;
    }

    *state_out = s_virtual_relays[relay_number];
    return true;
}

/**
 * @brief Serializza lo stato di tutti i relè virtuali in JSON
 *
 * L'array risultante contiene oggetti con campi relay_number, status e
 * duration. L'allocazione deve essere liberata con free().
 *
 * @return stringa JSON oppure NULL
 */
char *web_ui_virtual_relays_to_json(void)
{
    programs_init_defaults();

    cJSON *root = cJSON_CreateObject();
    cJSON *relays = cJSON_CreateArray();
    if (!root || !relays) {
        cJSON_Delete(root);
        cJSON_Delete(relays);
        return NULL;
    }

    for (uint8_t relay = 1; relay <= WEB_UI_VIRTUAL_RELAY_MAX; ++relay) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            continue;
        }
        cJSON_AddNumberToObject(item, "relay_number", relay);
        cJSON_AddBoolToObject(item, "status", s_virtual_relays[relay].status);
        cJSON_AddNumberToObject(item, "duration", s_virtual_relays[relay].duration_ms);
        cJSON_AddItemToArray(relays, item);
    }

    cJSON_AddItemToObject(root, "relays", relays);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}
