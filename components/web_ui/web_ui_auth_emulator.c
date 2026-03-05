#include "web_ui_internal.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "fsm.h"
#include "web_ui_programs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WEB_UI_EMU";

static const web_ui_program_entry_t *find_program_by_id(uint8_t program_id)
{
    const web_ui_program_table_t *table = web_ui_program_table_get();
    if (!table) {
        return NULL;
    }

    for (uint8_t i = 0; i < table->count; ++i) {
        const web_ui_program_entry_t *entry = &table->programs[i];
        if (entry->program_id == program_id) {
            return entry;
        }
    }

    return NULL;
}

#define WEB_UI_PROTECTED_PASSWORD "factoryUser"
#define WEB_UI_PASSWORD_FILE "/spiffs/ui_password.txt"

static char s_boot_password[64] = {0};
static bool s_boot_password_loaded = false;


/**
 * @brief Carica la password di avvio dalla configurazione web.
 * 
 * @param [in/out] s_boot_password_loaded Indica se la password di avvio è già stata caricata.
 * @return void
 */
static void web_ui_load_boot_password(void)
{
    if (s_boot_password_loaded) {
        return;
    }

    snprintf(s_boot_password, sizeof(s_boot_password), "%s", WEB_UI_PROTECTED_PASSWORD);

    FILE *file = fopen(WEB_UI_PASSWORD_FILE, "r");
    if (!file) {
        s_boot_password_loaded = true;
        return;
    }

    char line[64] = {0};
    if (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) >= 4) {
            snprintf(s_boot_password, sizeof(s_boot_password), "%s", line);
        }
    }

    fclose(file);
    s_boot_password_loaded = true;
}

const char *web_ui_boot_password_get(void)
{
    web_ui_load_boot_password();
    return s_boot_password;
}


/**
 * @brief Imposta la password di avvio per l'interfaccia web.
 * 
 * @param [in] new_password Nuova password da impostare. Deve essere una stringa non vuota, con una lunghezza compresa tra 4 e 32 caratteri.
 * @return esp_err_t Errore generato dalla funzione.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_ERR_INVALID_ARG: Argomento non valido (password nulla o lunghezza non valida).
 *         - ESP_ERR_NO_MEM: Memoria insufficiente per memorizzare la nuova password.
 */
esp_err_t web_ui_boot_password_set(const char *new_password)
{
    if (!new_password || strlen(new_password) < 4 || strlen(new_password) >= sizeof(s_boot_password)) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(WEB_UI_PASSWORD_FILE, "w");
    if (!file) {
        return ESP_FAIL;
    }

    size_t len = strlen(new_password);
    size_t written = fwrite(new_password, 1, len, file);
    fclose(file);
    if (written != len) {
        return ESP_FAIL;
    }

    snprintf(s_boot_password, sizeof(s_boot_password), "%s", new_password);
    s_boot_password_loaded = true;
    return ESP_OK;
}


/**
 * @brief Controlla se la richiesta HTTP ha una password valida.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return true se la password è valida, false altrimenti.
 */
bool web_ui_has_valid_password(httpd_req_t *req)
{
    if (!req) {
        return false;
    }

    char query[128] = {0};
    char password[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }
    if (httpd_query_key_value(query, "pwd", password, sizeof(password)) != ESP_OK) {
        return false;
    }

    return strcmp(password, web_ui_boot_password_get()) == 0;
}


/**
 * @brief Invia una richiesta di password all'interfaccia utente web.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @param title Titolo della pagina di autenticazione.
 * @param target URL o risorsa a cui l'utente deve essere reindirizzato dopo l'autenticazione.
 * @return esp_err_t Codice di errore.
 */
esp_err_t web_ui_send_password_required(httpd_req_t *req, const char *title, const char *target)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_status(req, "401 Unauthorized");

    char html[2048] = {0};
    snprintf(html, sizeof(html),
             "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>%s</title></head>"
             "<body style='font-family:Arial;background:#f5f5f5;padding:24px;'>"
             "<div style='max-width:440px;margin:40px auto;background:#fff;border-radius:10px;padding:20px;box-shadow:0 8px 24px rgba(0,0,0,.12)'>"
             "<h2 style='margin-top:0;color:#2c3e50;'>🔒 Password richiesta</h2>"
             "<p style='color:#566573;'>Inserire la password per continuare.</p>"
             "<form method='GET' action='%s' style='display:flex;flex-direction:column;gap:10px;'>"
             "<input name='pwd' type='password' autocomplete='current-password' placeholder='Password' style='padding:10px;border:1px solid #ccc;border-radius:6px;' required autofocus>"
             "<div style='display:flex;justify-content:flex-end;gap:8px;'>"
             "<a href='/' style='padding:10px 14px;background:#7f8c8d;color:#fff;text-decoration:none;border-radius:6px;'>Annulla</a>"
             "<button type='submit' style='padding:10px 14px;background:#3498db;color:white;border:none;border-radius:6px;cursor:pointer;'>Continua</button>"
             "</div>"
             "</form>"
             "</div></body></html>",
             title,
             target);

    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}


