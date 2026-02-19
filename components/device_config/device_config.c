#include "device_config.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "esp_rom_crc.h"
#include "eeprom_24lc16.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/stat.h>

static const char *TAG = "DEVICE_CFG";
static const char *NVS_NAMESPACE = "device_config";
static const char *UI_LANG_DEFAULT = "it";
static const char *UI_TEXTS_DEFAULT_IT_JSON =
    "["
    "{\"lang\":\"it\",\"scope\":\"nav\",\"key\":\"home\",\"text\":\"Home\"},"
    "{\"lang\":\"it\",\"scope\":\"nav\",\"key\":\"config\",\"text\":\"Config\"},"
    "{\"lang\":\"it\",\"scope\":\"nav\",\"key\":\"stats\",\"text\":\"Statistiche\"},"
    "{\"lang\":\"it\",\"scope\":\"nav\",\"key\":\"tasks\",\"text\":\"Task\"},"
    "{\"lang\":\"it\",\"scope\":\"nav\",\"key\":\"logs\",\"text\":\"Log\"},"
    "{\"lang\":\"it\",\"scope\":\"nav\",\"key\":\"test\",\"text\":\"Test\"},"
    "{\"lang\":\"it\",\"scope\":\"nav\",\"key\":\"ota\",\"text\":\"OTA\"},"
    "{\"lang\":\"it\",\"scope\":\"nav\",\"key\":\"emulator\",\"text\":\"Emulatore\"},"
    "{\"lang\":\"it\",\"scope\":\"header\",\"key\":\"time_not_set\",\"text\":\"Ora non impostata\"},"
    "{\"lang\":\"it\",\"scope\":\"lvgl\",\"key\":\"time_not_available\",\"text\":\"Ora non disponibile\"}"
    "]";

#define EEPROM_MAGIC 0x57415645 // "WAVE"
#define EEPROM_HEADER_ADDR 0

typedef struct {
    uint32_t magic;
    uint32_t crc;
    uint32_t length;
    uint16_t modified;
    uint16_t version;
} eeprom_header_t;

static device_config_t s_config = {0};
static bool s_initialized = false;

static const char *_effective_lang(const char *language);

static cJSON *s_i18n_lookup_cache = NULL;
static char s_i18n_lookup_lang[8] = {0};

static void _i18n_lookup_cache_clear(void)
{
    if (s_i18n_lookup_cache) {
        cJSON_Delete(s_i18n_lookup_cache);
        s_i18n_lookup_cache = NULL;
    }
    s_i18n_lookup_lang[0] = '\0';
}

static esp_err_t _i18n_lookup_cache_build_for_lang(const char *language)
{
    const char *lang = _effective_lang(language);
    if (s_i18n_lookup_cache && strcmp(s_i18n_lookup_lang, lang) == 0) {
        return ESP_OK;
    }

    _i18n_lookup_cache_clear();

    char *records_json = device_config_get_ui_texts_records_json(lang);
    if (!records_json) {
        return ESP_ERR_NO_MEM;
    }

    cJSON *records = cJSON_Parse(records_json);
    free(records_json);
    if (!records || !cJSON_IsArray(records)) {
        if (records) {
            cJSON_Delete(records);
        }
        return ESP_FAIL;
    }

    cJSON *table = cJSON_CreateObject();
    if (!table) {
        cJSON_Delete(records);
        return ESP_ERR_NO_MEM;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, records) {
        if (!cJSON_IsObject(item)) {
            continue;
        }

        cJSON *scope = cJSON_GetObjectItem(item, "scope");
        cJSON *key = cJSON_GetObjectItem(item, "key");
        cJSON *text = cJSON_GetObjectItem(item, "text");
        if (!cJSON_IsString(scope) || !scope->valuestring ||
            !cJSON_IsString(key) || !key->valuestring ||
            !cJSON_IsString(text) || !text->valuestring) {
            continue;
        }

        char scoped_key[96] = {0};
        snprintf(scoped_key, sizeof(scoped_key), "%s.%s", scope->valuestring, key->valuestring);

        if (!cJSON_GetObjectItemCaseSensitive(table, scoped_key)) {
            cJSON_AddStringToObject(table, scoped_key, text->valuestring);
        }

        if (!cJSON_GetObjectItemCaseSensitive(table, key->valuestring)) {
            cJSON_AddStringToObject(table, key->valuestring, text->valuestring);
        }
    }

    cJSON_Delete(records);

    s_i18n_lookup_cache = table;
    strncpy(s_i18n_lookup_lang, lang, sizeof(s_i18n_lookup_lang) - 1);
    s_i18n_lookup_lang[sizeof(s_i18n_lookup_lang) - 1] = '\0';
    return ESP_OK;
}

