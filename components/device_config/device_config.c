#include "device_config.h"
#include "i18n_map_defs.h"
#include <stdint.h>

/* Minimal forward declaration to avoid including web_ui_internal.h here.
 * The real implementation lives in web_ui_common.c; we only call the
 * concat helper which returns a heap-allocated string or NULL.
 */
char *i18n_concat_from_psram(uint8_t scope_id, uint16_t key_id);
#include "esp_log.h"
#include "esp_check.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_rom_crc.h"
#include "eeprom_24lc16.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>

static const char *TAG = "DEVICE_CFG";
static const char *NVS_NAMESPACE = "device_config";
static const char *UI_LANG_DEFAULT = "it";
static const char *I18N_V2_FILE_PATH = "/spiffs/i18n_v2.json";
#define DEVICE_CFG_MODBUS_MAX_POINTS 64
#define DEVICE_CFG_SERIAL_BAUD_MIN 300
#define DEVICE_CFG_SERIAL_BAUD_MAX 3000000
#define DEVICE_CFG_SERIAL_DATA_BITS_MIN 5
#define DEVICE_CFG_SERIAL_DATA_BITS_MAX 8
#define DEVICE_CFG_SERIAL_PARITY_MIN 0
#define DEVICE_CFG_SERIAL_PARITY_MAX 2
#define DEVICE_CFG_SERIAL_STOP_BITS_MIN 1
#define DEVICE_CFG_SERIAL_STOP_BITS_MAX 2
#define DEVICE_CFG_SERIAL_RX_BUF_MIN 64
#define DEVICE_CFG_SERIAL_BUF_MAX 65536

static const device_serial_config_t s_serial_default_rs232 = {
    .baud_rate = 9600,
    .data_bits = 8,
    .parity = 0,
    .stop_bits = 1,
    .rx_buf_size = 2048,
    .tx_buf_size = 0,
};

static const device_serial_config_t s_serial_default_rs485 = {
    .baud_rate = 9600,
    .data_bits = 8,
    .parity = 0,
    .stop_bits = 1,
    .rx_buf_size = 2048,
    .tx_buf_size = 0,
};

static const device_serial_config_t s_serial_default_mdb = {
    .baud_rate = 9600,
    .data_bits = 8,
    .parity = 0,
    .stop_bits = 1,
    .rx_buf_size = 1024,
    .tx_buf_size = 1024,
};


/**
 * @brief Legge un intero da un oggetto JSON con chiave primaria e legacy.
 *
 * @param [in] obj Oggetto JSON sorgente.
 * @param [in] primary_key Chiave principale.
 * @param [in] legacy_key Chiave legacy opzionale (può essere NULL).
 * @param [out] out_value Valore letto.
 * @return true Se il campo numerico è stato trovato.
 * @return false Se il campo non è presente o non è numerico.
 */
static bool _json_read_int(cJSON *obj, const char *primary_key, const char *legacy_key, int *out_value)
{
    if (!obj || !primary_key || !out_value) {
        return false;
    }

    cJSON *item = cJSON_GetObjectItem(obj, primary_key);
    if ((!item || !cJSON_IsNumber(item)) && legacy_key) {
        item = cJSON_GetObjectItem(obj, legacy_key);
    }

    if (!item || !cJSON_IsNumber(item)) {
        return false;
    }

    *out_value = item->valueint;
    return true;
}


/**
 * @brief Restituisce il valore se nel range, altrimenti il default.
 *
 * @param [in] value Valore da validare.
 * @param [in] min_value Limite minimo incluso.
 * @param [in] max_value Limite massimo incluso.
 * @param [in] default_value Default di fallback.
 * @return int Valore validato o fallback.
 */
static int _clamp_int_or_default(int value, int min_value, int max_value, int default_value)
{
    if (value < min_value || value > max_value) {
        return default_value;
    }
    return value;
}


/**
 * @brief Esegue parsing sicuro della configurazione seriale da JSON.
 *
 * @param [in] serial_obj Oggetto JSON seriale.
 * @param [in/out] target Struttura target da aggiornare.
 * @param [in] defaults Valori di fallback per campi invalidi.
 */
static void _parse_serial_cfg_json(cJSON *serial_obj,
                                   device_serial_config_t *target,
                                   const device_serial_config_t *defaults)
{
    if (!serial_obj || !target || !defaults) {
        return;
    }

    int value = 0;

    if (_json_read_int(serial_obj, "baud", "baud_rate", &value)) {
        target->baud_rate = _clamp_int_or_default(value,
                                                  DEVICE_CFG_SERIAL_BAUD_MIN,
                                                  DEVICE_CFG_SERIAL_BAUD_MAX,
                                                  defaults->baud_rate);
    }
    if (_json_read_int(serial_obj, "data", "data_bits", &value)) {
        target->data_bits = _clamp_int_or_default(value,
                                                  DEVICE_CFG_SERIAL_DATA_BITS_MIN,
                                                  DEVICE_CFG_SERIAL_DATA_BITS_MAX,
                                                  defaults->data_bits);
    }
    if (_json_read_int(serial_obj, "par", "parity", &value)) {
        target->parity = _clamp_int_or_default(value,
                                               DEVICE_CFG_SERIAL_PARITY_MIN,
                                               DEVICE_CFG_SERIAL_PARITY_MAX,
                                               defaults->parity);
    }
    if (_json_read_int(serial_obj, "stop", "stop_bits", &value)) {
        target->stop_bits = _clamp_int_or_default(value,
                                                  DEVICE_CFG_SERIAL_STOP_BITS_MIN,
                                                  DEVICE_CFG_SERIAL_STOP_BITS_MAX,
                                                  defaults->stop_bits);
    }
    if (_json_read_int(serial_obj, "rx_buf", "rx_buf_size", &value)) {
        target->rx_buf_size = _clamp_int_or_default(value,
                                                    DEVICE_CFG_SERIAL_RX_BUF_MIN,
                                                    DEVICE_CFG_SERIAL_BUF_MAX,
                                                    defaults->rx_buf_size);
    }
    if (_json_read_int(serial_obj, "tx_buf", "tx_buf_size", &value)) {
        target->tx_buf_size = _clamp_int_or_default(value,
                                                    0,
                                                    DEVICE_CFG_SERIAL_BUF_MAX,
                                                    defaults->tx_buf_size);
    }
}

#define EEPROM_MAGIC 0x57415645 // "WAVE"
#define EEPROM_HEADER_ADDR 0

typedef struct {
    uint32_t magic;
    uint32_t crc;
    uint32_t length;
    uint16_t modified;
    uint16_t version;
} eeprom_header_t;

#define EEPROM_STORAGE_SIZE_BYTES 2048U
#define EEPROM_JSON_MAX_LENGTH (EEPROM_STORAGE_SIZE_BYTES - sizeof(eeprom_header_t))

static device_config_t s_config = {0};
static bool s_initialized = false;

static const char *_effective_lang(const char *language);
static cJSON *s_i18n_v2_root = NULL;
static cJSON *_i18n_v2_build_records_for_language(const char *language);
static const char *_i18n_v2_entry_pick_text(cJSON *entry, const char *lang);
static void _i18n_v2_append_entry(cJSON *array, cJSON *entry, const char *lang);
static void _i18n_v2_clear_cache(void);

static cJSON *s_i18n_lookup_cache = NULL;
static char s_i18n_lookup_lang[8] = {0};



/**
 * @brief Cancella la cache di ricerca internazionale.
 */
static void _i18n_lookup_cache_clear(void)
{
    if (s_i18n_lookup_cache) {
        cJSON_Delete(s_i18n_lookup_cache);
        s_i18n_lookup_cache = NULL;
    }
    s_i18n_lookup_lang[0] = '\0';
}



/**
 * @brief Aggiunge un testo JSON all'oggetto cJSON.
 * 
 * @param object Puntatore all'oggetto cJSON a cui aggiungere il testo.
 * @param key Chiave del campo JSON da aggiungere.
 * @param text Testo da aggiungere.
 * @return true Se l'aggiunta è stata effettuata con successo.
 * @return false Se l'aggiunta non è stata effettuata (oggetto o chiave non validi).
 */
static bool _append_json_text(cJSON *object, const char *key, const char *text)
{
    if (!object || !key || key[0] == '\0') {
        return false;
    }

    const char *chunk = text ? text : "";
    cJSON *existing = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!existing) {
        cJSON_AddStringToObject(object, key, chunk);
        return true;
    }

    if (!cJSON_IsString(existing) || !existing->valuestring) {
        return false;
    }

    size_t old_len = strlen(existing->valuestring);
    size_t add_len = strlen(chunk);
    char *merged = malloc(old_len + add_len + 1);
    if (!merged) {
        return false;
    }

    memcpy(merged, existing->valuestring, old_len);
    memcpy(merged + old_len, chunk, add_len);
    merged[old_len + add_len] = '\0';

    cJSON *new_item = cJSON_CreateString(merged);
    free(merged);
    if (!new_item) {
        return false;
    }

    cJSON_ReplaceItemInObjectCaseSensitive(object, key, new_item);
    return true;
}


/**
 * @brief Aggiunge una mappatura di ricerca se non esiste già.
 * 
 * @param [in] table Puntatore alla tabella JSON in cui aggiungere la mappatura.
 * @param [in] key Chiave della mappatura da aggiungere.
 * @param [in] value Valore della mappatura da aggiungere.
 * @return void Nessun valore di ritorno.
 */
static void _add_lookup_mapping_if_missing(cJSON *table, const char *key, const char *value)
{
    if (!table || !key || !value || key[0] == '\0') {
        return;
    }
    if (!cJSON_GetObjectItemCaseSensitive(table, key)) {
        cJSON_AddStringToObject(table, key, value);
    }
}


/**
 * @brief Costruisce il cache per la traduzione per un determinato linguaggio.
 *
 * @param [in] language Il codice del linguaggio per cui costruire il cache.
 * @return esp_err_t Errore se la costruzione del cache fallisce, altrimenti ESP_OK.
 */