/**
 * @brief Gestisce la richiesta per la pagina dell'emulatore.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * 
 * @note Questa funzione verifica se la password della web UI è valida prima di procedere.
 */
esp_err_t emulator_page_handler_local(httpd_req_t *req)
{
    if (!web_ui_has_valid_password(req)) {
        return web_ui_send_password_required(req, "Emulator", "/emulator");
    }

#if WEB_UI_USE_EMBEDDED_PAGES == 0
    return webpages_send_external_or_error(req, "emulator.html", "text/html; charset=utf-8");
#else

    esp_err_t ext_page_ret = webpages_try_send_external(req, "emulator.html", "text/html; charset=utf-8");
    if (ext_page_ret == ESP_OK) {
        return ESP_OK;
    }

    const char *extra_style = WEBPAGE_EMULATOR_EXTRA_STYLE;

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Emulator", extra_style, false);

    const char *body = WEBPAGE_EMULATOR_BODY;

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
#endif
}


/**
 * @brief Gestisce gli eventi coin dell'emulatore.
 * 
 * Questa funzione gestisce gli eventi coin dell'emulatore. 
 * Se il metodo della richiesta non è HTTP_POST, la funzione restituisce ESP_FAIL.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * @retval ESP_OK Se il metodo della richiesta è HTTP_POST.
 * @retval ESP_FAIL Se il metodo della richiesta non è HTTP_POST.
 */
esp_err_t api_emulator_coin_event(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (!fsm_event_queue_init(0)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "FSM queue non disponibile", -1);
    }

    if (req->content_len <= 0 || req->content_len > 256) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char payload[256] = {0};
    int ret = httpd_req_recv(req, payload, req->content_len);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Errore lettura payload", -1);
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    cJSON *coin = cJSON_GetObjectItem(root, "coin");
    if (!cJSON_IsNumber(coin) || coin->valueint <= 0) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Campo coin obbligatorio e > 0", -1);
    }

    int value = coin->valueint;

    /* campo source opzionale: 'qr', 'card', 'cash' */
    char source[16] = "qr";
    cJSON *jsource = cJSON_GetObjectItem(root, "source");
    if (cJSON_IsString(jsource) && jsource->valuestring) {
        strncpy(source, jsource->valuestring, sizeof(source)-1);
        source[sizeof(source)-1] = '\0';
    }
    cJSON_Delete(root);

    {
        /* testo evento: '<source>_coin_emulator' per tracciabilità */
        char ev_text[FSM_EVENT_TEXT_MAX_LEN];
        snprintf(ev_text, sizeof(ev_text), "%s_coin_emulator", source);

        /* tipo evento in base alla sorgente del pagamento */
        fsm_input_event_type_t ev_type =
            (strcmp(source, "card") == 0) ? FSM_INPUT_EVENT_CARD_CREDIT :
            (strcmp(source, "qr")   == 0) ? FSM_INPUT_EVENT_QR_CREDIT   :
                                             FSM_INPUT_EVENT_TOKEN;

        fsm_input_event_t ev = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PAYMENT_ACCEPTED,
            .type = ev_type,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = value,
            .value_u32 = 0,
            .aux_u32 = 0,
            .text = {0},
        };
        strncpy(ev.text, ev_text, sizeof(ev.text)-1);
        if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
            ESP_LOGW(TAG, "Queue FSM piena/non disponibile per coin=%d", value);
            httpd_resp_set_status(req, "503 Service Unavailable");
            return httpd_resp_send(req, "Coda FSM piena", -1);
        }
    }

    char response[128];
    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"coin\":%d,\"source\":\"%s\"}", value, source);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}


/**
 * @brief Ottiene i messaggi dell'automa Finite State Machine (FSM) tramite una richiesta HTTP GET.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * @note Questa funzione gestisce solo le richieste HTTP GET. Qualsiasi altro metodo restituirà un errore.
 */
