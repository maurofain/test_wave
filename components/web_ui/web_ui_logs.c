#include "web_ui_internal.h"
#include "web_ui.h"
#include "device_config.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

/**
 * @file web_ui_logs.c
 * @brief Gestione endpoint e buffer interno per i log ricevuti via HTTP
 */
#include "cJSON.h"
#include "esp_http_server.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TAG "WEB_UI"
#define LOG_INITIAL_CAPACITY 300
#define LOG_CIRCULAR_CAPACITY 300
#define MAX_ALLOWED_LOG_LEVEL ESP_LOG_VERBOSE  // permetti DEBUG/VERBOSE (rimosso clamp su INFO)

/**
 * @brief Converte una stringa livello log nel corrispondente valore numerico ESP-IDF.
 *
 * @param s Livello testuale (es. `ERROR`, `WARN`, `INFO`).
 * @return Valore `esp_log_level_t` compatibile.
 */
static int level_str_to_num(const char *s)
{
    if (!s) return ESP_LOG_NONE;
    if (strcasecmp(s, "NONE") == 0) return ESP_LOG_NONE;
    if (strcasecmp(s, "ERROR") == 0) return ESP_LOG_ERROR;
    if (strcasecmp(s, "WARN") == 0 || strcasecmp(s, "WARNING") == 0) return ESP_LOG_WARN;
    if (strcasecmp(s, "INFO") == 0) return ESP_LOG_INFO;
    if (strcasecmp(s, "DEBUG") == 0) return ESP_LOG_DEBUG;
    if (strcasecmp(s, "VERBOSE") == 0) return ESP_LOG_VERBOSE;
    return ESP_LOG_INFO; // default
}

// Struttura per memorizzare i log ricevuti
typedef struct {
    char timestamp[20];
    char level[8];
    char tag[32];
    char message[256];
} stored_log_t;

static stored_log_t *initial_logs = NULL;
static stored_log_t *circular_logs = NULL;
static int initial_log_count = 0;
static int circular_log_count = 0;
static int circular_log_index = 0;

/**
 * @brief Garantisce l'allocazione dei buffer log in PSRAM.
 *
 * @return true se i buffer sono disponibili, false altrimenti.
 */
