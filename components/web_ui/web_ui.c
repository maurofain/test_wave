#include "web_ui.h"
#include "web_ui_internal.h"
#include "lvgl_panel.h"

#include "esp_log.h"
#include "esp_check.h"
#include "device_config.h"
#include "cJSON.h"
#include "fsm.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_partition.h"
#include "bsp/display.h"
#include "bsp/esp32_p4_nano.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include "usb_cdc_scanner.h"
#include "usb/usb_host.h"
#include "driver/uart.h"
#include "init.h"
#include "led.h"
#include "mdb_test.h"
#include <stdlib.h>
#include "app_version.h"
#include "serial_test.h"
#include "mdb.h"
#include "led_test.h"
#include "pwm_test.h"
#include "io_expander.h"
#include "io_expander_test.h"
#include "rs232.h"
#include "rs485.h"
#include "tasks.h"
#include "sd_card.h"
#include "aux_gpio.h"
#include "eeprom_test.h"
#include "sht40.h"
#include "http_services.h"
#include "web_ui_programs.h"
#include "digital_io.h"

/*
 * @file web_ui.c
 * @brief Implementazione centrale dei gestori HTTP e utilità della Web UI
 *
 * Questo file contiene la maggior parte dei percorsi HTML e API non
 * specificamente classificati in altri moduli. Include anche helpers per la
 * gestione della lingua, dei reboot, del caricamento file e altro.
 */

#if DNA_SYS_MONITOR == 0
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/task.h"
#endif

#define TAG "WEB_UI"
#define MAX_STORED_LOGS 100
#define I18N_V2_FILE_PATH "/spiffs/i18n_v2.json"

static void web_ui_lang_label_from_code(const char *lang, char *label, size_t label_len);

/**
 * @brief Restituisce la versione e data build correnti
 *
 * Endpoint: `GET /api/version`
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK dopo invio risposta JSON.
 */
esp_err_t api_version_get(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    cJSON_AddStringToObject(root, "version", APP_VERSION);

    cJSON_AddStringToObject(root, "build_timestamp", __DATE__ " " __TIME__);
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (!json_str) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    
    return ret;
}

/*
 * Forzatura temporanea: disabilita SEMPRE la parte video.
 *
 * 1 = headless forzato (ignora richieste web per abilitare display/LVGL)
 * 0 = comportamento normale da configurazione utente
 */
#define FORCE_VIDEO_DISABLED 0

#define WEB_UI_CCTALK_BAUD      9600
#define WEB_UI_CCTALK_DATA_BITS 8
#define WEB_UI_CCTALK_PARITY    0
#define WEB_UI_CCTALK_STOP_BITS 1
#define WEB_UI_CCTALK_RX_BUF    256
#define WEB_UI_CCTALK_TX_BUF    256
#define WEB_UI_MODBUS_MAX_POINTS 64
#define WEB_UI_SERIAL_BAUD_MIN 300
#define WEB_UI_SERIAL_BAUD_MAX 3000000
#define WEB_UI_SERIAL_DATA_BITS_MIN 5
#define WEB_UI_SERIAL_DATA_BITS_MAX 8
#define WEB_UI_SERIAL_PARITY_MIN 0
#define WEB_UI_SERIAL_PARITY_MAX 2
#define WEB_UI_SERIAL_STOP_BITS_MIN 1
#define WEB_UI_SERIAL_STOP_BITS_MAX 2
#define WEB_UI_SERIAL_RX_BUF_MIN 64
#define WEB_UI_SERIAL_BUF_MAX 65536

static const device_serial_config_t s_web_ui_serial_default_rs232 = {
    .baud_rate = 9600,
    .data_bits = 8,
    .parity = 0,
    .stop_bits = 1,
    .rx_buf_size = 2048,
    .tx_buf_size = 0,
};

static const device_serial_config_t s_web_ui_serial_default_rs485 = {
    .baud_rate = 9600,
    .data_bits = 8,
    .parity = 0,
    .stop_bits = 1,
    .rx_buf_size = 2048,
    .tx_buf_size = 0,
};

static const device_serial_config_t s_web_ui_serial_default_mdb = {
    .baud_rate = 9600,
    .data_bits = 8,
    .parity = 0,
    .stop_bits = 1,
    .rx_buf_size = 1024,
    .tx_buf_size = 1024,
};

static uint8_t web_ui_sanitize_touch_input_id(int value)
{
    if (value < (int)DEVICE_TOUCH_INPUT_MIN || value > (int)DEVICE_TOUCH_INPUT_MAX) {
        return DEVICE_TOUCH_BUTTON_UNASSIGNED;
    }
    if (!digital_io_input_is_touch_mappable((uint8_t)value)) {
        return DEVICE_TOUCH_BUTTON_UNASSIGNED;
    }
    return (uint8_t)value;
}

static void web_ui_add_touch_map_to_json(cJSON *root, const device_config_t *cfg)
{
    if (!root || !cfg) {
        return;
    }

    cJSON *touch_map = cJSON_CreateObject();
    cJSON *buttons = cJSON_CreateArray();
    if (!touch_map || !buttons) {
        cJSON_Delete(touch_map);
        cJSON_Delete(buttons);
        return;
    }

    for (size_t button_index = 0; button_index < DEVICE_TOUCH_BUTTON_MAX; ++button_index) {
        uint8_t mapped_input = cfg->touch_button_map.button_to_input[button_index];
        mapped_input = web_ui_sanitize_touch_input_id((int)mapped_input);
        cJSON_AddItemToArray(buttons, cJSON_CreateNumber(mapped_input));
    }

    cJSON_AddItemToObject(touch_map, "buttons", buttons);
    cJSON_AddNumberToObject(touch_map, "max_buttons", DEVICE_TOUCH_BUTTON_MAX);
    cJSON_AddNumberToObject(touch_map, "input_min", DEVICE_TOUCH_INPUT_MIN);
    cJSON_AddNumberToObject(touch_map, "input_max", DEVICE_TOUCH_INPUT_MAX);
    cJSON_AddItemToObject(root, "touch_map", touch_map);
}

static void web_ui_add_digital_inputs_to_json(cJSON *root)
{
    if (!root) {
        return;
    }

    cJSON *inputs = cJSON_CreateArray();
    if (!inputs) {
        return;
    }

    digital_io_input_info_t infos[DIGITAL_IO_INPUT_COUNT] = {0};
    size_t count = digital_io_get_input_infos(infos, DIGITAL_IO_INPUT_COUNT);
    for (size_t index = 0; index < count; ++index) {
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            continue;
        }

        cJSON_AddNumberToObject(item, "id", infos[index].input_id);
        cJSON_AddStringToObject(item, "code", infos[index].code);
        cJSON_AddBoolToObject(item, "available", infos[index].available);
        cJSON_AddBoolToObject(item, "is_local", infos[index].is_local);
        cJSON_AddBoolToObject(item, "touch_mappable", infos[index].touch_mappable);
        cJSON_AddBoolToObject(item, "io_process_consumer", infos[index].io_process_consumer);
        cJSON_AddItemToArray(inputs, item);
    }

    cJSON_AddItemToObject(root, "digital_io_inputs", inputs);
}

static void web_ui_parse_touch_map_from_json(cJSON *root, device_config_t *cfg)
{
    if (!root || !cfg) {
        return;
    }

    cJSON *touch_map = cJSON_GetObjectItem(root, "touch_map");
    if (!touch_map) {
        touch_map = cJSON_GetObjectItem(root, "touch_button_map");
    }
    if (!touch_map || !cJSON_IsObject(touch_map)) {
        return;
    }

    cJSON *buttons = cJSON_GetObjectItem(touch_map, "buttons");
    if (!buttons || !cJSON_IsArray(buttons)) {
        return;
    }

    int count = cJSON_GetArraySize(buttons);
    for (size_t button_index = 0; button_index < DEVICE_TOUCH_BUTTON_MAX; ++button_index) {
        uint8_t mapped_input = DEVICE_TOUCH_BUTTON_UNASSIGNED;
        if ((int)button_index < count) {
            cJSON *item = cJSON_GetArrayItem(buttons, (int)button_index);
            if (cJSON_IsNumber(item)) {
                mapped_input = web_ui_sanitize_touch_input_id(item->valueint);
            } else if (cJSON_IsString(item) && item->valuestring) {
                mapped_input = web_ui_sanitize_touch_input_id((int)strtoul(item->valuestring, NULL, 10));
            }
        }
        cfg->touch_button_map.button_to_input[button_index] = mapped_input;
    }
}

typedef void (*lvgl_test_page_fn_t)(void);

static void lvgl_panel_show_out_of_service_test(void)
{
    lvgl_panel_show_out_of_service(0);
}

static const struct {
    const char *name;
    lvgl_test_page_fn_t fn;
} s_lvgl_test_pages[] = {
    {"boot_logo", lvgl_panel_show_boot_logo},
    {"main", lvgl_panel_show_main_page},
    {"ads", lvgl_panel_show_ads_page},
    {"language", lvgl_panel_show_language_select},
    {"out_of_service", lvgl_panel_show_out_of_service_test},
};

static lvgl_test_page_fn_t find_lvgl_test_page(const char *name)
{
    if (!name) {
        return NULL;
    }
    for (size_t i = 0; i < sizeof(s_lvgl_test_pages) / sizeof(s_lvgl_test_pages[0]); ++i) {
        if (strcmp(s_lvgl_test_pages[i].name, name) == 0) {
            return s_lvgl_test_pages[i].fn;
        }
    }
    return NULL;
}

static bool wait_for_fsm_state(fsm_state_t desired, TickType_t timeout_ticks)
{
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        fsm_ctx_t snap = {0};
        if (fsm_runtime_snapshot(&snap) && snap.state == desired) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return false;
}

static esp_err_t lvgl_test_show_page(const char *page)
{
    lvgl_test_page_fn_t fn = find_lvgl_test_page(page);
    if (!fn) {
        ESP_LOGW(TAG, "[C] LVGL test: page '%s' non trovata", page ? page : "NULL");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "[C] LVGL test: richiesta page=%s", page);

    if (!fsm_enter_lvgl_pages_test()) {
        ESP_LOGE(TAG, "[C] LVGL test: impossibile pubblicare evento ENTER");
        return ESP_FAIL;
    }

    if (!wait_for_fsm_state(FSM_STATE_LVGL_PAGES_TEST, pdMS_TO_TICKS(500))) {
        ESP_LOGE(TAG, "[C] LVGL test: timeout in attesa di stato TEST");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "[C] LVGL test: stato TEST attivo, mostro pagina %s", page);
    fn();
    ESP_LOGI(TAG, "[C] LVGL test: pagina %s mostrata", page);
    return ESP_OK;
}

esp_err_t api_display_lvgl_test(httpd_req_t *req)
{
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"Body required\"}", -1);
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"Invalid JSON\"}", -1);
    }

    const cJSON *action = cJSON_GetObjectItem(root, "action");
    const cJSON *page = cJSON_GetObjectItem(root, "page");
    char page_name[32] = {0};
    if (page && cJSON_IsString(page) && page->valuestring) {
        snprintf(page_name, sizeof(page_name), "%s", page->valuestring);
    }
    esp_err_t err = ESP_OK;

    if (action && cJSON_IsString(action) && strcasecmp(action->valuestring, "reboot") == 0) {
        ESP_LOGI(TAG, "[C] LVGL test: richiesta reboot dal web UI");
        fsm_exit_lvgl_pages_test();
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"rebooting\"}", -1);
        vTaskDelay(pdMS_TO_TICKS(250));
        esp_restart();
        return ESP_OK;
    }

    if (page_name[0] == '\0') {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"Missing page\"}", -1);
    }

    err = lvgl_test_show_page(page_name);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[C] LVGL test: comando page=%s completato", page_name);
        httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
    } else if (err == ESP_ERR_NOT_FOUND) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_send(req, "{\"error\":\"Page not found\"}", -1);
    } else {
        ESP_LOGE(TAG, "[C] LVGL test: comando page=%s fallito (err=%d)", page_name, (int)err);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"Failed\"}", -1);
    }
    return ESP_OK;
}


/**
 * @brief Legge un intero da un oggetto JSON con chiave primaria/legacy.
 *
 * @param [in] obj Oggetto JSON sorgente.
 * @param [in] primary_key Chiave principale.
 * @param [in] legacy_key Chiave legacy opzionale.
 * @param [out] out_value Valore estratto.
 * @return true Se il campo è presente e numerico.
 * @return false Altrimenti.
 */
static bool web_ui_json_read_int(cJSON *obj,
                                 const char *primary_key,
                                 const char *legacy_key,
                                 int *out_value)
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
 * @brief Valida un valore intero e applica fallback se fuori range.
 *
 * @param [in] value Valore da validare.
 * @param [in] min_value Limite minimo incluso.
 * @param [in] max_value Limite massimo incluso.
 * @param [in] default_value Fallback se valore non valido.
 * @return int Valore finale validato.
 */
static int web_ui_clamp_int_or_default(int value, int min_value, int max_value, int default_value)
{
    if (value < min_value || value > max_value) {
        return default_value;
    }
    return value;
}


/**
 * @brief Applica parsing robusto della configurazione seriale da JSON.
 *
 * @param [in] serial_obj Oggetto JSON con campi seriali.
 * @param [in/out] target Destinazione da aggiornare.
 * @param [in] defaults Valori fallback per campi invalidi.
 */
static void web_ui_apply_serial_cfg_from_json(cJSON *serial_obj,
                                              device_serial_config_t *target,
                                              const device_serial_config_t *defaults)
{
    if (!serial_obj || !target || !defaults) {
        return;
    }

    int value = 0;

    if (web_ui_json_read_int(serial_obj, "baud", "baud_rate", &value)) {
        target->baud_rate = web_ui_clamp_int_or_default(value,
                                                        WEB_UI_SERIAL_BAUD_MIN,
                                                        WEB_UI_SERIAL_BAUD_MAX,
                                                        defaults->baud_rate);
    }
    if (web_ui_json_read_int(serial_obj, "data", "data_bits", &value)) {
        target->data_bits = web_ui_clamp_int_or_default(value,
                                                        WEB_UI_SERIAL_DATA_BITS_MIN,
                                                        WEB_UI_SERIAL_DATA_BITS_MAX,
                                                        defaults->data_bits);
    }
    if (web_ui_json_read_int(serial_obj, "par", "parity", &value)) {
        target->parity = web_ui_clamp_int_or_default(value,
                                                     WEB_UI_SERIAL_PARITY_MIN,
                                                     WEB_UI_SERIAL_PARITY_MAX,
                                                     defaults->parity);
    }
    if (web_ui_json_read_int(serial_obj, "stop", "stop_bits", &value)) {
        target->stop_bits = web_ui_clamp_int_or_default(value,
                                                        WEB_UI_SERIAL_STOP_BITS_MIN,
                                                        WEB_UI_SERIAL_STOP_BITS_MAX,
                                                        defaults->stop_bits);
    }
    if (web_ui_json_read_int(serial_obj, "rx_buf", "rx_buf_size", &value)) {
        target->rx_buf_size = web_ui_clamp_int_or_default(value,
                                                          WEB_UI_SERIAL_RX_BUF_MIN,
                                                          WEB_UI_SERIAL_BUF_MAX,
                                                          defaults->rx_buf_size);
    }
    if (web_ui_json_read_int(serial_obj, "tx_buf", "tx_buf_size", &value)) {
        target->tx_buf_size = web_ui_clamp_int_or_default(value,
                                                          0,
                                                          WEB_UI_SERIAL_BUF_MAX,
                                                          defaults->tx_buf_size);
    }
}

/* Log store e handler spostati in components/web_ui/web_ui_logs.c */



/* Handle del server spostato in web_ui_server.c */

// Elementi HTML comuni
#include "web_ui_internal.h" /* spostato in web_ui_common.c */
// Handle dei task di test per Serial Blink Test
TaskHandle_t s_rs232_test_handle = NULL;
TaskHandle_t s_rs485_test_handle = NULL;
extern bool dump_cctalk_log;

/**
 * @brief Estrae il codice lingua da un nome file i18n_<lang>.json
 *
 * Controlla formato e ritorna il codice in `out_lang` normalizzato.
 *
 * @param name nome file (es. "i18n_it.json")
 * @param out_lang buffer output
 * @param out_len dimensione del buffer
 * @return true se estrazione riuscita
 */
static bool web_ui_extract_lang_from_filename(const char *name, char *out_lang, size_t out_len)
{
    if (!name || !out_lang || out_len < 3) {
        return false;
    }

    const char *prefix = "i18n_";
    const char *suffix = ".json";
    const size_t prefix_len = 5;
    const size_t suffix_len = 5;
    const size_t name_len = strlen(name);
    if (name_len <= (prefix_len + suffix_len)) {
        return false;
    }
    if (strncmp(name, prefix, prefix_len) != 0) {
        return false;
    }
    if (strcmp(name + name_len - suffix_len, suffix) != 0) {
        return false;
    }

    const size_t lang_len = name_len - prefix_len - suffix_len;
    if (lang_len < 2 || lang_len >= out_len || lang_len > 7) {
        return false;
    }

    for (size_t i = 0; i < lang_len; ++i) {
        char c = name[prefix_len + i];
        bool valid = ((c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') ||
                      c == '_' || c == '-');
        if (!valid) {
            return false;
        }
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        out_lang[i] = c;
    }
    out_lang[lang_len] = '\0';
    return true;
}

/**
 * @brief Verifica che il codice lingua abbia un formato plausibile.
 *
 * Accetta codici come `it`, `en`, `pt-br` e rifiuta artefatti come `v2`.
 */
static bool web_ui_is_valid_lang_code(const char *lang)
{
    if (!lang) {
        return false;
    }

    size_t len = strlen(lang);
    if (len < 2 || len > 7) {
        return false;
    }

    size_t letters = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = lang[i];
        bool is_lower = (c >= 'a' && c <= 'z');
        bool is_sep = (c == '-' || c == '_');
        if (!is_lower && !is_sep) {
            return false;
        }
        if (is_lower) {
            ++letters;
        }
        if (is_sep && (i == 0 || i == (len - 1))) {
            return false;
        }
    }

    return letters >= 2;
}