esp_err_t api_emulator_fsm_messages_get(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    (void)fsm_event_queue_init(0);

    char messages[16][FSM_EVENT_TEXT_MAX_LEN] = {{0}};
    size_t count = fsm_pending_messages_copy(messages, 16);
    fsm_ctx_t snapshot = {0};
    bool has_snapshot = fsm_runtime_snapshot(&snapshot);

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!root || !arr) {
        if (root) cJSON_Delete(root);
        if (arr) cJSON_Delete(arr);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    for (size_t i = 0; i < count; ++i) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(messages[i]));
    }
    cJSON_AddNumberToObject(root, "count", (double)count);
    cJSON_AddNumberToObject(root, "credit_cents", has_snapshot ? (double)snapshot.credit_cents : 0.0);
    cJSON_AddNumberToObject(root, "ecd_coins", has_snapshot ? (double)snapshot.ecd_coins : 0.0);
    cJSON_AddNumberToObject(root, "vcd_coins", has_snapshot ? (double)snapshot.vcd_coins : 0.0);
    cJSON_AddStringToObject(root, "state", has_snapshot ? fsm_state_to_string(snapshot.state) : "unknown");
    cJSON_AddNumberToObject(root, "running_elapsed_ms", has_snapshot ? (double)snapshot.running_elapsed_ms : 0.0);
    cJSON_AddNumberToObject(root, "running_target_ms",  has_snapshot ? (double)snapshot.running_target_ms  : 0.0);
    cJSON_AddNumberToObject(root, "running_price_units", has_snapshot ? (double)snapshot.running_price_units : 0.0);
    cJSON_AddNumberToObject(root, "pause_elapsed_ms", has_snapshot ? (double)snapshot.pause_elapsed_ms : 0.0);

    cJSON *relays = cJSON_CreateArray();
    if (!relays) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    for (uint8_t relay = 1; relay <= WEB_UI_VIRTUAL_RELAY_MAX; ++relay) {
        web_ui_virtual_relay_state_t state = {0};
        if (!web_ui_virtual_relay_get(relay, &state)) {
            continue;
        }
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            continue;
        }
        cJSON_AddNumberToObject(item, "relay_number", relay);
        cJSON_AddBoolToObject(item, "status", state.status);
        cJSON_AddNumberToObject(item, "duration", (double)state.duration_ms);
        cJSON_AddItemToArray(relays, item);
    }
    cJSON_AddItemToObject(root, "relays", relays);

    cJSON_AddItemToObject(root, "messages", arr);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t send_ret = httpd_resp_send(req, json, -1);
    cJSON_free(json);
    return send_ret;
}


/**
 * Avvia il programma emulatore.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return Errore generato dalla funzione.
 * @note La funzione controlla se il metodo della richiesta è HTTP_POST.
 */
esp_err_t api_emulator_program_start(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (!fsm_event_queue_init(0)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "FSM queue non disponibile", -1);
    }

    if (req->content_len <= 0 || req->content_len > 256) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char payload[256] = {0};
    int ret = httpd_req_recv(req, payload, req->content_len);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Errore lettura payload", -1);
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    cJSON *program_id = cJSON_GetObjectItem(root, "program_id");
    if (!cJSON_IsNumber(program_id) || program_id->valueint <= 0 || program_id->valueint > 255) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Campo program_id obbligatorio e valido", -1);
    }

    uint8_t pid = (uint8_t)program_id->valueint;
    cJSON_Delete(root);

    const web_ui_program_entry_t *entry = find_program_by_id(pid);
    if (!entry) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Programma non trovato\"}", -1);
    }

    if (!entry->enabled) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Programma disabilitato\"}", -1);
    }

    fsm_ctx_t snapshot = {0};
    bool has_snapshot = fsm_runtime_snapshot(&snapshot);
    if (!has_snapshot) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"FSM snapshot non disponibile\"}", -1);
    }

    if (snapshot.credit_cents < (int32_t)entry->price_units) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Credito insufficiente per il programma\"}", -1);
    }

    for (uint8_t relay = 1; relay <= WEB_UI_VIRTUAL_RELAY_MAX; ++relay) {
        bool on = ((entry->relay_mask & (1U << (relay - 1))) != 0);
        (void)web_ui_virtual_relay_control(relay, on, (uint32_t)entry->duration_sec * 1000U);
    }

    if (snapshot.state == FSM_STATE_RUNNING || snapshot.state == FSM_STATE_PAUSED) {
        /* cambio programma a macchina accesa: scala il tempo residuo, non fermare */
        fsm_input_event_t sw_ev = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PROGRAM_SELECTED,
            .type = FSM_INPUT_EVENT_PROGRAM_SWITCH,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = (int32_t)entry->price_units,
            .value_u32 = (uint32_t)entry->pause_max_suspend_sec * 1000U,
            .aux_u32   = (uint32_t)entry->duration_sec * 1000U,
            .text = {0},
        };
        snprintf(sw_ev.text, sizeof(sw_ev.text), "%s", entry->name);
        if (!fsm_event_publish(&sw_ev, pdMS_TO_TICKS(20))) {
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
        }
    } else {
        fsm_input_event_t event = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PROGRAM_SELECTED,
            .type = FSM_INPUT_EVENT_PROGRAM_SELECTED,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = (int32_t)entry->price_units,
            .value_u32 = (uint32_t)entry->pause_max_suspend_sec * 1000U,
            .aux_u32   = (uint32_t)entry->duration_sec * 1000U,
            .text = {0},
        };
        snprintf(event.text, sizeof(event.text), "%s", entry->name);
        if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
        }
    }

    char response[256];
    snprintf(response, sizeof(response),
             "{\"status\":\"ok\",\"program_id\":%u,\"name\":\"%s\",\"price_units\":%u,\"duration_sec\":%u,\"pause_max_suspend_sec\":%u,\"relay_mask\":%u}",
             (unsigned)pid,
             entry->name,
             (unsigned)entry->price_units,
             (unsigned)entry->duration_sec,
             (unsigned)entry->pause_max_suspend_sec,
             (unsigned)entry->relay_mask);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}