static bool _is_iso2_lang(const char *language)
{
    return language && strlen(language) == 2;
}

static const char *_effective_lang(const char *language)
{
    if (_is_iso2_lang(language)) {
        return language;
    }
    if (_is_iso2_lang(s_config.ui.language)) {
        return s_config.ui.language;
    }
    return UI_LANG_DEFAULT;
}

static void _build_i18n_path(const char *language, char *out, size_t out_len)
{
    snprintf(out, out_len, "/spiffs/i18n_%s.json", _effective_lang(language));
}

static bool _is_valid_i18n_records_json(const char *records_json)
{
    if (!records_json) {
        return false;
    }

    cJSON *root = cJSON_Parse(records_json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        return false;
    }

    bool valid = true;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (!cJSON_IsObject(item)) {
            valid = false;
            break;
        }
        cJSON *lang = cJSON_GetObjectItem(item, "lang");
        cJSON *scope = cJSON_GetObjectItem(item, "scope");
        cJSON *key = cJSON_GetObjectItem(item, "key");
        cJSON *text = cJSON_GetObjectItem(item, "text");
        if (!cJSON_IsString(lang) || !lang->valuestring ||
            !cJSON_IsString(scope) || !scope->valuestring ||
            !cJSON_IsString(key) || !key->valuestring ||
            !cJSON_IsString(text) || !text->valuestring) {
            valid = false;
            break;
        }
    }

    cJSON_Delete(root);
    return valid;
}