static bool ensure_logs_storage(void)
{
    if (initial_logs && circular_logs) {
        return true;
    }

    if (!initial_logs) {
        initial_logs = (stored_log_t *)heap_caps_calloc(LOG_INITIAL_CAPACITY, sizeof(stored_log_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (!circular_logs) {
        circular_logs = (stored_log_t *)heap_caps_calloc(LOG_CIRCULAR_CAPACITY, sizeof(stored_log_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    if (!(initial_logs && circular_logs)) {
        ESP_LOGE(TAG, "[C] Buffer log in PSRAM non disponibile");
        if (initial_logs) {
            free(initial_logs);
            initial_logs = NULL;
        }
        if (circular_logs) {
            free(circular_logs);
            circular_logs = NULL;
        }
        return false;
    }

    return true;
}

/**
 * @brief Copia un log nel doppio buffer: prima area storica, poi area circolare.
 *
 * @param level Livello log.
 * @param tag Tag log.
 * @param message Messaggio log.
 * @param timestamp Timestamp testuale.
 */
static void append_stored_log(const char *level, const char *tag, const char *message, const char *timestamp)
{
    if (!ensure_logs_storage()) {
        return;
    }

    stored_log_t *log = NULL;
    if (initial_log_count < LOG_INITIAL_CAPACITY) {
        log = &initial_logs[initial_log_count++];
    } else {
        log = &circular_logs[circular_log_index];
        circular_log_index = (circular_log_index + 1) % LOG_CIRCULAR_CAPACITY;
        if (circular_log_count < LOG_CIRCULAR_CAPACITY) {
            circular_log_count++;
        }
    }

    strncpy(log->level, level, sizeof(log->level) - 1);
    strncpy(log->tag, tag, sizeof(log->tag) - 1);
    strncpy(log->message, message, sizeof(log->message) - 1);
    strncpy(log->timestamp, timestamp, sizeof(log->timestamp) - 1);

    log->level[sizeof(log->level) - 1] = '\0';
    log->tag[sizeof(log->tag) - 1] = '\0';
    log->message[sizeof(log->message) - 1] = '\0';
    log->timestamp[sizeof(log->timestamp) - 1] = '\0';
}

/**
 * @brief Riceve un log remoto e lo memorizza nel buffer circolare locale.
 *
 * Endpoint: `POST /api/logs`.
 *
 * @param req Richiesta HTTP POST con body JSON.
 * @return ESP_OK dopo invio risposta HTTP.
 */
esp_err_t api_logs_receive(httpd_req_t *req)
{
    //ESP_LOGD(TAG, "[C] POST /api/logs");

    // Headers CORS per permettere richieste dal browser
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    char buf[512] = {0};
    int received = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (received <= 0) {
        const char *resp_str = "{\"error\":\"Empty body\"}";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        const char *resp_str = "{\"error\":\"Invalid JSON\"}";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }

    cJSON *level = cJSON_GetObjectItem(root, "level");
    cJSON *tag = cJSON_GetObjectItem(root, "tag");
    cJSON *message = cJSON_GetObjectItem(root, "message");
    cJSON *msg = cJSON_GetObjectItem(root, "msg");
    cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");

    const cJSON *effective_message = (message && cJSON_IsString(message)) ? message : msg;

    if (level && cJSON_IsString(level) && tag && cJSON_IsString(tag) && effective_message && cJSON_IsString(effective_message)) {
        if (!ensure_logs_storage()) {
            cJSON_Delete(root);
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_send(req, "{\"error\":\"PSRAM logs unavailable\"}", HTTPD_RESP_USE_STRLEN);
            return ESP_OK;
        }

        char ts[20] = {0};
        if (timestamp && cJSON_IsString(timestamp) && timestamp->valuestring[0] != '\0') {
            strncpy(ts, timestamp->valuestring, sizeof(ts) - 1);
            ts[sizeof(ts) - 1] = '\0';
        } else {
            time_t now = time(NULL);
            struct tm timeinfo;
            if (now != (time_t)-1) {
                localtime_r(&now, &timeinfo);
                strftime(ts, sizeof(ts), "%H:%M:%S", &timeinfo);
            } else {
                strncpy(ts, "??:??:??", sizeof(ts) - 1);
                ts[sizeof(ts) - 1] = '\0';
            }
        }

        append_stored_log(level->valuestring, tag->valuestring, effective_message->valuestring, ts);
    } else {
        cJSON_Delete(root);
        const char *resp_str = "{\"error\":\"Missing required fields: level, tag, message/msg\"}";
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }

    cJSON_Delete(root);
    const char *resp_str = "{\"status\":\"ok\"}";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

/**
 * @brief Aggiunge un log internamente (per uso da altri componenti)
 */
/**
 * @brief Inserisce un log nel buffer circolare usato dalla Web UI.
 *
 * @param level Livello testuale del log.
 * @param tag Tag sorgente.
 * @param message Messaggio da memorizzare.
 */
void web_ui_add_log(const char *level, const char *tag, const char *message)
{
    if (!ensure_logs_storage()) {
        return;
    }

    // Ottieni timestamp corrente
    time_t now = time(NULL);
    struct tm timeinfo;
    char timestamp[20];
    
    if (now != (time_t)-1) {
        localtime_r(&now, &timeinfo);
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &timeinfo);
    } else {
        strncpy(timestamp, "??:??:??", sizeof(timestamp));
    }

    append_stored_log(level ? level : "INFO", tag ? tag : "WEB_UI", message ? message : "", timestamp);
}

/**
 * @brief Restituisce i log memorizzati nel buffer circolare.
 *
 * Endpoint: `GET /api/logs`.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK dopo invio risposta JSON.
 */
esp_err_t api_logs_get(httpd_req_t *req)
{
    // Headers CORS per permettere richieste dal browser
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    cJSON *root = cJSON_CreateArray();

    if (ensure_logs_storage()) {
        for (int i = 0; i < initial_log_count; i++) {
            stored_log_t *log = &initial_logs[i];

            int msg_lvl = level_str_to_num(log->level);
            if (msg_lvl > MAX_ALLOWED_LOG_LEVEL) {
                continue;
            }

            cJSON *log_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(log_obj, "timestamp", log->timestamp);
            cJSON_AddStringToObject(log_obj, "level", log->level);
            cJSON_AddStringToObject(log_obj, "tag", log->tag);
            cJSON_AddStringToObject(log_obj, "message", log->message);
            cJSON_AddItemToArray(root, log_obj);
        }

        int start_idx = (circular_log_count < LOG_CIRCULAR_CAPACITY) ? 0 : circular_log_index;
        for (int i = 0; i < circular_log_count; i++) {
            int idx = (start_idx + i) % LOG_CIRCULAR_CAPACITY;
            stored_log_t *log = &circular_logs[idx];

            int msg_lvl = level_str_to_num(log->level);
            if (msg_lvl > MAX_ALLOWED_LOG_LEVEL) {
                continue;
            }

            cJSON *log_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(log_obj, "timestamp", log->timestamp);
            cJSON_AddStringToObject(log_obj, "level", log->level);
            cJSON_AddStringToObject(log_obj, "tag", log->tag);
            cJSON_AddStringToObject(log_obj, "message", log->message);
            cJSON_AddItemToArray(root, log_obj);
        }
    }

    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));

    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief Restituisce i livelli log correnti per i tag presenti nel buffer.
 *
 * Endpoint: `GET /api/logs/levels`.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK dopo invio risposta JSON.
 */
esp_err_t api_logs_levels_get(httpd_req_t *req)
{
//    ESP_LOGI(TAG, "[C] GET /api/logs/levels");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    cJSON *root = cJSON_CreateArray();
    if (ensure_logs_storage()) {
        // raccolta tag unici dalla parte iniziale
        for (int i = 0; i < initial_log_count; i++) {
            stored_log_t *log = &initial_logs[i];
            bool found = false;
            for (int j = 0; j < cJSON_GetArraySize(root); j++) {
                cJSON *item = cJSON_GetArrayItem(root, j);
                cJSON *t = cJSON_GetObjectItem(item, "tag");
                if (t && strcmp(t->valuestring, log->tag) == 0) {
                    found = true;
                    break;
                }
            }
            if (found) {
                continue;
            }
            int lvl = esp_log_level_get(log->tag);
            int applied = (lvl > MAX_ALLOWED_LOG_LEVEL) ? MAX_ALLOWED_LOG_LEVEL : lvl;
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddStringToObject(obj, "tag", log->tag);
            cJSON_AddNumberToObject(obj, "level", applied);
            cJSON_AddItemToArray(root, obj);
        }

        // raccolta tag unici dalla parte circolare (ordine cronologico)
        int start_idx = (circular_log_count < LOG_CIRCULAR_CAPACITY) ? 0 : circular_log_index;
        for (int i = 0; i < circular_log_count; i++) {
            int idx = (start_idx + i) % LOG_CIRCULAR_CAPACITY;
            stored_log_t *log = &circular_logs[idx];
            bool found = false;
            for (int j = 0; j < cJSON_GetArraySize(root); j++) {
                cJSON *item = cJSON_GetArrayItem(root, j);
                cJSON *t = cJSON_GetObjectItem(item, "tag");
                if (t && strcmp(t->valuestring, log->tag) == 0) {
                    found = true;
                    break;
                }
            }
            if (found) {
                continue;
            }
            int lvl = esp_log_level_get(log->tag);
            int applied = (lvl > MAX_ALLOWED_LOG_LEVEL) ? MAX_ALLOWED_LOG_LEVEL : lvl;
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddStringToObject(obj, "tag", log->tag);
            cJSON_AddNumberToObject(obj, "level", applied);
            cJSON_AddItemToArray(root, obj);
        }
    }

    char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

/**
 * @brief Imposta il livello log di un tag specifico con clamp di sicurezza.
 *
 * Endpoint: `POST /api/logs/level` con body JSON `{tag, level}`.
 *
 * @param req Richiesta HTTP POST.
 * @return ESP_OK dopo invio risposta JSON; ESP_FAIL in caso di body/JSON non valido.
 */
esp_err_t api_logs_set_level(httpd_req_t *req)
{
    //ESP_LOGI(TAG, "[C] POST /api/logs/level");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (ret <= 0) return ESP_FAIL;

    cJSON *root = cJSON_Parse(buf);
    if (!root) { httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON"); return ESP_FAIL; }

    cJSON *tag = cJSON_GetObjectItem(root, "tag");
    cJSON *level = cJSON_GetObjectItem(root, "level");
    if (!tag || !level) { cJSON_Delete(root); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing tag/level"); return ESP_FAIL; }

    int requested = level->valueint;
    if (requested < 0 || requested > 5) { cJSON_Delete(root); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid level"); return ESP_FAIL; }

    /* Applica clamp: non permettere livelli maggiori di MAX_ALLOWED_LOG_LEVEL */
    int applied = (requested > MAX_ALLOWED_LOG_LEVEL) ? MAX_ALLOWED_LOG_LEVEL : requested;
    esp_log_level_set(tag->valuestring, (esp_log_level_t)applied);

    /* Risposta JSON esplicita con valore richiesto e valore effettivamente applicato */
    cJSON *resp_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp_obj, "requested", requested);
    cJSON_AddNumberToObject(resp_obj, "applied", applied);
    cJSON_AddBoolToObject(resp_obj, "clamped", applied != requested);
    char *resp_str = cJSON_PrintUnformatted(resp_obj);

    cJSON_Delete(resp_obj);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    free(resp_str);
    return ESP_OK;
}

/**
 * @brief Restituisce lo stato di invio log in rete (broadcast UDP).
 *
 * Endpoint: `GET /api/logs/network`.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK dopo invio risposta JSON.
 */
esp_err_t api_logs_network_get(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    device_config_t *cfg = device_config_get();
    if (!cfg) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "{\"error\":\"config unavailable\"}", HTTPD_RESP_USE_STRLEN);
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON_AddBoolToObject(root, "enabled", cfg->remote_log.use_broadcast);
    cJSON_AddNumberToObject(root, "port", cfg->remote_log.server_port);

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
 * @brief Imposta lo stato di invio log in rete (broadcast UDP) e salva config.
 *
 * Endpoint: `POST /api/logs/network` con body JSON `{enabled: true|false}`.
 *
 * @param req Richiesta HTTP POST.
 * @return ESP_OK dopo invio risposta JSON.
 */
esp_err_t api_logs_network_set(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");

    device_config_t *cfg = device_config_get();
    if (!cfg) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "{\"error\":\"config unavailable\"}", HTTPD_RESP_USE_STRLEN);
    }

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
    if (!enabled) {
        enabled = cJSON_GetObjectItem(root, "use_broadcast");
    }

    bool use_broadcast = false;
    if (cJSON_IsBool(enabled)) {
        use_broadcast = cJSON_IsTrue(enabled);
    } else if (cJSON_IsNumber(enabled)) {
        use_broadcast = (enabled->valueint != 0);
    } else {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"missing/invalid enabled\"}", HTTPD_RESP_USE_STRLEN);
    }

    cfg->remote_log.use_broadcast = use_broadcast;
    cfg->updated = true;

    if (device_config_save(cfg) != ESP_OK) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"config save failed\"}", HTTPD_RESP_USE_STRLEN);
    }

    ESP_LOGI(TAG, "[C] Invio log rete %s (porta=%u)",
             use_broadcast ? "abilitato" : "disabilitato",
             (unsigned)cfg->remote_log.server_port);

    cJSON_Delete(root);

    cJSON *resp = cJSON_CreateObject();
    if (!resp) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddBoolToObject(resp, "enabled", cfg->remote_log.use_broadcast);
    cJSON_AddNumberToObject(resp, "port", cfg->remote_log.server_port);

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

/**
 * @brief Gestisce il preflight CORS per endpoint logs.
 *
 * Endpoint: `OPTIONS /api/logs`.
 *
 * @param req Richiesta HTTP OPTIONS.
 * @return ESP_OK dopo invio header CORS.
 */
esp_err_t api_logs_options(httpd_req_t *req)
{
    //ESP_LOGD(TAG, "[C] OPTIONS /api/logs");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_set_hdr(req, "Access-Control-Max-Age", "86400");

    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/**
 * @brief Renderizza la pagina web di monitoraggio log.
 *
 * Include visualizzazione log remoti, filtri e controlli livelli runtime.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la pagina viene inviata correttamente.
 */
esp_err_t logs_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "logs.html", "text/html; charset=utf-8");
}
