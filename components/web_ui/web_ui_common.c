#include "web_ui_internal.h"
#include "device_config.h"
#include "app_version.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "cJSON.h"
#include <lwip/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "esp_heap_caps.h"

/*
 * @file web_ui_common.c
 * @brief Funzioni condivise dal sottosistema Web UI (i18n, utility, OTA)
 *
 * Questo modulo contiene helper interni utilizzati da più file del
 * componente web_ui. Molte di queste funzioni gestiscono la cache delle
 * traduzioni, conversioni di indirizzi IP, e assistenza per l'OTA.
 */

static const char *HTML_STYLE_NAV =
    "nav{background:#000;padding:10px;display:flex;justify-content:center;gap:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}"
    "nav a{color:white;text-decoration:none;padding:8px 15px;border-radius:4px;background:#2c3e50;font-weight:bold;font-size:14px;transition:.2s}"
    "nav a:hover{background:#3498db}";

static const char *TAG = "WEB_UI_COMMON";

/* PSRAM-backed compact i18n record (matches `i18n_record_t` in header) */
typedef struct
{
    uint8_t scope_id;
    uint16_t key_id;
    uint8_t section;
    char text[32];
} ps_i18n_rec_t;

/* Allocate in PSRAM when possible */
/**
 * @brief Allocate memory in PSRAM when possible
 *
 * This function attempts to allocate memory in PSRAM if it is available.
 * If PSRAM is not available, it will fall back to allocating memory in DRAM.
 *
 * @param sz The size of the memory to allocate
 * @return void* A pointer to the allocated memory, or NULL if allocation failed
 */
static void *ps_malloc(size_t sz)
{
    void *p = NULL;
#ifdef MALLOC_CAP_SPIRAM
    p = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    if (!p)
    {
        p = malloc(sz);
    }
    return p;
}

/**
 * @brief Free memory that was allocated with ps_malloc
 *
 * This function frees memory that was allocated with the ps_malloc function.
 * If the pointer is NULL, this function does nothing.
 *
 * @param p A pointer to the memory to free
 */
static void ps_free(void *p)
{
#ifdef MALLOC_CAP_SPIRAM
    heap_caps_free(p);
#else
    free(p);
#endif
}

#define I18N_CACHE_MAX_ENTRIES 8

typedef struct
{
    char lang[8];
    char scope[16];
    char *table_json;
} i18n_cache_entry_t;

static i18n_cache_entry_t s_i18n_cache[I18N_CACHE_MAX_ENTRIES];
static size_t s_i18n_cache_next_slot = 0;

/* PSRAM resident dictionary */
static i18n_record_t *s_i18n_ps_records = NULL;
static size_t s_i18n_ps_count = 0;

/**
 * @brief Carica le risorse linguistiche nella PSRAM se necessario e applica l'indicato linguaggio all'interfaccia web.
 *
 * Questa funzione controlla se la lingua specificata è già caricata; in caso contrario,
 * prova a caricare i file di localizzazione dalla PSRAM (Parallel SRAM) per migliorare le prestazioni.
 *
 * @param[in] lang  Codice lingua da utilizzare (es. "it", "en", "fr"). Deve essere una stringa null-terminated valida.
 *                  Non deve contenere caratteri speciali o spazi.
 * @return @c true se il linguaggio è stato caricato con successo o se era già presente, \n
 *         @c false in caso di errore (es. lingua non trovata, fallimento nella lettura PSRAM).
 *
 * @note Questa funzione gestisce l'allocation della memoria necessaria nel PSRAM e libera risorse inutilizzate.
 */
esp_err_t web_ui_i18n_load_language_psram(const char *language)
{
    if (s_i18n_ps_records)
    {
        i18n_free_dictionary_psram(s_i18n_ps_records);
        s_i18n_ps_records = NULL;
        s_i18n_ps_count = 0;
    }

    size_t count = 0;
    i18n_record_t *arr = i18n_load_full_dictionary_psram(language, &count);
    if (!arr || count == 0)
    {
        return ESP_FAIL;
    }
    s_i18n_ps_records = arr;
    s_i18n_ps_count = count;
    ESP_LOGI(TAG, "Loaded %zu i18n records into PSRAM for language %s", count, language);
    return ESP_OK;
}

/**
 * @brief Concatenate sections for given numeric ids from PSRAM array
 *
 * This function concatenates all sections that match the given
 * `scope_id` and `key_id` from the PSRAM-resident dictionary.
 *
 * @param scope_id The numeric scope identifier to match
 * @param key_id The numeric key identifier to match
 * @return char* A newly allocated string containing the concatenated
 *         sections, or NULL if no matching sections were found or
 *         memory allocation failed. The caller is responsible for
 *         freeing the returned string.
 */
char *i18n_concat_from_psram(uint8_t scope_id, uint16_t key_id)
{
    if (!s_i18n_ps_records || s_i18n_ps_count == 0)
    {
        return NULL;
    }

    /* First pass: calculate total length and count sections */
    size_t total_len = 0;
    size_t sections = 0;
    for (size_t i = 0; i < s_i18n_ps_count; ++i)
    {
        if (s_i18n_ps_records[i].scope_id == scope_id &&
            s_i18n_ps_records[i].key_id == key_id)
        {
            total_len += strnlen(s_i18n_ps_records[i].text,
                                 sizeof(s_i18n_ps_records[i].text));
            ++sections;
        }
    }
    if (sections == 0)
    {
        return NULL;
    }

    /* Allocate buffer for concatenated string */
    char *out = malloc(total_len + 1);
    if (!out)
    {
        return NULL;
    }

    /* Second pass: copy matching sections into output buffer */
    size_t pos = 0;
    for (size_t i = 0; i < s_i18n_ps_count; ++i)
    {
        if (s_i18n_ps_records[i].scope_id == scope_id &&
            s_i18n_ps_records[i].key_id == key_id)
        {
            size_t part_len = strnlen(s_i18n_ps_records[i].text,
                                      sizeof(s_i18n_ps_records[i].text));
            if (part_len == 0)
            {
                continue;
            }
            memcpy(out + pos, s_i18n_ps_records[i].text, part_len);
            pos += part_len;
        }
    }

    out[pos] = '\0';
    return out;
}

