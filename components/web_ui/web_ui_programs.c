#include "web_ui_programs.h"

#include "cJSON.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "WEB_UI_PROGRAMS"
#define PROGRAMS_STORAGE_PATH "/spiffs/programs.json"

static web_ui_program_table_t s_program_table;
static web_ui_virtual_relay_state_t s_virtual_relays[WEB_UI_VIRTUAL_RELAY_MAX + 1];
static bool s_initialized = false;

static esp_err_t programs_save_to_storage(void)
{
    char *json = web_ui_program_table_to_json();
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

static void programs_init_defaults(void)
{
    if (s_initialized) {
        return;
    }

    memset(&s_program_table, 0, sizeof(s_program_table));
    s_program_table.count = 8;
    for (uint8_t index = 0; index < s_program_table.count; ++index) {
        web_ui_program_entry_t *entry = &s_program_table.programs[index];
        entry->program_id = (uint8_t)(index + 1);
        snprintf(entry->name, sizeof(entry->name), "Programma %u", (unsigned)entry->program_id);
        entry->enabled = true;
        entry->price_units = 10;
        entry->duration_sec = 60;
        entry->relay_mask = (uint16_t)(1U << index);
    }

    memset(s_virtual_relays, 0, sizeof(s_virtual_relays));
    s_initialized = true;

    programs_try_load_from_storage();
}

const web_ui_program_table_t *web_ui_program_table_get(void)
{
    programs_init_defaults();
    return &s_program_table;
}

char *web_ui_program_table_to_json(void)
{
    programs_init_defaults();

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
        cJSON_AddStringToObject(item, "name", entry->name);
        cJSON_AddBoolToObject(item, "enabled", entry->enabled);
        cJSON_AddNumberToObject(item, "price_units", entry->price_units);
        cJSON_AddNumberToObject(item, "duration_sec", entry->duration_sec);
        cJSON_AddNumberToObject(item, "relay_mask", entry->relay_mask);
        cJSON_AddItemToArray(programs, item);
    }

    cJSON_AddItemToObject(root, "programs", programs);
    cJSON_AddNumberToObject(root, "count", s_program_table.count);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

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
        cJSON *name = cJSON_GetObjectItem(item, "name");
        cJSON *enabled = cJSON_GetObjectItem(item, "enabled");
        cJSON *price_units = cJSON_GetObjectItem(item, "price_units");
        cJSON *duration_sec = cJSON_GetObjectItem(item, "duration_sec");
        cJSON *relay_mask = cJSON_GetObjectItem(item, "relay_mask");

        if (!cJSON_IsNumber(program_id) || !cJSON_IsString(name) || !cJSON_IsBool(enabled) ||
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
        int mask = relay_mask->valueint;

        if (pid <= 0 || pid > 255 || price < 0 || price > 65535 || duration < 0 || duration > 65535 || mask < 0 || mask > 0xFFFF) {
            cJSON_Delete(root);
            if (err_msg && err_msg_len > 0) {
                snprintf(err_msg, err_msg_len, "valori fuori range entry %d", index);
            }
            return ESP_ERR_INVALID_ARG;
        }

        entry->program_id = (uint8_t)pid;
        snprintf(entry->name, sizeof(entry->name), "%s", name->valuestring);
        entry->enabled = cJSON_IsTrue(enabled);
        entry->price_units = (uint16_t)price;
        entry->duration_sec = (uint16_t)duration;
        entry->relay_mask = (uint16_t)mask;
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

esp_err_t web_ui_virtual_relay_control(uint8_t relay_number, bool status, uint32_t duration_ms)
{
    programs_init_defaults();

    if (relay_number == 0 || relay_number > WEB_UI_VIRTUAL_RELAY_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    s_virtual_relays[relay_number].status = status;
    s_virtual_relays[relay_number].duration_ms = duration_ms;

    ESP_LOGI(TAG, "[STUB] relay %u => status=%s duration_ms=%lu",
             (unsigned)relay_number,
             status ? "ON" : "OFF",
             (unsigned long)duration_ms);

    return ESP_OK;
}

bool web_ui_virtual_relay_get(uint8_t relay_number, web_ui_virtual_relay_state_t *state_out)
{
    programs_init_defaults();

    if (relay_number == 0 || relay_number > WEB_UI_VIRTUAL_RELAY_MAX || !state_out) {
        return false;
    }

    *state_out = s_virtual_relays[relay_number];
    return true;
}

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