/**
 * @brief Aggiunge una lingua alla risposta JSON evitando duplicati.
 */
static bool web_ui_append_language_item(cJSON *languages,
                                        const char *lang,
                                        const char *file_path,
                                        bool *has_it)
{
    if (!languages || !lang || !file_path || !web_ui_is_valid_lang_code(lang)) {
        return false;
    }

    int count = cJSON_GetArraySize(languages);
    for (int i = 0; i < count; ++i) {
        cJSON *it = cJSON_GetArrayItem(languages, i);
        cJSON *code = cJSON_GetObjectItem(it, "code");
        if (cJSON_IsString(code) && code->valuestring && strcmp(code->valuestring, lang) == 0) {
            return false;
        }
    }

    char label[32] = {0};
    web_ui_lang_label_from_code(lang, label, sizeof(label));

    cJSON *item = cJSON_CreateObject();
    if (!item) {
        return false;
    }

    cJSON_AddStringToObject(item, "code", lang);
    cJSON_AddStringToObject(item, "label", label);
    cJSON_AddStringToObject(item, "file", file_path);
    cJSON_AddItemToArray(languages, item);

    if (has_it && strcmp(lang, "it") == 0) {
        *has_it = true;
    }

    return true;
}

/**
 * @brief Carica la lista lingue dal catalogo unico i18n_v2.json.
 */
static bool web_ui_append_languages_from_i18n_v2(cJSON *languages, bool *has_it)
{
    if (!languages) {
        return false;
    }

    FILE *f = fopen(I18N_V2_FILE_PATH, "r");
    if (!f) {
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        ESP_LOGW(TAG, "[C] Impossibile posizionarsi su %s", I18N_V2_FILE_PATH);
        return false;
    }

    long file_size = ftell(f);
    if (file_size <= 0) {
        fclose(f);
        ESP_LOGW(TAG, "[C] File i18n vuoto o non leggibile: %s", I18N_V2_FILE_PATH);
        return false;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        ESP_LOGW(TAG, "[C] Impossibile riavvolgere %s", I18N_V2_FILE_PATH);
        return false;
    }

    char *json = (char *)malloc((size_t)file_size + 1U);
    if (!json) {
        fclose(f);
        ESP_LOGE(TAG, "[C] Memoria insufficiente per leggere %s", I18N_V2_FILE_PATH);
        return false;
    }

    size_t read_size = fread(json, 1, (size_t)file_size, f);
    fclose(f);
    json[read_size] = '\0';
    if (read_size != (size_t)file_size) {
        free(json);
        ESP_LOGW(TAG, "[C] Lettura incompleta del catalogo %s", I18N_V2_FILE_PATH);
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        ESP_LOGW(TAG, "[C] Catalogo i18n non valido: %s", I18N_V2_FILE_PATH);
        return false;
    }

    bool added_any = false;
    cJSON *langs = cJSON_GetObjectItemCaseSensitive(root, "languages");
    if (cJSON_IsArray(langs)) {
        cJSON *lang = NULL;
        cJSON_ArrayForEach(lang, langs) {
            if (!cJSON_IsString(lang) || !lang->valuestring) {
                continue;
            }
            if (web_ui_append_language_item(languages, lang->valuestring, I18N_V2_FILE_PATH, has_it)) {
                added_any = true;
            }
        }
    }

    cJSON_Delete(root);
    return added_any;
}

/**
 * @brief Fallback legacy: estrae le lingue da file i18n_<lang>.json.
 */
static void web_ui_append_languages_from_legacy_scan(cJSON *languages, bool *has_it)
{
    DIR *dir = opendir("/spiffs");
    if (!dir) {
        ESP_LOGW(TAG, "[C] Impossibile aprire /spiffs per scansione lingue");
        return;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        char lang[8] = {0};
        if (!web_ui_extract_lang_from_filename(entry->d_name, lang, sizeof(lang))) {
            continue;
        }
        if (!web_ui_is_valid_lang_code(lang)) {
            continue;
        }

        char file_path[320] = {0};
        snprintf(file_path, sizeof(file_path), "/spiffs/%s", entry->d_name);
        web_ui_append_language_item(languages, lang, file_path, has_it);
    }

    closedir(dir);
}

/**
 * @brief Converte un codice lingua in etichetta leggibile
 *
 * Ad esempio "it" => "Italiano (IT)"; usa l'upper-case per codici
 * non riconosciuti.
 */
static void web_ui_lang_label_from_code(const char *lang, char *label, size_t label_len)
{
    if (!label || label_len == 0) {
        return;
    }
    if (!lang || !lang[0]) {
        snprintf(label, label_len, "Unknown");
        return;
    }

    if (strcmp(lang, "it") == 0) {
        snprintf(label, label_len, "Italiano (IT)");
    } else if (strcmp(lang, "en") == 0) {
        snprintf(label, label_len, "English (EN)");
    } else if (strcmp(lang, "fr") == 0) {
        snprintf(label, label_len, "Français (FR)");
    } else if (strcmp(lang, "de") == 0) {
        snprintf(label, label_len, "Deutsch (DE)");
    } else if (strcmp(lang, "es") == 0) {
        snprintf(label, label_len, "Español (ES)");
    } else {
        char upper[16] = {0};
        size_t n = strlen(lang);
        if (n >= sizeof(upper)) {
            n = sizeof(upper) - 1;
        }
        for (size_t i = 0; i < n; ++i) {
            char c = lang[i];
            if (c >= 'a' && c <= 'z') {
                c = (char)(c - 'a' + 'A');
            }
            upper[i] = c;
        }
        upper[n] = '\0';
        snprintf(label, label_len, "%s", upper);
    }
}

// TEST UART: 0x55, 0xAA, 0x01, 0x07 (periodico)
/**
 * @brief Task di prova per invio sequenza periodica su UART
 *
 * Usato dai comandi di test RS232/RS485 per generare traffico e monitorarlo
 * via serial_test.
 *
 * @param arg porta UART (CONFIG_APP_RS232_UART_PORT o RS485)
 */
void uart_test_task(void *arg) {
    uart_port_t port = (uart_port_t)(intptr_t)arg;
    const char* seq_hex[] = {"\\0x55", "\\0xAA", "\\0x01", "\\0x07"};
    uint8_t rx_buf[128];
    size_t rx_len;
    ESP_LOGI(TAG, "Test Porta UART %d: Avvio (TX + RX Monitor HEX)", port);
    
    serial_test_init();
    
    while(1) {
        for (int i=0; i<4; i++) {
            // Invio tramite componente serial_test per avere visibilità anche del TX nel monitor
            ESP_LOGI(TAG,"Invio test RS485");
            serial_test_send_uart(port, seq_hex[i]);
            
            // Monitoraggio ricezione per circa 1 secondo tra un invio e l'altro
            for (int j=0; j<10; j++) { /* j<10*/
                // serial_test_read_uart ha un timeout interno di 50ms e logga automaticamente in HEX (prefix RX<)
                if (serial_test_read_uart(port, rx_buf, sizeof(rx_buf), &rx_len) == ESP_OK) {
                    ESP_LOGD(TAG, "UART %d ricevuti %d bytes", port, (int)rx_len);
                }
                vTaskDelay(pdMS_TO_TICKS(50)); /*MF*/
            }
        }
    }
}

// --- GESTORI (HANDLERS) ---

/* Helpers `ip_to_str` and `perform_ota` are implemented in `web_ui_common.c` (shared helpers). */

/**
 * @brief Handler HTTP per riavviare in modalità factory
 *
 * Protegge con password e mostra una pagina di attesa prima del reboot.
 */
esp_err_t reboot_factory_handler(httpd_req_t *req)
{
    if (!web_ui_has_valid_password(req)) {
        return web_ui_send_password_required(req, "Reboot Factory", "/reboot/factory");
    }

    httpd_resp_sendstr(req, "<html><body><h1>Reboot in Factory Mode...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_factory();
    return ESP_OK;
}

/**
 * @brief Handler per riavvio in modalità applicazione
 */
esp_err_t reboot_app_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in Production Mode...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_app();
    return ESP_OK;
}

/**
 * @brief Handler per riavvio sulla partizione "app last"
 */
esp_err_t reboot_app_last_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in App Last...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_app_last();
    return ESP_OK;
}

/**
 * @brief Handler per riavvio sulla partizione OTA0
 */
esp_err_t reboot_ota0_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in OTA0...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_ota0();
    return ESP_OK;
}

/**
 * @brief Handler per riavvio sulla partizione OTA1
 */
esp_err_t reboot_ota1_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in OTA1...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_ota1();
    return ESP_OK;
}

/**
 * @brief Registro un URI solo se non è già presente
 *
 * Evita errori quando si re-invoca la registrazione più volte (es. dopo
 * abilitazione dinamica di feature). Logga in caso di fallimento diverso da
 * già esistente.
 */
static esp_err_t register_uri_if_missing(httpd_handle_t server, httpd_uri_t *uri)
{
    esp_err_t err = httpd_register_uri_handler(server, uri);
    if (err == ESP_OK || err == ESP_ERR_HTTPD_HANDLER_EXISTS) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Errore registrazione endpoint runtime %s: %s", uri->uri, esp_err_to_name(err));
    return err;
}

/**
 * @brief Registra gli endpoint "factory-runtime" quando richiesto
 *
 * Questi URI vengono abilitati solo se un manutentore abilitato chiama
 * `/maintainer/enable` o tramite override intenzionale. Comprende task,
 * test, ecc.
 */
static esp_err_t web_ui_register_factory_runtime_endpoints(void)
{
    httpd_handle_t server = web_ui_get_server_handle();
    if (!server) {
        return ESP_ERR_INVALID_STATE;
    }

    httpd_uri_t uri_tasks = {.uri = "/tasks", .method = HTTP_GET, .handler = tasks_page_handler};
    ESP_RETURN_ON_ERROR(register_uri_if_missing(server, &uri_tasks), TAG, "register /tasks");

    httpd_uri_t uri_api_tasks = {.uri = "/api/tasks", .method = HTTP_GET, .handler = api_tasks_get};
    ESP_RETURN_ON_ERROR(register_uri_if_missing(server, &uri_api_tasks), TAG, "register /api/tasks");

    httpd_uri_t uri_api_tasks_save = {.uri = "/api/tasks/save", .method = HTTP_POST, .handler = api_tasks_save};
    ESP_RETURN_ON_ERROR(register_uri_if_missing(server, &uri_api_tasks_save), TAG, "register /api/tasks/save");

    httpd_uri_t uri_api_tasks_apply = {.uri = "/api/tasks/apply", .method = HTTP_POST, .handler = api_tasks_apply};
    ESP_RETURN_ON_ERROR(register_uri_if_missing(server, &uri_api_tasks_apply), TAG, "register /api/tasks/apply");

    httpd_uri_t uri_test = {.uri = "/test", .method = HTTP_GET, .handler = test_page_handler};
    ESP_RETURN_ON_ERROR(register_uri_if_missing(server, &uri_test), TAG, "register /test");

    httpd_uri_t uri_api_test = {.uri = "/api/test/*", .method = HTTP_POST, .handler = api_test_handler};
    ESP_RETURN_ON_ERROR(register_uri_if_missing(server, &uri_api_test), TAG, "register /api/test/*");

    httpd_uri_t uri_api_version = {.uri = "/api/version", .method = HTTP_GET, .handler = api_version_get};
    ESP_RETURN_ON_ERROR(register_uri_if_missing(server, &uri_api_version), TAG, "register /api/version");

    return ESP_OK;
}

/**
 * @brief Handler che attiva le feature di manutenzione (factory) in runtime
 *
 * Richiede password. Dopo l'attivazione registra gli endpoint aggiuntivi
 * e mostra un messaggio di conferma.
 */
esp_err_t maintainer_enable_handler(httpd_req_t *req)
{
    if (!web_ui_has_valid_password(req)) {
        return web_ui_send_password_required(req, "Mantainer", "/maintainer/enable");
    }

    web_ui_factory_features_override_set(true);
    esp_err_t reg_err = web_ui_register_factory_runtime_endpoints();
    if (reg_err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_send(req, "Mantainer attivato, ma registrazione endpoint runtime fallita", -1);
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_sendstr(req,
        "<html><body><h1>Mantainer attivato</h1>"
        "<p>Funzionalita Factory abilitate in runtime su app OTA.</p>"
        "<p><a href='/'>Torna alla Home</a></p>"
        "<script>setTimeout(function(){window.location.href='/'},800);</script>"
        "</body></html>");
}


// Handler per la pagina OTA
/**
 * @brief Risponde alla GET /ota mostrando il form HTML per l'aggiornamento
 *
 * Il form utilizza Javascript per inviare un POST binario a `/ota/upload` e
 * mostrare la progress bar.
 */
esp_err_t ota_get_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "ota.html", "text/html; charset=utf-8");
}

// Handler per l'upload OTA (POST)
/**
 * @brief Riceve un firmware binario via POST e lo scrive sulla partizione OTA
 *
 * Il body deve essere raw (`application/octet-stream`). Gestisce stream
 * e verifica CRC tramite API esp_ota_*; al termine imposta la partizione
 * di boot ed esegue `esp_restart()`.
 */
esp_err_t ota_upload_handler(httpd_req_t *req)
{
    char content_type[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) == ESP_OK) {
        if (strstr(content_type, "multipart/form-data") != NULL) {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_send(req, "Inviare binario raw (application/octet-stream), non multipart/form-data", -1);
            return ESP_FAIL;
        }
    }

    if (req->content_len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Payload OTA vuoto", -1);
        return ESP_FAIL;
    }

    const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
    if (!p) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_ota_handle_t h;
    esp_err_t err = esp_ota_begin(p, OTA_SIZE_UNKNOWN, &h);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Errore avvio OTA", -1);
        return ESP_FAIL;
    }

    char b[1024]; int rem = req->content_len;
    while (rem > 0) {
        int n = httpd_req_recv(req, b, MIN(rem, 1024));
        if (n <= 0) {
            esp_ota_abort(h);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Errore ricezione dati OTA", -1);
            return ESP_FAIL;
        }

        err = esp_ota_write(h, b, n);
        if (err != ESP_OK) {
            esp_ota_abort(h);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Errore scrittura OTA", -1);
            return ESP_FAIL;
        }
        rem -= n;
    }

    err = esp_ota_end(h);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Immagine OTA non valida", -1);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(p);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Errore impostazione boot partition", -1);
        return ESP_FAIL;
    }

    httpd_resp_send(req, "OTA completato con successo, riavvio in corso...", -1);
    vTaskDelay(pdMS_TO_TICKS(1000)); esp_restart();
    return ESP_OK;
}

// Handler per l'URL OTA (POST)
/**
 * @brief Avvia l'OTA scaricando un file remoto
 *
 * Attende un query parameter `url` e richiama `perform_ota`.
 */
esp_err_t ota_post_handler(httpd_req_t *req)
{
    char q[256], u[200] = {0};
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) httpd_query_key_value(q, "url", u, sizeof(u));
    if (strlen(u) == 0) return ESP_FAIL;
    perform_ota(u);
    return ESP_OK;
}

// Handler per errore 404
/**
 * @brief Risponde alle richieste non gestite con 404
 *
 * Questo handler viene registrato come default in httpd_config_t.
 */
esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t error)
{
    (void)error;
    if (req) {
        const char *uri = req->uri;
        if (strncmp(uri, "/js/", 4) == 0 || strncmp(uri, "/css/", 5) == 0) {
            const char *relative_path = uri + 1;
            esp_err_t ret = webpages_try_send_external(req, relative_path, NULL);
            if (ret == ESP_OK) {
                return ESP_OK;
            }
        }
    }
    httpd_resp_set_status(req, "404 Non Trovato");
    return httpd_resp_send(req, "404 Non Trovato", -1);
}

// Handler per pagina configurazione
/**
 * @brief Genera l'interfaccia HTML `/config` per tutte le impostazioni del dispositivo
 *
 * La pagina mostra campi input, toggle e sezioni collassabili. I valori vengono
 * caricati/salvati via API `/api/config`.
 */
esp_err_t config_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "config.html", "text/html; charset=utf-8");
}

// Handler API GET /api/debug/usb/enumerate