/**
 * @brief Duplica una stringa C in memoria heap
 *
 * Restituisce una nuova stringa allocata che contiene la copia
 * del contenuto di `src`. Se `src` è NULL o l'allocazione fallisce viene
 * restituito NULL.
 *
 * @param src stringa da duplicare
 * @return puntatore a stringa allocata (deallocare con free) o NULL
 */
static char *dup_cstr(const char *src)
{
    if (!src)
    {
        return NULL;
    }
    size_t len = strlen(src);
    char *dst = malloc(len + 1);
    if (!dst)
    {
        return NULL;
    }
    memcpy(dst, src, len + 1);
    return dst;
}

/**
 * @brief Determina lo scope i18n per un particolare URI
 *
 * L'URI viene confrontato con alcuni prefissi noti ("/config", "/logs", "/test"
 * ecc.) per stabilire quale gruppo di traduzioni utilizzare. Questo aiuta a
 * generare il file javascript delle stringhe solo per la pagina richiesta.
 *
 * @param uri URI della richiesta HTTP
 * @return nome dello scope i18n (es. "p_runtime", "p_logs")
 */
static const char *i18n_scope_for_uri(const char *uri)
{
    if (!uri || uri[0] == '\0')
    {
        return "p_runtime";
    }

    if (strncmp(uri, "/config/programs", 16) == 0)
    {
        return "p_programs";
    }
    if (strncmp(uri, "/config", 7) == 0)
    {
        return "p_config";
    }
    if (strncmp(uri, "/emulator", 9) == 0)
    {
        return "p_emulator";
    }
    if (strncmp(uri, "/logs", 5) == 0)
    {
        return "p_logs";
    }
    if (strncmp(uri, "/test", 5) == 0)
    {
        return "p_test";
    }

    return "p_runtime";
}

/**
 * @brief Verifica se una chiave di traduzione appartiene allo scope corrente
 *
 * Alcune chiavi sono globali (nav, header, lvgl) e devono essere sempre
 * incluse; altre vengono mostrate solo se coincidono con lo scope della pagina.
 *
 * @param scope chiave richiesta
 * @param page_scope scope corrente della pagina
 * @return true se la chiave deve essere inclusa
 */
static bool i18n_scope_allowed(const char *scope, const char *page_scope)
{
    if (!scope || !page_scope)
    {
        return false;
    }

    if (strcmp(scope, "nav") == 0 ||
        strcmp(scope, "header") == 0 ||
        strcmp(scope, "lvgl") == 0)
    {
        return true;
    }

    return strcmp(scope, page_scope) == 0;
}

/**
 * @brief Recupera dalla cache l'oggetto JSON delle stringhe i18n
 *
 * Se la coppia <lingua, scope> è stata precedentemente memorizzata viene
 * restituita una duplica della stringa json; altrimenti NULL.
 *
 * @param lang codice lingua ("en","it", ...)
 * @param page_scope scope della pagina
 * @return copia JSON oppure NULL se non presente
 */
static char *i18n_cache_get(const char *lang, const char *page_scope)
{
    if (!lang || !page_scope)
    {
        return NULL;
    }

    for (size_t i = 0; i < I18N_CACHE_MAX_ENTRIES; i++)
    {
        i18n_cache_entry_t *entry = &s_i18n_cache[i];
        if (!entry->table_json)
        {
            continue;
        }
        if (strcmp(entry->lang, lang) == 0 && strcmp(entry->scope, page_scope) == 0)
        {
            return dup_cstr(entry->table_json);
        }
    }

    return NULL;
}

/**
 * @brief Inserisce o aggiorna un elemento nella cache i18n
 *
 * La cache è circolare con dimensione fissa, sovrascrivendo le voci più
 * vecchie quando necessario.
 *
 * @param lang codice lingua
 * @param page_scope scope pagina
 * @param table_json stringa JSON delle traduzioni
 */
static void i18n_cache_put(const char *lang, const char *page_scope, const char *table_json)
{
    if (!lang || !page_scope || !table_json)
    {
        return;
    }

    char *copy = dup_cstr(table_json);
    if (!copy)
    {
        return;
    }

    for (size_t i = 0; i < I18N_CACHE_MAX_ENTRIES; i++)
    {
        i18n_cache_entry_t *entry = &s_i18n_cache[i];
        if (entry->table_json && strcmp(entry->lang, lang) == 0 && strcmp(entry->scope, page_scope) == 0)
        {
            free(entry->table_json);
            entry->table_json = copy;
            return;
        }
    }

    i18n_cache_entry_t *entry = &s_i18n_cache[s_i18n_cache_next_slot % I18N_CACHE_MAX_ENTRIES];
    free(entry->table_json);
    entry->table_json = copy;
    strncpy(entry->lang, lang, sizeof(entry->lang) - 1);
    entry->lang[sizeof(entry->lang) - 1] = '\0';
    strncpy(entry->scope, page_scope, sizeof(entry->scope) - 1);
    entry->scope[sizeof(entry->scope) - 1] = '\0';
    s_i18n_cache_next_slot = (s_i18n_cache_next_slot + 1) % I18N_CACHE_MAX_ENTRIES;
}

/**
 * @brief Svuota tutta la cache dei file di traduzione
 *
 * Deve essere chiamata quando le risorse i18n vengono aggiornate dall'esterno
 * (es. cambio dei file su SPIFFS) in modo che le richieste successive
 * rileggeranno i nuovi contenuti.
 */
void web_ui_i18n_cache_invalidate(void)
{
    for (size_t i = 0; i < I18N_CACHE_MAX_ENTRIES; i++)
    {
        free(s_i18n_cache[i].table_json);
        s_i18n_cache[i].table_json = NULL;
        s_i18n_cache[i].lang[0] = '\0';
        s_i18n_cache[i].scope[0] = '\0';
    }
    s_i18n_cache_next_slot = 0;
}

/*
 * Load full i18n dictionary for a language into PSRAM as an array of
 * `i18n_record_t`-compatible records. Caller must free with
 * `i18n_free_dictionary_psram()`.
 */