static esp_err_t _write_i18n_file(const char *language, const char *records_json)
{
    if (!_is_iso2_lang(language) || !_is_valid_i18n_records_json(records_json)) {
        return ESP_ERR_INVALID_ARG;
    }

    char path[64] = {0};
    _build_i18n_path(language, path, sizeof(path));

    FILE *file = fopen(path, "w");
    if (!file) {
        ESP_LOGE(TAG, "[I18N] Impossibile aprire %s in scrittura", path);
        return ESP_FAIL;
    }

    size_t len = strlen(records_json);
    size_t written = fwrite(records_json, 1, len, file);
    fclose(file);

    if (written != len) {
        ESP_LOGE(TAG, "[I18N] Scrittura incompleta su %s", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static char *_read_i18n_file(const char *language)
{
    char path[64] = {0};
    _build_i18n_path(language, path, sizeof(path));

    FILE *file = fopen(path, "r");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size) {
        free(buffer);
        return NULL;
    }
    buffer[size] = '\0';

    if (!_is_valid_i18n_records_json(buffer)) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

static void _ensure_default_i18n_it_file(void)
{
    char *existing = _read_i18n_file(UI_LANG_DEFAULT);
    if (existing) {
        free(existing);
        return;
    }
    esp_err_t err = _write_i18n_file(UI_LANG_DEFAULT, UI_TEXTS_DEFAULT_IT_JSON);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[I18N] Creazione default i18n it fallita: %s", esp_err_to_name(err));
    }
}

// Configurazione predefinita
static void _set_defaults(device_config_t *config)
{
    memset(config, 0, sizeof(device_config_t));
    config->updated = false;

    // Nome dispositivo
    strncpy(config->device_name, "TestWave-Device", sizeof(config->device_name) - 1);

    // Default Ethernet
    config->eth.enabled = true;
    config->eth.dhcp_enabled = true;
    strncpy(config->eth.ip, "192.168.1.100", sizeof(config->eth.ip) - 1);
    strncpy(config->eth.subnet, "255.255.255.0", sizeof(config->eth.subnet) - 1);
    strncpy(config->eth.gateway, "192.168.1.1", sizeof(config->eth.gateway) - 1);

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

    // Default Sensori (tutti abilitati per impostazione predefinita)
    config->sensors.io_expander_enabled = true;
    config->sensors.temperature_enabled = true;
    config->sensors.led_enabled = true;
    config->sensors.led_count = CONFIG_APP_WS2812_LEDS;
    config->sensors.rs232_enabled = true;
    config->sensors.rs485_enabled = true;
    config->sensors.mdb_enabled = true;
    config->sensors.cctalk_enabled = true; /* CCtalk enabled by default */
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

    // Default testi UI / lingua
    strncpy(config->ui.language, UI_LANG_DEFAULT, sizeof(config->ui.language) - 1);
    config->ui.texts_json[0] = '\0';
}

static uint32_t _calculate_crc(const char *json_str)
{
    if (!json_str) return 0;
    return esp_rom_crc32_le(0, (const uint8_t *)json_str, strlen(json_str));
}

// -----------------------------------------------------------------------------
// Helper EEProm
// -----------------------------------------------------------------------------

static esp_err_t _write_to_eeprom(const char *json_str, bool modified)
{
    if (!eeprom_24lc16_is_available()) return ESP_ERR_INVALID_STATE;

    eeprom_header_t header;
    header.magic = EEPROM_MAGIC;
    header.length = strlen(json_str);
    header.crc = _calculate_crc(json_str);
    header.modified = modified ? 1 : 0;
    header.version = 1;

    // Scrivi header
    ESP_RETURN_ON_ERROR(eeprom_24lc16_write(EEPROM_HEADER_ADDR, (uint8_t *)&header, sizeof(header)), TAG, "Scrittura header EEPROM fallita");
    // Scrivi dati subito dopo l'header
    ESP_RETURN_ON_ERROR(eeprom_24lc16_write(EEPROM_HEADER_ADDR + sizeof(header), (const uint8_t *)json_str, header.length), TAG, "Scrittura JSON EEPROM fallita");

    ESP_LOGI(TAG, "[C] Config salvata in EEPROM (len=%lu, crc=0x%08lX, modified=%d)", 
             (unsigned long)header.length, (unsigned long)header.crc, header.modified);
    return ESP_OK;
}

static char* _read_from_eeprom(bool *out_modified)
{
    if (!eeprom_24lc16_is_available()) return NULL;

    eeprom_header_t header;
    if (eeprom_24lc16_read(EEPROM_HEADER_ADDR, (uint8_t *)&header, sizeof(header)) != ESP_OK) return NULL;

    if (header.magic != EEPROM_MAGIC) {
        ESP_LOGW(TAG, "[C] Magic EEPROM non valido (0x%08lX)", (unsigned long)header.magic);
        return NULL;
    }

    if (header.length == 0 || header.length > 2000) {
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

esp_err_t device_config_init(void)
{
    ESP_LOGI(TAG, "[C] Inizializzazione sistema configurazione");
    _set_defaults(&s_config);
    s_initialized = true;
    esp_err_t err = device_config_load(&s_config);
    if (err != ESP_OK) {
        return err;
    }

    _ensure_default_i18n_it_file();

    char *startup_i18n = _read_i18n_file(s_config.ui.language);
    if (startup_i18n) {
        ESP_LOGI(TAG, "[I18N] Tabella lingua '%s' caricata da SPIFFS all'avvio", _effective_lang(s_config.ui.language));
        free(startup_i18n);
    } else {
        ESP_LOGW(TAG, "[I18N] Tabella lingua '%s' non disponibile all'avvio, uso fallback IT", _effective_lang(s_config.ui.language));
    }

    if (_i18n_lookup_cache_build_for_lang(s_config.ui.language) == ESP_OK) {
        ESP_LOGI(TAG, "[I18N] Cache lookup lingua '%s' precaricata", _effective_lang(s_config.ui.language));
    }

    return ESP_OK;
}

esp_err_t device_config_load(device_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;

    _set_defaults(config);

    bool eeprom_modified = false;
    char *json_str = _read_from_eeprom(&eeprom_modified);
    bool source_is_eeprom = false;

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
            // Sincronizza NVS -> EEPROM e imposta flag 'modified' (come da specifica 2.1)
            _write_to_eeprom(json_str, true);
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
        cJSON *root = cJSON_Parse(json_str);
        if (root) {
            // Nome dispositivo
            cJSON *name = cJSON_GetObjectItem(root, "device_name");
            if (name && name->valuestring) strncpy(config->device_name, name->valuestring, sizeof(config->device_name) - 1);

                // Analisi config Ethernet
                cJSON *eth_obj = cJSON_GetObjectItem(root, "eth");
                if (eth_obj) {
                    config->eth.enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "enabled"));
                    config->eth.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "dhcp_enabled"));
                    cJSON *ip = cJSON_GetObjectItem(eth_obj, "ip");
                    if (ip && ip->valuestring) strncpy(config->eth.ip, ip->valuestring, sizeof(config->eth.ip) - 1);
                    cJSON *subnet = cJSON_GetObjectItem(eth_obj, "subnet");
                    if (subnet && subnet->valuestring) strncpy(config->eth.subnet, subnet->valuestring, sizeof(config->eth.subnet) - 1);
                    cJSON *gateway = cJSON_GetObjectItem(eth_obj, "gateway");
                    if (gateway && gateway->valuestring) strncpy(config->eth.gateway, gateway->valuestring, sizeof(config->eth.gateway) - 1);
                }

                // Analisi config WiFi
                cJSON *wifi_obj = cJSON_GetObjectItem(root, "wifi");
                if (wifi_obj) {
                    config->wifi.sta_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "sta_enabled"));
                    config->wifi.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "dhcp_enabled"));
                    cJSON *ssid = cJSON_GetObjectItem(wifi_obj, "ssid");
                    if (ssid && ssid->valuestring) strncpy(config->wifi.ssid, ssid->valuestring, sizeof(config->wifi.ssid) - 1);
                    cJSON *password = cJSON_GetObjectItem(wifi_obj, "password");
                    if (password && password->valuestring) strncpy(config->wifi.password, password->valuestring, sizeof(config->wifi.password) - 1);
                    cJSON *ip = cJSON_GetObjectItem(wifi_obj, "ip");
                    if (ip && ip->valuestring) strncpy(config->wifi.ip, ip->valuestring, sizeof(config->wifi.ip) - 1);
                    cJSON *subnet = cJSON_GetObjectItem(wifi_obj, "subnet");
                    if (subnet && subnet->valuestring) strncpy(config->wifi.subnet, subnet->valuestring, sizeof(config->wifi.subnet) - 1);
                    cJSON *gateway = cJSON_GetObjectItem(wifi_obj, "gateway");
                    if (gateway && gateway->valuestring) strncpy(config->wifi.gateway, gateway->valuestring, sizeof(config->wifi.gateway) - 1);
                }

                // NTP enabled
                config->ntp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "ntp_enabled"));

                // Analisi config NTP
                cJSON *ntp_obj = cJSON_GetObjectItem(root, "ntp");
                if (ntp_obj) {
                    cJSON *server1 = cJSON_GetObjectItem(ntp_obj, "server1");
                    if (server1 && server1->valuestring) strncpy(config->ntp.server1, server1->valuestring, sizeof(config->ntp.server1) - 1);
                    cJSON *server2 = cJSON_GetObjectItem(ntp_obj, "server2");
                    if (server2 && server2->valuestring) strncpy(config->ntp.server2, server2->valuestring, sizeof(config->ntp.server2) - 1);
                    cJSON *tz_offset = cJSON_GetObjectItem(ntp_obj, "timezone_offset");
                    if (tz_offset && cJSON_IsNumber(tz_offset)) config->ntp.timezone_offset = tz_offset->valueint;
                }

                // Analisi config Server/Cloud
                cJSON *server_obj = cJSON_GetObjectItem(root, "server");
                if (server_obj) {
                    config->server.enabled = cJSON_IsTrue(cJSON_GetObjectItem(server_obj, "enabled"));
                    cJSON *url = cJSON_GetObjectItem(server_obj, "url");
                    if (url && url->valuestring) strncpy(config->server.url, url->valuestring, sizeof(config->server.url) - 1);
                    cJSON *serial = cJSON_GetObjectItem(server_obj, "serial");
                    if (serial && serial->valuestring) strncpy(config->server.serial, serial->valuestring, sizeof(config->server.serial) - 1);
                    cJSON *password = cJSON_GetObjectItem(server_obj, "password");
                    if (password && password->valuestring) strncpy(config->server.password, password->valuestring, sizeof(config->server.password) - 1);
                }

                // Analisi config Remote Logging
                cJSON *remote_log_obj = cJSON_GetObjectItem(root, "remote_log");
                if (remote_log_obj) {
                    cJSON *server_port = cJSON_GetObjectItem(remote_log_obj, "server_port");
                    if (server_port) config->remote_log.server_port = (uint16_t)server_port->valueint;
                    config->remote_log.use_broadcast = cJSON_IsTrue(cJSON_GetObjectItem(remote_log_obj, "use_broadcast"));
                }

                // Analisi config Sensori
                cJSON *sensors_obj = cJSON_GetObjectItem(root, "sensors");
                if (sensors_obj) {
                    config->sensors.io_expander_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "io_expander_enabled"));
                    config->sensors.temperature_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "temperature_enabled"));
                    config->sensors.led_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "led_enabled"));
                    cJSON *lc = cJSON_GetObjectItem(sensors_obj, "led_count");
                    if (lc) config->sensors.led_count = (uint32_t)lc->valueint;
                    config->sensors.rs232_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs232_enabled"));
                    config->sensors.rs485_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs485_enabled"));
                    config->sensors.mdb_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "mdb_enabled"));
                    config->sensors.cctalk_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "cctalk_enabled"));
                    config->sensors.pwm1_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm1_enabled"));
                    config->sensors.pwm2_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm2_enabled"));
                    config->sensors.sd_card_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "sd_card_enabled"));
                }

                // Analisi config Scanner USB
                cJSON *scanner_obj = cJSON_GetObjectItem(root, "scanner");
                if (scanner_obj) {
                    config->scanner.enabled = cJSON_IsTrue(cJSON_GetObjectItem(scanner_obj, "enabled"));
                    cJSON *vid = cJSON_GetObjectItem(scanner_obj, "vid");
                    cJSON *pid = cJSON_GetObjectItem(scanner_obj, "pid");
                    cJSON *dual = cJSON_GetObjectItem(scanner_obj, "dual_pid");
                    if (vid && cJSON_IsNumber(vid)) config->scanner.vid = (uint16_t)vid->valueint;
                    if (pid && cJSON_IsNumber(pid)) config->scanner.pid = (uint16_t)pid->valueint;
                    if (dual && cJSON_IsNumber(dual)) config->scanner.dual_pid = (uint16_t)dual->valueint;
                }

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
                    cJSON *enabled = cJSON_GetObjectItem(disp_obj, "enabled");
                    if (enabled) config->display.enabled = cJSON_IsTrue(enabled);
                    cJSON *bright = cJSON_GetObjectItem(disp_obj, "lcd_brightness");
                    if (bright) config->display.lcd_brightness = (uint8_t)bright->valueint;
                }

                // Analisi config Seriali (RS232, RS485, MDB)
                cJSON *rs232_obj = cJSON_GetObjectItem(root, "rs232");
                if (rs232_obj) {
                    config->rs232.baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "baud"));
                    config->rs232.data_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "data_bits"));
                    config->rs232.parity = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "parity"));
                    config->rs232.stop_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "stop_bits"));
                    config->rs232.rx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "rx_buf"));
                    config->rs232.tx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "tx_buf"));
                }
                cJSON *rs485_obj = cJSON_GetObjectItem(root, "rs485");
                if (rs485_obj) {
                    config->rs485.baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "baud"));
                    config->rs485.data_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "data_bits"));
                    config->rs485.parity = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "parity"));
                    config->rs485.stop_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "stop_bits"));
                    config->rs485.rx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "rx_buf"));
                    config->rs485.tx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "tx_buf"));
                }
                cJSON *mdb_s_obj = cJSON_GetObjectItem(root, "mdb_serial");
                if (mdb_s_obj) {
                    config->mdb_serial.baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "baud"));
                    config->mdb_serial.data_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "data_bits"));
                    config->mdb_serial.parity = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "parity"));
                    config->mdb_serial.stop_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "stop_bits"));
                    config->mdb_serial.rx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "rx_buf"));
                    config->mdb_serial.tx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "tx_buf"));
                }

                // Analisi GPIOs configurabili
                cJSON *gpios_obj = cJSON_GetObjectItem(root, "gpios");
                if (gpios_obj) {
                    cJSON *g33 = cJSON_GetObjectItem(gpios_obj, "gpio33");
                    if (g33) {
                        config->gpios.gpio33.mode = (device_gpio_cfg_mode_t)cJSON_GetNumberValue(cJSON_GetObjectItem(g33, "mode"));
                        config->gpios.gpio33.initial_state = cJSON_IsTrue(cJSON_GetObjectItem(g33, "state"));
                    }
                }

                // Analisi tabella UI (solo lingua corrente; i testi sono su SPIFFS)
                cJSON *ui_obj = cJSON_GetObjectItem(root, "ui");
                if (ui_obj) {
                    cJSON *language = cJSON_GetObjectItem(ui_obj, "language");
                    if (language && cJSON_IsString(language) && language->valuestring) {
                        strncpy(config->ui.language, language->valuestring, sizeof(config->ui.language) - 1);
                    }
                }

                cJSON *ui_lang_flat = cJSON_GetObjectItem(root, "ui_language");
                if (ui_lang_flat && cJSON_IsString(ui_lang_flat) && ui_lang_flat->valuestring) {
                    strncpy(config->ui.language, ui_lang_flat->valuestring, sizeof(config->ui.language) - 1);
                }

            cJSON_Delete(root);
            ESP_LOGD(TAG, "[C] Configurazione caricata correttamente da %s", source_is_eeprom ? "EEPROM" : "NVS");
        } else {
            ESP_LOGE(TAG, "[C] Errore parsing JSON!");
        }
        free(json_str);
    }

    return ESP_OK;
}