/**
 * @brief Abilita l'enumerazione USB.
 *
 * Questa funzione abilita l'enumerazione USB tramite la richiesta HTTP fornita.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_debug_usb_enumerate(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "[C] GET /api/debug/usb/enumerate");
    uint8_t addr_list[16];
    int num_devs = 0;
    esp_err_t err = usb_host_device_addr_list_fill(sizeof(addr_list), addr_list, &num_devs);

    cJSON *root = cJSON_CreateObject();
    if (err != ESP_OK) {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(err));
    } else {
        cJSON_AddNumberToObject(root, "count", num_devs);
        cJSON *addrs = cJSON_CreateArray();
        for (int i = 0; i < num_devs; ++i) {
            cJSON_AddItemToArray(addrs, cJSON_CreateNumber(addr_list[i]));
        }
        cJSON_AddItemToObject(root, "addresses", addrs);

        usb_host_client_handle_t usb_client = NULL;
        const usb_host_client_config_t client_config = {
            .is_synchronous = false,
            .max_num_event_msg = 3,
            .async = { .client_event_callback = NULL, .callback_arg = NULL }
        };
        if (usb_host_client_register(&client_config, &usb_client) == ESP_OK) {
            cJSON *devices = cJSON_CreateArray();
            for (int i = 0; i < num_devs; ++i) {
                usb_device_handle_t dev_hdl;
                if (usb_host_device_open(usb_client, addr_list[i], &dev_hdl) == ESP_OK) {
                    const usb_device_desc_t *device_desc = NULL;
                    if (usb_host_get_device_descriptor(dev_hdl, &device_desc) == ESP_OK && device_desc) {
                        cJSON *d = cJSON_CreateObject();
                        cJSON_AddNumberToObject(d, "addr", addr_list[i]);
                        cJSON_AddNumberToObject(d, "vid", device_desc->idVendor);
                        cJSON_AddNumberToObject(d, "pid", device_desc->idProduct);
                        cJSON_AddNumberToObject(d, "class", device_desc->bDeviceClass);
                        cJSON_AddItemToArray(devices, d);
                    }
                    usb_host_device_close(usb_client, dev_hdl);
                }
            }
            cJSON_AddItemToObject(root, "devices", devices);
            usb_host_client_deregister(usb_client);
        } else {
            cJSON_AddStringToObject(root, "client", "register_failed");
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Sperimentali: POST /api/debug/usb/restart -> forces bsp_usb_host_stop + bsp_usb_host_start

/**
 * @brief Riavvia il dispositivo USB.
 *
 * Questa funzione riavvia il dispositivo USB in risposta a una richiesta HTTP.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_debug_usb_restart(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "[C] POST /api/debug/usb/restart (Sperimentali)");
#ifdef CONFIG_USB_OTG_SUPPORTED
    web_ui_add_log("INFO", "USB_DBG", "[Sperimentali] Restarting USB Host via API");
    esp_err_t err = bsp_usb_host_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "bsp_usb_host_stop returned %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    err = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "bsp_usb_host_start returned %s", esp_err_to_name(err));
        httpd_resp_sendstr(req, "restart_failed");
    } else {
        httpd_resp_sendstr(req, "ok");
    }
#else
    httpd_resp_sendstr(req, "usb_not_supported");
#endif
    return ESP_OK;
}

/**
 * @brief Legge lo stato runtime del log CCTALK TX/RX.
 *
 * Endpoint: `GET /api/cctalk/log_txrx`
 */
esp_err_t api_cctalk_log_txrx_get(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGD(TAG, "[C] GET /api/cctalk/log_txrx");

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddBoolToObject(root, "enabled", dump_cctalk_log);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

/**
 * @brief Aggiorna lo stato runtime del log CCTALK TX/RX (non persistente).
 *
 * Endpoint: `POST /api/cctalk/log_txrx` body JSON `{enabled:true|false}`
 */
esp_err_t api_cctalk_log_txrx_set(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "[C] POST /api/cctalk/log_txrx");

    char body[128] = {0};
    int received = httpd_req_recv(req, body, sizeof(body) - 1);
    if (received <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"empty body\"}", HTTPD_RESP_USE_STRLEN);
    }
    body[received] = '\0';

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"invalid json\"}", HTTPD_RESP_USE_STRLEN);
    }

    cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
    bool new_state = false;
    if (cJSON_IsBool(enabled)) {
        new_state = cJSON_IsTrue(enabled);
    } else if (cJSON_IsNumber(enabled)) {
        new_state = (enabled->valueint != 0);
    } else {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"missing/invalid enabled\"}", HTTPD_RESP_USE_STRLEN);
    }

    dump_cctalk_log = new_state;
    cJSON_Delete(root);

    ESP_LOGI(TAG,
             "[C] CCTALK Log TX/RX %s",
             dump_cctalk_log ? "abilitato" : "disabilitato");

    cJSON *resp = cJSON_CreateObject();
    if (!resp) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddBoolToObject(resp, "enabled", dump_cctalk_log);

    char *json = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    free(json);
    return ret;
}

// Handler API GET /api/config

/**
 * @brief Ottiene la configurazione dell'API.
 *
 * Questa funzione viene utilizzata per recuperare la configurazione corrente dell'API.
 *
 * @param [in] req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_config_get(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGD(TAG, "[C] GET /api/config");
    device_config_t *cfg = device_config_get();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "dname", cfg->device_name);
    cJSON_AddStringToObject(root, "loc", cfg->location_name);
    cJSON_AddStringToObject(root, "image_source", cfg->image_source == IMAGE_SOURCE_SDCARD ? "sdcard" : "spiffs");
    cJSON_AddNumberToObject(root, "n_prg", cfg->num_programs ? cfg->num_programs : 10);
    web_ui_add_touch_map_to_json(root, cfg);
    web_ui_add_digital_inputs_to_json(root);
    cJSON_AddNumberToObject(root, "lat",  cfg->latitude);
    cJSON_AddNumberToObject(root, "lon", cfg->longitude);
    
    cJSON *eth = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth, "en", cfg->eth.enabled);
    cJSON_AddBoolToObject(eth, "dhcp", cfg->eth.dhcp_enabled);
    cJSON_AddStringToObject(eth, "ip", cfg->eth.ip);
    cJSON_AddStringToObject(eth, "sub", cfg->eth.subnet);
    cJSON_AddStringToObject(eth, "gw", cfg->eth.gateway);
    cJSON_AddItemToObject(root, "eth", eth);
    
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi, "sta", cfg->wifi.sta_enabled);
    cJSON_AddBoolToObject(wifi, "dhcp", cfg->wifi.dhcp_enabled);
    cJSON_AddStringToObject(wifi, "ssid", cfg->wifi.ssid);
    cJSON_AddStringToObject(wifi, "pwd", cfg->wifi.password);
    cJSON_AddStringToObject(wifi, "ip", cfg->wifi.ip);
    cJSON_AddStringToObject(wifi, "sub", cfg->wifi.subnet);
    cJSON_AddStringToObject(wifi, "gw", cfg->wifi.gateway);
    cJSON_AddItemToObject(root, "wifi", wifi);
    
    cJSON_AddBoolToObject(root, "ntp_en", cfg->ntp_enabled);
    
    // NTP configuration
    cJSON *ntp = cJSON_CreateObject();
    cJSON_AddStringToObject(ntp, "s1", cfg->ntp.server1);
    cJSON_AddStringToObject(ntp, "s2", cfg->ntp.server2);
    cJSON_AddNumberToObject(ntp, "tz", cfg->ntp.timezone_offset);
    cJSON_AddItemToObject(root, "ntp", ntp);

    // Server/Cloud configuration
    cJSON *server = cJSON_CreateObject();
    cJSON_AddBoolToObject(server, "en", cfg->server.enabled);
    cJSON_AddStringToObject(server, "url", cfg->server.url);
    cJSON_AddStringToObject(server, "ser", cfg->server.serial);
    cJSON_AddStringToObject(server, "pwd", cfg->server.password);
    cJSON_AddItemToObject(root, "server", server);

    cJSON *ftp = cJSON_CreateObject();
    cJSON_AddBoolToObject(ftp, "en", cfg->ftp.enabled);
    cJSON_AddStringToObject(ftp, "server", cfg->ftp.server);
    cJSON_AddStringToObject(ftp, "user", cfg->ftp.user);
    cJSON_AddStringToObject(ftp, "password", cfg->ftp.password);
    cJSON_AddStringToObject(ftp, "path", cfg->ftp.path);
    cJSON_AddItemToObject(root, "ftp", ftp);
    
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors, "io_exp", cfg->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors, "temp", cfg->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors, "led", cfg->sensors.led_enabled);
    cJSON_AddNumberToObject(sensors, "led_n", cfg->sensors.led_count);
    cJSON_AddNumberToObject(sensors, "led_run_r", cfg->sensors.led_run_r);
    cJSON_AddNumberToObject(sensors, "led_run_g", cfg->sensors.led_run_g);
    cJSON_AddNumberToObject(sensors, "led_run_b", cfg->sensors.led_run_b);
    cJSON_AddNumberToObject(sensors, "led_prefine_r", cfg->sensors.led_prefine_r);
    cJSON_AddNumberToObject(sensors, "led_prefine_g", cfg->sensors.led_prefine_g);
    cJSON_AddNumberToObject(sensors, "led_prefine_b", cfg->sensors.led_prefine_b);
    cJSON_AddNumberToObject(sensors, "led_standby_r", cfg->sensors.led_standby_r);
    cJSON_AddNumberToObject(sensors, "led_standby_g", cfg->sensors.led_standby_g);
    cJSON_AddNumberToObject(sensors, "led_standby_b", cfg->sensors.led_standby_b);
    cJSON_AddNumberToObject(sensors, "led_flash_r", cfg->sensors.led_flash_r);
    cJSON_AddNumberToObject(sensors, "led_flash_g", cfg->sensors.led_flash_g);
    cJSON_AddNumberToObject(sensors, "led_flash_b", cfg->sensors.led_flash_b);
    cJSON_AddNumberToObject(sensors, "led_flash_count", cfg->sensors.led_flash_count);
    cJSON_AddBoolToObject(sensors, "rs232", cfg->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors, "rs485", cfg->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors, "mdb", cfg->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors, "cctalk", cfg->sensors.cctalk_enabled);
    cJSON_AddBoolToObject(sensors, "eeprom", cfg->sensors.eeprom_enabled);
    cJSON_AddBoolToObject(sensors, "sd", cfg->sensors.sd_card_enabled);
    cJSON_AddBoolToObject(sensors, "pwm1", cfg->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors, "pwm2", cfg->sensors.pwm2_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors);

    cJSON *display = cJSON_CreateObject();
    cJSON_AddBoolToObject(display, "en", cfg->display.enabled);
    cJSON_AddNumberToObject(display, "brt", cfg->display.lcd_brightness);
    cJSON_AddBoolToObject(display, "backlight", cfg->display.backlight);
    cJSON_AddBoolToObject(display, "ads_en", cfg->display.ads_enabled);
    cJSON_AddItemToObject(root, "display", display);

    // RS232
    cJSON *rs232 = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs232, "baud", cfg->rs232.baud_rate);
    cJSON_AddNumberToObject(rs232, "data", cfg->rs232.data_bits);
    cJSON_AddNumberToObject(rs232, "par", cfg->rs232.parity);
    cJSON_AddNumberToObject(rs232, "stop", cfg->rs232.stop_bits);
    cJSON_AddNumberToObject(rs232, "rx_buf", cfg->rs232.rx_buf_size);
    cJSON_AddNumberToObject(rs232, "tx_buf", cfg->rs232.tx_buf_size);
    cJSON_AddItemToObject(root, "rs232", rs232);

    // RS485
    cJSON *rs485 = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs485, "baud", cfg->rs485.baud_rate);
    cJSON_AddNumberToObject(rs485, "data", cfg->rs485.data_bits);
    cJSON_AddNumberToObject(rs485, "par", cfg->rs485.parity);
    cJSON_AddNumberToObject(rs485, "stop", cfg->rs485.stop_bits);
    cJSON_AddNumberToObject(rs485, "rx_buf", cfg->rs485.rx_buf_size);
    cJSON_AddNumberToObject(rs485, "tx_buf", cfg->rs485.tx_buf_size);
    cJSON_AddItemToObject(root, "rs485", rs485);

    // Modbus RTU
    cJSON *modbus = cJSON_CreateObject();
    cJSON_AddBoolToObject(modbus, "enabled", cfg->modbus.enabled);
    cJSON_AddNumberToObject(modbus, "slave_id", cfg->modbus.slave_id);
    cJSON_AddNumberToObject(modbus, "poll_ms", cfg->modbus.poll_ms);
    cJSON_AddNumberToObject(modbus, "timeout_ms", cfg->modbus.timeout_ms);
    cJSON_AddNumberToObject(modbus, "retries", cfg->modbus.retries);
    cJSON_AddNumberToObject(modbus, "relay_start", cfg->modbus.relay_start);
    cJSON_AddNumberToObject(modbus, "relay_count", cfg->modbus.relay_count);
    cJSON_AddNumberToObject(modbus, "input_start", cfg->modbus.input_start);
    cJSON_AddNumberToObject(modbus, "input_count", cfg->modbus.input_count);
    cJSON_AddItemToObject(root, "modbus", modbus);

    // MDB Serial
    cJSON *mdb_s = cJSON_CreateObject();
    cJSON_AddNumberToObject(mdb_s, "baud", cfg->mdb_serial.baud_rate);
    cJSON_AddNumberToObject(mdb_s, "data", cfg->mdb_serial.data_bits);
    cJSON_AddNumberToObject(mdb_s, "par", cfg->mdb_serial.parity);
    cJSON_AddNumberToObject(mdb_s, "stop", cfg->mdb_serial.stop_bits);
    cJSON_AddNumberToObject(mdb_s, "rx_buf", cfg->mdb_serial.rx_buf_size);
    cJSON_AddNumberToObject(mdb_s, "tx_buf", cfg->mdb_serial.tx_buf_size);
    cJSON_AddItemToObject(root, "mdb_ser", mdb_s);

    // CCTalk Serial (configurazione fissa attuale)
    cJSON *cctalk_s = cJSON_CreateObject();
    cJSON_AddNumberToObject(cctalk_s, "baud", WEB_UI_CCTALK_BAUD);
    cJSON_AddNumberToObject(cctalk_s, "data_bits", WEB_UI_CCTALK_DATA_BITS);
    cJSON_AddNumberToObject(cctalk_s, "parity", WEB_UI_CCTALK_PARITY);
    cJSON_AddNumberToObject(cctalk_s, "stop_bits", WEB_UI_CCTALK_STOP_BITS);
    cJSON_AddNumberToObject(cctalk_s, "rx_buf", WEB_UI_CCTALK_RX_BUF);
    cJSON_AddNumberToObject(cctalk_s, "tx_buf", WEB_UI_CCTALK_TX_BUF);
    cJSON_AddNumberToObject(cctalk_s, "addr", cfg->cctalk.address);
    cJSON_AddBoolToObject(cctalk_s, "read_only", true);
    cJSON_AddItemToObject(root, "cctalk_serial", cctalk_s);

    cJSON *cctalk_cfg = cJSON_CreateObject();
    cJSON_AddNumberToObject(cctalk_cfg, "address", cfg->cctalk.address);
    cJSON_AddItemToObject(root, "cctalk", cctalk_cfg);

    // GPIOs
    cJSON *gpios = cJSON_CreateObject();
    cJSON *g33 = cJSON_CreateObject();
    cJSON_AddNumberToObject(g33, "m", cfg->gpios.gpio33.mode);
    cJSON_AddBoolToObject(g33, "st", cfg->gpios.gpio33.initial_state);
    cJSON_AddItemToObject(gpios, "gpio33", g33);
    cJSON_AddItemToObject(root, "gpios", gpios);

    // Remote logging configuration
    cJSON *remote_log = cJSON_CreateObject();
    cJSON_AddNumberToObject(remote_log, "port", cfg->remote_log.server_port);
    cJSON_AddBoolToObject(remote_log, "bcast", cfg->remote_log.use_broadcast);
    cJSON_AddBoolToObject(remote_log, "to_sd", cfg->remote_log.write_to_sd);
    cJSON_AddItemToObject(root, "rlog", remote_log);

    // Scanner configuration
    cJSON *scanner = cJSON_CreateObject();
    cJSON_AddBoolToObject(scanner, "en", cfg->scanner.enabled);
    cJSON_AddNumberToObject(scanner, "vid", cfg->scanner.vid);
    cJSON_AddNumberToObject(scanner, "pid", cfg->scanner.pid);
    cJSON_AddNumberToObject(scanner, "dpid", cfg->scanner.dual_pid);
    cJSON_AddNumberToObject(scanner, "cool", cfg->scanner.cooldown_ms);
    cJSON_AddItemToObject(root, "scanner", scanner);

    // Timeout applicativi
    cJSON *timeouts = cJSON_CreateObject();
    cJSON_AddNumberToObject(timeouts, "t_prg", cfg->timeouts.exit_programs_ms);
    cJSON_AddNumberToObject(timeouts, "t_lang", cfg->timeouts.exit_language_ms);
    cJSON_AddNumberToObject(timeouts, "idle_before_ads_ms", cfg->timeouts.idle_before_ads_ms);
    cJSON_AddNumberToObject(timeouts, "ad_rotation_ms", cfg->timeouts.ad_rotation_ms);
    cJSON_AddNumberToObject(timeouts, "credit_reset_timeout_ms", cfg->timeouts.credit_reset_timeout_ms);
    cJSON_AddNumberToObject(timeouts, "pre_fine_ciclo_percent", cfg->timeouts.pre_fine_ciclo_percent);
    cJSON_AddNumberToObject(timeouts, "pause_max_suspend_sec", cfg->timeouts.pause_max_suspend_sec);
    cJSON_AddItemToObject(root, "timeouts", timeouts);

    // UI multilingua
    cJSON *ui = cJSON_CreateObject();
    cJSON_AddStringToObject(ui, "ulang", cfg->ui.user_language);
    cJSON_AddStringToObject(ui, "blang", cfg->ui.backend_language);
    cJSON_AddNumberToObject(ui, "program_end_message_sec", cfg->ui.program_end_message_sec);
    cJSON_AddStringToObject(ui, "storage", "spiffs");
    cJSON_AddItemToObject(root, "ui", ui);
    cJSON_AddStringToObject(root, "ui_language", cfg->ui.user_language);
    
    char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler API GET /api/ui/texts

/**
 * @brief Ottiene i testi dell'interfaccia utente.
 *
 * Questa funzione gestisce la richiesta per ottenere i testi dell'interfaccia utente.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_ui_texts_get(httpd_req_t *req)
{
    device_config_t *cfg = device_config_get();
    char language[8] = {0};
    /* default to backend language for texts served to the web UI */
    strncpy(language, cfg->ui.backend_language, sizeof(language) - 1);

    char query[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char lang_q[8] = {0};
        if (httpd_query_key_value(query, "lang", lang_q, sizeof(lang_q)) == ESP_OK && strlen(lang_q) == 2) {
            strncpy(language, lang_q, sizeof(language) - 1);
        }
    }

    char *records_json = device_config_get_ui_texts_records_json(language);
    if (!records_json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"i18n_load_failed\"}", -1);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "language", language);
    cJSON_AddStringToObject(root, "storage", "spiffs");

    char path[64] = {0};
    snprintf(path, sizeof(path), "/spiffs/i18n_%s.json", language);
    cJSON_AddStringToObject(root, "file", path);

    cJSON *records = cJSON_Parse(records_json);
    free(records_json);
    if (records && cJSON_IsArray(records)) {
        cJSON_AddItemToObject(root, "records", records);
    } else {
        if (records) {
            cJSON_Delete(records);
        }
        cJSON_AddItemToObject(root, "records", cJSON_CreateArray());
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"json_build_failed\"}", -1);
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