i18n_record_t *i18n_load_full_dictionary_psram(const char *language, size_t *out_count)
{
    if (!language)
    {
        if (out_count)
            *out_count = 0;
        return NULL;
    }

    char *json = device_config_get_ui_texts_records_json(language);
    if (!json)
    {
        if (out_count)
            *out_count = 0;
        return NULL;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root || !cJSON_IsArray(root))
    {
        if (root)
            cJSON_Delete(root);
        if (out_count)
            *out_count = 0;
        return NULL;
    }

    int total = cJSON_GetArraySize(root);
    if (total <= 0)
    {
        cJSON_Delete(root);
        if (out_count)
            *out_count = 0;
        return NULL;
    }

    ps_i18n_rec_t *arr = ps_malloc((size_t)total * sizeof(ps_i18n_rec_t));
    if (!arr)
    {
        cJSON_Delete(root);
        if (out_count)
            *out_count = 0;
        return NULL;
    }

    memset(arr, 0, (size_t)total * sizeof(ps_i18n_rec_t));

    int idx = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root)
    {
        if (!cJSON_IsObject(item))
            continue;

        uint8_t scope_id = 0;
        uint16_t key_id = 0;
        uint8_t section = 0;
        const char *text = NULL;

        cJSON *j_scope_id = cJSON_GetObjectItemCaseSensitive(item, "scope_id");
        cJSON *j_key_id = cJSON_GetObjectItemCaseSensitive(item, "key_id");
        cJSON *j_section = cJSON_GetObjectItemCaseSensitive(item, "section");
        cJSON *j_section_old = cJSON_GetObjectItemCaseSensitive(item, "section");

        if (cJSON_IsNumber(j_scope_id))
        {
            scope_id = (uint8_t)j_scope_id->valueint;
        }
        else
        {
            cJSON *j_scope = cJSON_GetObjectItemCaseSensitive(item, "scope");
            if (cJSON_IsNumber(j_scope))
                scope_id = (uint8_t)j_scope->valueint;
        }

        if (cJSON_IsNumber(j_key_id))
        {
            key_id = (uint16_t)j_key_id->valueint;
        }
        else
        {
            cJSON *j_key = cJSON_GetObjectItemCaseSensitive(item, "key");
            if (cJSON_IsNumber(j_key))
                key_id = (uint16_t)j_key->valueint;
        }

        if (cJSON_IsNumber(j_section))
        {
            section = (uint8_t)j_section->valueint;
        }
        else if (cJSON_IsNumber(j_section_old))
        {
            section = (uint8_t)j_section_old->valueint;
        }

        cJSON *j_text = cJSON_GetObjectItemCaseSensitive(item, "text");
        if (cJSON_IsString(j_text) && j_text->valuestring)
        {
            text = j_text->valuestring;
        }

        arr[idx].scope_id = scope_id;
        arr[idx].key_id = key_id;
        arr[idx].section = section;
        if (text)
        {
            strncpy(arr[idx].text, text, sizeof(arr[idx].text) - 1);
            arr[idx].text[sizeof(arr[idx].text) - 1] = '\0';
        }
        else
        {
            arr[idx].text[0] = '\0';
        }
        idx++;
        if (idx >= total)
            break;
    }

    cJSON_Delete(root);

    if (out_count)
        *out_count = (size_t)idx;
    return (i18n_record_t *)arr;
}


/**
 * @brief Libera la memoria PSRAM utilizzata per un dizionario di record internazionalizzati.
 *
 * Questa funzione libera la memoria PSRAM allocata per un dizionario di record internazionalizzati.
 * Ogni record inizia con un campo di lunghezza variabile che indica la lunghezza del record.
 *
 * @param [in/out] records Puntatore al primo record del dizionario da deallocare.
 * @return Nessun valore di ritorno.
 */
void i18n_free_dictionary_psram(i18n_record_t *records)
{
    if (!records)
        return;
    ps_free(records);
}

/**
 * @brief Aggiunge una coppia chiave/valore ad un oggetto cJSON se mancante
 *
 * Utility usata internamente durante la costruzione della tabella i18n per
 * evitare duplicati.
 */
static void add_table_mapping(cJSON *table, const char *key, const char *value)
{
    if (!table || !key || !value || key[0] == '\0')
    {
        return;
    }
    if (!cJSON_GetObjectItemCaseSensitive(table, key))
    {
        cJSON_AddStringToObject(table, key, value);
    }
}


/**
 * @brief Aggiunge una mappatura di tabella alla struttura cJSON.
 * 
 * @param table Puntatore alla struttura cJSON in cui aggiungere la mappatura.
 * @param key Chiave della mappatura da aggiungere.
 * @param value Valore della mappatura da aggiungere.
 * @return true Se la mappatura è stata aggiunta con successo.
 * @return false Se la mappatura non è stata aggiunta (table o key non validi).
 */
static bool append_table_mapping(cJSON *table, const char *key, const char *value)
{
    if (!table || !key || key[0] == '\0')
    {
        return false;
    }

    const char *chunk = value ? value : "";
    cJSON *existing = cJSON_GetObjectItemCaseSensitive(table, key);
    if (!existing)
    {
        cJSON_AddStringToObject(table, key, chunk);
        return true;
    }

    if (!cJSON_IsString(existing) || !existing->valuestring)
    {
        return false;
    }

    size_t old_len = strlen(existing->valuestring);
    size_t add_len = strlen(chunk);
    char *merged = malloc(old_len + add_len + 1);
    if (!merged)
    {
        return false;
    }

    memcpy(merged, existing->valuestring, old_len);
    memcpy(merged + old_len, chunk, add_len);
    merged[old_len + add_len] = '\0';

    cJSON *new_item = cJSON_CreateString(merged);
    free(merged);
    if (!new_item)
    {
        return false;
    }

    cJSON_ReplaceItemInObjectCaseSensitive(table, key, new_item);
    return true;
}