char* device_config_to_json(const device_config_t *config)
{
    if (!config) return NULL;

    cJSON *root = cJSON_CreateObject();

    // Updated flag
    cJSON_AddBoolToObject(root, "updated", config->updated);

    // Nome dispositivo
    cJSON_AddStringToObject(root, "device_name", config->device_name);

    // Ethernet
    cJSON *eth_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth_obj, "enabled", config->eth.enabled);
    cJSON_AddBoolToObject(eth_obj, "dhcp_enabled", config->eth.dhcp_enabled);
    cJSON_AddStringToObject(eth_obj, "ip", config->eth.ip);
    cJSON_AddStringToObject(eth_obj, "subnet", config->eth.subnet);
    cJSON_AddStringToObject(eth_obj, "gateway", config->eth.gateway);
    cJSON_AddItemToObject(root, "eth", eth_obj);

    // WiFi
    cJSON *wifi_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_obj, "sta_enabled", config->wifi.sta_enabled);
    cJSON_AddBoolToObject(wifi_obj, "dhcp_enabled", config->wifi.dhcp_enabled);
    cJSON_AddStringToObject(wifi_obj, "ssid", config->wifi.ssid);
    cJSON_AddStringToObject(wifi_obj, "password", config->wifi.password);
    cJSON_AddStringToObject(wifi_obj, "ip", config->wifi.ip);
    cJSON_AddStringToObject(wifi_obj, "subnet", config->wifi.subnet);
    cJSON_AddStringToObject(wifi_obj, "gateway", config->wifi.gateway);
    cJSON_AddItemToObject(root, "wifi", wifi_obj);

    // NTP
    cJSON_AddBoolToObject(root, "ntp_enabled", config->ntp_enabled);
    cJSON *ntp_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(ntp_obj, "server1", config->ntp.server1);
    cJSON_AddStringToObject(ntp_obj, "server2", config->ntp.server2);
    cJSON_AddNumberToObject(ntp_obj, "timezone_offset", config->ntp.timezone_offset);
    cJSON_AddItemToObject(root, "ntp", ntp_obj);

    // Server/Cloud
    cJSON *server_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(server_obj, "enabled", config->server.enabled);
    cJSON_AddStringToObject(server_obj, "url", config->server.url);
    cJSON_AddStringToObject(server_obj, "serial", config->server.serial);
    cJSON_AddStringToObject(server_obj, "password", config->server.password);
    cJSON_AddItemToObject(root, "server", server_obj);

    // Remote Logging
    cJSON *remote_log_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(remote_log_obj, "server_port", config->remote_log.server_port);
    cJSON_AddBoolToObject(remote_log_obj, "use_broadcast", config->remote_log.use_broadcast);
    cJSON_AddItemToObject(root, "remote_log", remote_log_obj);

    // Sensors
    cJSON *sensors_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors_obj, "io_expander_enabled", config->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors_obj, "temperature_enabled", config->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors_obj, "led_enabled", config->sensors.led_enabled);
    cJSON_AddNumberToObject(sensors_obj, "led_count", config->sensors.led_count);
    cJSON_AddBoolToObject(sensors_obj, "rs232_enabled", config->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors_obj, "rs485_enabled", config->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors_obj, "mdb_enabled", config->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors_obj, "cctalk_enabled", config->sensors.cctalk_enabled);
    cJSON_AddBoolToObject(sensors_obj, "pwm1_enabled", config->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors_obj, "pwm2_enabled", config->sensors.pwm2_enabled);
    cJSON_AddBoolToObject(sensors_obj, "sd_card_enabled", config->sensors.sd_card_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors_obj);

    // Scanner
    cJSON *scanner_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(scanner_obj, "enabled", config->scanner.enabled);
    cJSON_AddNumberToObject(scanner_obj, "vid", config->scanner.vid);
    cJSON_AddNumberToObject(scanner_obj, "pid", config->scanner.pid);
    cJSON_AddNumberToObject(scanner_obj, "dual_pid", config->scanner.dual_pid);
    cJSON_AddItemToObject(root, "scanner", scanner_obj);

    // MDB
    cJSON *mdb_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(mdb_obj, "coin_en", config->mdb.coin_acceptor_en);
    cJSON_AddBoolToObject(mdb_obj, "bill_en", config->mdb.bill_validator_en);
    cJSON_AddBoolToObject(mdb_obj, "cashless_en", config->mdb.cashless_en);
    cJSON_AddItemToObject(root, "mdb", mdb_obj);

    // Display
    cJSON *disp_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(disp_obj, "enabled", config->display.enabled);
    cJSON_AddNumberToObject(disp_obj, "lcd_brightness", config->display.lcd_brightness);
    cJSON_AddItemToObject(root, "display", disp_obj);

    // Seriale RS232
    cJSON *rs232_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs232_obj, "baud", config->rs232.baud_rate);
    cJSON_AddNumberToObject(rs232_obj, "data_bits", config->rs232.data_bits);
    cJSON_AddNumberToObject(rs232_obj, "parity", config->rs232.parity);
    cJSON_AddNumberToObject(rs232_obj, "stop_bits", config->rs232.stop_bits);
    cJSON_AddNumberToObject(rs232_obj, "rx_buf", config->rs232.rx_buf_size);
    cJSON_AddNumberToObject(rs232_obj, "tx_buf", config->rs232.tx_buf_size);
    cJSON_AddItemToObject(root, "rs232", rs232_obj);

    // Seriale RS485
    cJSON *rs485_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs485_obj, "baud", config->rs485.baud_rate);
    cJSON_AddNumberToObject(rs485_obj, "data_bits", config->rs485.data_bits);
    cJSON_AddNumberToObject(rs485_obj, "parity", config->rs485.parity);
    cJSON_AddNumberToObject(rs485_obj, "stop_bits", config->rs485.stop_bits);
    cJSON_AddNumberToObject(rs485_obj, "rx_buf", config->rs485.rx_buf_size);
    cJSON_AddNumberToObject(rs485_obj, "tx_buf", config->rs485.tx_buf_size);
    cJSON_AddItemToObject(root, "rs485", rs485_obj);

    // Seriale MDB
    cJSON *mdb_s_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(mdb_s_obj, "baud", config->mdb_serial.baud_rate);
    cJSON_AddNumberToObject(mdb_s_obj, "data_bits", config->mdb_serial.data_bits);
    cJSON_AddNumberToObject(mdb_s_obj, "parity", config->mdb_serial.parity);
    cJSON_AddNumberToObject(mdb_s_obj, "stop_bits", config->mdb_serial.stop_bits);
    cJSON_AddNumberToObject(mdb_s_obj, "rx_buf", config->mdb_serial.rx_buf_size);
    cJSON_AddNumberToObject(mdb_s_obj, "tx_buf", config->mdb_serial.tx_buf_size);
    cJSON_AddItemToObject(root, "mdb_serial", mdb_s_obj);

    // GPIOs configurabili
    cJSON *gpios_obj = cJSON_CreateObject();
    cJSON *g33_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(g33_obj, "mode", config->gpios.gpio33.mode);
    cJSON_AddBoolToObject(g33_obj, "state", config->gpios.gpio33.initial_state);
    cJSON_AddItemToObject(gpios_obj, "gpio33", g33_obj);
    cJSON_AddItemToObject(root, "gpios", gpios_obj);

    // UI multilingua (solo lingua; tabelle su SPIFFS per file lingua)
    cJSON *ui_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(ui_obj, "language", config->ui.language);
    cJSON_AddItemToObject(root, "ui", ui_obj);
    cJSON_AddStringToObject(root, "ui_language", config->ui.language);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

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

