#include "web_ui_internal.h"
#include "web_ui_programs.h"
#include "device_config.h"
#include "digital_io.h"
#include "lvgl_panel.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "WEB_UI_PROGRAMS_PAGE"

static bool programs_page_is_valid_language_code(const char *language)
{
    return language && strlen(language) == 2;
}

static esp_err_t programs_page_apply_name_translations(cJSON *root, bool *refresh_lvgl, char *err_msg, size_t err_msg_len)
{
    if (refresh_lvgl) {
        *refresh_lvgl = false;
    }

    if (!root) {
        if (err_msg && err_msg_len > 0) {
            snprintf(err_msg, err_msg_len, "payload traduzioni non valido");
        }
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *translations = cJSON_GetObjectItemCaseSensitive(root, "program_name_translations");
    if (!translations) {
        return ESP_OK;
    }

    if (!cJSON_IsObject(translations)) {
        if (err_msg && err_msg_len > 0) {
            snprintf(err_msg, err_msg_len, "program_name_translations non valido");
        }
        return ESP_ERR_INVALID_ARG;
    }

    const device_config_t *cfg = device_config_get();
    const char *user_language = (cfg && cfg->ui.user_language[0] != '\0') ? cfg->ui.user_language : "it";

    cJSON *language_item = NULL;
    cJSON_ArrayForEach(language_item, translations) {
        const char *language = language_item->string;
        if (!programs_page_is_valid_language_code(language) || !cJSON_IsObject(language_item)) {
            if (err_msg && err_msg_len > 0) {
                snprintf(err_msg, err_msg_len, "lingua traduzioni non valida");
            }
            return ESP_ERR_INVALID_ARG;
        }

        cJSON *program_item = NULL;
        cJSON_ArrayForEach(program_item, language_item) {
            if (!program_item->string || !cJSON_IsString(program_item) || !program_item->valuestring) {
                if (err_msg && err_msg_len > 0) {
                    snprintf(err_msg, err_msg_len, "nome programma tradotto non valido");
                }
                return ESP_ERR_INVALID_ARG;
            }

            char *end_ptr = NULL;
            long program_id = strtol(program_item->string, &end_ptr, 10);
            if (!end_ptr || *end_ptr != '\0' || program_id <= 0 || program_id > WEB_UI_PROGRAM_MAX) {
                if (err_msg && err_msg_len > 0) {
                    snprintf(err_msg, err_msg_len, "id programma tradotto fuori range");
                }
                return ESP_ERR_INVALID_ARG;
            }

            char program_key[24] = {0};
            snprintf(program_key, sizeof(program_key), "program_name_%02ld", program_id);
            esp_err_t err = device_config_update_program_text_i18n(program_key, language, program_item->valuestring);
            if (err != ESP_OK) {
                if (err_msg && err_msg_len > 0) {
                    snprintf(err_msg, err_msg_len, "salvataggio traduzioni programma fallito");
                }
                return err;
            }

            if (refresh_lvgl && strcmp(language, user_language) == 0) {
                *refresh_lvgl = true;
            }
        }
    }

    return ESP_OK;
}

/**
 * @brief Genera la pagina HTML di editing della tabella programmi
 *
 * Viene servita solo se l'opzione WEB_UI_FEATURE_ENDPOINT_PROGRAMS è
 * abilitata. L'HTML contiene la logica JS per caricare/salvare via API.
 */
esp_err_t programs_page_handler(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "404 Non Trovato");
        return httpd_resp_send(req, "404 Non Trovato", -1);
    }

    return webpages_send_external_or_error(req, "programs.html", "text/html; charset=utf-8");
}

/**
 * @brief API GET /api/programs
 *
 * Restituisce lo stato corrente della tabella programmi come JSON.
 */