// Handler API GET /api/ui/languages

/**
 * @brief Ottiene la lista delle lingue supportate dall'interfaccia utente.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_ui_languages_get(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *languages = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "languages", languages);

    bool has_it = false;
    if (!web_ui_append_languages_from_i18n_v2(languages, &has_it)) {
        web_ui_append_languages_from_legacy_scan(languages, &has_it);
    }

    if (!has_it) {
        web_ui_append_language_item(languages, "it", I18N_V2_FILE_PATH, &has_it);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"json_build_failed\"}", -1);
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

// Handler API POST /api/config/backup

/**
 * @brief Configura il backup dell'API.
 *
 * Questa funzione configura il backup dell'API utilizzando la richiesta HTTP fornita.
 *
 * @param [in] req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_config_backup(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "[C] POST /api/config/backup");

    device_config_t *cfg = device_config_get();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "dname", cfg->device_name);
    cJSON_AddStringToObject(root, "loc", cfg->location_name);
    cJSON_AddStringToObject(root, "image_source", cfg->image_source == IMAGE_SOURCE_SDCARD ? "sdcard" : "spiffs");
    web_ui_add_touch_map_to_json(root, cfg);

    cJSON *eth = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth, "en", cfg->eth.enabled);
    cJSON_AddBoolToObject(eth, "dhcp", cfg->eth.dhcp_enabled);
    cJSON_AddStringToObject(eth, "ip", cfg->eth.ip);
    cJSON_AddStringToObject(eth, "sub", cfg->eth.subnet);
    cJSON_AddStringToObject(eth, "gw", cfg->eth.gateway);
    cJSON_AddItemToObject(root, "eth", eth);
    
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi, "sta", cfg->wifi.sta_enabled);
    cJSON_AddBoolToObject(wifi, "dhcp", cfg->wifi.dhcp_enabled);
    cJSON_AddStringToObject(wifi, "ssid", cfg->wifi.ssid);
    cJSON_AddStringToObject(wifi, "pwd", cfg->wifi.password);
    cJSON_AddStringToObject(wifi, "ip", cfg->wifi.ip);
    cJSON_AddStringToObject(wifi, "sub", cfg->wifi.subnet);
    cJSON_AddStringToObject(wifi, "gw", cfg->wifi.gateway);
    cJSON_AddItemToObject(root, "wifi", wifi);
    
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors, "io_exp", cfg->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors, "temp", cfg->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors, "led", cfg->sensors.led_enabled);
    cJSON_AddNumberToObject(sensors, "led_n", cfg->sensors.led_count);
    cJSON_AddBoolToObject(sensors, "rs232", cfg->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors, "rs485", cfg->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors, "mdb", cfg->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors, "cctalk", cfg->sensors.cctalk_enabled);
    cJSON_AddBoolToObject(sensors, "eeprom", cfg->sensors.eeprom_enabled);
    cJSON_AddBoolToObject(sensors, "sd", cfg->sensors.sd_card_enabled);
    cJSON_AddBoolToObject(sensors, "pwm1", cfg->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors, "pwm2", cfg->sensors.pwm2_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors);

    cJSON *display = cJSON_CreateObject();
    cJSON_AddBoolToObject(display, "en", cfg->display.enabled);
    cJSON_AddNumberToObject(display, "brt", cfg->display.lcd_brightness);
    cJSON_AddBoolToObject(display, "backlight", cfg->display.backlight);
    cJSON_AddBoolToObject(display, "ads_en", cfg->display.ads_enabled);
    cJSON_AddItemToObject(root, "display", display);

    // RS232
    cJSON *rs232 = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs232, "baud", cfg->rs232.baud_rate);
    cJSON_AddNumberToObject(rs232, "data", cfg->rs232.data_bits);
    cJSON_AddNumberToObject(rs232, "par", cfg->rs232.parity);
    cJSON_AddNumberToObject(rs232, "stop", cfg->rs232.stop_bits);
    cJSON_AddNumberToObject(rs232, "rx_buf", cfg->rs232.rx_buf_size);
    cJSON_AddNumberToObject(rs232, "tx_buf", cfg->rs232.tx_buf_size);
    cJSON_AddItemToObject(root, "rs232", rs232);

    // RS485
    cJSON *rs485 = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs485, "baud", cfg->rs485.baud_rate);
    cJSON_AddNumberToObject(rs485, "data", cfg->rs485.data_bits);
    cJSON_AddNumberToObject(rs485, "par", cfg->rs485.parity);
    cJSON_AddNumberToObject(rs485, "stop", cfg->rs485.stop_bits);
    cJSON_AddNumberToObject(rs485, "rx_buf", cfg->rs485.rx_buf_size);
    cJSON_AddNumberToObject(rs485, "tx_buf", cfg->rs485.tx_buf_size);
    cJSON_AddItemToObject(root, "rs485", rs485);

    // Modbus RTU
    cJSON *modbus = cJSON_CreateObject();
    cJSON_AddBoolToObject(modbus, "enabled", cfg->modbus.enabled);
    cJSON_AddNumberToObject(modbus, "slave_id", cfg->modbus.slave_id);
    cJSON_AddNumberToObject(modbus, "poll_ms", cfg->modbus.poll_ms);
    cJSON_AddNumberToObject(modbus, "timeout_ms", cfg->modbus.timeout_ms);
    cJSON_AddNumberToObject(modbus, "retries", cfg->modbus.retries);
    cJSON_AddNumberToObject(modbus, "relay_start", cfg->modbus.relay_start);
    cJSON_AddNumberToObject(modbus, "relay_count", cfg->modbus.relay_count);
    cJSON_AddNumberToObject(modbus, "input_start", cfg->modbus.input_start);
    cJSON_AddNumberToObject(modbus, "input_count", cfg->modbus.input_count);
    cJSON_AddItemToObject(root, "modbus", modbus);

    // MDB Serial
    cJSON *mdb_s = cJSON_CreateObject();
    cJSON_AddNumberToObject(mdb_s, "baud", cfg->mdb_serial.baud_rate);
    cJSON_AddNumberToObject(mdb_s, "data", cfg->mdb_serial.data_bits);
    cJSON_AddNumberToObject(mdb_s, "par", cfg->mdb_serial.parity);
    cJSON_AddNumberToObject(mdb_s, "stop", cfg->mdb_serial.stop_bits);
    cJSON_AddNumberToObject(mdb_s, "rx_buf", cfg->mdb_serial.rx_buf_size);
    cJSON_AddNumberToObject(mdb_s, "tx_buf", cfg->mdb_serial.tx_buf_size);
    cJSON_AddItemToObject(root, "mdb_ser", mdb_s);

    // CCTalk Serial (configurazione fissa attuale)
    cJSON *cctalk_s = cJSON_CreateObject();
    cJSON_AddNumberToObject(cctalk_s, "baud", WEB_UI_CCTALK_BAUD);
    cJSON_AddNumberToObject(cctalk_s, "data_bits", WEB_UI_CCTALK_DATA_BITS);
    cJSON_AddNumberToObject(cctalk_s, "parity", WEB_UI_CCTALK_PARITY);
    cJSON_AddNumberToObject(cctalk_s, "stop_bits", WEB_UI_CCTALK_STOP_BITS);
    cJSON_AddNumberToObject(cctalk_s, "rx_buf", WEB_UI_CCTALK_RX_BUF);
    cJSON_AddNumberToObject(cctalk_s, "tx_buf", WEB_UI_CCTALK_TX_BUF);
    cJSON_AddNumberToObject(cctalk_s, "addr", cfg->cctalk.address);
    cJSON_AddBoolToObject(cctalk_s, "read_only", true);
    cJSON_AddItemToObject(root, "cctalk_serial", cctalk_s);

    cJSON *cctalk_cfg = cJSON_CreateObject();
    cJSON_AddNumberToObject(cctalk_cfg, "address", cfg->cctalk.address);
    cJSON_AddItemToObject(root, "cctalk", cctalk_cfg);

    // UI multilingua
    cJSON *ui = cJSON_CreateObject();
    cJSON_AddStringToObject(ui, "ulang", cfg->ui.user_language);
    cJSON_AddStringToObject(ui, "blang", cfg->ui.backend_language);
    cJSON_AddNumberToObject(ui, "program_end_message_sec", cfg->ui.program_end_message_sec);
    cJSON_AddStringToObject(ui, "storage", "spiffs");
    cJSON_AddItemToObject(root, "ui", ui);
    cJSON_AddStringToObject(root, "ui_language", cfg->ui.user_language);

    char *json = cJSON_Print(root);

    char response[256] = {0};
    bool spiffs_ok = false;
    bool sd_attempted = false;
    bool sd_ok = false;

    if (json) {
        FILE *f = fopen("/spiffs/config.jsn", "w");
        if (f) {
            if (fputs(json, f) >= 0) {
                spiffs_ok = true;
            }
            fclose(f);
        } else {
            ESP_LOGE(TAG, "[C] Backup config: impossibile aprire /spiffs/config.jsn in scrittura");
        }

        if (sd_card_is_mounted()) {
            sd_attempted = true;
            sd_ok = (sd_card_write_file("/sdcard/config.jsn", json) == ESP_OK);
            if (!sd_ok) {
                ESP_LOGW(TAG, "[C] Backup config: scrittura su SD fallita");
            }
        }
    }

    if (json) {
        free(json);
    }
    cJSON_Delete(root);

    if (!spiffs_ok) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        if (sd_attempted && sd_ok) {
            snprintf(response,
                     sizeof(response),
                     "{\"error\":\"Backup SPIFFS fallito; backup SD eseguito in /sdcard/config.jsn\"}");
        } else {
            snprintf(response,
                     sizeof(response),
                     "{\"error\":\"Backup SPIFFS fallito\"}");
        }
    } else {
        if (sd_attempted && sd_ok) {
            snprintf(response,
                     sizeof(response),
                     "{\"status\":\"ok\",\"message\":\"Backup salvato in /spiffs/config.jsn e /sdcard/config.jsn\"}");
        } else if (sd_attempted && !sd_ok) {
            snprintf(response,
                     sizeof(response),
                     "{\"status\":\"ok\",\"message\":\"Backup salvato in /spiffs/config.jsn; backup SD non riuscito\"}");
        } else {
            snprintf(response,
                     sizeof(response),
                     "{\"status\":\"ok\",\"message\":\"Backup salvato in /spiffs/config.jsn (SD non montata)\"}");
        }
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

// Handler API POST /api/config/save

/**
 * @brief Salva la configurazione dell'API.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Errore generato dalla funzione.
 * 
 * @note La funzione controlla se la funzionalità WEB_UI_FEATURE_ENDPOINT_PROGRAMS è abilitata.
 * Se non è abilitata, la funzione restituisce un errore.
 */