static esp_err_t _i18n_lookup_cache_build_for_lang(const char *language)
{
    const char *lang = _effective_lang(language);
    if (s_i18n_lookup_cache && strcmp(s_i18n_lookup_lang, lang) == 0) {
        return ESP_OK;
    }

    _i18n_lookup_cache_clear();

    const char *lang_or_default = (lang && lang[0]) ? lang : UI_LANG_DEFAULT;
    cJSON *records = _i18n_v2_build_records_for_language(lang_or_default);
    if (!records && strcmp(lang_or_default, UI_LANG_DEFAULT) != 0) {
        records = _i18n_v2_build_records_for_language(UI_LANG_DEFAULT);
    }
    if (!records) {
        return ESP_FAIL;
    }

    cJSON *table = cJSON_CreateObject();
    cJSON *aggregated = cJSON_CreateObject();
    cJSON *aggregated_scope = cJSON_CreateObject();
    cJSON *aggregated_key = cJSON_CreateObject();
    if (!table || !aggregated || !aggregated_scope || !aggregated_key) {
        if (table) {
            cJSON_Delete(table);
        }
        if (aggregated) {
            cJSON_Delete(aggregated);
        }
        if (aggregated_scope) {
            cJSON_Delete(aggregated_scope);
        }
        if (aggregated_key) {
            cJSON_Delete(aggregated_key);
        }
        cJSON_Delete(records);
        return ESP_ERR_NO_MEM;
    }


    cJSON *item = NULL;
    cJSON_ArrayForEach(item, records) {
        if (!cJSON_IsObject(item)) {
            continue;
        }

        cJSON *text = cJSON_GetObjectItemCaseSensitive(item, "text");
        if (!cJSON_IsString(text) || !text->valuestring) {
            continue;
        }

        cJSON *scope = cJSON_GetObjectItemCaseSensitive(item, "scope");
        cJSON *key = cJSON_GetObjectItemCaseSensitive(item, "key");
        if (cJSON_IsNumber(scope) && cJSON_IsNumber(key)) {
            int scope_id = scope->valueint;
            int key_id = key->valueint;
            if (scope_id <= 0 || key_id <= 0) {
                continue;
            }

            char scoped_key[96] = {0};
            snprintf(scoped_key, sizeof(scoped_key), "%d.%d", scope_id, key_id);
            _append_json_text(aggregated, scoped_key, text->valuestring);

            const char *scope_text = i18n_scope_name(scope_id);
            const char *key_text   = i18n_key_name(key_id);
            if (scope_text && !cJSON_GetObjectItemCaseSensitive(aggregated_scope, scoped_key)) {
                cJSON_AddStringToObject(aggregated_scope, scoped_key, scope_text);
            }
            if (key_text && !cJSON_GetObjectItemCaseSensitive(aggregated_key, scoped_key)) {
                cJSON_AddStringToObject(aggregated_key, scoped_key, key_text);
            }
            continue;
        }

        if (!cJSON_IsString(scope) || !scope->valuestring ||
            !cJSON_IsString(key) || !key->valuestring) {
            continue;
        }

        char scoped_key[96] = {0};
        snprintf(scoped_key, sizeof(scoped_key), "%s.%s", scope->valuestring, key->valuestring);

        _append_json_text(aggregated, scoped_key, text->valuestring);
    }

    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, aggregated) {
        if (!cJSON_IsString(entry) || !entry->string || !entry->valuestring) {
            continue;
        }

        const char *scoped_key = entry->string;
        const char *value = entry->valuestring;
        _add_lookup_mapping_if_missing(table, scoped_key, value);

        const char *dot = strchr(scoped_key, '.');
        if (dot && dot[1] != '\0') {
            _add_lookup_mapping_if_missing(table, dot + 1, value);
        }
    }

    cJSON_Delete(records);
    cJSON_Delete(aggregated);

    s_i18n_lookup_cache = table;
    strncpy(s_i18n_lookup_lang, lang_or_default, sizeof(s_i18n_lookup_lang) - 1);
    s_i18n_lookup_lang[sizeof(s_i18n_lookup_lang) - 1] = '\0';
    return ESP_OK;
}


/**
 * @brief Controlla se la lingua specificata è un ISO 639-1 codice di due lettere.
 *
 * @param [in] language Puntatore alla stringa che rappresenta la lingua.
 * @return true se la lingua è un ISO 639-1 codice di due lettere, false altrimenti.
 */
static bool _is_iso2_lang(const char *language)
{
    return language && strlen(language) == 2;
}

static const char *_effective_lang(const char *language)
{
    if (_is_iso2_lang(language)) {
        return language;
    }
    if (_is_iso2_lang(s_config.ui.user_language)) {
        return s_config.ui.user_language;
    }
    if (_is_iso2_lang(s_config.ui.backend_language)) {
        return s_config.ui.backend_language;
    }
    return UI_LANG_DEFAULT;
}


/**
 * @brief Costruisce il percorso del file di traduzione per un determinato linguaggio.
 *
 * @param [in] language Il codice del linguaggio per cui si desidera costruire il percorso.
 * @param [out] out Buffer in cui verrà memorizzato il percorso del file di traduzione.
 * @param [in] out_len Dimensione del buffer di output.
 *
 * @return void
 */
static char *_read_text_file(const char *path, size_t *out_len)
{
    if (out_len) {
        *out_len = 0;
    }
    if (!path) {
        return NULL;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long sz = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)sz + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)sz, file);
    fclose(file);
    if (read != (size_t)sz) {
        free(buffer);
        return NULL;
    }

    buffer[sz] = '\0';
    if (out_len) {
        *out_len = (size_t)sz;
    }
    return buffer;
}

static cJSON *_i18n_v2_root_get(void)
{
    if (s_i18n_v2_root) {
        return s_i18n_v2_root;
    }

    size_t sz = 0;
    char *json = _read_text_file(I18N_V2_FILE_PATH, &sz);
    if (!json || sz == 0) {
        if (json) {
            free(json);
        }
        ESP_LOGE(TAG, "[I18N_V2] Impossibile leggere %s", I18N_V2_FILE_PATH);
        return NULL;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root || !cJSON_IsObject(root)) {
        ESP_LOGE(TAG, "[I18N_V2] JSON invalido in %s", I18N_V2_FILE_PATH);
        if (root) {
            cJSON_Delete(root);
        }
        return NULL;
    }

    s_i18n_v2_root = root;
    return s_i18n_v2_root;
}

static void _i18n_v2_clear_cache(void)
{
    if (s_i18n_v2_root) {
        cJSON_Delete(s_i18n_v2_root);
        s_i18n_v2_root = NULL;
    }
}

static bool _legacy_id_parse(const char *legacy_id, int *out_scope, int *out_key)
{
    if (!legacy_id || !out_scope || !out_key) {
        return false;
    }

    int scope = 0;
    int key = 0;
    if (sscanf(legacy_id, "%d.%d", &scope, &key) != 2) {
        return false;
    }
    if (scope <= 0 || key <= 0) {
        return false;
    }

    *out_scope = scope;
    *out_key = key;
    return true;
}

static cJSON *_build_record_object(int scope_id, int key_id, const char *text)
{
    if (scope_id <= 0 || key_id <= 0 || !text) {
        return NULL;
    }

    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        return NULL;
    }

    cJSON_AddNumberToObject(obj, "scope", scope_id);
    cJSON_AddNumberToObject(obj, "key", key_id);
    cJSON_AddNumberToObject(obj, "section", 0);
    cJSON_AddStringToObject(obj, "text", text);
    return obj;
}

static void _records_append_or_free(cJSON *array, cJSON *obj)
{
    if (!array || !obj) {
        if (obj) {
            cJSON_Delete(obj);
        }
        return;
    }

    if (!cJSON_IsArray(array)) {
        cJSON_Delete(obj);
        return;
    }

    cJSON_AddItemToArray(array, obj);
}

static const char *_i18n_v2_entry_pick_text(cJSON *entry, const char *lang)
{
    if (!entry || !cJSON_IsObject(entry)) {
        return NULL;
    }

    cJSON *legacy = cJSON_GetObjectItemCaseSensitive(entry, "legacyId");
    if (!cJSON_IsString(legacy) || !legacy->valuestring) {
        return NULL;
    }

    cJSON *text_obj = cJSON_GetObjectItemCaseSensitive(entry, "text");
    if (!cJSON_IsObject(text_obj)) {
        return NULL;
    }

    cJSON *lang_item = cJSON_GetObjectItemCaseSensitive(text_obj, lang);
    if (cJSON_IsString(lang_item) && lang_item->valuestring && lang_item->valuestring[0]) {
        return lang_item->valuestring;
    }

    cJSON *fallback_it = cJSON_GetObjectItemCaseSensitive(text_obj, UI_LANG_DEFAULT);
    if (cJSON_IsString(fallback_it) && fallback_it->valuestring) {
        return fallback_it->valuestring;
    }
    return NULL;
}

static void _i18n_v2_append_entry(cJSON *array, cJSON *entry, const char *lang)
{
    if (!array) {
        return;
    }
    if (!entry || !cJSON_IsObject(entry)) {
        return;
    }

    cJSON *legacy = cJSON_GetObjectItemCaseSensitive(entry, "legacyId");
    if (!cJSON_IsString(legacy) || !legacy->valuestring) {
        return;
    }

    int scope_id = 0;
    int key_id = 0;
    if (!_legacy_id_parse(legacy->valuestring, &scope_id, &key_id)) {
        return;
    }

    const char *text = _i18n_v2_entry_pick_text(entry, lang);
    if (!text) {
        return;
    }

    cJSON *record = _build_record_object(scope_id, key_id, text);
    _records_append_or_free(array, record);
}

static cJSON *_i18n_v2_build_records_for_language(const char *language)
{
    const char *lang = _effective_lang(language);
    cJSON *root = _i18n_v2_root_get();
    if (!root || !lang) {
        return NULL;
    }

    cJSON *web = cJSON_GetObjectItemCaseSensitive(root, "web");
    cJSON *lvgl = cJSON_GetObjectItemCaseSensitive(root, "lvgl");
    if (!cJSON_IsObject(web) && !cJSON_IsObject(lvgl)) {
        return NULL;
    }

    cJSON *array = cJSON_CreateArray();
    if (!array) {
        return NULL;
    }

    if (cJSON_IsObject(web)) {
        cJSON *page = NULL;
        cJSON_ArrayForEach(page, web) {
            if (!cJSON_IsObject(page)) {
                continue;
            }
            cJSON *entry = NULL;
            cJSON_ArrayForEach(entry, page) {
                _i18n_v2_append_entry(array, entry, lang);
            }
        }
    }

    if (cJSON_IsObject(lvgl)) {
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, lvgl) {
            _i18n_v2_append_entry(array, entry, lang);
        }
    }

    return array;
}

// Configurazione predefinita

/**
 * @brief Imposta i valori di default per la configurazione del dispositivo.
 *
 * @param [in/out] config Puntatore alla struttura di configurazione del dispositivo.
 * @return Nessun valore di ritorno.
 */