esp_err_t api_programs_get(httpd_req_t *req)
{
    char *json = web_ui_program_table_to_json();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(json);
    if (root && cJSON_IsObject(root)) {
        cJSON_AddNumberToObject(root, "relay_outputs_count", DIGITAL_IO_OUTPUT_COUNT);
        cJSON_AddNumberToObject(root, "relay_local_count", DIGITAL_IO_LOCAL_OUTPUT_COUNT);
        cJSON_AddNumberToObject(root, "relay_modbus_group_count", DIGITAL_IO_MODBUS_OUTPUT_COUNT);

        char *enriched = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        free(json);

        if (!enriched) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        httpd_resp_set_type(req, "application/json");
        esp_err_t ret = httpd_resp_send(req, enriched, strlen(enriched));
        free(enriched);
        return ret;
    }

    if (root) {
        cJSON_Delete(root);
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

/**
 * @brief API POST /api/programs/save
 *
 * Riceve un payload JSON contenente l'elenco dei programmi, lo valida usando
 * web_ui_program_table_update_from_json e risponde con status ok o errore.
 */
esp_err_t api_programs_save(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "404 Non Trovato");
        return httpd_resp_send(req, "404 Non Trovato", -1);
    }

    if (req->content_len <= 0 || req->content_len > 16384) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char *payload = calloc(1, (size_t)req->content_len + 1);
    if (!payload) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, payload + received, req->content_len - received);
        if (r <= 0) {
            free(payload);
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "Errore lettura payload", -1);
        }
        received += r;
    }

    cJSON *root = cJSON_ParseWithLength(payload, (size_t)received);
    if (!root) {
        free(payload);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    char err_msg[128] = {0};
    esp_err_t err = web_ui_program_table_update_from_json(payload, (size_t)received, err_msg, sizeof(err_msg));
    bool refresh_lvgl = false;
    if (err == ESP_OK) {
        err = programs_page_apply_name_translations(root, &refresh_lvgl, err_msg, sizeof(err_msg));
    }
    cJSON_Delete(root);
    free(payload);

    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, err_msg[0] ? err_msg : "Errore validazione", -1);
    }

    (void)web_ui_program_table_get();
    if (refresh_lvgl) {
        lvgl_panel_refresh_texts();
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
}

/**
 * @brief API usata dall'emulatore per comandare relè virtuali
 *
 * Accetta JSON con relay_number, status e duration. Ritorna lo stato
 * aggiornato dei relè.
 */
esp_err_t api_emulator_relay_control(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 1024) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char *payload = calloc(1, (size_t)req->content_len + 1);
    if (!payload) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, payload + received, req->content_len - received);
        if (r <= 0) {
            free(payload);
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "Errore lettura payload", -1);
        }
        received += r;
    }

    cJSON *root = cJSON_Parse(payload);
    free(payload);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    cJSON *relay_number = cJSON_GetObjectItem(root, "relay_number");
    cJSON *status = cJSON_GetObjectItem(root, "status");
    cJSON *duration = cJSON_GetObjectItem(root, "duration");

    if (!cJSON_IsNumber(relay_number) || !cJSON_IsBool(status) || !cJSON_IsNumber(duration)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Campi relay_number/status/duration obbligatori", -1);
    }

    esp_err_t err = web_ui_virtual_relay_control((uint8_t)relay_number->valueint, cJSON_IsTrue(status), (uint32_t)duration->valueint);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "relay_number fuori range", -1);
    }

    char *json = web_ui_virtual_relays_to_json();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

/**
 * @brief API per cambiare la password protetta del boot
 *
 * Questa chiamata è protetta da feature e verifica payload minimo.
 */
esp_err_t api_security_password(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "404 Non Trovato");
        return httpd_resp_send(req, "404 Non Trovato", -1);
    }

    if (req->method == HTTP_GET) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"ok\",\"editable\":true}", -1);
    }

    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (req->content_len <= 0 || req->content_len > 1024) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char payload[1024] = {0};
    int len = httpd_req_recv(req, payload, req->content_len);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Errore lettura payload", -1);
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    cJSON *current = cJSON_GetObjectItem(root, "current_password");
    cJSON *next = cJSON_GetObjectItem(root, "new_password");
    if (!cJSON_IsString(current) || !cJSON_IsString(next)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Campi current_password/new_password obbligatori", -1);
    }

    if (strcmp(current->valuestring, web_ui_boot_password_get()) != 0) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Password attuale non valida", -1);
    }

    esp_err_t err = web_ui_boot_password_set(next->valuestring);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Nuova password non valida o errore salvataggio", -1);
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
}