esp_err_t api_config_save(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Configurazione in sola lettura in modalità APP", -1);
    }

    ESP_LOGI(TAG, "[C] Ricevuta richiesta salvataggio configurazione");
    
    char buf[4096] = {0};
    httpd_req_recv(req, buf, sizeof(buf)-1);
    
    // Log del JSON ricevuto per debugging
    ESP_LOGI(TAG, "[C] JSON configurazione ricevuto: %s", buf);
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        const char *resp_str = "{\"error\":\"Invalid JSON\"}";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }
    
    device_config_t *cfg = device_config_get();
    bool prev_server_enabled = cfg->server.enabled;
    char prev_server_url[sizeof(cfg->server.url)] = {0};
    char prev_server_serial[sizeof(cfg->server.serial)] = {0};
    char prev_server_password[sizeof(cfg->server.password)] = {0};
    char prev_user_lang[sizeof(cfg->ui.user_language)] = {0};
    char prev_backend_lang[sizeof(cfg->ui.backend_language)] = {0};
    strncpy(prev_server_url, cfg->server.url, sizeof(prev_server_url) - 1);
    strncpy(prev_server_serial, cfg->server.serial, sizeof(prev_server_serial) - 1);
    strncpy(prev_server_password, cfg->server.password, sizeof(prev_server_password) - 1);
    strncpy(prev_user_lang, cfg->ui.user_language, sizeof(prev_user_lang) - 1);
    strncpy(prev_backend_lang, cfg->ui.backend_language, sizeof(prev_backend_lang) - 1);

    cJSON *name_obj = cJSON_GetObjectItem(root, "dname");
    if (!name_obj) name_obj = cJSON_GetObjectItem(root, "device_name"); /* compat */
    if (name_obj && name_obj->valuestring) strncpy(cfg->device_name, name_obj->valuestring, sizeof(cfg->device_name)-1);
    cJSON *loc_obj = cJSON_GetObjectItem(root, "loc");
    if (!loc_obj) loc_obj = cJSON_GetObjectItem(root, "location_name"); /* compat */
    if (loc_obj && loc_obj->valuestring) strncpy(cfg->location_name, loc_obj->valuestring, sizeof(cfg->location_name)-1);
    cJSON *img_src_obj = cJSON_GetObjectItem(root, "image_source");
    if (img_src_obj && img_src_obj->valuestring) {
        cfg->image_source = (strcmp(img_src_obj->valuestring, "sdcard") == 0)
            ? IMAGE_SOURCE_SDCARD : IMAGE_SOURCE_SPIFFS;
    }

    /* [C] Numero pulsanti programma (valori ammessi: 1,2,3,4,5,6,8,10) */
    cJSON *num_prog_obj = cJSON_GetObjectItem(root, "n_prg");
    if (!num_prog_obj) num_prog_obj = cJSON_GetObjectItem(root, "num_programs"); /* compat */
    if (num_prog_obj && cJSON_IsNumber(num_prog_obj)) {
        static const uint8_t s_valid_np[] = {1, 2, 3, 4, 5, 6, 8, 10};
        uint8_t np = (uint8_t)num_prog_obj->valueint;
        bool np_ok = false;
        for (int _i = 0; _i < (int)(sizeof(s_valid_np)/sizeof(s_valid_np[0])); _i++) {
            if (s_valid_np[_i] == np) { np_ok = true; break; }
        }
        cfg->num_programs = np_ok ? np : 10;
    }

    web_ui_parse_touch_map_from_json(root, cfg);

    /* [C] Coordinate geografiche impianto */
    cJSON *lat_obj = cJSON_GetObjectItem(root, "lat");
    if (!lat_obj) lat_obj = cJSON_GetObjectItem(root, "latitude"); /* compat */
    if (lat_obj && cJSON_IsNumber(lat_obj)) cfg->latitude  = lat_obj->valuedouble;
    cJSON *lon_obj = cJSON_GetObjectItem(root, "lon");
    if (!lon_obj) lon_obj = cJSON_GetObjectItem(root, "longitude"); /* compat */
    if (lon_obj && cJSON_IsNumber(lon_obj)) cfg->longitude = lon_obj->valuedouble;
    
    cJSON *eth_obj = cJSON_GetObjectItem(root, "eth");
    if (eth_obj) {
        cfg->eth.enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "en")) ||
                           cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "enabled")); /* compat */
        cfg->eth.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "dhcp")) ||
                                cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "dhcp_enabled")); /* compat */
        cJSON *ip = cJSON_GetObjectItem(eth_obj, "ip");
        if (ip && ip->valuestring) strncpy(cfg->eth.ip, ip->valuestring, sizeof(cfg->eth.ip)-1);
        cJSON *subnet = cJSON_GetObjectItem(eth_obj, "sub");
        if (!subnet) subnet = cJSON_GetObjectItem(eth_obj, "subnet"); /* compat */
        if (subnet && subnet->valuestring) strncpy(cfg->eth.subnet, subnet->valuestring, sizeof(cfg->eth.subnet)-1);
        cJSON *gateway = cJSON_GetObjectItem(eth_obj, "gw");
        if (!gateway) gateway = cJSON_GetObjectItem(eth_obj, "gateway"); /* compat */
        if (gateway && gateway->valuestring) strncpy(cfg->eth.gateway, gateway->valuestring, sizeof(cfg->eth.gateway)-1);
    }
    
    cJSON *wifi_obj = cJSON_GetObjectItem(root, "wifi");
    if (wifi_obj) {
        cfg->wifi.sta_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "sta")) ||
                                cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "sta_enabled")); /* compat */
        cfg->wifi.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "dhcp")) ||
                                 cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "dhcp_enabled")); /* compat */
        cJSON *ssid = cJSON_GetObjectItem(wifi_obj, "ssid");
        if (ssid && ssid->valuestring) strncpy(cfg->wifi.ssid, ssid->valuestring, sizeof(cfg->wifi.ssid)-1);
        cJSON *password = cJSON_GetObjectItem(wifi_obj, "pwd");
        if (!password) password = cJSON_GetObjectItem(wifi_obj, "password"); /* compat */
        if (password && password->valuestring) strncpy(cfg->wifi.password, password->valuestring, sizeof(cfg->wifi.password)-1);
    }
    
    // NTP configuration
    cfg->ntp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "ntp_en")) ||
                       cJSON_IsTrue(cJSON_GetObjectItem(root, "ntp_enabled")); /* compat */
    cJSON *ntp_obj = cJSON_GetObjectItem(root, "ntp");
    if (ntp_obj) {
        cJSON *server1 = cJSON_GetObjectItem(ntp_obj, "s1");
        if (!server1) server1 = cJSON_GetObjectItem(ntp_obj, "server1"); /* compat */
        if (server1 && server1->valuestring) strncpy(cfg->ntp.server1, server1->valuestring, sizeof(cfg->ntp.server1)-1);
        cJSON *server2 = cJSON_GetObjectItem(ntp_obj, "s2");
        if (!server2) server2 = cJSON_GetObjectItem(ntp_obj, "server2"); /* compat */
        if (server2 && server2->valuestring) strncpy(cfg->ntp.server2, server2->valuestring, sizeof(cfg->ntp.server2)-1);
        cJSON *tz_offset = cJSON_GetObjectItem(ntp_obj, "tz");
        if (!tz_offset) tz_offset = cJSON_GetObjectItem(ntp_obj, "timezone_offset"); /* compat */
        if (tz_offset && cJSON_IsNumber(tz_offset)) cfg->ntp.timezone_offset = tz_offset->valueint;
    }

    // Server/Cloud configuration
    cJSON *server_obj = cJSON_GetObjectItem(root, "server");
    if (server_obj) {
        cfg->server.enabled = cJSON_IsTrue(cJSON_GetObjectItem(server_obj, "en")) ||
                              cJSON_IsTrue(cJSON_GetObjectItem(server_obj, "enabled")); /* compat */
        cJSON *url = cJSON_GetObjectItem(server_obj, "url");
        if (url && url->valuestring) strncpy(cfg->server.url, url->valuestring, sizeof(cfg->server.url)-1);
        cJSON *serial = cJSON_GetObjectItem(server_obj, "ser");
        if (!serial) serial = cJSON_GetObjectItem(server_obj, "serial"); /* compat */
        if (serial && serial->valuestring) strncpy(cfg->server.serial, serial->valuestring, sizeof(cfg->server.serial)-1);
        cJSON *password = cJSON_GetObjectItem(server_obj, "pwd");
        if (!password) password = cJSON_GetObjectItem(server_obj, "password"); /* compat */
        if (password && password->valuestring) strncpy(cfg->server.password, password->valuestring, sizeof(cfg->server.password)-1);
    }

    cJSON *ftp_obj = cJSON_GetObjectItem(root, "ftp");
    if (ftp_obj) {
        cfg->ftp.enabled = cJSON_IsTrue(cJSON_GetObjectItem(ftp_obj, "en")) ||
                           cJSON_IsTrue(cJSON_GetObjectItem(ftp_obj, "enabled"));
        cJSON *ftp_server = cJSON_GetObjectItem(ftp_obj, "server");
        if (ftp_server && ftp_server->valuestring) strncpy(cfg->ftp.server, ftp_server->valuestring, sizeof(cfg->ftp.server)-1);
        cJSON *ftp_user = cJSON_GetObjectItem(ftp_obj, "user");
        if (ftp_user && ftp_user->valuestring) strncpy(cfg->ftp.user, ftp_user->valuestring, sizeof(cfg->ftp.user)-1);
        cJSON *ftp_password = cJSON_GetObjectItem(ftp_obj, "password");
        if (ftp_password && ftp_password->valuestring) strncpy(cfg->ftp.password, ftp_password->valuestring, sizeof(cfg->ftp.password)-1);
        cJSON *ftp_path = cJSON_GetObjectItem(ftp_obj, "path");
        if (ftp_path && ftp_path->valuestring) strncpy(cfg->ftp.path, ftp_path->valuestring, sizeof(cfg->ftp.path)-1);
    }
    
    cJSON *sensors_obj = cJSON_GetObjectItem(root, "sensors");
    if (sensors_obj) {
        cfg->sensors.io_expander_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "io_exp")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "io_expander_enabled")); /* compat */
        cfg->sensors.temperature_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "temp")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "temperature_enabled")); /* compat */
        
        bool old_led_enabled = cfg->sensors.led_enabled;
        uint32_t old_led_count = cfg->sensors.led_count;
        
        cfg->sensors.led_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "led")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "led_enabled")); /* compat */
        cJSON *lc = cJSON_GetObjectItem(sensors_obj, "led_n");
        if (!lc) lc = cJSON_GetObjectItem(sensors_obj, "led_count"); /* compat */
        if (lc) cfg->sensors.led_count = (uint32_t)lc->valueint;

        cJSON *led_run_r = cJSON_GetObjectItem(sensors_obj, "led_run_r");
        if (led_run_r && cJSON_IsNumber(led_run_r)) {
            int val = led_run_r->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_run_r = (uint8_t)val;
        }
        cJSON *led_run_g = cJSON_GetObjectItem(sensors_obj, "led_run_g");
        if (led_run_g && cJSON_IsNumber(led_run_g)) {
            int val = led_run_g->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_run_g = (uint8_t)val;
        }
        cJSON *led_run_b = cJSON_GetObjectItem(sensors_obj, "led_run_b");
        if (led_run_b && cJSON_IsNumber(led_run_b)) {
            int val = led_run_b->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_run_b = (uint8_t)val;
        }

        cJSON *led_prefine_r = cJSON_GetObjectItem(sensors_obj, "led_prefine_r");
        if (led_prefine_r && cJSON_IsNumber(led_prefine_r)) {
            int val = led_prefine_r->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_prefine_r = (uint8_t)val;
        }
        cJSON *led_prefine_g = cJSON_GetObjectItem(sensors_obj, "led_prefine_g");
        if (led_prefine_g && cJSON_IsNumber(led_prefine_g)) {
            int val = led_prefine_g->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_prefine_g = (uint8_t)val;
        }
        cJSON *led_prefine_b = cJSON_GetObjectItem(sensors_obj, "led_prefine_b");
        if (led_prefine_b && cJSON_IsNumber(led_prefine_b)) {
            int val = led_prefine_b->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_prefine_b = (uint8_t)val;
        }

        cJSON *led_standby_r = cJSON_GetObjectItem(sensors_obj, "led_standby_r");
        if (led_standby_r && cJSON_IsNumber(led_standby_r)) {
            int val = led_standby_r->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_standby_r = (uint8_t)val;
        }
        cJSON *led_standby_g = cJSON_GetObjectItem(sensors_obj, "led_standby_g");
        if (led_standby_g && cJSON_IsNumber(led_standby_g)) {
            int val = led_standby_g->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_standby_g = (uint8_t)val;
        }
        cJSON *led_standby_b = cJSON_GetObjectItem(sensors_obj, "led_standby_b");
        if (led_standby_b && cJSON_IsNumber(led_standby_b)) {
            int val = led_standby_b->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_standby_b = (uint8_t)val;
        }

        cJSON *led_flash_r = cJSON_GetObjectItem(sensors_obj, "led_flash_r");
        if (led_flash_r && cJSON_IsNumber(led_flash_r)) {
            int val = led_flash_r->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_flash_r = (uint8_t)val;
        }
        cJSON *led_flash_g = cJSON_GetObjectItem(sensors_obj, "led_flash_g");
        if (led_flash_g && cJSON_IsNumber(led_flash_g)) {
            int val = led_flash_g->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_flash_g = (uint8_t)val;
        }
        cJSON *led_flash_b = cJSON_GetObjectItem(sensors_obj, "led_flash_b");
        if (led_flash_b && cJSON_IsNumber(led_flash_b)) {
            int val = led_flash_b->valueint;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            cfg->sensors.led_flash_b = (uint8_t)val;
        }

        cJSON *led_flash_count = cJSON_GetObjectItem(sensors_obj, "led_flash_count");
        if (led_flash_count && cJSON_IsNumber(led_flash_count)) {
            int val = led_flash_count->valueint;
            if (val < 1) val = 1;
            if (val > 20) val = 20;
            cfg->sensors.led_flash_count = (uint8_t)val;
        }
        
        cfg->sensors.rs232_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs232")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs232_enabled")); /* compat */
        cfg->sensors.rs485_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs485")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs485_enabled")); /* compat */
        cfg->sensors.mdb_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "mdb")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "mdb_enabled")); /* compat */
        cfg->sensors.cctalk_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "cctalk")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "cctalk_enabled")); /* compat */
        cJSON *eeprom_enabled = cJSON_GetObjectItem(sensors_obj, "eeprom");
        if (!eeprom_enabled) eeprom_enabled = cJSON_GetObjectItem(sensors_obj, "eeprom_enabled"); /* compat */
        if (eeprom_enabled) {
            cfg->sensors.eeprom_enabled = cJSON_IsTrue(eeprom_enabled);
        }
        cfg->sensors.pwm1_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm1")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm1_enabled")); /* compat */
        cfg->sensors.pwm2_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm2")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm2_enabled")); /* compat */
        cfg->sensors.sd_card_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "sd")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "sd_card_enabled")); /* compat */

        // Se i LED sono abilitati e il numero è cambiato, re-inizializziamo la stripe
        if (cfg->sensors.led_enabled && (cfg->sensors.led_count != old_led_count || !old_led_enabled)) {
            ESP_LOGI(TAG, "[C] Cambio configurazione LED rilevato: re-inizializzo driver");
            led_init();
        }
    }

    cJSON *display_obj = cJSON_GetObjectItem(root, "display");
    if (display_obj) {
        cJSON *enabled = cJSON_GetObjectItem(display_obj, "enabled");
        if (enabled) {
#if FORCE_VIDEO_DISABLED
            /* In build headless forzata ignoriamo il valore ricevuto dalla pagina /config */
            cfg->display.enabled = false;
            ESP_LOGW(TAG, "[C] FORCE_VIDEO_DISABLED attivo: richiesta display.enabled ignorata");
#else
            cfg->display.enabled = cJSON_IsTrue(enabled);
            ESP_LOGI(TAG, "[C] Display enabled set to %d", cfg->display.enabled);
#endif
        }
        cJSON *bright = cJSON_GetObjectItem(display_obj, "brt");
        if (!bright) bright = cJSON_GetObjectItem(display_obj, "lcd_brightness"); /* compat */
        if (bright) {
            cfg->display.lcd_brightness = (uint8_t)bright->valueint;
            // Applica immediatamente la luminosità del display SOLO se display abilitato
            if (cfg->display.enabled) {
                bsp_display_brightness_set(cfg->display.lcd_brightness);
                /* Registra nel log SOLO il valore persistito (evita i log dei valori live inviati durante lo slider) */
                char __msg_bright[64];
                snprintf(__msg_bright, sizeof(__msg_bright), "Luminosità persistita: %u%%", cfg->display.lcd_brightness);
                web_ui_add_log("INFO", "DISPLAY", __msg_bright);
            } else {
                web_ui_add_log("WARN", "DISPLAY", "Display disabilitato: ignorata richiesta luminosità");
            }
        }
        
        cJSON *backlight = cJSON_GetObjectItem(display_obj, "backlight");
        if (backlight) {
            cfg->display.backlight = cJSON_IsTrue(backlight);
            // Applica immediatamente il stato del backlight SOLO se display abilitato
            if (cfg->display.enabled) {
                if (cfg->display.backlight) {
                    if (!bright) {
                        bsp_display_backlight_on();
                    }
                    web_ui_add_log("INFO", "DISPLAY", "Backlight acceso");
                } else {
                    bsp_display_backlight_off();
                    web_ui_add_log("INFO", "DISPLAY", "Backlight spento");
                }
            } else {
                web_ui_add_log("WARN", "DISPLAY", "Display disabilitato: ignorata richiesta backlight");
            }
        }
        cJSON *ads_enabled = cJSON_GetObjectItem(display_obj, "ads_en");
        if (!ads_enabled) ads_enabled = cJSON_GetObjectItem(display_obj, "ads_enabled");
        if (ads_enabled) {
            cfg->display.ads_enabled = cJSON_IsTrue(ads_enabled);
        }
    }

    cJSON *gpios_obj = cJSON_GetObjectItem(root, "gpios");
    if (gpios_obj) {
        cJSON *g33 = cJSON_GetObjectItem(gpios_obj, "gpio33");
        if (g33) {
            cfg->gpios.gpio33.mode = (device_gpio_cfg_mode_t)cJSON_GetNumberValue(
                cJSON_GetObjectItem(g33, "m") ? cJSON_GetObjectItem(g33, "m") : cJSON_GetObjectItem(g33, "mode"));
            cfg->gpios.gpio33.initial_state = cJSON_IsTrue(
                cJSON_GetObjectItem(g33, "st") ? cJSON_GetObjectItem(g33, "st") : cJSON_GetObjectItem(g33, "state"));
        }
    }

    // RS232 cfg
    cJSON *rs232_obj = cJSON_GetObjectItem(root, "rs232");
    if (rs232_obj) {
        web_ui_apply_serial_cfg_from_json(rs232_obj,
                                          &cfg->rs232,
                                          &s_web_ui_serial_default_rs232);
    }
    // RS485 cfg
    cJSON *rs485_obj = cJSON_GetObjectItem(root, "rs485");
    if (rs485_obj) {
        web_ui_apply_serial_cfg_from_json(rs485_obj,
                                          &cfg->rs485,
                                          &s_web_ui_serial_default_rs485);
    }

    cJSON *modbus_obj = cJSON_GetObjectItem(root, "modbus");
    if (modbus_obj) {
        cJSON *enabled = cJSON_GetObjectItem(modbus_obj, "enabled");
        if (enabled) cfg->modbus.enabled = cJSON_IsTrue(enabled);

        cJSON *slave_id = cJSON_GetObjectItem(modbus_obj, "slave_id");
        if (slave_id && cJSON_IsNumber(slave_id)) {
            int val = slave_id->valueint;
            if (val < 1) val = 1;
            if (val > 255) val = 255;
            cfg->modbus.slave_id = (uint8_t)val;
        }

        cJSON *poll_ms = cJSON_GetObjectItem(modbus_obj, "poll_ms");
        if (poll_ms && cJSON_IsNumber(poll_ms)) {
            int val = poll_ms->valueint;
            if (val < 20) val = 20;
            if (val > 5000) val = 5000;
            cfg->modbus.poll_ms = (uint16_t)val;
        }

        cJSON *timeout_ms = cJSON_GetObjectItem(modbus_obj, "timeout_ms");
        if (timeout_ms && cJSON_IsNumber(timeout_ms)) {
            int val = timeout_ms->valueint;
            if (val < 50) val = 50;
            if (val > 2000) val = 2000;
            cfg->modbus.timeout_ms = (uint16_t)val;
        }

        cJSON *retries = cJSON_GetObjectItem(modbus_obj, "retries");
        if (retries && cJSON_IsNumber(retries)) {
            int val = retries->valueint;
            if (val < 0) val = 0;
            if (val > 5) val = 5;
            cfg->modbus.retries = (uint8_t)val;
        }

        cJSON *relay_start = cJSON_GetObjectItem(modbus_obj, "relay_start");
        if (relay_start && cJSON_IsNumber(relay_start)) {
            int val = relay_start->valueint;
            if (val < 0) val = 0;
            cfg->modbus.relay_start = (uint16_t)val;
        }

        cJSON *relay_count = cJSON_GetObjectItem(modbus_obj, "relay_count");
        if (relay_count && cJSON_IsNumber(relay_count)) {
            int val = relay_count->valueint;
            if (val < 1) val = 1;
            if (val > WEB_UI_MODBUS_MAX_POINTS) val = WEB_UI_MODBUS_MAX_POINTS;
            cfg->modbus.relay_count = (uint16_t)val;
        }

        cJSON *input_start = cJSON_GetObjectItem(modbus_obj, "input_start");
        if (input_start && cJSON_IsNumber(input_start)) {
            int val = input_start->valueint;
            if (val < 0) val = 0;
            cfg->modbus.input_start = (uint16_t)val;
        }

        cJSON *input_count = cJSON_GetObjectItem(modbus_obj, "input_count");
        if (input_count && cJSON_IsNumber(input_count)) {
            int val = input_count->valueint;
            if (val < 1) val = 1;
            if (val > WEB_UI_MODBUS_MAX_POINTS) val = WEB_UI_MODBUS_MAX_POINTS;
            cfg->modbus.input_count = (uint16_t)val;
        }
    }

    cJSON *cctalk_obj = cJSON_GetObjectItem(root, "cctalk");
    if (cctalk_obj) {
        cJSON *address = cJSON_GetObjectItem(cctalk_obj, "address");
        if (address && cJSON_IsNumber(address)) {
            int val = address->valueint;
            if (val < 1) val = 1;
            if (val > 255) val = 255;
            cfg->cctalk.address = (uint8_t)val;
        }
    }

    cJSON *cctalk_serial_obj = cJSON_GetObjectItem(root, "cctalk_serial");
    if (cctalk_serial_obj) {
        cJSON *addr = cJSON_GetObjectItem(cctalk_serial_obj, "addr");
        if (addr && cJSON_IsNumber(addr)) {
            int val = addr->valueint;
            if (val < 1) val = 1;
            if (val > 255) val = 255;
            cfg->cctalk.address = (uint8_t)val;
        }
    }

    // MDB cfg
    cJSON *mdb_s_obj = cJSON_GetObjectItem(root, "mdb_ser");
    if (!mdb_s_obj) mdb_s_obj = cJSON_GetObjectItem(root, "mdb_serial"); /* compat */
    if (mdb_s_obj) {
        web_ui_apply_serial_cfg_from_json(mdb_s_obj,
                                          &cfg->mdb_serial,
                                          &s_web_ui_serial_default_mdb);
    }

    // Remote logging configuration
    cJSON *remote_log_obj = cJSON_GetObjectItem(root, "rlog");
    if (!remote_log_obj) remote_log_obj = cJSON_GetObjectItem(root, "remote_log"); /* compat */
    if (remote_log_obj) {
        cJSON *server_port = cJSON_GetObjectItem(remote_log_obj, "port");
        if (!server_port) server_port = cJSON_GetObjectItem(remote_log_obj, "server_port"); /* compat */
        if (server_port) {
            cfg->remote_log.server_port = (uint16_t)server_port->valueint;
        }
        cfg->remote_log.use_broadcast = cJSON_IsTrue(cJSON_GetObjectItem(remote_log_obj, "bcast")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(remote_log_obj, "use_broadcast")); /* compat */
        cfg->remote_log.write_to_sd = cJSON_IsTrue(cJSON_GetObjectItem(remote_log_obj, "to_sd")) ||
            cJSON_IsTrue(cJSON_GetObjectItem(remote_log_obj, "write_to_sd")); /* compat */
        ESP_LOGI(TAG, "[C] Remote logging config: port=%d, broadcast=%d, sd=%d",
                cfg->remote_log.server_port, cfg->remote_log.use_broadcast, cfg->remote_log.write_to_sd);
    }

    // Scanner configuration (API save)
    cJSON *scanner_obj = cJSON_GetObjectItem(root, "scanner");
    if (scanner_obj) {
        cJSON *_sc_en = cJSON_GetObjectItem(scanner_obj, "en");
        if (!_sc_en) _sc_en = cJSON_GetObjectItem(scanner_obj, "enabled");
        cfg->scanner.enabled = cJSON_IsTrue(_sc_en);
        ESP_LOGI(TAG, "[CONFIG] Scanner enabled set to: %d (from web UI)", cfg->scanner.enabled);
        cJSON *sv = cJSON_GetObjectItem(scanner_obj, "vid");
        if (sv) {
            if (cJSON_IsNumber(sv)) cfg->scanner.vid = (uint16_t)sv->valueint;
            else if (cJSON_IsString(sv) && sv->valuestring) cfg->scanner.vid = (uint16_t)strtoul(sv->valuestring, NULL, 0);
        }
        cJSON *sp = cJSON_GetObjectItem(scanner_obj, "pid");
        if (sp) {
            if (cJSON_IsNumber(sp)) cfg->scanner.pid = (uint16_t)sp->valueint;
            else if (cJSON_IsString(sp) && sp->valuestring) cfg->scanner.pid = (uint16_t)strtoul(sp->valuestring, NULL, 0);
        }
        cJSON *sdual = cJSON_GetObjectItem(scanner_obj, "dpid");
        if (!sdual) sdual = cJSON_GetObjectItem(scanner_obj, "dual_pid"); /* compat */
        if (sdual) {
            if (cJSON_IsNumber(sdual)) cfg->scanner.dual_pid = (uint16_t)sdual->valueint;
            else if (cJSON_IsString(sdual) && sdual->valuestring) cfg->scanner.dual_pid = (uint16_t)strtoul(sdual->valuestring, NULL, 0);
        }
        cJSON *scool = cJSON_GetObjectItem(scanner_obj, "cool");
        if (!scool) scool = cJSON_GetObjectItem(scanner_obj, "cooldown_ms"); /* compat */
        if (scool) {
            if (cJSON_IsNumber(scool) && scool->valueint > 0) {
                cfg->scanner.cooldown_ms = (uint32_t)scool->valueint;
            } else if (cJSON_IsString(scool) && scool->valuestring) {
                cfg->scanner.cooldown_ms = (uint32_t)strtoul(scool->valuestring, NULL, 0);
            }
            if (cfg->scanner.cooldown_ms < 500) cfg->scanner.cooldown_ms = 500;
            if (cfg->scanner.cooldown_ms > 60000) cfg->scanner.cooldown_ms = 60000;
        }
    }

    // Timeout applicativi
    cJSON *timeouts_obj = cJSON_GetObjectItem(root, "timeouts");
    if (timeouts_obj) {
        cJSON *t_prg = cJSON_GetObjectItem(timeouts_obj, "t_prg");
        if (!t_prg) t_prg = cJSON_GetObjectItem(timeouts_obj, "language_return_ms"); /* compat */
        if (t_prg) {
            if (cJSON_IsNumber(t_prg) && t_prg->valueint > 0) {
                cfg->timeouts.exit_programs_ms = (uint32_t)t_prg->valueint;
            } else if (cJSON_IsString(t_prg) && t_prg->valuestring) {
                cfg->timeouts.exit_programs_ms = (uint32_t)strtoul(t_prg->valuestring, NULL, 0);
            }
        }
        cJSON *t_lang = cJSON_GetObjectItem(timeouts_obj, "t_lang");
        if (t_lang) {
            if (cJSON_IsNumber(t_lang) && t_lang->valueint > 0) {
                cfg->timeouts.exit_language_ms = (uint32_t)t_lang->valueint;
            } else if (cJSON_IsString(t_lang) && t_lang->valuestring) {
                cfg->timeouts.exit_language_ms = (uint32_t)strtoul(t_lang->valuestring, NULL, 0);
            }
        }
        cJSON *pause_suspend = cJSON_GetObjectItem(timeouts_obj, "pause_max_suspend_sec");
        if (pause_suspend) {
            if (cJSON_IsNumber(pause_suspend)) {
                uint32_t val = (uint32_t)pause_suspend->valuedouble;
                if (val <= 65535) {
                    cfg->timeouts.pause_max_suspend_sec = val;
                }
            }
        }
        cJSON *pre_fine = cJSON_GetObjectItem(timeouts_obj, "pre_fine_ciclo_percent");
        if (pre_fine) {
            if (cJSON_IsNumber(pre_fine)) {
                uint8_t val = (uint8_t)pre_fine->valueint;
                if (val <= 99) {
                    cfg->timeouts.pre_fine_ciclo_percent = val;
                }
            }
        }
    }
    if (cfg->timeouts.exit_programs_ms < 1000U)  cfg->timeouts.exit_programs_ms = 1000U;
    if (cfg->timeouts.exit_programs_ms > 600000U) cfg->timeouts.exit_programs_ms = 600000U;
    if (cfg->timeouts.exit_language_ms < 1000U)  cfg->timeouts.exit_language_ms = 1000U;
    if (cfg->timeouts.exit_language_ms > 600000U) cfg->timeouts.exit_language_ms = 600000U;
    if (cfg->timeouts.idle_before_ads_ms < 1000U) cfg->timeouts.idle_before_ads_ms = 1000U;
    if (cfg->timeouts.idle_before_ads_ms > 600000U) cfg->timeouts.idle_before_ads_ms = 600000U;
    if (cfg->timeouts.ad_rotation_ms < 1000U) cfg->timeouts.ad_rotation_ms = 1000U;
    if (cfg->timeouts.ad_rotation_ms > 60000U) cfg->timeouts.ad_rotation_ms = 60000U;
    if (cfg->timeouts.credit_reset_timeout_ms < 1000U) cfg->timeouts.credit_reset_timeout_ms = 1000U;
    if (cfg->timeouts.credit_reset_timeout_ms > 3600000U) cfg->timeouts.credit_reset_timeout_ms = 3600000U;
    if (cfg->timeouts.pause_max_suspend_sec > 65535U) cfg->timeouts.pause_max_suspend_sec = 65535U;
    if (cfg->timeouts.pre_fine_ciclo_percent > 99U) cfg->timeouts.pre_fine_ciclo_percent = 99U;

    // UI multilingua
    cJSON *ui_obj = cJSON_GetObjectItem(root, "ui");
    if (ui_obj) {
        cJSON *user_lang = cJSON_GetObjectItem(ui_obj, "ulang");
        if (!user_lang) user_lang = cJSON_GetObjectItem(ui_obj, "user_language"); /* compat */
        if (!user_lang) user_lang = cJSON_GetObjectItem(ui_obj, "language"); /* compat */
        if (user_lang && cJSON_IsString(user_lang) && user_lang->valuestring) {
            strncpy(cfg->ui.user_language, user_lang->valuestring, sizeof(cfg->ui.user_language) - 1);
        }

        cJSON *backend_lang = cJSON_GetObjectItem(ui_obj, "blang");
        if (!backend_lang) backend_lang = cJSON_GetObjectItem(ui_obj, "backend_language"); /* compat */
        if (backend_lang && cJSON_IsString(backend_lang) && backend_lang->valuestring) {
            strncpy(cfg->ui.backend_language, backend_lang->valuestring, sizeof(cfg->ui.backend_language) - 1);
        }

        cJSON *program_end_sec = cJSON_GetObjectItem(ui_obj, "program_end_message_sec");
        if (!program_end_sec) program_end_sec = cJSON_GetObjectItem(ui_obj, "program_end_sec"); /* compat */
        if (program_end_sec && cJSON_IsNumber(program_end_sec)) {
            int sec = program_end_sec->valueint;
            if (sec < 0) sec = 0;
            if (sec > 10) sec = 10;
            cfg->ui.program_end_message_sec = (uint8_t)sec;
        }

        cJSON *records = cJSON_GetObjectItem(ui_obj, "records");
        if (!records) {
            records = cJSON_GetObjectItem(ui_obj, "texts");
        }
        if (records && cJSON_IsArray(records)) {
            char *records_json = cJSON_PrintUnformatted(records);
            if (records_json) {
                /* Save records associated with the user language (compat) */
                device_config_set_ui_texts_records_json(cfg->ui.user_language, records_json);
                web_ui_i18n_cache_invalidate();
                free(records_json);
            }
        }
    }

    /* root-level compatibility: ui_language -> user_language */
    cJSON *ui_lang_flat = cJSON_GetObjectItem(root, "ui_language");
    if (ui_lang_flat && cJSON_IsString(ui_lang_flat) && ui_lang_flat->valuestring) {
        strncpy(cfg->ui.user_language, ui_lang_flat->valuestring, sizeof(cfg->ui.user_language) - 1);
    }

    bool server_cfg_changed =
        (prev_server_enabled != cfg->server.enabled) ||
        (strncmp(prev_server_url, cfg->server.url, sizeof(prev_server_url)) != 0) ||
        (strncmp(prev_server_serial, cfg->server.serial, sizeof(prev_server_serial)) != 0) ||
        (strncmp(prev_server_password, cfg->server.password, sizeof(prev_server_password)) != 0);
    
    cfg->updated = true;
    device_config_save(cfg);
    if (server_cfg_changed) {
        esp_err_t hs_sync_err = http_services_sync_runtime_state(true);
        if (hs_sync_err != ESP_OK && cfg->server.enabled) {
            ESP_LOGW(TAG,
                     "[C] Sync runtime server remoto dopo save fallita: %s",
                     esp_err_to_name(hs_sync_err));
        }
    }
    // Applica stati task che dipendono dalla configurazione (es. display on/off)
    tasks_apply_n_run();
    /* Se la lingua è cambiata: carica la nuova tabella in PSRAM, invalida cache e
     * aggiorna i testi LVGL in runtime. Il client web si ricaricherà da sola. */
    {
        /* Backend language changed -> invalidate web i18n cache and load PSRAM for backend */
        if (strncmp(prev_backend_lang, cfg->ui.backend_language, sizeof(prev_backend_lang)) != 0) {
            ESP_LOGI(TAG, "[C] Backend lingua cambiata: '%s' -> '%s'", prev_backend_lang, cfg->ui.backend_language);
            web_ui_i18n_cache_invalidate();
            if (web_ui_i18n_load_language_psram(cfg->ui.backend_language) != ESP_OK) {
                ESP_LOGW(TAG, "[C] Impossibile caricare tabella lingua backend '%s' in PSRAM", cfg->ui.backend_language);
            }
        }

        /* User panel language changed -> refresh LVGL texts (uses device_config_get_ui_text_scoped)
         * and invalidate lookup cache so subsequent lookups use new language. */
        if (strncmp(prev_user_lang, cfg->ui.user_language, sizeof(prev_user_lang)) != 0) {
            ESP_LOGI(TAG, "[C] Pannello lingua cambiata: '%s' -> '%s'", prev_user_lang, cfg->ui.user_language);
            web_ui_i18n_cache_invalidate();
            lvgl_panel_refresh_texts();
        }
    }
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}