device_config_t* device_config_get(void)
{
    return &s_config;
}
uint32_t device_config_get_crc(void)
{
    char *json = device_config_to_json(&s_config);
    if (!json) return 0;
    uint32_t crc = _calculate_crc(json);
    free(json);
    return crc;
}

bool device_config_is_updated(void)
{
    return s_config.updated;
}
esp_err_t device_config_reset_defaults(void)
{
    _set_defaults(&s_config);
    return device_config_save(&s_config);
}

void device_config_reboot_factory(void)
{
    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory) {
        ESP_LOGI(TAG, "Impostazione partizione di boot: FACTORY");
        esp_ota_set_boot_partition(factory);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Partizione Factory non trovata!");
    }
}

void device_config_reboot_app(void)
{
    device_config_reboot_app_last();
}

void device_config_reboot_ota0(void)
{
    const esp_partition_t *ota0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (ota0) {
        ESP_LOGI(TAG, "Impostazione partizione di boot: OTA_0");
        esp_ota_set_boot_partition(ota0);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Partizione OTA_0 non trovata!");
    }
}

void device_config_reboot_ota1(void)
{
    const esp_partition_t *ota1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    if (ota1) {
        ESP_LOGI(TAG, "Impostazione partizione di boot: OTA_1");
        esp_ota_set_boot_partition(ota1);
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Partizione OTA_1 non trovata!");
    }
}

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
        esp_ota_set_boot_partition(target);
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
    return s_config.ui.language;
}