static void _set_defaults(device_config_t *config)
{
    memset(config, 0, sizeof(device_config_t));
    config->updated = false;

    // Nome dispositivo
    strncpy(config->device_name, "TestWave-Device", sizeof(config->device_name) - 1);
    strncpy(config->location_name, "", sizeof(config->location_name) - 1);
    config->image_source = IMAGE_SOURCE_SPIFFS;

    // Numero pulsanti programma e coordinate geografiche
    config->num_programs = 10;   /* default: 10 programmi (2 colonne x 5 righe) */
    config->latitude     = 0.0;
    config->longitude    = 0.0;

    // Default Ethernet
    config->eth.enabled = true;
    config->eth.dhcp_enabled = true;
    strncpy(config->eth.ip, "192.168.1.100", sizeof(config->eth.ip) - 1);
    strncpy(config->eth.subnet, "255.255.255.0", sizeof(config->eth.subnet) - 1);
    strncpy(config->eth.gateway, "192.168.1.1", sizeof(config->eth.gateway) - 1);
    strncpy(config->eth.dns1, "8.8.8.8", sizeof(config->eth.dns1) - 1);
    strncpy(config->eth.dns2, "8.8.4.4", sizeof(config->eth.dns2) - 1);

    // Default WiFi
    config->wifi.sta_enabled = false;
    config->wifi.dhcp_enabled = true;
    strncpy(config->wifi.ssid, "", sizeof(config->wifi.ssid) - 1);
    strncpy(config->wifi.password, "", sizeof(config->wifi.password) - 1);
    strncpy(config->wifi.ip, "192.168.1.101", sizeof(config->wifi.ip) - 1);
    strncpy(config->wifi.subnet, "255.255.255.0", sizeof(config->wifi.subnet) - 1);
    strncpy(config->wifi.gateway, "192.168.1.1", sizeof(config->wifi.gateway) - 1);

    // NTP enabled by default
    config->ntp_enabled = true;

    // Default NTP
    strncpy(config->ntp.server1, "time.google.com", sizeof(config->ntp.server1) - 1);
    strncpy(config->ntp.server2, "pool.ntp.org", sizeof(config->ntp.server2) - 1);
    config->ntp.timezone_offset = 1;  // Default to CET (Central European Time)

    // Default Server/Cloud settings
    config->server.enabled = false; // not enabled by default
    strncpy(config->server.url, "http://195.231.69.227:5556/", sizeof(config->server.url) - 1);
    /* Default API credentials used by http_services when calling the remote server */
    strncpy(config->server.serial, "AD-34-DFG-333", sizeof(config->server.serial) - 1);
    strncpy(config->server.password, "c1ef6429c5e0f753ff24a114de6ee7d4", sizeof(config->server.password) - 1);

    // Default Remote Logging (broadcast disabled by default)
    config->remote_log.server_port = 9514;  // Default port for broadcast
    config->remote_log.use_broadcast = false; // No broadcast by default
    config->remote_log.write_to_sd = false; // Salvataggio su SD disabilitato di default

    // Default Sensori (tutti abilitati per impostazione predefinita)
    config->sensors.io_expander_enabled = true;
    config->sensors.temperature_enabled = true;
    config->sensors.led_enabled = true;
    config->sensors.led_count = CONFIG_APP_WS2812_LEDS;
    config->sensors.rs232_enabled = true;
    config->sensors.rs485_enabled = true;
    config->sensors.mdb_enabled = true;
    config->sensors.cctalk_enabled = true; /* CCtalk enabled by default */
    config->sensors.eeprom_enabled = true;
    config->sensors.pwm1_enabled = true;
    config->sensors.pwm2_enabled = true;
    config->sensors.sd_card_enabled = true;

    // Default MDB
    config->mdb.coin_acceptor_en = true;
    config->mdb.bill_validator_en = false;
    config->mdb.cashless_en = false;

    // Default Display
    config->display.enabled = true; // display abilitato di default
    config->display.lcd_brightness = 80;

    // Default RS232
    config->rs232.baud_rate = 9600;
    config->rs232.data_bits = 8;
    config->rs232.parity = 0;
    config->rs232.stop_bits = 1;
    config->rs232.rx_buf_size = 2048;
    config->rs232.tx_buf_size = 0;

    // Default RS485
    config->rs485.baud_rate = 9600;
    config->rs485.data_bits = 8;
    config->rs485.parity = 0;
    config->rs485.stop_bits = 1;
    config->rs485.rx_buf_size = 2048;
    config->rs485.tx_buf_size = 0;

    // Default Modbus RTU su RS485
    config->modbus.enabled = false;
    config->modbus.slave_id = 1;
    config->modbus.poll_ms = 100;
    config->modbus.timeout_ms = 200;
    config->modbus.retries = 2;
    config->modbus.relay_start = 0;
    config->modbus.relay_count = 8;
    config->modbus.input_start = 0;
    config->modbus.input_count = 8;

    // Default CCtalk
    config->cctalk.address = 2;

    // Default MDB Serial
    config->mdb_serial.baud_rate = 9600;
    config->mdb_serial.data_bits = 8;
    config->mdb_serial.parity = 0;
    config->mdb_serial.stop_bits = 1;
    config->mdb_serial.rx_buf_size = 1024;
    config->mdb_serial.tx_buf_size = 1024;

    // Default GPIOs Configurable
    config->gpios.gpio33.mode = GPIO_CFG_MODE_INPUT_PULLUP;
    config->gpios.gpio33.initial_state = false;

    // Default Scanner config: use sdkconfig defaults if available
    config->scanner.enabled = true;
    config->scanner.vid = (uint16_t)CONFIG_USB_CDC_SCANNER_VID;
    config->scanner.pid = (uint16_t)CONFIG_USB_CDC_SCANNER_PID;
    config->scanner.dual_pid = (uint16_t)CONFIG_USB_CDC_SCANNER_DUAL_PID;
    config->scanner.cooldown_ms = 10000;

    // Default timeout applicativi
    config->timeouts.exit_programs_ms = 60000;
    config->timeouts.exit_language_ms = 60000;
    config->timeouts.idle_before_ads_ms = 60000;    // Default 60s prima di mostrare ads
    config->timeouts.ad_rotation_ms = 30000;         // Default 30s per rotazione slide (tempo cambio slide)
    config->timeouts.credit_reset_timeout_ms = 300000; // Default 5min per reset crediti
    config->display.ads_enabled = true;

    // Default testi UI / lingua
    strncpy(config->ui.user_language, UI_LANG_DEFAULT, sizeof(config->ui.user_language) - 1);
    config->ui.texts_json[0] = '\0';
}


/**
 * @brief Calcola il CRC32 di una stringa JSON.
 *
 * Questa funzione calcola il valore CRC32 di una stringa JSON fornita in input.
 *
 * @param [in] json_str Puntatore alla stringa JSON di cui calcolare il CRC32.
 * @return Il valore CRC32 calcolato.
 */
static uint32_t _calculate_crc(const char *json_str)
{
    if (!json_str) return 0;
    return esp_rom_crc32_le(0, (const uint8_t *)json_str, strlen(json_str));
}

// -----------------------------------------------------------------------------
// Helper EEProm
// -----------------------------------------------------------------------------


/**
 * @brief Scrive una stringa JSON in EEPROM.
 *
 * @param [in] json_str Puntatore alla stringa JSON da scrivere in EEPROM.
 * @param [in] modified Flag che indica se i dati sono stati modificati.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t _write_to_eeprom(const char *json_str, bool modified)
{
    if (!json_str) return ESP_ERR_INVALID_ARG;
    if (!eeprom_24lc16_is_available()) return ESP_ERR_INVALID_STATE;

    size_t json_len = strlen(json_str);
    if (json_len == 0 || json_len > EEPROM_JSON_MAX_LENGTH) {
        ESP_LOGE(TAG,
                 "[C] JSON troppo grande per EEPROM (len=%lu, max=%lu)",
                 (unsigned long)json_len,
                 (unsigned long)EEPROM_JSON_MAX_LENGTH);
        return ESP_ERR_INVALID_SIZE;
    }

    eeprom_header_t header;
    header.magic = EEPROM_MAGIC;
    header.length = (uint32_t)json_len;
    header.crc = _calculate_crc(json_str);
    header.modified = modified ? 1 : 0;
    header.version = 1;

    // Scrivi header
    esp_err_t wr_err = eeprom_24lc16_write(EEPROM_HEADER_ADDR, (uint8_t *)&header, sizeof(header));
    if (wr_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "[C] Scrittura header EEPROM fallita (len=%lu, err=%s)",
                 (unsigned long)header.length,
                 esp_err_to_name(wr_err));
        return wr_err;
    }

    // Scrivi dati subito dopo l'header
    wr_err = eeprom_24lc16_write(EEPROM_HEADER_ADDR + sizeof(header), (const uint8_t *)json_str, header.length);
    if (wr_err != ESP_OK) {
        ESP_LOGE(TAG,
                 "[C] Scrittura JSON EEPROM fallita (len=%lu, err=%s)",
                 (unsigned long)header.length,
                 esp_err_to_name(wr_err));
        return wr_err;
    }

    ESP_LOGI(TAG, "[C] Config salvata in EEPROM (len=%lu, crc=0x%08lX, modified=%d)", 
             (unsigned long)header.length, (unsigned long)header.crc, header.modified);
    return ESP_OK;
}


/** Legge i dati dalla EEPROM.
 * @param [out] out_modified Indica se i dati sono stati modificati.
 * @return Puntatore alla stringa letta dalla EEPROM, NULL in caso di errore.
 */
static char* _read_from_eeprom(bool *out_modified)
{
    if (!eeprom_24lc16_is_available()) return NULL;

    eeprom_header_t header;
    if (eeprom_24lc16_read(EEPROM_HEADER_ADDR, (uint8_t *)&header, sizeof(header)) != ESP_OK) return NULL;

    if (header.magic != EEPROM_MAGIC) {
        ESP_LOGW(TAG, "[C] Magic EEPROM non valido (0x%08lX)", (unsigned long)header.magic);
        return NULL;
    }

    if (header.length == 0 || header.length > EEPROM_JSON_MAX_LENGTH) {
        ESP_LOGW(TAG, "[C] Lunghezza JSON EEPROM non valida (%lu)", (unsigned long)header.length);
        return NULL;
    }

    char *json_str = malloc(header.length + 1);
    if (!json_str) return NULL;

    if (eeprom_24lc16_read(EEPROM_HEADER_ADDR + sizeof(header), (uint8_t *)json_str, header.length) != ESP_OK) {
        free(json_str);
        return NULL;
    }
    json_str[header.length] = '\0';
    ESP_LOGD(TAG, "[C] EEPROM JSON caricato: %s", json_str);

    uint32_t c_crc = _calculate_crc(json_str);
    if (c_crc != header.crc) {
        ESP_LOGE(TAG, "[C] Errore CRC EEPROM: calc=0x%08lX, saved=0x%08lX", (unsigned long)c_crc, (unsigned long)header.crc);
        free(json_str);
        return NULL;
    }

    if (out_modified) *out_modified = (header.modified == 1);
    ESP_LOGD(TAG, "[C] Config caricata da EEPROM (modified=%d)", header.modified);
    return json_str;
}

// -----------------------------------------------------------------------------
// Helper NVS
// -----------------------------------------------------------------------------