// Handler API GET /api/tasks

/**
 * @brief Ottiene le informazioni sulle attività.
 *
 * Questa funzione gestisce la richiesta HTTP per ottenere le informazioni sulle attività.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_tasks_get(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "[C] GET /api/tasks (Lettura da SPIFFS)");

    FILE *f = fopen("/spiffs/tasks.json", "r");
    if (!f) {
        ESP_LOGE(TAG, "[C] Impossibile aprire tasks.json");
        httpd_resp_set_status(req, "500");
        return httpd_resp_send(req, "{\"error\":\"File non trovato\"}", -1);
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 32768) {
        fclose(f);
        httpd_resp_set_status(req, "500");
        return httpd_resp_send(req, "{\"error\":\"Dimensione file non valida\"}", -1);
    }

    char *buf = malloc((size_t)file_size + 1);
    if (!buf) {
        fclose(f);
        httpd_resp_set_status(req, "500");
        return httpd_resp_send(req, "{\"error\":\"Out of memory\"}", -1);
    }
    fread(buf, 1, (size_t)file_size, f);
    fclose(f);
    buf[file_size] = '\0';

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, buf, (ssize_t)file_size);
    free(buf);
    return ret;
}

// Handler API POST /api/tasks/save

/**
 * @brief Salva le informazioni delle attività in un file.
 *
 * @param [in] req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_tasks_save(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "[C] POST /api/tasks/save");

    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 16384) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"Content-Length non valido\"}", -1);
    }

    char *buf = calloc(1, total_len + 1);
    if (!buf) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"Out of memory\"}", -1);
    }

    int received = 0;
    while (received < total_len) {
        int r = httpd_req_recv(req, buf + received, total_len - received);
        if (r <= 0) {
            free(buf);
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "{\"error\":\"Ricezione interrotta\"}", -1);
        }
        received += r;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "[C] tasks/save: JSON non valido");
        if (root) cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"JSON non valido\"}", -1);
    }
    
    // Serializza su file JSON con chiavi compatte (n/s/p/c/m/w/k + enum numerici)
    // Accetta in input sia formato esteso (name/state/...) sia compatto (n/s/...).
    int count = cJSON_GetArraySize(root);
    int converted = 0;
    cJSON *compact = cJSON_CreateArray();
    cJSON *item;
    cJSON_ArrayForEach(item, root) {
        cJSON *jn = cJSON_GetObjectItem(item, "name");
        if (!cJSON_IsString(jn)) {
            jn = cJSON_GetObjectItem(item, "n");
        }
        cJSON *js = cJSON_GetObjectItem(item, "state");
        if (!js) {
            js = cJSON_GetObjectItem(item, "s");
        }
        cJSON *jp = cJSON_GetObjectItem(item, "priority");
        if (!jp) {
            jp = cJSON_GetObjectItem(item, "p");
        }
        cJSON *jc = cJSON_GetObjectItem(item, "core");
        if (!jc) {
            jc = cJSON_GetObjectItem(item, "c");
        }
        cJSON *jm = cJSON_GetObjectItem(item, "period_ms");
        if (!jm) {
            jm = cJSON_GetObjectItem(item, "m");
        }
        cJSON *jw = cJSON_GetObjectItem(item, "stack_words");
        if (!jw) {
            jw = cJSON_GetObjectItem(item, "w");
        }
        cJSON *jk = cJSON_GetObjectItem(item, "stack_caps");
        if (!jk) {
            jk = cJSON_GetObjectItem(item, "k");
        }
        if (!cJSON_IsString(jn) || !jn->valuestring || jn->valuestring[0] == '\0') continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "n", jn->valuestring ? jn->valuestring : "");
        /* state: JS invia già int (0/1/2), ma accettiamo anche stringa per compatibilità */
        int sv = 0;
        if (js && cJSON_IsNumber(js)) { sv = js->valueint; }
        else if (js && cJSON_IsString(js)) {
            if (strcmp(js->valuestring, "run") == 0) sv = 1;
            else if (strcmp(js->valuestring, "pause") == 0) sv = 2;
        }
        cJSON_AddNumberToObject(o, "s", sv);
        cJSON_AddNumberToObject(o, "p", jp ? jp->valueint : 4);
        cJSON_AddNumberToObject(o, "c", jc ? jc->valueint : 0);
        cJSON_AddNumberToObject(o, "m", jm ? jm->valueint : 0);
        cJSON_AddNumberToObject(o, "w", jw ? jw->valueint : 2048);
        int kv = 0;
        if (jk && cJSON_IsNumber(jk)) { kv = jk->valueint; }
        else if (jk && cJSON_IsString(jk) && strcmp(jk->valuestring, "internal") == 0) { kv = 1; }
        cJSON_AddNumberToObject(o, "k", kv);
        cJSON_AddItemToArray(compact, o);
        converted++;
    }
    cJSON_Delete(root);
    if (count > 0 && converted == 0) {
        cJSON_Delete(compact);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"Formato task non riconosciuto\"}", -1);
    }
    char *json_out = cJSON_PrintUnformatted(compact);
    cJSON_Delete(compact);
    if (!json_out) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"JSON encode failed\"}", -1);
    }

    FILE *f = fopen("/spiffs/tasks.json", "w");
    if (!f) {
        ESP_LOGE(TAG, "[C] Impossibile aprire tasks.json per scrittura");
        free(json_out);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"Impossibile scrivere il file\"}", -1);
    }
    fputs(json_out, f);
    fclose(f);
    free(json_out);

    ESP_LOGI(TAG, "[C] Tasks salvate: %d voci", count);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
}

// Handler API POST /api/tasks/apply

/**
 * @brief Applica le impostazioni alle attività.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_tasks_apply(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "[C] POST /api/tasks/apply");
    
    // Ricarica la configurazione dal file CSV salvato su SPIFFS
    tasks_load_config("/spiffs/tasks.json");
    
    // Applica i cambiamenti ai task FreeRTOS
    tasks_apply_n_run();
    
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\",\"message\":\"Task applicati con successo\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

// Handler pagina test legacy rimosso (spostato in web_ui_test_pages.c)


// Handler API POST /api/config/reset

/**
 * @brief Resetta la configurazione dell'API.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * @note La funzione verifica se la funzionalità WEB_UI_FEATURE_ENDPOINT_PROGRAMS è abilitata.
 */
esp_err_t api_config_reset(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Configurazione in sola lettura in modalità APP", -1);
    }

    if (send_http_log) ESP_LOGI(TAG, "[C] POST /api/config/reset");
    device_config_reset_defaults();
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