static cJSON *load_i18n_map_json(const char *language)
{
    char path[64] = {0};
    const char *lang = (language && strlen(language) == 2) ? language : "it";
    snprintf(path, sizeof(path), "/spiffs/i18n_%s.map.json", lang);

    FILE *f = fopen(path, "r");
    if (!f && strcmp(lang, "it") != 0)
    {
        snprintf(path, sizeof(path), "/spiffs/i18n_it.map.json");
        f = fopen(path, "r");
    }
    if (!f)
    {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0)
    {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (read != (size_t)size)
    {
        free(buf);
        return NULL;
    }
    buf[size] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    return root;
}

static const char *lookup_map_name(cJSON *map_root, const char *section_name, int id)
{
    if (!map_root || !section_name || id <= 0)
    {
        return NULL;
    }

    cJSON *section = cJSON_GetObjectItemCaseSensitive(map_root, section_name);
    if (!cJSON_IsObject(section))
    {
        return NULL;
    }

    char id_key[16] = {0};
    snprintf(id_key, sizeof(id_key), "%d", id);
    cJSON *value = cJSON_GetObjectItemCaseSensitive(section, id_key);
    if (cJSON_IsString(value) && value->valuestring)
    {
        return value->valuestring;
    }
    return NULL;
}

static char *build_i18n_table_json(const char *records_json, const char *base_records_json, const char *language)
{
    if (!records_json)
    {
        return NULL;
    }

    cJSON *records = cJSON_Parse(records_json);
    if (!records || !cJSON_IsArray(records))
    {
        if (records)
        {
            cJSON_Delete(records);
        }
        return NULL;
    }

    cJSON *table = cJSON_CreateObject();
    cJSON *target_lookup = cJSON_CreateObject();
    cJSON *target_scope_names = cJSON_CreateObject();
    cJSON *target_key_names = cJSON_CreateObject();
    cJSON *base_lookup = cJSON_CreateObject();
    if (!table || !target_lookup || !target_scope_names || !target_key_names || !base_lookup)
    {
        if (table)
        {
            cJSON_Delete(table);
        }
        if (target_lookup)
        {
            cJSON_Delete(target_lookup);
        }
        if (target_scope_names)
        {
            cJSON_Delete(target_scope_names);
        }
        if (target_key_names)
        {
            cJSON_Delete(target_key_names);
        }
        if (base_lookup)
        {
            cJSON_Delete(base_lookup);
        }
        cJSON_Delete(records);
        return NULL;
    }

    cJSON *map_root = load_i18n_map_json(language);

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, records)
    {
        if (!cJSON_IsObject(item))
        {
            continue;
        }

        cJSON *text = cJSON_GetObjectItemCaseSensitive(item, "text");
        if (!cJSON_IsString(text) || !text->valuestring)
        {
            continue;
        }

        cJSON *scope = cJSON_GetObjectItemCaseSensitive(item, "scope");
        cJSON *key = cJSON_GetObjectItemCaseSensitive(item, "key");
        if (cJSON_IsNumber(scope) && cJSON_IsNumber(key))
        {
            int scope_id = scope->valueint;
            int key_id = key->valueint;
            if (scope_id <= 0 || key_id <= 0)
            {
                continue;
            }

            char scoped_key[96] = {0};
            snprintf(scoped_key, sizeof(scoped_key), "%d.%d", scope_id, key_id);
            append_table_mapping(target_lookup, scoped_key, text->valuestring);

            const char *scope_text = lookup_map_name(map_root, "scopes", scope_id);
            const char *key_text = lookup_map_name(map_root, "keys", key_id);
            if (scope_text && !cJSON_GetObjectItemCaseSensitive(target_scope_names, scoped_key))
            {
                cJSON_AddStringToObject(target_scope_names, scoped_key, scope_text);
            }
            if (key_text && !cJSON_GetObjectItemCaseSensitive(target_key_names, scoped_key))
            {
                cJSON_AddStringToObject(target_key_names, scoped_key, key_text);
            }
            continue;
        }

        if (!cJSON_IsString(scope) || !scope->valuestring ||
            !cJSON_IsString(key) || !key->valuestring)
        {
            continue;
        }

        char scoped_key[96] = {0};
        snprintf(scoped_key, sizeof(scoped_key), "%s.%s", scope->valuestring, key->valuestring);
        append_table_mapping(target_lookup, scoped_key, text->valuestring);
        if (!cJSON_GetObjectItemCaseSensitive(target_scope_names, scoped_key))
        {
            cJSON_AddStringToObject(target_scope_names, scoped_key, scope->valuestring);
        }
        if (!cJSON_GetObjectItemCaseSensitive(target_key_names, scoped_key))
        {
            cJSON_AddStringToObject(target_key_names, scoped_key, key->valuestring);
        }
    }

    if (base_records_json && base_records_json[0] != '\0')
    {
        cJSON *base_records = cJSON_Parse(base_records_json);
        if (base_records && cJSON_IsArray(base_records))
        {
            cJSON *b_item = NULL;
            cJSON_ArrayForEach(b_item, base_records)
            {
                if (!cJSON_IsObject(b_item))
                {
                    continue;
                }

                cJSON *b_text = cJSON_GetObjectItemCaseSensitive(b_item, "text");
                if (!cJSON_IsString(b_text) || !b_text->valuestring)
                {
                    continue;
                }

                cJSON *b_scope = cJSON_GetObjectItemCaseSensitive(b_item, "scope");
                cJSON *b_key = cJSON_GetObjectItemCaseSensitive(b_item, "key");
                if (cJSON_IsNumber(b_scope) && cJSON_IsNumber(b_key))
                {
                    int scope_id = b_scope->valueint;
                    int key_id = b_key->valueint;
                    if (scope_id <= 0 || key_id <= 0)
                    {
                        continue;
                    }
                    char b_scoped_key[96] = {0};
                    snprintf(b_scoped_key, sizeof(b_scoped_key), "%d.%d", scope_id, key_id);
                    append_table_mapping(base_lookup, b_scoped_key, b_text->valuestring);
                    continue;
                }

                if (!cJSON_IsString(b_scope) || !b_scope->valuestring ||
                    !cJSON_IsString(b_key) || !b_key->valuestring)
                {
                    continue;
                }

                char b_scoped_key[96] = {0};
                snprintf(b_scoped_key, sizeof(b_scoped_key), "%s.%s", b_scope->valuestring, b_key->valuestring);
                append_table_mapping(base_lookup, b_scoped_key, b_text->valuestring);
            }
        }
        if (base_records)
        {
            cJSON_Delete(base_records);
        }
    }

    cJSON *target_item = NULL;
    cJSON_ArrayForEach(target_item, target_lookup)
    {
        if (!cJSON_IsString(target_item) || !target_item->string || !target_item->valuestring)
        {
            continue;
        }

        const char *scoped_key = target_item->string;
        const char *target_text = target_item->valuestring;
        add_table_mapping(table, scoped_key, target_text);

        const char *dot = strchr(scoped_key, '.');
        if (dot && dot[1] != '\0')
        {
            add_table_mapping(table, dot + 1, target_text);
        }

        cJSON *scope_name = cJSON_GetObjectItemCaseSensitive(target_scope_names, scoped_key);
        cJSON *key_name = cJSON_GetObjectItemCaseSensitive(target_key_names, scoped_key);
        if (cJSON_IsString(scope_name) && scope_name->valuestring &&
            cJSON_IsString(key_name) && key_name->valuestring)
        {
            char textual_scoped_key[128] = {0};
            snprintf(textual_scoped_key,
                     sizeof(textual_scoped_key),
                     "%s.%s",
                     scope_name->valuestring,
                     key_name->valuestring);
            add_table_mapping(table, textual_scoped_key, target_text);
            add_table_mapping(table, key_name->valuestring, target_text);
        }

        cJSON *base_text = cJSON_GetObjectItemCaseSensitive(base_lookup, scoped_key);
        if (cJSON_IsString(base_text) && base_text->valuestring && strcmp(base_text->valuestring, target_text) != 0)
        {
            add_table_mapping(table, base_text->valuestring, target_text);

            const char *start = base_text->valuestring;
            while (*start && isspace((unsigned char)*start))
            {
                start++;
            }
            const char *end = base_text->valuestring + strlen(base_text->valuestring);
            while (end > start && isspace((unsigned char)end[-1]))
            {
                end--;
            }
            size_t trim_len = (size_t)(end - start);
            if (trim_len > 0 &&
                (start != base_text->valuestring || end != base_text->valuestring + strlen(base_text->valuestring)))
            {
                char *trimmed = malloc(trim_len + 1);
                if (trimmed)
                {
                    memcpy(trimmed, start, trim_len);
                    trimmed[trim_len] = '\0';
                    add_table_mapping(table, trimmed, target_text);
                    free(trimmed);
                }
            }
        }
    }

    char *out = cJSON_PrintUnformatted(table);
    cJSON_Delete(base_lookup);
    cJSON_Delete(target_key_names);
    cJSON_Delete(target_scope_names);
    cJSON_Delete(target_lookup);
    if (map_root)
    {
        cJSON_Delete(map_root);
    }
    cJSON_Delete(table);
    cJSON_Delete(records);
    return out;
}