/**
 * @brief Scrive una stringa JSON in NVS (Non-Volatile Storage).
 *
 * @param json_str [in] La stringa JSON da scrivere in NVS.
 * @return esp_err_t Errore restituito dal sistema.
 */
static esp_err_t _write_to_nvs(const char *json_str)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "Apertura NVS fallita");
    
    esp_err_t ret = nvs_set_str(handle, "config_json", json_str);
    if (ret == ESP_OK) {
        // Salviamo il CRC in un JSON separato per evitare la dipendenza circolare (Specifica User)
        uint32_t crc_val = _calculate_crc(json_str);
        char meta_str[64];
        snprintf(meta_str, sizeof(meta_str), "{\"crc\":%lu,\"len\":%u}", (unsigned long)crc_val, (unsigned int)strlen(json_str));
        nvs_set_str(handle, "config_meta", meta_str);
        
        nvs_commit(handle);
        ESP_LOGI(TAG, "[C] Backup NVS completato con meta: %s", meta_str);
    }
    
    nvs_close(handle);
    return ret;
}


/**
 * @brief Legge i dati da NVS (Non-Volatile Storage).
 *
 * @param [in] Non ci sono parametri di input per questa funzione.
 *
 * @return char* Un puntatore alla stringa contenente i dati letti, o NULL in caso di errore.
 */
static char* _read_from_nvs(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return NULL;
    
    size_t size = 0;
    char *json_str = NULL;
    char meta_str[64];
    size_t meta_size = sizeof(meta_str);
    
    bool valid = false;
    
    // Leggiamo il meta per verificare il CRC
    if (nvs_get_str(handle, "config_meta", meta_str, &meta_size) == ESP_OK) {
        ESP_LOGI(TAG, "[C] NVS Meta caricato: %s", meta_str);
        cJSON *meta_root = cJSON_Parse(meta_str);
        if (meta_root) {
            uint32_t expected_crc = (uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItem(meta_root, "crc"));
            
            if (nvs_get_str(handle, "config_json", NULL, &size) == ESP_OK && size > 1) {
                json_str = malloc(size);
                if (json_str) {
                    if (nvs_get_str(handle, "config_json", json_str, &size) == ESP_OK) {
                        ESP_LOGI(TAG, "[C] NVS JSON caricato: %s", json_str);
                        uint32_t actual_crc = _calculate_crc(json_str);
                        if (actual_crc == expected_crc) {
                            valid = true;
                        } else {
                            ESP_LOGE(TAG, "[C] Errore CRC backup NVS! (Exp: %lx, Got: %lx)", (unsigned long)expected_crc, (unsigned long)actual_crc);
                        }
                    }
                }
            }
            cJSON_Delete(meta_root);
        }
    } else {
        ESP_LOGW(TAG, "[C] Nessun dato Meta trovato in NVS");
    }
    
    nvs_close(handle);
    
    if (!valid && json_str) {
        free(json_str);
        return NULL;
    }
    
    return json_str;
}


/**
 * @brief Inizializza la configurazione del dispositivo.
 *
 * Questa funzione inizializza la configurazione del dispositivo, preparandola per l'uso successivo.
 *
 * @return esp_err_t
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione fallita.
 */
esp_err_t device_config_init(void)
{
    ESP_LOGI(TAG, "[C] Inizializzazione sistema configurazione");
    _set_defaults(&s_config);
    s_initialized = true;
    esp_err_t err = device_config_load(&s_config);
    if (err != ESP_OK) {
        return err;
    }

    if (_i18n_lookup_cache_build_for_lang(s_config.ui.user_language) == ESP_OK) {
        ESP_LOGI(TAG, "[I18N] Cache lookup lingua user '%s' precaricata", _effective_lang(s_config.ui.user_language));
    }

    return ESP_OK;
}


/**
 * @brief Carica la configurazione del dispositivo.
 *
 * Questa funzione carica la configurazione del dispositivo da una fonte specifica
 * e la memorizza nella struttura device_config_t fornita.
 *
 * @param [in/out] config Puntatore alla struttura device_config_t dove verrà
 *                        memorizzata la configurazione del dispositivo.
 *
 * @return
 * - ESP_OK: La configurazione è stata caricata con successo.
 * - ESP_FAIL: Si è verificato un errore durante il caricamento della configurazione.
 */