const char* device_config_get_ui_texts_json(void)
{
    return s_config.ui.texts_json;
}

char* device_config_get_ui_texts_records_json(const char *language)
{
    _ensure_default_i18n_it_file();

    const char *lang = _effective_lang(language);
    char *json = _read_i18n_file(lang);
    if (json) {
        return json;
    }

    if (strcmp(lang, UI_LANG_DEFAULT) != 0) {
        json = _read_i18n_file(UI_LANG_DEFAULT);
        if (json) {
            return json;
        }
    }

    return strdup(UI_TEXTS_DEFAULT_IT_JSON);
}

esp_err_t device_config_set_ui_texts_records_json(const char *language, const char *records_json)
{
    esp_err_t ret = _write_i18n_file(language, records_json);
    if (ret == ESP_OK) {
        _i18n_lookup_cache_clear();
    }
    return ret;
}

esp_err_t device_config_get_ui_text_scoped(const char *scope, const char *key, const char *fallback, char *out, size_t out_len)
{
    if (!scope || !key || !out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
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

esp_err_t device_config_get_ui_text(const char *key, const char *fallback, char *out, size_t out_len)
{
    if (!key || !out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *sep = strchr(key, '_');
    if (!sep || sep == key || *(sep + 1) == '\0') {
        if (fallback) {
            strncpy(out, fallback, out_len - 1);
            out[out_len - 1] = '\0';
        } else {
            out[0] = '\0';
        }
        return ESP_ERR_NOT_FOUND;
    }

    char scope[32] = {0};
    size_t scope_len = (size_t)(sep - key);
    if (scope_len >= sizeof(scope)) {
        scope_len = sizeof(scope) - 1;
    }
    memcpy(scope, key, scope_len);
    scope[scope_len] = '\0';
    const char *subkey = sep + 1;

    return device_config_get_ui_text_scoped(scope, subkey, fallback, out, out_len);
}