// Handler API GET /api/config/timeouts
/**
 * @brief Restituisce la configurazione dei timeout applicativi.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_config_timeouts_get(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGD(TAG, "[C] GET /api/config/timeouts");
    device_config_t *cfg = device_config_get();
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(root, "t_prg", cfg->timeouts.exit_programs_ms);
    cJSON_AddNumberToObject(root, "t_lang", cfg->timeouts.exit_language_ms);
    cJSON_AddNumberToObject(root, "idle_before_ads_ms", cfg->timeouts.idle_before_ads_ms);
    cJSON_AddNumberToObject(root, "ad_rotation_ms", cfg->timeouts.ad_rotation_ms);
    cJSON_AddNumberToObject(root, "credit_reset_timeout_ms", cfg->timeouts.credit_reset_timeout_ms);
    
    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ESP_OK;
}

// Handler API POST /api/config/timeouts
/**
 * @brief Aggiorna la configurazione dei timeout applicativi.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t api_config_timeouts_post(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Configurazione in sola lettura in modalità APP", -1);
    }

    if (send_http_log) ESP_LOGI(TAG, "[C] POST /api/config/timeouts");
    
    /* Leggi il body della richiesta */
    char buf[1024] = {0};
    int bytes_read = httpd_req_recv(req, buf, sizeof(buf) - 1);
    
    if (bytes_read < 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Errore lettura payload", -1);
    }
    
    /* Parse JSON */
    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }
    
    device_config_t *cfg = device_config_get();
    
    /* Aggiorna i timeout se presenti nel JSON */
    cJSON *item;
    if ((item = cJSON_GetObjectItem(json, "t_prg")) != NULL && item->type == cJSON_Number) {
        cfg->timeouts.exit_programs_ms = (uint32_t)item->valueint;
    }
    /* compat: accetta il vecchio nome */
    if ((item = cJSON_GetObjectItem(json, "language_return_ms")) != NULL && item->type == cJSON_Number) {
        cfg->timeouts.exit_programs_ms = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "t_lang")) != NULL && item->type == cJSON_Number) {
        cfg->timeouts.exit_language_ms = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "idle_before_ads_ms")) != NULL && item->type == cJSON_Number) {
        cfg->timeouts.idle_before_ads_ms = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "ad_rotation_ms")) != NULL && item->type == cJSON_Number) {
        cfg->timeouts.ad_rotation_ms = (uint32_t)item->valueint;
    }
    if ((item = cJSON_GetObjectItem(json, "credit_reset_timeout_ms")) != NULL && item->type == cJSON_Number) {
        cfg->timeouts.credit_reset_timeout_ms = (uint32_t)item->valueint;
    }
    
    cJSON_Delete(json);
    
    /* Salva la configurazione */
    cfg->updated = true;
    esp_err_t err = device_config_save(cfg);
    
    httpd_resp_set_type(req, "application/json");
    const char *response;
    if (err == ESP_OK) {
        response = "{\"status\":\"ok\",\"message\":\"Timeout configurati correttamente\"}";
    } else {
        response = "{\"status\":\"error\",\"message\":\"Errore salvataggio configurazione\"}";
    }
    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

// Handler API POST /api/ntp/sync

/**
 * @brief Sincronizza l'orologio del dispositivo con il server NTP.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * @note La funzione verifica se la funzionalità WEB_UI_FEATURE_ENDPOINT_PROGRAMS è abilitata prima di procedere con la sincronizzazione NTP.
 */
esp_err_t api_ntp_sync(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Configurazione in sola lettura in modalità APP", -1);
    }

    if (send_http_log) ESP_LOGI(TAG, "[C] POST /api/ntp/sync");
    httpd_resp_set_type(req, "application/json");
    
    esp_err_t ret = init_sync_ntp();
    if (ret == ESP_OK) {
        const char *ok_resp = "{\"status\":\"ok\",\"message\":\"NTP synchronization completed successfully\"}";
        httpd_resp_send(req, ok_resp, strlen(ok_resp));
    } else {
        const char *err_resp = "{\"status\":\"error\",\"message\":\"NTP synchronization failed\"}";
        httpd_resp_send(req, err_resp, strlen(err_resp));
    }
    return ESP_OK;
}


/**
 * @brief Gestisce la richiesta di crash di debug.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * 
 * @note Questa funzione viene chiamata quando viene ricevuta una richiesta di crash di debug.
 *       Controlla se la funzionalità di endpoint dei programmi è abilitata.
 *       Se la funzionalità non è abilitata, restituisce un errore.
 */
esp_err_t api_debug_crash(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Endpoint disponibile solo in Factory", -1);
    }

    if (send_http_log) ESP_LOGW(TAG, "[C] POST /api/debug/crash (factory)");
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\",\"message\":\"Crash intenzionale in corso\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));

    vTaskDelay(pdMS_TO_TICKS(120));
    volatile uint32_t *bad_ptr = (volatile uint32_t *)0x0;
    *bad_ptr = 0xC0DEFACE;
    return ESP_OK;
}


/**
 * @brief Ripristina la configurazione di debug.
 *
 * Questa funzione ripristina la configurazione di debug utilizzando i parametri forniti nella richiesta HTTP.
 *
 * @param req Puntatore alla richiesta HTTP contenente i parametri di configurazione di debug.
 * @return esp_err_t Codice di errore che indica il successo o la fallita dell'operazione.
 */
esp_err_t api_debug_restore(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running || running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Restore disponibile solo in APP", -1);
    }

    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                              ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                                              NULL);
    if (!factory) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Partizione Factory non trovata", -1);
    }

    esp_partition_subtype_t target_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    if (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
        target_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
    } else if (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) {
        target_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    }

    const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                             target_subtype,
                                                             NULL);
    if (!target) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Partizione OTA target non trovata", -1);
    }

    if (target->size < factory->size) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Dimensione OTA target insufficiente", -1);
    }

    if (send_http_log) ESP_LOGW(TAG, "[C] POST /api/debug/restore running=%s target=%s source=%s", running->label, target->label, factory->label);

    esp_err_t err = esp_partition_erase_range(target, 0, target->size);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Erase OTA target fallita", -1);
    }

    uint8_t *buf = malloc(4096);
    if (!buf) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Memoria insufficiente per restore", -1);
    }

    size_t copied = 0;
    while (copied < factory->size) {
        size_t chunk = factory->size - copied;
        if (chunk > 4096) {
            chunk = 4096;
        }

        err = esp_partition_read(factory, copied, buf, chunk);
        if (err != ESP_OK) {
            free(buf);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_send(req, "Lettura Factory fallita", -1);
        }

        err = esp_partition_write(target, copied, buf, chunk);
        if (err != ESP_OK) {
            free(buf);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_send(req, "Scrittura OTA target fallita", -1);
        }

        copied += chunk;
    }

    free(buf);

    char resp[160];
    snprintf(resp,
             sizeof(resp),
             "Restore completato: Factory (%s) copiata in %s (%u bytes)",
             factory->label,
             target->label,
             (unsigned)factory->size);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, resp, -1);
}


/**
 * @brief Promuove un dispositivo alla modalità di produzione utilizzando le informazioni fornite nel corpo della richiesta HTTP.
 *
 * @param req Puntatore alla struttura httpd_req_t che contiene le informazioni della richiesta HTTP.
 * @return esp_err_t Codice di errore che indica il successo o la fallita dell'operazione.
 */
esp_err_t api_debug_promote_factory(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Partizione running non disponibile", -1);
    }

    if (!web_ui_factory_features_override_get()) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Operazione disponibile solo in modalità Mantainer", -1);
    }

    if (!(running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
          running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Operazione consentita solo da runtime OTA", -1);
    }

    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                              ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                                              NULL);
    if (!factory) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Partizione Factory non trovata", -1);
    }

    if (factory->size < running->size) {
        ESP_LOGW(TAG,
                 "[C] promote_factory blocked: running=%s size=0x%08x, factory=%s size=0x%08x",
                 running->label,
                 (unsigned)running->size,
                 factory->label,
                 (unsigned)factory->size);
        char err_msg[220];
        snprintf(err_msg,
                 sizeof(err_msg),
                 "Dimensione Factory insufficiente: running %s=0x%X, factory %s=0x%X. Aggiornare partition table sul device.",
                 running->label,
                 (unsigned)running->size,
                 factory->label,
                 (unsigned)factory->size);
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_send(req, err_msg, -1);
    }

    if (send_http_log) ESP_LOGW(TAG, "[C] POST /api/debug/promote_factory running=%s target=%s", running->label, factory->label);

    esp_err_t err = esp_partition_erase_range(factory, 0, factory->size);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Erase Factory fallita", -1);
    }

    uint8_t *buf = malloc(4096);
    if (!buf) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Memoria insufficiente per copia", -1);
    }

    size_t copied = 0;
    while (copied < running->size) {
        size_t chunk = running->size - copied;
        if (chunk > 4096) {
            chunk = 4096;
        }

        err = esp_partition_read(running, copied, buf, chunk);
        if (err != ESP_OK) {
            free(buf);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_send(req, "Lettura OTA running fallita", -1);
        }

        err = esp_partition_write(factory, copied, buf, chunk);
        if (err != ESP_OK) {
            free(buf);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_send(req, "Scrittura Factory fallita", -1);
        }

        copied += chunk;
    }

    free(buf);

    char resp[160];
    snprintf(resp,
             sizeof(resp),
             "Copia completata: %s -> Factory (%u bytes)",
             running->label,
             (unsigned)running->size);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, resp, -1);
}

#if DNA_SYS_MONITOR == 0
/**
 * @brief API di diagnostica di sistema
 *
 * Endpoint `GET /api/sysinfo` usato dal frontend per visualizzare dati di
 * memoria e, quando disponibili, l'uso della CPU per core. La funzione viene
 * compilata solo se `DNA_SYS_MONITOR` è zero (feature monitor disabilitata nel
 * codice principale).
 *
 * La risposta è un oggetto JSON contenente:
 *   - heap.dram_free / heap.dram_total
 *   - heap.spiram_free / heap.spiram_total
 *   - cpu.available ("true"/"false")
 *   - cpu.core0_pct, cpu.core1_pct (se supportato)
 *   - uptime_s
 */
static esp_err_t sysinfo_get_handler(httpd_req_t *req)
{
    size_t dram_free  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t dram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t spi_free   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t spi_total  = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    long long uptime_s = (long long)(esp_timer_get_time() / 1000000LL);

    int core0_pct = -1, core1_pct = -1;
    bool cpu_available = false;

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    cpu_available = true;
    static uint32_t s_idle0 = 0, s_idle1 = 0, s_total = 0;
    UBaseType_t ntasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tbuf = heap_caps_malloc(ntasks * sizeof(TaskStatus_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (tbuf) {
        uint32_t total_time = 0;
        UBaseType_t got = uxTaskGetSystemState(tbuf, ntasks, &total_time);
        uint32_t idle0 = 0, idle1 = 0;
        for (UBaseType_t i = 0; i < got; i++) {
            if (strcmp(tbuf[i].pcTaskName, "IDLE0") == 0) idle0 = tbuf[i].ulRunTimeCounter;
            if (strcmp(tbuf[i].pcTaskName, "IDLE1") == 0) idle1 = tbuf[i].ulRunTimeCounter;
        }
        heap_caps_free(tbuf);
        if (s_total != 0 && total_time > s_total) {
            uint32_t dt = total_time - s_total;
            uint32_t d0 = (idle0 >= s_idle0) ? (idle0 - s_idle0) : 0;
            uint32_t d1 = (idle1 >= s_idle1) ? (idle1 - s_idle1) : 0;
            /* total_time è un contatore wall-clock (esp_timer µs): ogni core
             * accumula indipendentemente fino a dt µs — formula semplice. */
            core0_pct = (int)(100 - (int64_t)d0 * 100 / dt);
            core1_pct = (int)(100 - (int64_t)d1 * 100 / dt);
            if (core0_pct < 0)   core0_pct = 0;
            if (core1_pct < 0)   core1_pct = 0;
            if (core0_pct > 100) core0_pct = 100;
            if (core1_pct > 100) core1_pct = 100;
        }
        s_idle0 = idle0;  s_idle1 = idle1;  s_total = total_time;
    }
#endif /* CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS */

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"heap\":{\"dram_free\":%zu,\"dram_total\":%zu"
        ",\"spiram_free\":%zu,\"spiram_total\":%zu}"
        ",\"cpu\":{\"available\":%s,\"core0_pct\":%d,\"core1_pct\":%d}"
        ",\"uptime_s\":%lld}",
        dram_free, dram_total, spi_free, spi_total,
        cpu_available ? "true" : "false",
        core0_pct, core1_pct, uptime_s);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, -1);
}

/**
 * @brief Espone l'output testuale di vTaskGetRunTimeStats.
 *
 * Endpoint `GET /api/runtime_stats` usato dalla Home (sezione Sistema) per
 * visualizzare la tabella runtime dei task FreeRTOS.
 */
static int runtime_stats_cmp_desc(const void *a, const void *b)
{
    const TaskStatus_t *ta = (const TaskStatus_t *)a;
    const TaskStatus_t *tb = (const TaskStatus_t *)b;

    if (ta->ulRunTimeCounter < tb->ulRunTimeCounter) {
        return 1;
    }
    if (ta->ulRunTimeCounter > tb->ulRunTimeCounter) {
        return -1;
    }

    const char *na = ta->pcTaskName ? ta->pcTaskName : "";
    const char *nb = tb->pcTaskName ? tb->pcTaskName : "";
    return strcmp(na, nb);
}


/**
 * @brief Gestisce la richiesta per ottenere le statistiche di runtime.
 *
 * @param req Puntatore alla richiesta HTTPD.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t runtime_stats_get_handler(httpd_req_t *req)
{
#if defined(CONFIG_FREERTOS_USE_TRACE_FACILITY) && (CONFIG_FREERTOS_USE_TRACE_FACILITY == 1) && \
    defined(CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS) && (CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS == 1) && \
    defined(CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS) && (CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS == 1)
    UBaseType_t ntasks = uxTaskGetNumberOfTasks();
    if (ntasks == 0) {
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_sendstr(req, "Nessun task disponibile.");
    }

    size_t cap = (size_t)ntasks + 4U;
    TaskStatus_t *tasks = calloc(cap, sizeof(TaskStatus_t));
    if (!tasks) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    uint32_t total_runtime = 0;
    UBaseType_t filled = uxTaskGetSystemState(tasks, (UBaseType_t)cap, &total_runtime);
    if (filled == 0) {
        free(tasks);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    qsort(tasks, filled, sizeof(TaskStatus_t), runtime_stats_cmp_desc);

    size_t out_len = ((size_t)filled * 96U) + 256U;
    char *stats = malloc(out_len);
    if (!stats) {
        free(tasks);
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    size_t used = 0;
    int wrote = snprintf(stats + used, out_len - used, "%-24s %12s %8s\n", "Task", "RunTime", "CPU");
    if (wrote < 0) {
        free(tasks);
        free(stats);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    used += (size_t)wrote;

    wrote = snprintf(stats + used, out_len - used, "------------------------------------------------\n");
    if (wrote < 0) {
        free(tasks);
        free(stats);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    used += (size_t)wrote;

    for (UBaseType_t i = 0; i < filled; ++i) {
        const char *name = tasks[i].pcTaskName ? tasks[i].pcTaskName : "?";
        uint32_t pct = 0;
        if (total_runtime > 0) {
            pct = (uint32_t)(((uint64_t)tasks[i].ulRunTimeCounter * 100ULL) / (uint64_t)total_runtime);
        }

        wrote = snprintf(stats + used, out_len - used,
                         "%-24.24s %12lu %7lu%%\n",
                         name,
                         (unsigned long)tasks[i].ulRunTimeCounter,
                         (unsigned long)pct);
        if (wrote < 0) {
            break;
        }
        if ((size_t)wrote >= (out_len - used)) {
            used = out_len - 1;
            stats[used] = '\0';
            break;
        }
        used += (size_t)wrote;
    }

    free(tasks);

    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    esp_err_t ret = httpd_resp_send(req, stats, HTTPD_RESP_USE_STRLEN);
    free(stats);
    return ret;
#else
    const char *msg =
        "Runtime stats non disponibili: abilita CONFIG_FREERTOS_USE_TRACE_FACILITY, "
        "CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS e CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS.";
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
#endif
}
#endif /* DNA_SYS_MONITOR == 0 */


esp_err_t api_runtime_enable_api_log_get(httpd_req_t *req)
{
    extern bool enable_api_log;
    if (send_http_log) ESP_LOGI(TAG, "[C] GET /api/runtime/enable_api_log");
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"enabled\":%s}", enable_api_log ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, -1);
}

esp_err_t api_runtime_enable_api_log_post(httpd_req_t *req)
{
    extern bool enable_api_log;
    char buf[128];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Bad request", -1);
    }
    buf[ret] = '\0';
    cJSON *root = cJSON_Parse(buf);
    if (root) {
        cJSON *item = cJSON_GetObjectItem(root, "enabled");
        if (item) {
            if (cJSON_IsBool(item)) enable_api_log = cJSON_IsTrue(item);
            else if (cJSON_IsNumber(item)) enable_api_log = (item->valueint != 0);
        }
        cJSON_Delete(root);
    } else {
        if (strstr(buf, "true") != NULL) enable_api_log = true;
        else enable_api_log = false;
    }
    /* Mirror to legacy flag so existing logging checks work */
    send_http_log = enable_api_log;
    /* Always log the state change, regardless of enable_api_log status */
    ESP_LOGI(TAG, "[C] POST /api/runtime/enable_api_log -> %s", enable_api_log ? "true" : "false");
    char resp[64];
    snprintf(resp, sizeof(resp), "{\"status\":\"ok\",\"enabled\":%s}", enable_api_log ? "true" : "false");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, -1);
}

/**
 * @brief Registra tutti gli endpoint HTTP del Web UI
 *
 * Questa funzione costruisce e registra le URI usate dal server, sia
 * quelle di sistema (radice, logo, ecc.) sia quelle dinamiche per task e
 * configurazioni. Viene invocata durante l'inizializzazione del server e
 * può essere chiamata nuovamente se gli handler devono essere reinseriti.
 *
 * @param server Handle del server HTTP.
 * @return ESP_OK se tutte le registrazioni vanno a buon fine, altrimenti il
 *         primo errore riscontrato.
 */