esp_err_t device_config_load(device_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;

    _set_defaults(config);

    bool eeprom_modified = false;
    char *json_str = _read_from_eeprom(&eeprom_modified);
    bool source_is_eeprom = false;
    bool source_is_nvs = false;

    if (json_str) {
        // EEPROM valida
        source_is_eeprom = true;
        ESP_LOGI(TAG, "[C] Configurazione valida in EEPROM");
        
        if (eeprom_modified) {
            ESP_LOGI(TAG, "[C] Flag 'modified' attivo: sincronizzo EEPROM -> NVS");
            if (_write_to_nvs(json_str) == ESP_OK) {
                // Riscrivi in EEPROM per azzerare il flag di modifica
                _write_to_eeprom(json_str, false);
            }
        }
    } else {
        // EEPROM non valida, prova NVS
        ESP_LOGI(TAG, "[C] EEPROM non valida, provo NVS...");
        json_str = _read_from_nvs();
        if (json_str) {
            ESP_LOGI(TAG, "[C] Configurazione valida in NVS");
            source_is_nvs = true;
        } else {
            // Niente né in EEPROM né in NVS: Defaults
            ESP_LOGI(TAG, "[C] Nessun config trovato: inizializzo Defaults su NVS e EEPROM");
            char *def_json = device_config_to_json(config);
            if (def_json) {
                _write_to_nvs(def_json);
                _write_to_eeprom(def_json, false);
                free(def_json);
            }
            return ESP_OK; // Caricati i defaults in s_config via _set_defaults
        }
    }

    if (json_str) {
        bool parse_ok = false;
        ESP_LOGI(TAG, "[CONFIG] Loading JSON config (%zu bytes): %s", strlen(json_str), json_str);
        cJSON *root = cJSON_Parse(json_str);
        if (root) {
            parse_ok = true;
            // Nome dispositivo
            cJSON *name = cJSON_GetObjectItem(root, "device_name");
            if (name && name->valuestring) strncpy(config->device_name, name->valuestring, sizeof(config->device_name) - 1);
            cJSON *loc = cJSON_GetObjectItem(root, "location_name");
            if (loc && loc->valuestring) strncpy(config->location_name, loc->valuestring, sizeof(config->location_name) - 1);
            cJSON *img_src = cJSON_GetObjectItem(root, "image_source");
            if (img_src && img_src->valuestring) {
                config->image_source = (strcmp(img_src->valuestring, "sdcard") == 0)
                    ? IMAGE_SOURCE_SDCARD : IMAGE_SOURCE_SPIFFS;
            }

                // Numero pulsanti programma (valori ammessi: 1,2,3,4,5,6,8,10)
                cJSON *num_prog_j = cJSON_GetObjectItem(root, "n_prg");
                if (!num_prog_j) num_prog_j = cJSON_GetObjectItem(root, "num_programs"); /* compat */
                if (num_prog_j && cJSON_IsNumber(num_prog_j)) {
                    static const uint8_t valid_np[] = {1, 2, 3, 4, 5, 6, 8, 10};
                    uint8_t np = (uint8_t)num_prog_j->valueint;
                    bool ok = false;
                    for (int _i = 0; _i < (int)(sizeof(valid_np)/sizeof(valid_np[0])); _i++) {
                        if (valid_np[_i] == np) { ok = true; break; }
                    }
                    config->num_programs = ok ? np : 10;
                }

                // Coordinate geografiche impianto
                cJSON *lat_j = cJSON_GetObjectItem(root, "lat");
                if (!lat_j) lat_j = cJSON_GetObjectItem(root, "latitude"); /* compat */
                if (lat_j && cJSON_IsNumber(lat_j)) config->latitude  = lat_j->valuedouble;
                cJSON *lon_j = cJSON_GetObjectItem(root, "lon");
                if (!lon_j) lon_j = cJSON_GetObjectItem(root, "longitude"); /* compat */
                if (lon_j && cJSON_IsNumber(lon_j)) config->longitude = lon_j->valuedouble;

                // Analisi config Ethernet
                cJSON *eth_obj = cJSON_GetObjectItem(root, "eth");
                if (eth_obj) {
                    cJSON *_eth_en = cJSON_GetObjectItem(eth_obj, "en"); if (!_eth_en) _eth_en = cJSON_GetObjectItem(eth_obj, "enabled");
                    config->eth.enabled = cJSON_IsTrue(_eth_en);
                    cJSON *_eth_dhcp = cJSON_GetObjectItem(eth_obj, "dhcp"); if (!_eth_dhcp) _eth_dhcp = cJSON_GetObjectItem(eth_obj, "dhcp_enabled");
                    config->eth.dhcp_enabled = cJSON_IsTrue(_eth_dhcp);
                    cJSON *ip = cJSON_GetObjectItem(eth_obj, "ip");
                    if (ip && ip->valuestring) strncpy(config->eth.ip, ip->valuestring, sizeof(config->eth.ip) - 1);
                    cJSON *subnet = cJSON_GetObjectItem(eth_obj, "sub"); if (!subnet) subnet = cJSON_GetObjectItem(eth_obj, "subnet");
                    if (subnet && subnet->valuestring) strncpy(config->eth.subnet, subnet->valuestring, sizeof(config->eth.subnet) - 1);
                    cJSON *gateway = cJSON_GetObjectItem(eth_obj, "gw"); if (!gateway) gateway = cJSON_GetObjectItem(eth_obj, "gateway");
                    if (gateway && gateway->valuestring) strncpy(config->eth.gateway, gateway->valuestring, sizeof(config->eth.gateway) - 1);
                    cJSON *dns1 = cJSON_GetObjectItem(eth_obj, "dns1");
                    if (dns1 && dns1->valuestring) strncpy(config->eth.dns1, dns1->valuestring, sizeof(config->eth.dns1) - 1);
                    cJSON *dns2 = cJSON_GetObjectItem(eth_obj, "dns2");
                    if (dns2 && dns2->valuestring) strncpy(config->eth.dns2, dns2->valuestring, sizeof(config->eth.dns2) - 1);
                }

                // Analisi config WiFi
                cJSON *wifi_obj = cJSON_GetObjectItem(root, "wifi");
                if (wifi_obj) {
                    cJSON *_w_sta = cJSON_GetObjectItem(wifi_obj, "sta"); if (!_w_sta) _w_sta = cJSON_GetObjectItem(wifi_obj, "sta_enabled");
                    config->wifi.sta_enabled = cJSON_IsTrue(_w_sta);
                    cJSON *_w_dhcp = cJSON_GetObjectItem(wifi_obj, "dhcp"); if (!_w_dhcp) _w_dhcp = cJSON_GetObjectItem(wifi_obj, "dhcp_enabled");
                    config->wifi.dhcp_enabled = cJSON_IsTrue(_w_dhcp);
                    cJSON *ssid = cJSON_GetObjectItem(wifi_obj, "ssid");
                    if (ssid && ssid->valuestring) strncpy(config->wifi.ssid, ssid->valuestring, sizeof(config->wifi.ssid) - 1);
                    cJSON *password = cJSON_GetObjectItem(wifi_obj, "pwd"); if (!password) password = cJSON_GetObjectItem(wifi_obj, "password");
                    if (password && password->valuestring) strncpy(config->wifi.password, password->valuestring, sizeof(config->wifi.password) - 1);
                    cJSON *ip = cJSON_GetObjectItem(wifi_obj, "ip");
                    if (ip && ip->valuestring) strncpy(config->wifi.ip, ip->valuestring, sizeof(config->wifi.ip) - 1);
                    cJSON *subnet = cJSON_GetObjectItem(wifi_obj, "sub"); if (!subnet) subnet = cJSON_GetObjectItem(wifi_obj, "subnet");
                    if (subnet && subnet->valuestring) strncpy(config->wifi.subnet, subnet->valuestring, sizeof(config->wifi.subnet) - 1);
                    cJSON *gateway = cJSON_GetObjectItem(wifi_obj, "gw"); if (!gateway) gateway = cJSON_GetObjectItem(wifi_obj, "gateway");
                    if (gateway && gateway->valuestring) strncpy(config->wifi.gateway, gateway->valuestring, sizeof(config->wifi.gateway) - 1);
                }

                // NTP enabled
                cJSON *_ntp_en = cJSON_GetObjectItem(root, "ntp_en"); if (!_ntp_en) _ntp_en = cJSON_GetObjectItem(root, "ntp_enabled");
                config->ntp_enabled = cJSON_IsTrue(_ntp_en);

                // Analisi config NTP
                cJSON *ntp_obj = cJSON_GetObjectItem(root, "ntp");
                if (ntp_obj) {
                    cJSON *server1 = cJSON_GetObjectItem(ntp_obj, "s1"); if (!server1) server1 = cJSON_GetObjectItem(ntp_obj, "server1");
                    if (server1 && server1->valuestring) strncpy(config->ntp.server1, server1->valuestring, sizeof(config->ntp.server1) - 1);
                    cJSON *server2 = cJSON_GetObjectItem(ntp_obj, "s2"); if (!server2) server2 = cJSON_GetObjectItem(ntp_obj, "server2");
                    if (server2 && server2->valuestring) strncpy(config->ntp.server2, server2->valuestring, sizeof(config->ntp.server2) - 1);
                    cJSON *tz_offset = cJSON_GetObjectItem(ntp_obj, "tz"); if (!tz_offset) tz_offset = cJSON_GetObjectItem(ntp_obj, "timezone_offset");
                    if (tz_offset && cJSON_IsNumber(tz_offset)) config->ntp.timezone_offset = tz_offset->valueint;
                }

                // Analisi config Server/Cloud
                cJSON *server_obj = cJSON_GetObjectItem(root, "server");
                if (server_obj) {
                    cJSON *_srv_en = cJSON_GetObjectItem(server_obj, "en"); if (!_srv_en) _srv_en = cJSON_GetObjectItem(server_obj, "enabled");
                    config->server.enabled = cJSON_IsTrue(_srv_en);
                    cJSON *url = cJSON_GetObjectItem(server_obj, "url");
                    if (url && url->valuestring) strncpy(config->server.url, url->valuestring, sizeof(config->server.url) - 1);
                    cJSON *serial = cJSON_GetObjectItem(server_obj, "ser"); if (!serial) serial = cJSON_GetObjectItem(server_obj, "serial");
                    if (serial && serial->valuestring) strncpy(config->server.serial, serial->valuestring, sizeof(config->server.serial) - 1);
                    cJSON *password = cJSON_GetObjectItem(server_obj, "pwd"); if (!password) password = cJSON_GetObjectItem(server_obj, "password");
                    if (password && password->valuestring) strncpy(config->server.password, password->valuestring, sizeof(config->server.password) - 1);
                }

                // Analisi config Remote Logging
                cJSON *remote_log_obj = cJSON_GetObjectItem(root, "rlog");
                if (!remote_log_obj) remote_log_obj = cJSON_GetObjectItem(root, "remote_log"); /* compat */
                if (remote_log_obj) {
                    cJSON *server_port = cJSON_GetObjectItem(remote_log_obj, "port"); if (!server_port) server_port = cJSON_GetObjectItem(remote_log_obj, "server_port");
                    if (server_port) config->remote_log.server_port = (uint16_t)server_port->valueint;
                    cJSON *_bcast = cJSON_GetObjectItem(remote_log_obj, "bcast"); if (!_bcast) _bcast = cJSON_GetObjectItem(remote_log_obj, "use_broadcast");
                    config->remote_log.use_broadcast = cJSON_IsTrue(_bcast);
                    cJSON *_to_sd = cJSON_GetObjectItem(remote_log_obj, "to_sd"); if (!_to_sd) _to_sd = cJSON_GetObjectItem(remote_log_obj, "write_to_sd");
                    config->remote_log.write_to_sd = cJSON_IsTrue(_to_sd);
                }

                // Analisi config Sensori
                cJSON *sensors_obj = cJSON_GetObjectItem(root, "sensors");
                if (sensors_obj) {
                    cJSON *_io = cJSON_GetObjectItem(sensors_obj, "io_exp"); if (!_io) _io = cJSON_GetObjectItem(sensors_obj, "io_expander_enabled");
                    config->sensors.io_expander_enabled = cJSON_IsTrue(_io);
                    cJSON *_temp = cJSON_GetObjectItem(sensors_obj, "temp"); if (!_temp) _temp = cJSON_GetObjectItem(sensors_obj, "temperature_enabled");
                    config->sensors.temperature_enabled = cJSON_IsTrue(_temp);
                    cJSON *_led = cJSON_GetObjectItem(sensors_obj, "led"); if (!_led) _led = cJSON_GetObjectItem(sensors_obj, "led_enabled");
                    config->sensors.led_enabled = cJSON_IsTrue(_led);
                    cJSON *lc = cJSON_GetObjectItem(sensors_obj, "led_n"); if (!lc) lc = cJSON_GetObjectItem(sensors_obj, "led_count");
                    if (lc) config->sensors.led_count = (uint32_t)lc->valueint;
                    cJSON *_r232 = cJSON_GetObjectItem(sensors_obj, "rs232"); if (!_r232) _r232 = cJSON_GetObjectItem(sensors_obj, "rs232_enabled");
                    config->sensors.rs232_enabled = cJSON_IsTrue(_r232);
                    cJSON *_r485 = cJSON_GetObjectItem(sensors_obj, "rs485"); if (!_r485) _r485 = cJSON_GetObjectItem(sensors_obj, "rs485_enabled");
                    config->sensors.rs485_enabled = cJSON_IsTrue(_r485);
                    cJSON *_mdb = cJSON_GetObjectItem(sensors_obj, "mdb"); if (!_mdb) _mdb = cJSON_GetObjectItem(sensors_obj, "mdb_enabled");
                    config->sensors.mdb_enabled = cJSON_IsTrue(_mdb);
                    cJSON *_cct = cJSON_GetObjectItem(sensors_obj, "cctalk"); if (!_cct) _cct = cJSON_GetObjectItem(sensors_obj, "cctalk_enabled");
                    config->sensors.cctalk_enabled = cJSON_IsTrue(_cct);
                    cJSON *eeprom_enabled = cJSON_GetObjectItem(sensors_obj, "eeprom"); if (!eeprom_enabled) eeprom_enabled = cJSON_GetObjectItem(sensors_obj, "eeprom_enabled");
                    if (eeprom_enabled) {
                        config->sensors.eeprom_enabled = cJSON_IsTrue(eeprom_enabled);
                    }
                    cJSON *_pwm1 = cJSON_GetObjectItem(sensors_obj, "pwm1"); if (!_pwm1) _pwm1 = cJSON_GetObjectItem(sensors_obj, "pwm1_enabled");
                    config->sensors.pwm1_enabled = cJSON_IsTrue(_pwm1);
                    cJSON *_pwm2 = cJSON_GetObjectItem(sensors_obj, "pwm2"); if (!_pwm2) _pwm2 = cJSON_GetObjectItem(sensors_obj, "pwm2_enabled");
                    config->sensors.pwm2_enabled = cJSON_IsTrue(_pwm2);
                    cJSON *_sd = cJSON_GetObjectItem(sensors_obj, "sd"); if (!_sd) _sd = cJSON_GetObjectItem(sensors_obj, "sd_card_enabled");
                    config->sensors.sd_card_enabled = cJSON_IsTrue(_sd);
                }

                // Analisi config Scanner USB
                cJSON *scanner_obj = cJSON_GetObjectItem(root, "scanner");
                if (scanner_obj) {
                    cJSON *_sc_en = cJSON_GetObjectItem(scanner_obj, "en"); if (!_sc_en) _sc_en = cJSON_GetObjectItem(scanner_obj, "enabled");
                    if (_sc_en) {
                        ESP_LOGI(TAG, "[CONFIG] scanner._sc_en found - type: %d, valueint: %d, valuedouble: %f", 
                                 _sc_en->type, _sc_en->valueint, _sc_en->valuedouble);
                        ESP_LOGI(TAG, "[CONFIG] cJSON_IsTrue result: %d, cJSON_IsBool: %d", 
                                 cJSON_IsTrue(_sc_en), cJSON_IsBool(_sc_en));
                    } else {
                        ESP_LOGI(TAG, "[CONFIG] scanner._sc_en is NULL!");
                    }
                    config->scanner.enabled = cJSON_IsTrue(_sc_en);
                    ESP_LOGI(TAG, "[CONFIG] scanner.enabled FINAL VALUE: %d", config->scanner.enabled);
                    cJSON *vid = cJSON_GetObjectItem(scanner_obj, "vid");
                    cJSON *pid = cJSON_GetObjectItem(scanner_obj, "pid");
                    cJSON *dual = cJSON_GetObjectItem(scanner_obj, "dpid"); if (!dual) dual = cJSON_GetObjectItem(scanner_obj, "dual_pid");
                    cJSON *cooldown = cJSON_GetObjectItem(scanner_obj, "cool"); if (!cooldown) cooldown = cJSON_GetObjectItem(scanner_obj, "cooldown_ms");
                    if (vid && cJSON_IsNumber(vid)) config->scanner.vid = (uint16_t)vid->valueint;
                    if (pid && cJSON_IsNumber(pid)) config->scanner.pid = (uint16_t)pid->valueint;
                    if (dual && cJSON_IsNumber(dual)) config->scanner.dual_pid = (uint16_t)dual->valueint;
                    if (cooldown && cJSON_IsNumber(cooldown) && cooldown->valueint > 0) {
                        config->scanner.cooldown_ms = (uint32_t)cooldown->valueint;
                    }
                } else {
                    ESP_LOGI(TAG, "[CONFIG] scanner section not found in JSON");
                }

                // Analisi timeout applicativi
                cJSON *timeouts_obj = cJSON_GetObjectItem(root, "timeouts");
                if (timeouts_obj) {
                    cJSON *t_prg = cJSON_GetObjectItem(timeouts_obj, "t_prg");
                    /* compat: accetta anche il vecchio nome */
                    if (!t_prg) t_prg = cJSON_GetObjectItem(timeouts_obj, "language_return_ms");
                    if (t_prg && cJSON_IsNumber(t_prg) && t_prg->valueint > 0) {
                        config->timeouts.exit_programs_ms = (uint32_t)t_prg->valueint;
                    }
                    cJSON *t_lang = cJSON_GetObjectItem(timeouts_obj, "t_lang");
                    if (t_lang && cJSON_IsNumber(t_lang) && t_lang->valueint > 0) {
                        config->timeouts.exit_language_ms = (uint32_t)t_lang->valueint;
                    }
                }
                if (config->timeouts.exit_programs_ms < 1000U)  config->timeouts.exit_programs_ms = 1000U;
                if (config->timeouts.exit_programs_ms > 600000U) config->timeouts.exit_programs_ms = 600000U;
                if (config->timeouts.exit_language_ms < 1000U)  config->timeouts.exit_language_ms = 1000U;
                if (config->timeouts.exit_language_ms > 600000U) config->timeouts.exit_language_ms = 600000U;

                // Analisi config MDB
                cJSON *mdb_obj = cJSON_GetObjectItem(root, "mdb");
                if (mdb_obj) {
                    config->mdb.coin_acceptor_en = cJSON_IsTrue(cJSON_GetObjectItem(mdb_obj, "coin_en"));
                    config->mdb.bill_validator_en = cJSON_IsTrue(cJSON_GetObjectItem(mdb_obj, "bill_en"));
                    config->mdb.cashless_en = cJSON_IsTrue(cJSON_GetObjectItem(mdb_obj, "cashless_en"));
                }

                // Analisi config Display
                cJSON *disp_obj = cJSON_GetObjectItem(root, "display");
                if (disp_obj) {
                    cJSON *enabled = cJSON_GetObjectItem(disp_obj, "en"); if (!enabled) enabled = cJSON_GetObjectItem(disp_obj, "enabled");
                    if (enabled) config->display.enabled = cJSON_IsTrue(enabled);
                    cJSON *bright = cJSON_GetObjectItem(disp_obj, "brt"); if (!bright) bright = cJSON_GetObjectItem(disp_obj, "lcd_brightness");
                    if (bright) config->display.lcd_brightness = (uint8_t)bright->valueint;
                    cJSON *ads_enabled = cJSON_GetObjectItem(disp_obj, "ads_en"); if (!ads_enabled) ads_enabled = cJSON_GetObjectItem(disp_obj, "ads_enabled");
                    if (ads_enabled) config->display.ads_enabled = cJSON_IsTrue(ads_enabled);
                }

                // Analisi config Seriali (RS232, RS485, MDB)
                cJSON *rs232_obj = cJSON_GetObjectItem(root, "rs232");
                if (rs232_obj) {
                    _parse_serial_cfg_json(rs232_obj, &config->rs232, &s_serial_default_rs232);
                }
                cJSON *rs485_obj = cJSON_GetObjectItem(root, "rs485");
                if (rs485_obj) {
                    _parse_serial_cfg_json(rs485_obj, &config->rs485, &s_serial_default_rs485);
                }
                cJSON *modbus_obj = cJSON_GetObjectItem(root, "modbus");
                if (modbus_obj) {
                    cJSON *enabled = cJSON_GetObjectItem(modbus_obj, "enabled");
                    if (enabled) {
                        config->modbus.enabled = cJSON_IsTrue(enabled);
                    }

                    cJSON *slave_id = cJSON_GetObjectItem(modbus_obj, "slave_id");
                    if (slave_id && cJSON_IsNumber(slave_id)) {
                        int val = slave_id->valueint;
                        if (val < 1) val = 1;
                        if (val > 255) val = 255;
                        config->modbus.slave_id = (uint8_t)val;
                    }

                    cJSON *poll_ms = cJSON_GetObjectItem(modbus_obj, "poll_ms");
                    if (poll_ms && cJSON_IsNumber(poll_ms)) {
                        int val = poll_ms->valueint;
                        if (val < 20) val = 20;
                        if (val > 5000) val = 5000;
                        config->modbus.poll_ms = (uint16_t)val;
                    }

                    cJSON *timeout_ms = cJSON_GetObjectItem(modbus_obj, "timeout_ms");
                    if (timeout_ms && cJSON_IsNumber(timeout_ms)) {
                        int val = timeout_ms->valueint;
                        if (val < 50) val = 50;
                        if (val > 2000) val = 2000;
                        config->modbus.timeout_ms = (uint16_t)val;
                    }

                    cJSON *retries = cJSON_GetObjectItem(modbus_obj, "retries");
                    if (retries && cJSON_IsNumber(retries)) {
                        int val = retries->valueint;
                        if (val < 0) val = 0;
                        if (val > 5) val = 5;
                        config->modbus.retries = (uint8_t)val;
                    }

                    cJSON *relay_start = cJSON_GetObjectItem(modbus_obj, "relay_start");
                    if (relay_start && cJSON_IsNumber(relay_start)) {
                        int val = relay_start->valueint;
                        if (val < 0) val = 0;
                        config->modbus.relay_start = (uint16_t)val;
                    }

                    cJSON *relay_count = cJSON_GetObjectItem(modbus_obj, "relay_count");
                    if (relay_count && cJSON_IsNumber(relay_count)) {
                        int val = relay_count->valueint;
                        if (val < 1) val = 1;
                        if (val > DEVICE_CFG_MODBUS_MAX_POINTS) val = DEVICE_CFG_MODBUS_MAX_POINTS;
                        config->modbus.relay_count = (uint16_t)val;
                    }

                    cJSON *input_start = cJSON_GetObjectItem(modbus_obj, "input_start");
                    if (input_start && cJSON_IsNumber(input_start)) {
                        int val = input_start->valueint;
                        if (val < 0) val = 0;
                        config->modbus.input_start = (uint16_t)val;
                    }

                    cJSON *input_count = cJSON_GetObjectItem(modbus_obj, "input_count");
                    if (input_count && cJSON_IsNumber(input_count)) {
                        int val = input_count->valueint;
                        if (val < 1) val = 1;
                        if (val > DEVICE_CFG_MODBUS_MAX_POINTS) val = DEVICE_CFG_MODBUS_MAX_POINTS;
                        config->modbus.input_count = (uint16_t)val;
                    }
                }

                cJSON *cctalk_obj = cJSON_GetObjectItem(root, "cctalk");
                if (cctalk_obj) {
                    cJSON *addr = cJSON_GetObjectItem(cctalk_obj, "address");
                    if (addr && cJSON_IsNumber(addr)) {
                        int val = addr->valueint;
                        if (val < 1) val = 1;
                        if (val > 255) val = 255;
                        config->cctalk.address = (uint8_t)val;
                    }
                }

                cJSON *cctalk_serial_obj = cJSON_GetObjectItem(root, "cctalk_serial");
                if (cctalk_serial_obj) {
                    cJSON *addr = cJSON_GetObjectItem(cctalk_serial_obj, "addr");
                    if (addr && cJSON_IsNumber(addr)) {
                        int val = addr->valueint;
                        if (val < 1) val = 1;
                        if (val > 255) val = 255;
                        config->cctalk.address = (uint8_t)val;
                    }
                }

                cJSON *mdb_s_obj = cJSON_GetObjectItem(root, "mdb_ser");
                if (!mdb_s_obj) mdb_s_obj = cJSON_GetObjectItem(root, "mdb_serial"); /* compat */
                if (mdb_s_obj) {
                    _parse_serial_cfg_json(mdb_s_obj, &config->mdb_serial, &s_serial_default_mdb);
                }

                // Analisi GPIOs configurabili
                cJSON *gpios_obj = cJSON_GetObjectItem(root, "gpios");
                if (gpios_obj) {
                    cJSON *g33 = cJSON_GetObjectItem(gpios_obj, "gpio33");
                    if (g33) {
                        cJSON *_mode = cJSON_GetObjectItem(g33, "m"); if (!_mode) _mode = cJSON_GetObjectItem(g33, "mode");
                        config->gpios.gpio33.mode = (device_gpio_cfg_mode_t)cJSON_GetNumberValue(_mode);
                        cJSON *_st = cJSON_GetObjectItem(g33, "st"); if (!_st) _st = cJSON_GetObjectItem(g33, "state");
                        config->gpios.gpio33.initial_state = cJSON_IsTrue(_st);
                    }
                }

                // Analisi tabella UI (lingue: pannello utente e backend)
                cJSON *ui_obj = cJSON_GetObjectItem(root, "ui");
                if (ui_obj) {
                    cJSON *user_lang = cJSON_GetObjectItem(ui_obj, "ulang");
                    if (!user_lang) user_lang = cJSON_GetObjectItem(ui_obj, "user_language"); /* compat */
                    if (!user_lang) user_lang = cJSON_GetObjectItem(ui_obj, "language"); /* compat */
                    if (user_lang && cJSON_IsString(user_lang) && user_lang->valuestring) {
                        strncpy(config->ui.user_language, user_lang->valuestring, sizeof(config->ui.user_language) - 1);
                    }

                    cJSON *backend_lang = cJSON_GetObjectItem(ui_obj, "blang");
                    if (!backend_lang) backend_lang = cJSON_GetObjectItem(ui_obj, "backend_language"); /* compat */
                    if (backend_lang && cJSON_IsString(backend_lang) && backend_lang->valuestring) {
                        strncpy(config->ui.backend_language, backend_lang->valuestring, sizeof(config->ui.backend_language) - 1);
                    }
                }

                cJSON *ui_lang_flat = cJSON_GetObjectItem(root, "ui_language");
                if (ui_lang_flat && cJSON_IsString(ui_lang_flat) && ui_lang_flat->valuestring) {
                    strncpy(config->ui.user_language, ui_lang_flat->valuestring, sizeof(config->ui.user_language) - 1);
                }

            cJSON_Delete(root);
            ESP_LOGD(TAG, "[C] Configurazione caricata correttamente da %s", source_is_eeprom ? "EEPROM" : "NVS");
        } else {
            ESP_LOGE(TAG, "[C] Errore parsing JSON!");
        }

        if (parse_ok && source_is_nvs && eeprom_24lc16_is_available()) {
            /* Sincronizza NVS -> EEPROM usando JSON canonico ridotto,
             * evitando payload legacy potenzialmente troppo grandi. */
            char *canonical_json = device_config_to_json(config);
            if (canonical_json) {
                esp_err_t sync_err = _write_to_eeprom(canonical_json, true);
                if (sync_err != ESP_OK) {
                    ESP_LOGW(TAG, "[C] Sync NVS->EEPROM fallita: %s", esp_err_to_name(sync_err));
                }
                free(canonical_json);
            }
        }

        free(json_str);
    }

    return ESP_OK;
}