static char *filter_i18n_records_json_for_scope(const char *records_json, const char *page_scope, const char *language)
{
    if (!records_json || !page_scope)
    {
        return NULL;
    }

    cJSON *root = cJSON_Parse(records_json);
    if (!root || !cJSON_IsArray(root))
    {
        if (root)
        {
            cJSON_Delete(root);
        }
        return NULL;
    }

    cJSON *filtered = cJSON_CreateArray();
    if (!filtered)
    {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *map_root = load_i18n_map_json(language);

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root)
    {
        if (!cJSON_IsObject(item))
        {
            continue;
        }

        const char *scope_name = NULL;
        cJSON *scope = cJSON_GetObjectItemCaseSensitive(item, "scope");
        if (cJSON_IsString(scope) && scope->valuestring)
        {
            scope_name = scope->valuestring;
        }
        else if (cJSON_IsNumber(scope))
        {
            scope_name = lookup_map_name(map_root, "scopes", scope->valueint);
        }

        if (scope_name && !i18n_scope_allowed(scope_name, page_scope))
        {
            continue;
        }

        cJSON *copy = cJSON_Duplicate(item, 1);
        if (!copy)
        {
            cJSON_Delete(filtered);
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToArray(filtered, copy);
    }

    char *out = cJSON_PrintUnformatted(filtered);
    if (map_root)
    {
        cJSON_Delete(map_root);
    }
    cJSON_Delete(filtered);
    cJSON_Delete(root);
    return out;
}

static char *escape_script_end_tag(const char *src)
{
    if (!src)
    {
        return NULL;
    }

    const char *needle = "</script";
    const char *replacement = "<\\/script";
    const size_t needle_len = strlen(needle);
    const size_t repl_len = strlen(replacement);

    size_t count = 0;
    const char *p = src;
    while ((p = strstr(p, needle)) != NULL)
    {
        count++;
        p += needle_len;
    }

    if (count == 0)
    {
        char *copy = malloc(strlen(src) + 1);
        if (!copy)
        {
            return NULL;
        }
        strcpy(copy, src);
        return copy;
    }

    size_t src_len = strlen(src);
    size_t out_len = src_len + (count * (repl_len - needle_len));
    char *out = malloc(out_len + 1);
    if (!out)
    {
        return NULL;
    }

    const char *in = src;
    char *dst = out;
    while ((p = strstr(in, needle)) != NULL)
    {
        size_t chunk = (size_t)(p - in);
        memcpy(dst, in, chunk);
        dst += chunk;
        memcpy(dst, replacement, repl_len);
        dst += repl_len;
        in = p + needle_len;
    }

    strcpy(dst, in);
    return out;
}


/**
 * @brief Invia uno script di runtime internazionale tramite HTTP.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t send_i18n_runtime_script(httpd_req_t *req)
{
    const char *lang = device_config_get_ui_backend_language();
    const char *page_scope = i18n_scope_for_uri(req ? req->uri : NULL);

    char *table_json = i18n_cache_get(lang, page_scope);
    if (!table_json)
    {
        char *texts_json = device_config_get_ui_texts_records_json(lang);
        if (!texts_json)
        {
            return ESP_ERR_NO_MEM;
        }

        char *filtered_texts_json = filter_i18n_records_json_for_scope(texts_json, page_scope, lang);
        free(texts_json);
        if (!filtered_texts_json)
        {
            return ESP_ERR_NO_MEM;
        }

        char *base_filtered_json = NULL;
        if (strcmp(lang, "it") != 0)
        {
            char *base_json = device_config_get_ui_texts_records_json("it");
            if (base_json)
            {
                base_filtered_json = filter_i18n_records_json_for_scope(base_json, page_scope, "it");
                free(base_json);
            }
        }

        table_json = build_i18n_table_json(filtered_texts_json, base_filtered_json, lang);
        free(filtered_texts_json);
        if (base_filtered_json)
        {
            free(base_filtered_json);
        }
        if (!table_json)
        {
            return ESP_ERR_NO_MEM;
        }

        i18n_cache_put(lang, page_scope, table_json);
    }

    char *safe_texts_json = escape_script_end_tag(table_json);
    free(table_json);
    if (!safe_texts_json)
    {
        return ESP_ERR_NO_MEM;
    }

    const char *script_fmt =
        "<script>(function(){"
        "if(window.__ui_i18n_ready)return;"
        "window.__ui_i18n_ready=true;"
        "const lang='%s';"
        "const table=%s||{};"
        "const SKIP={SCRIPT:1,STYLE:1,NOSCRIPT:1};"
        "function mapText(t){if(!t)return t;return (table[t]!==undefined&&table[t]!==null)?String(table[t]):t;}"
        "function applyNode(node){if(!node)return;"
        "if(node.nodeType===Node.TEXT_NODE){const p=node.parentElement;if(p&&SKIP[p.tagName])return;const v=node.nodeValue;if(!v)return;const tt=v.trim();if(!tt)return;"
        "const tr=mapText(tt);if(tr!==tt){node.nodeValue=v.replace(tt,tr);}return;}"
        "if(node.nodeType!==Node.ELEMENT_NODE)return;"
        "if(SKIP[node.tagName])return;"
        "if(node.hasAttribute&&node.hasAttribute('data-i18n')){const k=node.getAttribute('data-i18n');if(k){const tr=mapText(k);if(tr&&tr!==k)node.textContent=tr;}}"
        "const attrs=['placeholder','title','aria-label','value'];"
        "for(const a of attrs){if(node.hasAttribute&&node.hasAttribute(a)){const ov=node.getAttribute(a);const nv=mapText(ov);if(nv!==ov)node.setAttribute(a,nv);}}"
        "for(const c of node.childNodes){applyNode(c);}"
        "}"
        "function apply(root){if(!root)return;applyNode(root);}"
        "window.uiI18n={language:lang,table:table,apply:apply,translate:mapText};"
        "document.addEventListener('DOMContentLoaded',function(){apply(document.body);"
        "const obs=new MutationObserver(function(ms){for(const m of ms){for(const n of m.addedNodes){applyNode(n);}}});"
        "obs.observe(document.body,{subtree:true,childList:true});"
        "});"
        "})();</script>";

    int needed = snprintf(NULL, 0, script_fmt, lang, safe_texts_json);
    if (needed < 0)
    {
        free(safe_texts_json);
        return ESP_FAIL;
    }

    char *script = malloc((size_t)needed + 1);
    if (!script)
    {
        free(safe_texts_json);
        return ESP_ERR_NO_MEM;
    }

    snprintf(script, (size_t)needed + 1, script_fmt, lang, safe_texts_json);
    free(safe_texts_json);

    esp_err_t ret = httpd_resp_sendstr_chunk(req, script);
    free(script);
    return ret;
}

// Nota: questa funzione è usata da diverse pagine; non è più `static` perché
// sarà condivisa tra i file del componente web_ui dopo lo split.

/**
 * @brief Invia la testata HTTP per una richiesta.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @param title Titolo della pagina.
 * @param extra_style Stili extra da applicare alla pagina.
 * @param show_nav Indica se mostrare la navigazione.
 * @return esp_err_t Codice di errore.
 */
esp_err_t send_head(httpd_req_t *req, const char *title, const char *extra_style, bool show_nav)
{
    const char *safe_title = title ? title : "";
    const char *safe_extra_style = extra_style ? extra_style : "";
    const char *req_uri = req ? req->uri : "";

    // Get current time
    time_t now = time(NULL);
    struct tm timeinfo;
    char time_not_set[32] = {0};
    device_config_get_ui_text_scoped("header", "time_not_set", "Time not set", time_not_set, sizeof(time_not_set));
    char time_str[20] = {0};
    strncpy(time_str, time_not_set, sizeof(time_str) - 1);
    if (now != (time_t)-1)
    {
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
    }

    const bool is_emulator_page =
        (strncmp(req_uri, "/emulator", 9) == 0 &&
         (req_uri[9] == '\0' || req_uri[9] == '?'));
    char txt_home[32] = {0};
    char txt_emulator[32] = {0};
    device_config_get_ui_text_scoped("nav", "home", "Home", txt_home, sizeof(txt_home));
    device_config_get_ui_text_scoped("nav", "emulator", "Emulatore", txt_emulator, sizeof(txt_emulator));

    const char *emu_button_fmt_home =
        "<a href='/' style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>%s</a>";
    const char *emu_button_fmt_emu =
        "<a href='#' onclick=\"return window.goProtectedPath('/emulator');\" style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>%s</a>";
    const char *emu_fmt = is_emulator_page ? emu_button_fmt_home : emu_button_fmt_emu;
    int emu_needed = snprintf(NULL, 0, emu_fmt, is_emulator_page ? txt_home : txt_emulator);
    if (emu_needed < 0)
    {
        return ESP_FAIL;
    }
    char *emu_button = malloc((size_t)emu_needed + 1);
    if (!emu_button)
    {
        return ESP_ERR_NO_MEM;
    }
    if (is_emulator_page)
    {
        snprintf(emu_button, (size_t)emu_needed + 1,
                 "<a href='/' style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>%s</a>",
                 txt_home);
    }
    else
    {
        snprintf(emu_button, (size_t)emu_needed + 1,
                 "<a href='#' onclick=\"return window.goProtectedPath('/emulator');\" style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>%s</a>",
                 txt_emulator);
    }

    const bool show_tasks = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TASKS);
    const bool show_test = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TEST);
    const bool show_programs = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS);
    char txt_nav_home[24] = {0};
    char txt_nav_config[24] = {0};
    char txt_nav_files[24] = {0};
    char txt_nav_tasks[24] = {0};
    char txt_nav_logs[24] = {0};
    char txt_nav_test[24] = {0};
    char txt_nav_programs[24] = {0};
    char txt_nav_ota[24] = {0};
    device_config_get_ui_text_scoped("nav", "home", "Home", txt_nav_home, sizeof(txt_nav_home));
    device_config_get_ui_text_scoped("nav", "config", "Config", txt_nav_config, sizeof(txt_nav_config));
    device_config_get_ui_text_scoped("nav", "files", "File", txt_nav_files, sizeof(txt_nav_files));
    device_config_get_ui_text_scoped("nav", "tasks", "Task", txt_nav_tasks, sizeof(txt_nav_tasks));
    device_config_get_ui_text_scoped("nav", "logs", "Log", txt_nav_logs, sizeof(txt_nav_logs));
    device_config_get_ui_text_scoped("nav", "test", "Test", txt_nav_test, sizeof(txt_nav_test));
    device_config_get_ui_text_scoped("nav", "programs", "Programmi", txt_nav_programs, sizeof(txt_nav_programs));
    device_config_get_ui_text_scoped("nav", "ota", "OTA", txt_nav_ota, sizeof(txt_nav_ota));

    char nav_tasks[96] = {0};
    char nav_test[96] = {0};
    char nav_programs[96] = {0};
    if (show_tasks)
    {
        snprintf(nav_tasks, sizeof(nav_tasks), "<a href='/tasks'>📋 %s</a>", txt_nav_tasks);
    }
    if (show_test)
    {
        snprintf(nav_test, sizeof(nav_test), "<a href='/test'>🔧 %s</a>", txt_nav_test);
    }
    if (show_programs)
    {
        snprintf(nav_programs, sizeof(nav_programs), "<a href='/config/programs'>📊 %s</a>", txt_nav_programs);
    }
    int nav_needed = snprintf(NULL, 0,
                              "<nav><a href='/'>🏠 %s</a><a href='/config'>⚙️ %s</a>%s<a href='/files'>📁 %s</a>%s<a href='/logs'>📋 %s</a>%s<a href='/ota'>🔄 %s</a></nav>",
                              txt_nav_home,
                              txt_nav_config,
                              nav_test,
                              txt_nav_files,
                              nav_tasks,
                              txt_nav_logs,
                              nav_programs,
                              txt_nav_ota);
    if (nav_needed < 0)
    {
        free(emu_button);
        return ESP_FAIL;
    }
    char *nav_html = malloc((size_t)nav_needed + 1);
    if (!nav_html)
    {
        free(emu_button);
        return ESP_ERR_NO_MEM;
    }
    snprintf(nav_html, (size_t)nav_needed + 1,
             "<nav><a href='/'>🏠 %s</a><a href='/config'>⚙️ %s</a>%s<a href='/files'>📁 %s</a>%s<a href='/logs'>📋 %s</a>%s<a href='/ota'>🔄 %s</a></nav>",
             txt_nav_home,
             txt_nav_config,
             nav_test,
             txt_nav_files,
             nav_tasks,
             txt_nav_logs,
             nav_programs,
             txt_nav_ota);

    int needed = snprintf(
        NULL,
        0,
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>%s</title><style>"
        "body{font-family:Arial;background:#f5f5f5;color:#333;margin:0}header{background:#000;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".container{max-width:1000px;margin:20px auto;padding:0 20px}"
        "%s %s"
        "</style></head><body>"
        "<header>"
        "<div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>%s [%s] - <span id='hdr_clock'>%s</span></h1>%s</div>"
        "<div style='text-align:right;font-size:12px;opacity:0.8;'>v%s (%s)</div>"
        "</header>"
        "%s"
        "<script>/* Global fetch wrapper: injects Authorization */"
        "(function(){if(window.__auth_wrapped) return; window.__auth_wrapped=true; const _fetch = window.fetch.bind(window);"
        "window.setAuthToken = function(t){ if(t) localStorage.setItem('httpservices_token', t); else localStorage.removeItem('httpservices_token'); };"
        "window.getAuthToken = function(){ return localStorage.getItem('httpservices_token'); };"
        "window.clearAuthToken = function(){ localStorage.removeItem('httpservices_token'); };"
        "window.fetch = function(input, init){ try{ const token = window.getAuthToken(); if(token){ init = init || {}; if(!init.headers){ init.headers = {'Authorization':'Bearer '+token}; } else if(init.headers instanceof Headers){ if(!init.headers.get('Authorization')) init.headers.set('Authorization','Bearer '+token); } else if(Array.isArray(init.headers)){ let has=false; for(const h of init.headers){ if(h[0].toLowerCase()==='authorization'){ has=true; break; } } if(!has) init.headers.push(['Authorization','Bearer '+token]); } else if(typeof init.headers==='object'){ if(!init.headers['Authorization'] && !init.headers['authorization']) init.headers['Authorization'] = 'Bearer '+token; } } }catch(e){} return _fetch(input, init); };"
        "window.goProtectedPath=function(path){window.location.href=path;return false;};"
        "(function(){function tc(){var e=document.getElementById('hdr_clock');if(e)e.textContent=new Date().toTimeString().slice(0,8);}tc();setInterval(tc,1000);})();"
        "})();</script>",
        safe_title, show_nav ? HTML_STYLE_NAV : "", safe_extra_style, safe_title, device_config_get_running_app_name(), time_str, emu_button, APP_VERSION, APP_DATE, show_nav ? nav_html : "");
    if (needed < 0)
    {
        free(nav_html);
        free(emu_button);
        return ESP_FAIL;
    }

    char *buf = malloc((size_t)needed + 1);
    if (!buf)
    {
        free(nav_html);
        free(emu_button);
        return ESP_ERR_NO_MEM;
    }

    snprintf(buf, (size_t)needed + 1,
             "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>%s</title><style>"
             "body{font-family:Arial;background:#f5f5f5;color:#333;margin:0}header{background:#000;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
             ".container{max-width:1000px;margin:20px auto;padding:0 20px}"
             "%s %s"
             "</style></head><body>"
             "<header>"
             "<div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>%s [%s] - <span id='hdr_clock'>%s</span></h1>%s</div>"
             "<div style='text-align:right;font-size:12px;opacity:0.8;'>v%s (%s)</div>"
             "</header>"
             "%s"
             "<script>/* Global fetch wrapper: injects Authorization */"
             "(function(){if(window.__auth_wrapped) return; window.__auth_wrapped=true; const _fetch = window.fetch.bind(window);"
             "window.setAuthToken = function(t){ if(t) localStorage.setItem('httpservices_token', t); else localStorage.removeItem('httpservices_token'); };"
             "window.getAuthToken = function(){ return localStorage.getItem('httpservices_token'); };"
             "window.clearAuthToken = function(){ localStorage.removeItem('httpservices_token'); };"
             "window.fetch = function(input, init){ try{ const token = window.getAuthToken(); if(token){ init = init || {}; if(!init.headers){ init.headers = {'Authorization':'Bearer '+token}; } else if(init.headers instanceof Headers){ if(!init.headers.get('Authorization')) init.headers.set('Authorization','Bearer '+token); } else if(Array.isArray(init.headers)){ let has=false; for(const h of init.headers){ if(h[0].toLowerCase()==='authorization'){ has=true; break; } } if(!has) init.headers.push(['Authorization','Bearer '+token]); } else if(typeof init.headers==='object'){ if(!init.headers['Authorization'] && !init.headers['authorization']) init.headers['Authorization'] = 'Bearer '+token; } } }catch(e){} return _fetch(input, init); };"
             "window.goProtectedPath=function(path){window.location.href=path;return false;};"
             "(function(){function tc(){var e=document.getElementById('hdr_clock');if(e)e.textContent=new Date().toTimeString().slice(0,8);}tc();setInterval(tc,1000);})();"
             "})();</script>",
             safe_title, show_nav ? HTML_STYLE_NAV : "", safe_extra_style, safe_title, device_config_get_running_app_name(), time_str, emu_button, APP_VERSION, APP_DATE, show_nav ? nav_html : "");
    esp_err_t send_ret = httpd_resp_sendstr_chunk(req, buf);
    free(buf);
    free(nav_html);
    free(emu_button);

    if (send_ret != ESP_OK)
    {
        if (send_ret != ESP_ERR_HTTPD_RESP_SEND)
        {
            ESP_LOGW(TAG, "send_head: errore invio header: %s", esp_err_to_name(send_ret));
        }
        return ESP_OK;
    }

    esp_err_t i18n_ret = send_i18n_runtime_script(req);
    if (i18n_ret != ESP_OK && i18n_ret != ESP_ERR_HTTPD_RESP_SEND)
    {
        ESP_LOGW(TAG, "send_head: errore script i18n: %s", esp_err_to_name(i18n_ret));
    }
    return ESP_OK;
}