/**
 * @brief Arresta il programma emulatore.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * @retval ESP_OK Operazione riuscita.
 * @retval ESP_FAIL Operazione non riuscita.
 */
esp_err_t api_emulator_program_stop(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (!fsm_event_queue_init(0)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "FSM queue non disponibile", -1);
    }

    uint8_t pid = 0;
    if (req->content_len > 0 && req->content_len <= 256) {
        char payload[256] = {0};
        int ret = httpd_req_recv(req, payload, req->content_len);
        if (ret > 0) {
            cJSON *root = cJSON_Parse(payload);
            if (root) {
                cJSON *program_id = cJSON_GetObjectItem(root, "program_id");
                if (cJSON_IsNumber(program_id) && program_id->valueint > 0 && program_id->valueint <= 255) {
                    pid = (uint8_t)program_id->valueint;
                }
                cJSON_Delete(root);
            }
        }
    }

    for (uint8_t relay = 1; relay <= WEB_UI_VIRTUAL_RELAY_MAX; ++relay) {
        (void)web_ui_virtual_relay_control(relay, false, 0);
    }

    fsm_input_event_t event = {
        .from = AGN_ID_WEB_UI,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PROGRAM_STOP,

        .type = FSM_INPUT_EVENT_PROGRAM_STOP,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = (uint32_t)pid,
        .aux_u32 = 0,
        .text = {0},
    };
    snprintf(event.text, sizeof(event.text), "program_stop");

    if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
    }

    char response[128];
    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"program_id\":%u}", (unsigned)pid);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}


/**
 * @brief Gestisce la pausa e ripresa dell'emulatore.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * 
 * @note Questa funzione gestisce la pausa e ripresa dell'emulatore in risposta a una richiesta HTTP POST.
 * Se il metodo della richiesta non è HTTP_POST, la funzione restituisce ESP_FAIL.
 */
esp_err_t api_emulator_program_pause_toggle(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (!fsm_event_queue_init(0)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "FSM queue non disponibile", -1);
    }

    uint8_t pid = 0;
    if (req->content_len > 0 && req->content_len <= 256) {
        char payload[256] = {0};
        int ret = httpd_req_recv(req, payload, req->content_len);
        if (ret > 0) {
            cJSON *root = cJSON_Parse(payload);
            if (root) {
                cJSON *program_id = cJSON_GetObjectItem(root, "program_id");
                if (cJSON_IsNumber(program_id) && program_id->valueint > 0 && program_id->valueint <= 255) {
                    pid = (uint8_t)program_id->valueint;
                }
                cJSON_Delete(root);
            }
        }
    }

    fsm_input_event_t event = {
        .from = AGN_ID_WEB_UI,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PROGRAM_PAUSE_TOGGLE,

        .type = FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = (uint32_t)pid,
        .aux_u32 = 0,
        .text = {0},
    };
    snprintf(event.text, sizeof(event.text), "program_pause_toggle");

    if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
    }

    fsm_ctx_t snapshot = {0};
    const char *state = "unknown";
    if (fsm_runtime_snapshot(&snapshot)) {
        state = fsm_state_to_string(snapshot.state);
    }

    char response[160];
    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"program_id\":%u,\"state\":\"%s\"}", (unsigned)pid, state);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}