/**
 * @brief Converte una configurazione di dispositivo in un formato JSON.
 *
 * @param [in] config Puntatore alla struttura di configurazione del dispositivo.
 * @return char* Puntatore alla stringa JSON rappresentante la configurazione del dispositivo.
 */
char* device_config_to_json(const device_config_t *config)
{
    if (!config) return NULL;

    cJSON *root = cJSON_CreateObject();

    // Updated flag
    cJSON_AddBoolToObject(root, "updated", config->updated);

    // Nome dispositivo
    cJSON_AddStringToObject(root, "dname", config->device_name);
    cJSON_AddStringToObject(root, "loc", config->location_name);
    cJSON_AddStringToObject(root, "image_source", config->image_source == IMAGE_SOURCE_SDCARD ? "sdcard" : "spiffs");

    // Numero programmi e coordinate geografiche
    cJSON_AddNumberToObject(root, "n_prg", config->num_programs);
    cJSON_AddNumberToObject(root, "lat", config->latitude);
    cJSON_AddNumberToObject(root, "lon", config->longitude);

    // Ethernet
    cJSON *eth_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth_obj, "en", config->eth.enabled);
    cJSON_AddBoolToObject(eth_obj, "dhcp", config->eth.dhcp_enabled);
    cJSON_AddStringToObject(eth_obj, "ip", config->eth.ip);
    cJSON_AddStringToObject(eth_obj, "sub", config->eth.subnet);
    cJSON_AddStringToObject(eth_obj, "gw", config->eth.gateway);
    cJSON_AddStringToObject(eth_obj, "dns1", config->eth.dns1);
    cJSON_AddStringToObject(eth_obj, "dns2", config->eth.dns2);
    cJSON_AddItemToObject(root, "eth", eth_obj);

    // WiFi
    cJSON *wifi_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_obj, "sta", config->wifi.sta_enabled);
    cJSON_AddBoolToObject(wifi_obj, "dhcp", config->wifi.dhcp_enabled);
    cJSON_AddStringToObject(wifi_obj, "ssid", config->wifi.ssid);
    cJSON_AddStringToObject(wifi_obj, "pwd", config->wifi.password);
    cJSON_AddStringToObject(wifi_obj, "ip", config->wifi.ip);
    cJSON_AddStringToObject(wifi_obj, "sub", config->wifi.subnet);
    cJSON_AddStringToObject(wifi_obj, "gw", config->wifi.gateway);
    cJSON_AddItemToObject(root, "wifi", wifi_obj);

    // NTP
    cJSON_AddBoolToObject(root, "ntp_en", config->ntp_enabled);
    cJSON *ntp_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(ntp_obj, "s1", config->ntp.server1);
    cJSON_AddStringToObject(ntp_obj, "s2", config->ntp.server2);
    cJSON_AddNumberToObject(ntp_obj, "tz", config->ntp.timezone_offset);
    cJSON_AddItemToObject(root, "ntp", ntp_obj);

    // Server/Cloud
    cJSON *server_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(server_obj, "en", config->server.enabled);
    cJSON_AddStringToObject(server_obj, "url", config->server.url);
    cJSON_AddStringToObject(server_obj, "ser", config->server.serial);
    cJSON_AddStringToObject(server_obj, "pwd", config->server.password);
    cJSON_AddItemToObject(root, "server", server_obj);

    // Remote Logging
    cJSON *remote_log_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(remote_log_obj, "port", config->remote_log.server_port);
    cJSON_AddBoolToObject(remote_log_obj, "bcast", config->remote_log.use_broadcast);
    cJSON_AddBoolToObject(remote_log_obj, "to_sd", config->remote_log.write_to_sd);
    cJSON_AddItemToObject(root, "rlog", remote_log_obj);

    // Sensors
    cJSON *sensors_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors_obj, "io_exp", config->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors_obj, "temp", config->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors_obj, "led", config->sensors.led_enabled);
    cJSON_AddNumberToObject(sensors_obj, "led_n", config->sensors.led_count);
    cJSON_AddBoolToObject(sensors_obj, "rs232", config->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors_obj, "rs485", config->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors_obj, "mdb", config->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors_obj, "cctalk", config->sensors.cctalk_enabled);
    cJSON_AddBoolToObject(sensors_obj, "eeprom", config->sensors.eeprom_enabled);
    cJSON_AddBoolToObject(sensors_obj, "pwm1", config->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors_obj, "pwm2", config->sensors.pwm2_enabled);
    cJSON_AddBoolToObject(sensors_obj, "sd", config->sensors.sd_card_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors_obj);

    // Scanner
    cJSON *scanner_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(scanner_obj, "en", config->scanner.enabled);
    cJSON_AddNumberToObject(scanner_obj, "vid", config->scanner.vid);
    cJSON_AddNumberToObject(scanner_obj, "pid", config->scanner.pid);
    cJSON_AddNumberToObject(scanner_obj, "dpid", config->scanner.dual_pid);
    cJSON_AddNumberToObject(scanner_obj, "cool", config->scanner.cooldown_ms);
    cJSON_AddItemToObject(root, "scanner", scanner_obj);

    // Timeouts
    cJSON *timeouts_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(timeouts_obj, "t_prg", config->timeouts.exit_programs_ms);
    cJSON_AddNumberToObject(timeouts_obj, "t_lang", config->timeouts.exit_language_ms);
    cJSON_AddItemToObject(root, "timeouts", timeouts_obj);

    // MDB
    cJSON *mdb_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(mdb_obj, "coin_en", config->mdb.coin_acceptor_en);
    cJSON_AddBoolToObject(mdb_obj, "bill_en", config->mdb.bill_validator_en);
    cJSON_AddBoolToObject(mdb_obj, "cashless_en", config->mdb.cashless_en);
    cJSON_AddItemToObject(root, "mdb", mdb_obj);

    // Display
    cJSON *disp_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(disp_obj, "en", config->display.enabled);
    cJSON_AddNumberToObject(disp_obj, "brt", config->display.lcd_brightness);
    cJSON_AddBoolToObject(disp_obj, "ads_en", config->display.ads_enabled);
    cJSON_AddItemToObject(root, "display", disp_obj);

    // Seriale RS232
    cJSON *rs232_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs232_obj, "baud", config->rs232.baud_rate);
    cJSON_AddNumberToObject(rs232_obj, "data", config->rs232.data_bits);
    cJSON_AddNumberToObject(rs232_obj, "par", config->rs232.parity);
    cJSON_AddNumberToObject(rs232_obj, "stop", config->rs232.stop_bits);
    cJSON_AddNumberToObject(rs232_obj, "rx_buf", config->rs232.rx_buf_size);
    cJSON_AddNumberToObject(rs232_obj, "tx_buf", config->rs232.tx_buf_size);
    cJSON_AddItemToObject(root, "rs232", rs232_obj);

    // Seriale RS485
    cJSON *rs485_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs485_obj, "baud", config->rs485.baud_rate);
    cJSON_AddNumberToObject(rs485_obj, "data", config->rs485.data_bits);
    cJSON_AddNumberToObject(rs485_obj, "par", config->rs485.parity);
    cJSON_AddNumberToObject(rs485_obj, "stop", config->rs485.stop_bits);
    cJSON_AddNumberToObject(rs485_obj, "rx_buf", config->rs485.rx_buf_size);
    cJSON_AddNumberToObject(rs485_obj, "tx_buf", config->rs485.tx_buf_size);
    cJSON_AddItemToObject(root, "rs485", rs485_obj);

    // Modbus RTU
    cJSON *modbus_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(modbus_obj, "enabled", config->modbus.enabled);
    cJSON_AddNumberToObject(modbus_obj, "slave_id", config->modbus.slave_id);
    cJSON_AddNumberToObject(modbus_obj, "poll_ms", config->modbus.poll_ms);
    cJSON_AddNumberToObject(modbus_obj, "timeout_ms", config->modbus.timeout_ms);
    cJSON_AddNumberToObject(modbus_obj, "retries", config->modbus.retries);
    cJSON_AddNumberToObject(modbus_obj, "relay_start", config->modbus.relay_start);
    cJSON_AddNumberToObject(modbus_obj, "relay_count", config->modbus.relay_count);
    cJSON_AddNumberToObject(modbus_obj, "input_start", config->modbus.input_start);
    cJSON_AddNumberToObject(modbus_obj, "input_count", config->modbus.input_count);
    cJSON_AddItemToObject(root, "modbus", modbus_obj);

    // CCtalk
    cJSON *cctalk_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(cctalk_obj, "address", config->cctalk.address);
    cJSON_AddItemToObject(root, "cctalk", cctalk_obj);

    // Seriale MDB
    cJSON *mdb_s_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(mdb_s_obj, "baud", config->mdb_serial.baud_rate);
    cJSON_AddNumberToObject(mdb_s_obj, "data", config->mdb_serial.data_bits);
    cJSON_AddNumberToObject(mdb_s_obj, "par", config->mdb_serial.parity);
    cJSON_AddNumberToObject(mdb_s_obj, "stop", config->mdb_serial.stop_bits);
    cJSON_AddNumberToObject(mdb_s_obj, "rx_buf", config->mdb_serial.rx_buf_size);
    cJSON_AddNumberToObject(mdb_s_obj, "tx_buf", config->mdb_serial.tx_buf_size);
    cJSON_AddItemToObject(root, "mdb_ser", mdb_s_obj);

    // GPIOs configurabili
    cJSON *gpios_obj = cJSON_CreateObject();
    cJSON *g33_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(g33_obj, "m", config->gpios.gpio33.mode);
    cJSON_AddBoolToObject(g33_obj, "st", config->gpios.gpio33.initial_state);
    cJSON_AddItemToObject(gpios_obj, "gpio33", g33_obj);
    cJSON_AddItemToObject(root, "gpios", gpios_obj);

    // UI multilingua (due linguaggi: pannello utente e backend; tabelle su SPIFFS per file lingua)
    cJSON *ui_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(ui_obj, "ulang", config->ui.user_language);
    cJSON_AddStringToObject(ui_obj, "blang", config->ui.backend_language);
    cJSON_AddItemToObject(root, "ui", ui_obj);
    /* backward-compatible single field: ui_language -> user_language */
    cJSON_AddStringToObject(root, "ui_language", config->ui.user_language);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}