// Sposto qui la response per /logo.jpg (era in web_ui.c). Rendendola visibile
// tramite header interno possiamo registrarla dagli altri file.

/**
 * @brief Ottiene il gestore per la richiesta HTTP.
 *
 * @param [in] req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t logo_get_handler(httpd_req_t *req)
{
    // Carica logo da filesystem (se presente) oppure restituisce 204
    FILE *f = fopen("/spiffs/logo.jpg", "rb");
    if (!f)
    {
        httpd_resp_set_status(req, "204 No Content");
        return httpd_resp_send(req, NULL, 0);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz);
    if (!buf)
    {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(buf, 1, sz, f);
    fclose(f);
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, buf, sz);
    free(buf);
    return ESP_OK;
}

/* Helper condivisi (spostati qui per rendere disponibili le funzioni alle pagine divise)
   - ip_to_str(): converte esp_netif IP in stringa (usata da /status)
   - perform_ota(): avvia OTA HTTPS con timeout e riavvio */
void ip_to_str(esp_netif_t *netif, char *out, size_t len)
{
    if (!netif || !out)
    {
        if (out && len > 0)
            out[0] = '\0';
        return;
    }
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(netif, &info) == ESP_OK)
    {
        ip4addr_ntoa_r((const ip4_addr_t *)&info.ip, out, len);
    }
    else
    {
        if (len > 0)
            out[0] = '\0';
    }
}