esp_err_t web_ui_register_handlers(httpd_handle_t server)
{
    // Registrazione URI di sistema (ex-init.c)
    httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
    httpd_register_uri_handler(server, &uri_root);
    
    httpd_uri_t uri_logo = {.uri = "/logo.jpg", .method = HTTP_GET, .handler = logo_get_handler};
    httpd_register_uri_handler(server, &uri_logo);
    
    httpd_uri_t uri_status = {.uri = "/status", .method = HTTP_GET, .handler = status_get_handler};
    httpd_register_uri_handler(server, &uri_status);

    httpd_uri_t uri_api_version = {.uri = "/api/version", .method = HTTP_GET, .handler = api_version_get};
    httpd_register_uri_handler(server, &uri_api_version);

#if DNA_SYS_MONITOR == 0
    httpd_uri_t uri_sysinfo = {.uri = "/api/sysinfo", .method = HTTP_GET, .handler = sysinfo_get_handler};
    httpd_register_uri_handler(server, &uri_sysinfo);

    httpd_uri_t uri_runtime_stats = {.uri = "/api/runtime_stats", .method = HTTP_GET, .handler = runtime_stats_get_handler};
    httpd_register_uri_handler(server, &uri_runtime_stats);
#endif

    httpd_uri_t uri_ota_get = {.uri = "/ota", .method = HTTP_GET, .handler = ota_get_handler};
    httpd_register_uri_handler(server, &uri_ota_get);
    
    httpd_uri_t uri_ota_post = {.uri = "/ota", .method = HTTP_POST, .handler = ota_post_handler};
    httpd_register_uri_handler(server, &uri_ota_post);

    httpd_uri_t uri_ota_upload = {.uri = "/ota/upload", .method = HTTP_POST, .handler = ota_upload_handler};
    httpd_register_uri_handler(server, &uri_ota_upload);

    // Registrazione URI Web UI (Pagine e API)
    httpd_uri_t uri_config = {.uri = "/config", .method = HTTP_GET, .handler = config_page_handler};
    httpd_register_uri_handler(server, &uri_config);

    /* Load i18n dictionary into PSRAM for current backend language */
    const char *lang = device_config_get_ui_backend_language();
    if (!lang) lang = "en";
    if (web_ui_i18n_load_language_psram(lang) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load i18n into PSRAM for language %s", lang);
    }
    
    httpd_uri_t uri_stats = {.uri = "/stats", .method = HTTP_GET, .handler = stats_page_handler};
    httpd_register_uri_handler(server, &uri_stats);

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_uri_t uri_programs = {.uri = "/config/programs", .method = HTTP_GET, .handler = programs_page_handler};
        httpd_register_uri_handler(server, &uri_programs);
    }
    
    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TASKS)) {
        httpd_uri_t uri_tasks = {.uri = "/tasks", .method = HTTP_GET, .handler = tasks_page_handler};
        httpd_register_uri_handler(server, &uri_tasks);
    }

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TEST)) {
        httpd_uri_t uri_test = {.uri = "/test", .method = HTTP_GET, .handler = test_page_handler};
        httpd_register_uri_handler(server, &uri_test);
        
        httpd_uri_t uri_led_bar_test = {.uri = "/led_bar_test", .method = HTTP_GET, .handler = led_bar_test_page_handler};
        httpd_register_uri_handler(server, &uri_led_bar_test);
        
        httpd_uri_t uri_digital_io_test = {.uri = "/digital_io_test", .method = HTTP_GET, .handler = digital_io_test_page_handler};
        httpd_register_uri_handler(server, &uri_digital_io_test);
    }

    httpd_uri_t uri_files = {.uri = "/files", .method = HTTP_GET, .handler = files_page_handler};
    httpd_register_uri_handler(server, &uri_files);

    httpd_uri_t uri_httpservices = {.uri = "/httpservices", .method = HTTP_GET, .handler = httpservices_page_handler};
    httpd_register_uri_handler(server, &uri_httpservices);
    
    httpd_uri_t uri_logs = {.uri = "/logs", .method = HTTP_GET, .handler = logs_page_handler};
    httpd_register_uri_handler(server, &uri_logs);

    httpd_uri_t uri_api_index = {.uri = "/api", .method = HTTP_GET, .handler = api_index_page_handler};
    httpd_register_uri_handler(server, &uri_api_index);
    ESP_LOGI(TAG, "Registered GET /api (index)");

    httpd_uri_t uri_api_index_slash = {.uri = "/api/", .method = HTTP_GET, .handler = api_index_page_handler};
    httpd_register_uri_handler(server, &uri_api_index_slash);
    ESP_LOGI(TAG, "Registered GET /api/ (index)");

    // API
    httpd_uri_t uri_api_get = {.uri = "/api/config", .method = HTTP_GET, .handler = api_config_get};
    httpd_register_uri_handler(server, &uri_api_get);

    httpd_uri_t uri_api_cctalk_log_get = {.uri = "/api/cctalk/log_txrx", .method = HTTP_GET, .handler = api_cctalk_log_txrx_get};
    httpd_register_uri_handler(server, &uri_api_cctalk_log_get);

    httpd_uri_t uri_api_cctalk_log_set = {.uri = "/api/cctalk/log_txrx", .method = HTTP_POST, .handler = api_cctalk_log_txrx_set};
    httpd_register_uri_handler(server, &uri_api_cctalk_log_set);

    /* Runtime flag: enable_api_log (GET/POST) */
    httpd_uri_t uri_api_runtime_get = {.uri = "/api/runtime/enable_api_log", .method = HTTP_GET, .handler = api_runtime_enable_api_log_get};
    httpd_register_uri_handler(server, &uri_api_runtime_get);
    httpd_uri_t uri_api_runtime_post = {.uri = "/api/runtime/enable_api_log", .method = HTTP_POST, .handler = api_runtime_enable_api_log_post};
    httpd_register_uri_handler(server, &uri_api_runtime_post);

    httpd_uri_t uri_api_ui_texts = {.uri = "/api/ui/texts", .method = HTTP_GET, .handler = api_ui_texts_get};
    httpd_register_uri_handler(server, &uri_api_ui_texts);

    httpd_uri_t uri_api_ui_languages = {.uri = "/api/ui/languages", .method = HTTP_GET, .handler = api_ui_languages_get};
    httpd_register_uri_handler(server, &uri_api_ui_languages);

    httpd_uri_t uri_api_lvgl_test = {.uri = "/api/display/lvgl_test", .method = HTTP_POST, .handler = api_display_lvgl_test};
    httpd_register_uri_handler(server, &uri_api_lvgl_test);

    httpd_uri_t uri_api_save = {.uri = "/api/config/save", .method = HTTP_POST, .handler = api_config_save};
    httpd_register_uri_handler(server, &uri_api_save);

    httpd_uri_t uri_api_backup = {.uri = "/api/config/backup", .method = HTTP_POST, .handler = api_config_backup};
    httpd_register_uri_handler(server, &uri_api_backup);
    
    httpd_uri_t uri_api_reset = {.uri = "/api/config/reset", .method = HTTP_POST, .handler = api_config_reset};
    httpd_register_uri_handler(server, &uri_api_reset);

    httpd_uri_t uri_api_timeouts_get = {.uri = "/api/config/timeouts", .method = HTTP_GET, .handler = api_config_timeouts_get};
    httpd_register_uri_handler(server, &uri_api_timeouts_get);

    httpd_uri_t uri_api_timeouts_post = {.uri = "/api/config/timeouts", .method = HTTP_POST, .handler = api_config_timeouts_post};
    httpd_register_uri_handler(server, &uri_api_timeouts_post);

    httpd_uri_t uri_api_ntp_sync = {.uri = "/api/ntp/sync", .method = HTTP_POST, .handler = api_ntp_sync};
    httpd_register_uri_handler(server, &uri_api_ntp_sync);

    httpd_uri_t uri_api_files_list = {.uri = "/api/files/list", .method = HTTP_GET, .handler = api_files_list_get};
    httpd_register_uri_handler(server, &uri_api_files_list);

    httpd_uri_t uri_api_files_upload = {.uri = "/api/files/upload", .method = HTTP_POST, .handler = api_files_upload_post};
    httpd_register_uri_handler(server, &uri_api_files_upload);

    httpd_uri_t uri_api_files_delete = {.uri = "/api/files/delete", .method = HTTP_POST, .handler = api_files_delete_post};
    httpd_register_uri_handler(server, &uri_api_files_delete);

    httpd_uri_t uri_api_files_download = {.uri = "/api/files/download", .method = HTTP_GET, .handler = api_files_download_get};
    httpd_register_uri_handler(server, &uri_api_files_download);

    httpd_uri_t uri_api_files_copy = {.uri = "/api/files/copy", .method = HTTP_POST, .handler = api_files_copy_post};
    httpd_register_uri_handler(server, &uri_api_files_copy);

    httpd_uri_t uri_api_remote_files_list = {.uri = "/api/remote/files/list", .method = HTTP_GET, .handler = api_files_list_get};
    httpd_register_uri_handler(server, &uri_api_remote_files_list);

    httpd_uri_t uri_api_remote_files_upload = {.uri = "/api/remote/files/upload", .method = HTTP_POST, .handler = api_files_upload_post};
    httpd_register_uri_handler(server, &uri_api_remote_files_upload);

    httpd_uri_t uri_api_remote_files_delete = {.uri = "/api/remote/files/delete", .method = HTTP_POST, .handler = api_files_delete_post};
    httpd_register_uri_handler(server, &uri_api_remote_files_delete);

    httpd_uri_t uri_api_remote_files_download = {.uri = "/api/remote/files/download", .method = HTTP_GET, .handler = api_files_download_get};
    httpd_register_uri_handler(server, &uri_api_remote_files_download);

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TASKS)) {
        httpd_uri_t uri_api_tasks = {.uri = "/api/tasks", .method = HTTP_GET, .handler = api_tasks_get};
        httpd_register_uri_handler(server, &uri_api_tasks);

        httpd_uri_t uri_api_tasks_save = {.uri = "/api/tasks/save", .method = HTTP_POST, .handler = api_tasks_save};
        httpd_register_uri_handler(server, &uri_api_tasks_save);

        httpd_uri_t uri_api_tasks_apply = {.uri = "/api/tasks/apply", .method = HTTP_POST, .handler = api_tasks_apply};
        httpd_register_uri_handler(server, &uri_api_tasks_apply);
    }

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TEST)) {
        httpd_uri_t uri_api_test = {.uri = "/api/test/*", .method = HTTP_POST, .handler = api_test_handler};
        httpd_register_uri_handler(server, &uri_api_test);
    }

    {
        httpd_uri_t uri_api_programs_get = {.uri = "/api/programs", .method = HTTP_GET, .handler = api_programs_get};
        httpd_register_uri_handler(server, &uri_api_programs_get);
    }

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_uri_t uri_api_programs_save = {.uri = "/api/programs/save", .method = HTTP_POST, .handler = api_programs_save};
        httpd_register_uri_handler(server, &uri_api_programs_save);

        httpd_uri_t uri_api_security_password = {.uri = "/api/security/password", .method = HTTP_POST, .handler = api_security_password};
        httpd_register_uri_handler(server, &uri_api_security_password);
    }

    httpd_uri_t uri_api_emulator_relay = {.uri = "/api/emulator/relay", .method = HTTP_POST, .handler = api_emulator_relay_control};
    httpd_register_uri_handler(server, &uri_api_emulator_relay);

    httpd_uri_t uri_api_emulator_coin = {.uri = "/api/emulator/coin", .method = HTTP_POST, .handler = api_emulator_coin_event};
    httpd_register_uri_handler(server, &uri_api_emulator_coin);

    httpd_uri_t uri_api_emulator_program_start = {.uri = "/api/emulator/program/start", .method = HTTP_POST, .handler = api_emulator_program_start};
    httpd_register_uri_handler(server, &uri_api_emulator_program_start);

    httpd_uri_t uri_api_emulator_program_stop = {.uri = "/api/emulator/program/stop", .method = HTTP_POST, .handler = api_emulator_program_stop};
    httpd_register_uri_handler(server, &uri_api_emulator_program_stop);

    httpd_uri_t uri_api_emulator_program_pause_toggle = {.uri = "/api/emulator/program/pause_toggle", .method = HTTP_POST, .handler = api_emulator_program_pause_toggle};
    httpd_register_uri_handler(server, &uri_api_emulator_program_pause_toggle);

    httpd_uri_t uri_api_emulator_fsm_messages = {.uri = "/api/emulator/fsm/messages", .method = HTTP_GET, .handler = api_emulator_fsm_messages_get};
    httpd_register_uri_handler(server, &uri_api_emulator_fsm_messages);
    
    httpd_uri_t uri_api_logs_get = {.uri = "/api/logs", .method = HTTP_GET, .handler = api_logs_get};
    httpd_register_uri_handler(server, &uri_api_logs_get);
    ESP_LOGI(TAG, "Registered GET /api/logs handler");

    httpd_uri_t uri_api_logs_receive = {.uri = "/api/logs/receive", .method = HTTP_POST, .handler = api_logs_receive};
    httpd_register_uri_handler(server, &uri_api_logs_receive);
    ESP_LOGI(TAG, "Registered POST /api/logs/receive handler");

    // API: lista livelli correnti per tag (usata dalla UI per costruire i combobox)
    httpd_uri_t uri_api_logs_levels = {.uri = "/api/logs/levels", .method = HTTP_GET, .handler = api_logs_levels_get};
    httpd_register_uri_handler(server, &uri_api_logs_levels);
    ESP_LOGI(TAG, "Registered GET /api/logs/levels handler");

    // API: imposta livello di log per un singolo tag
    httpd_uri_t uri_api_logs_set_level = {.uri = "/api/logs/level", .method = HTTP_POST, .handler = api_logs_set_level};
    httpd_register_uri_handler(server, &uri_api_logs_set_level);
    ESP_LOGI(TAG, "Registered POST /api/logs/level handler");

    httpd_uri_t uri_api_logs_network_get = {.uri = "/api/logs/network", .method = HTTP_GET, .handler = api_logs_network_get};
    httpd_register_uri_handler(server, &uri_api_logs_network_get);
    ESP_LOGI(TAG, "Registered GET /api/logs/network handler");

    httpd_uri_t uri_api_logs_network_set = {.uri = "/api/logs/network", .method = HTTP_POST, .handler = api_logs_network_set};
    httpd_register_uri_handler(server, &uri_api_logs_network_set);
    ESP_LOGI(TAG, "Registered POST /api/logs/network handler");

    httpd_uri_t uri_api_logs_options = {.uri = "/api/logs/*", .method = HTTP_OPTIONS, .handler = api_logs_options};
    httpd_register_uri_handler(server, &uri_api_logs_options);
    ESP_LOGI(TAG, "Registered OPTIONS /api/logs/* handler");

    // Register debug USB enumeration endpoint
    httpd_uri_t uri_api_debug_usb = {.uri = "/api/debug/usb/enumerate", .method = HTTP_GET, .handler = api_debug_usb_enumerate};
    httpd_register_uri_handler(server, &uri_api_debug_usb);
    ESP_LOGI(TAG, "Registered GET /api/debug/usb/enumerate handler");

    // Sperimentali: register POST /api/debug/usb/restart
    httpd_uri_t uri_api_debug_usb_restart = {.uri = "/api/debug/usb/restart", .method = HTTP_POST, .handler = api_debug_usb_restart};
    httpd_register_uri_handler(server, &uri_api_debug_usb_restart);
    ESP_LOGI(TAG, "Registered POST /api/debug/usb/restart handler (Sperimentali)");

    httpd_uri_t uri_api_debug_crash = {.uri = "/api/debug/crash", .method = HTTP_POST, .handler = api_debug_crash};
    httpd_register_uri_handler(server, &uri_api_debug_crash);
    ESP_LOGI(TAG, "Registered POST /api/debug/crash handler");

    httpd_uri_t uri_api_debug_restore = {.uri = "/api/debug/restore", .method = HTTP_POST, .handler = api_debug_restore};
    httpd_register_uri_handler(server, &uri_api_debug_restore);
    ESP_LOGI(TAG, "Registered POST /api/debug/restore handler");

    httpd_uri_t uri_api_debug_promote_factory = {.uri = "/api/debug/promote_factory", .method = HTTP_POST, .handler = api_debug_promote_factory};
    httpd_register_uri_handler(server, &uri_api_debug_promote_factory);
    ESP_LOGI(TAG, "Registered POST /api/debug/promote_factory handler");

    // Register API handlers from http_services component
    http_services_register_handlers(server);

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_EMULATOR)) {
        httpd_uri_t uri_emulator = {.uri = "/emulator", .method = HTTP_GET, .handler = emulator_page_handler_local};
        httpd_register_uri_handler(server, &uri_emulator);
    }

    httpd_uri_t uri_maintainer_enable = {.uri = "/maintainer/enable", .method = HTTP_GET, .handler = maintainer_enable_handler};
    httpd_register_uri_handler(server, &uri_maintainer_enable);

    // Reboot Handlers
    httpd_uri_t uri_reboot_factory = {.uri = "/reboot/factory", .method = HTTP_GET, .handler = reboot_factory_handler};
    httpd_register_uri_handler(server, &uri_reboot_factory);

    httpd_uri_t uri_reboot_app = {.uri = "/reboot/app", .method = HTTP_GET, .handler = reboot_app_handler};
    httpd_register_uri_handler(server, &uri_reboot_app);

    httpd_uri_t uri_reboot_app_last = {.uri = "/reboot/app_last", .method = HTTP_GET, .handler = reboot_app_last_handler};
    httpd_register_uri_handler(server, &uri_reboot_app_last);

    httpd_uri_t uri_reboot_ota0 = {.uri = "/reboot/ota0", .method = HTTP_GET, .handler = reboot_ota0_handler};
    httpd_register_uri_handler(server, &uri_reboot_ota0);

    httpd_uri_t uri_reboot_ota1 = {.uri = "/reboot/ota1", .method = HTTP_GET, .handler = reboot_ota1_handler};
    httpd_register_uri_handler(server, &uri_reboot_ota1);

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found_handler);

    return ESP_OK;
}