/**
 * @brief Legge la configurazione del dispositivo in formato JSON dalla EEPROM.
 *
 * @param [out] Nessun parametro di input.
 * @return char* Puntatore alla stringa JSON contenente la configurazione del dispositivo, o NULL in caso di errore.
 */
char* device_config_read_json_from_eeprom(void)
{
    bool eeprom_modified = false;
    return _read_from_eeprom(&eeprom_modified);
}


/**
 * @brief Salva la configurazione del dispositivo.
 *
 * Questa funzione salva la configurazione del dispositivo in una memoria persistente.
 *
 * @param [in] config Puntatore alla struttura di configurazione del dispositivo.
 * @return esp_err_t Codice di errore che indica il successo o la causa dell'errore.
 */
esp_err_t device_config_save(const device_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "[C] Salvataggio configurazione: LED count = %lu, LCD bright = %d", 
             config->sensors.led_count, config->display.lcd_brightness);

    char *json_str = device_config_to_json(config);
    if (!json_str) return ESP_ERR_NO_MEM;

    ESP_LOGD(TAG, "[C] JSON da salvare: %s", json_str);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    
    if (eeprom_24lc16_is_available()) {
        // Alla modifica (tasto save), salviamo in EEPROM con flag 'modified' (Specifica 3)
        err = _write_to_eeprom(json_str, true);
    }

    if (err != ESP_OK) {
        // Se EEPROM non disponibile o salvataggio fallito, scriviamo direttamente in NVS (Specifica User)
        _write_to_nvs(json_str);
        if (err == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "[C] EEPROM non disponibile, salvataggio diretto su NVS");
        } else {
            ESP_LOGE(TAG, "[C] Salvataggio EEPROM fallito (0x%x), ripiego su NVS", err);
        }
    } else {
        ESP_LOGI(TAG, "[C] Config salvato in EEPROM (flag modified settato)");
    }
    
    free(json_str);
    return ESP_OK;
}