/**
 * @brief Esegue l'operazione di aggiornamento del firmware tramite URL fornito.
 *
 * @param [in] url URL del firmware da aggiornare.
 * @return esp_err_t Codice di errore che indica il risultato dell'operazione.
 */
esp_err_t perform_ota(const char *url)
{
    if (!url || strlen(url) == 0)
        return ESP_ERR_INVALID_ARG;
    ESP_LOGI("WEB_UI", "Avvio OTA da %s", url);
    esp_http_client_config_t client_cfg = {.url = url, .timeout_ms = 15000, .cert_pem = NULL, .skip_cert_common_name_check = true};
    esp_https_ota_config_t ota_cfg = {.http_config = &client_cfg};
    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK)
    {
        ESP_LOGI("WEB_UI", "OTA riuscito. Riavvio in corso...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    return ret;
}


/**
 * @brief Controlla se il runtime della factory è attivo.
 * 
 * Questa funzione verifica se le funzionalità della factory sono state sovrascritte.
 * 
 * @return true se il runtime della factory è attivo, false altrimenti.
 */
static bool is_factory_runtime(void)
{
    if (web_ui_factory_features_override_get())
    {
        return true;
    }
    const esp_partition_t *running = esp_ota_get_running_partition();
    return (running && running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY);
}

static bool s_factory_features_override = false;


/** @brief Imposta lo stato di override delle funzionalità dell'interfaccia web UI.
 * 
 * @param [in] enabled Se true, abilita l'override delle funzionalità dell'interfaccia web UI; altrimenti, lo disabilita.
 * @return Niente.
 */
void web_ui_factory_features_override_set(bool enabled)
{
    s_factory_features_override = enabled;
}


/**
 * @brief Ottiene le funzionalità di override della factory per l'interfaccia web UI.
 *
 * @param [out] features Puntatore alla struttura che conterrà le funzionalità di override.
 * @return true se le funzionalità di override sono state ottenute con successo, false altrimenti.
 */
bool web_ui_factory_features_override_get(void)
{
    return s_factory_features_override;
}

const char *web_ui_profile_view_label(void)
{
    return is_factory_runtime() ? "Factory View" : "App View";
}


/**
 * @brief Controlla se una funzionalità dell'interfaccia utente web è abilitata.
 *
 * @param feature La funzionalità dell'interfaccia utente web da controllare.
 * @return true se la funzionalità è abilitata, false altrimenti.
 */
bool web_ui_feature_enabled(web_ui_feature_t feature)
{
    bool is_factory = is_factory_runtime();
    return web_ui_scope_allows(web_ui_feature_scope(feature), is_factory);
}