/**
 * @brief Ottiene un puntatore alla configurazione del dispositivo.
 *
 * @return device_config_t* Puntatore alla configurazione del dispositivo.
 */
device_config_t* device_config_get(void)
{
    return &s_config;
}

/**
 * @brief Ottiene il CRC della configurazione del dispositivo.
 *
 * @return uint32_t Il valore del CRC della configurazione del dispositivo.
 */
uint32_t device_config_get_crc(void)
{
    char *json = device_config_to_json(&s_config);
    if (!json) return 0;
    uint32_t crc = _calculate_crc(json);
    free(json);
    return crc;
}


/**
 * @brief Controlla se la configurazione del dispositivo è stata aggiornata.
 *
 * Questa funzione verifica lo stato della configurazione del dispositivo e restituisce
 * un valore booleano che indica se la configurazione è stata aggiornata o meno.
 *
 * @return bool - TRUE se la configurazione è stata aggiornata, FALSE altrimenti.
 */
bool device_config_is_updated(void)
{
    return s_config.updated;
}

/**
 * @brief Resetta i valori di configurazione del dispositivo ai valori predefiniti.
 *
 * Questa funzione reimposta tutti i parametri di configurazione del dispositivo ai loro valori predefiniti.
 *
 * @return esp_err_t
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione non riuscita.
 */
esp_err_t device_config_reset_defaults(void)
{
    _set_defaults(&s_config);
    return device_config_save(&s_config);
}


/**
 * @brief Riavvia il dispositivo alla configurazione di fabbrica.
 *
 * Questa funzione riavvia il dispositivo, reimpostando tutte le sue impostazioni
 * su quelle di fabbrica.
 *
 * @return Niente.
 */
void device_config_reboot_factory(void)
{
    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    _i18n_v2_clear_cache();
    if (factory) {
        ESP_LOGI(TAG, "Impostazione partizione di boot: FACTORY");
        esp_err_t err = esp_ota_set_boot_partition(factory);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore set boot partition FACTORY: %s", esp_err_to_name(err));
        }
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Partizione Factory non trovata!");
    }
}


/**
 * @brief Riavvia l'applicazione del dispositivo.
 *
 * Questa funzione riavvia l'applicazione del dispositivo, reimpostando tutte le impostazioni a quelle di default.
 *
 * @return Nessun valore di ritorno.
 */
void device_config_reboot_app(void)
{
    device_config_reboot_app_last();
}


/**
 * @brief Riavvia il dispositivo utilizzando l'immagine OTA0.
 *
 * Questa funzione invia un comando di riavvio al dispositivo, utilizzando l'immagine di sistema
 * memorizzata nell'area OTA0. Questo comando causa il riavvio del dispositivo, che successivamente
 * avvierà con l'immagine di sistema OTA0.
 *
 * @return Nessun valore di ritorno.
 */
void device_config_reboot_ota0(void)
{
    const esp_partition_t *ota0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (ota0) {
        ESP_LOGI(TAG, "Impostazione partizione di boot: OTA_0");
        esp_err_t err = esp_ota_set_boot_partition(ota0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore set boot partition OTA_0: %s", esp_err_to_name(err));
        }
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Partizione OTA_0 non trovata!");
    }
}


/**
 * @brief Riavvia il dispositivo utilizzando l'aggiornamento OTA1.
 *
 * Questa funzione invia un comando di riavvio al dispositivo, utilizzando il protocollo di aggiornamento OTA1.
 *
 * @return Niente.
 */
void device_config_reboot_ota1(void)
{
    const esp_partition_t *ota1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    if (ota1) {
        ESP_LOGI(TAG, "Impostazione partizione di boot: OTA_1");
        esp_err_t err = esp_ota_set_boot_partition(ota1);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore set boot partition OTA_1: %s", esp_err_to_name(err));
        }
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Partizione OTA_1 non trovata!");
    }
}


/**
 * @brief Riavvia l'applicazione con la configurazione dell'ultimo dispositivo.
 *
 * Questa funzione riavvia l'applicazione utilizzando la configurazione dell'ultimo dispositivo
 * che ha interagito con il sistema. Questo garantisce che l'applicazione si avvii con le impostazioni
 * corrette e le preferenze dell'utente.
 *
 * @return Niente.
 */
void device_config_reboot_app_last(void)
{
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    const esp_partition_t *target = NULL;

    if (boot && (boot->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 || boot->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1)) {
        target = boot;
    } else {
        const esp_partition_t *running = esp_ota_get_running_partition();
        if (running && (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 || running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1)) {
            target = running;
        }
    }

    if (!target) {
        target = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    }

    if (target) {
        ESP_LOGI(TAG, "Impostazione partizione di boot: APP LAST (%s)", target->label);
        esp_err_t err = esp_ota_set_boot_partition(target);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Errore set boot partition APP LAST (%s): %s", target->label, esp_err_to_name(err));
        }
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Partizione APP LAST non trovata (OTA_0/OTA_1 assenti)!");
    }
}

const char* device_config_get_running_app_name(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        return "MAINTENANCE";
    } else if (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
        return "OTA0";
    } else if (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) {
        return "OTA1";
    }
    return "UNKNOWN";
}

const char* device_config_get_ui_language(void)
{
    return s_config.ui.user_language;
}

const char* device_config_get_ui_user_language(void)
{
    return s_config.ui.user_language;
}

const char* device_config_get_ui_backend_language(void)
{
    return s_config.ui.backend_language;
}


/**
 * @brief Ottiene i record di testo dell'interfaccia utente in formato JSON per un determinato linguaggio.
 *
 * @param [in] language Il codice del linguaggio per cui si desidera ottenere i record di testo dell'interfaccia utente.
 * @return char* Un puntatore a una stringa JSON contenente i record di testo dell'interfaccia utente, oppure NULL in caso di errore.
 */
char* device_config_get_ui_texts_records_json(const char *language)
{
    cJSON *records = _i18n_v2_build_records_for_language(language);
    if (!records) {
        return NULL;
    }
    char *json = cJSON_PrintUnformatted(records);
    cJSON_Delete(records);
    return json;
}


/**
 * @brief Imposta i testi UI per i record JSON in base alla lingua specificata.
 *
 * @param [in] language La lingua per cui impostare i testi UI.
 * @param [in] records_json I record JSON contenenti i testi UI.
 *
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t device_config_set_ui_texts_records_json(const char *language, const char *records_json)
{
    (void)language;
    (void)records_json;
    return ESP_ERR_NOT_SUPPORTED;
}


/**
 * @brief Ottiene il testo dell'interfaccia utente per un determinato ambito e chiave.
 * 
 * @param [in] scope L'ambito per cui si desidera ottenere il testo dell'interfaccia utente.
 * @param [in] key La chiave per cui si desidera ottenere il testo dell'interfaccia utente.
 * @param [in] fallback Il testo di fallback da utilizzare se non viene trovato il testo dell'interfaccia utente.
 * @param [out] out Il buffer in cui verrà memorizzato il testo dell'interfaccia utente.
 * @param [in] out_len La dimensione del buffer di output.
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t device_config_get_ui_text_scoped(const char *scope, const char *key, const char *fallback, char *out, size_t out_len)
{
    if (!scope || !key || !out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Prefer numeric lookup via PSRAM when the dictionary is loaded. */
    {
        uint8_t  scope_id = (uint8_t)i18n_scope_id(scope);
        uint16_t key_id   = (uint16_t)i18n_key_id(key);
        if (scope_id != 0 && key_id != 0) {
            char *concat = i18n_concat_from_psram(scope_id, key_id);
            if (concat) {
                strncpy(out, concat, out_len - 1);
                out[out_len - 1] = '\0';
                free(concat);
                return ESP_OK;
            }
        }
    }

    if (_i18n_lookup_cache_build_for_lang(NULL) != ESP_OK || !s_i18n_lookup_cache) {
        if (fallback) {
            strncpy(out, fallback, out_len - 1);
            out[out_len - 1] = '\0';
        } else {
            out[0] = '\0';
        }
        return ESP_ERR_INVALID_STATE;
    }

    char scoped_key[96] = {0};
    snprintf(scoped_key, sizeof(scoped_key), "%s.%s", scope, key);

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    cJSON *val = cJSON_GetObjectItemCaseSensitive(s_i18n_lookup_cache, scoped_key);
    if (!cJSON_IsString(val) || !val->valuestring) {
        val = cJSON_GetObjectItemCaseSensitive(s_i18n_lookup_cache, key);
    }

    if (cJSON_IsString(val) && val->valuestring) {
        strncpy(out, val->valuestring, out_len - 1);
        out[out_len - 1] = '\0';
        ret = ESP_OK;
    }

    if (ret != ESP_OK) {
        if (fallback) {
            strncpy(out, fallback, out_len - 1);
            out[out_len - 1] = '\0';
        } else {
            out[0] = '\0';
        }
    }

    return ret;
}
