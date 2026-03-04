#include "web_ui_internal.h"
#include "web_ui.h"
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
#define MAX_ALLOWED_LOG_LEVEL ESP_LOG_INFO  // non mostrare/applicare livelli > INFO

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
    ESP_LOGI(TAG, "[C] GET /logs");
    const char *extra_style = 
        ".section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px}.log-container{font-family:monospace;font-size:12px;background:#f8f9fa;border:1px solid #dee2e6;border-radius:4px;padding:15px;max-height:600px;overflow-y:auto;white-space:pre-wrap}"
        ".log-entry{margin:2px 0;padding:2px;border-radius:3px}"
        ".log-error{background:#f8d7da;color:#721c24}.log-warn{background:#fff3cd;color:#856404}.log-info{background:#d1ecf1;color:#0c5460}.log-debug{background:#e2e3e5;color:#383d41}"
        ".log-timestamp{color:#6c757d;font-weight:bold}.log-level{font-weight:bold;margin:0 8px}.log-tag{color:#495057;margin-right:8px}.log-message{color:#212529}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Log Remoto", extra_style, true);

    const char *body = 
        "<div class='container'>"
        "<div class='section'><h2>📋 Log Remoto Ricevuti</h2>"
        "<p>I log vengono ricevuti via UDP dal server configurato. Aggiorna la pagina per vedere i nuovi log.</p>"
        "<div id='levelsContainer' style='margin-bottom:12px;'></div>"
        "<div style='margin-bottom:10px;'>"
        "  <button id='applyLevelsBtn' style='margin-right:8px;padding:6px 10px;background:#3498db;color:white;border-radius:4px;border:none;'>Applica livelli</button>"
        "  <input id='logFilterInput' placeholder='Filtro? (usa * e ?)' style='width:260px;padding:6px;margin-right:6px;border-radius:4px;border:1px solid #ccc'/>"
        "  <button id='applyLogFilterBtn' style='padding:6px 10px;background:#2ecc71;color:white;border-radius:4px;border:none;margin-right:6px;'>Applica filtro</button>"
        "  <button id='clearLogFilterBtn' style='padding:6px 8px;background:#95a5a6;color:white;border-radius:4px;border:none;'>Pulisci filtro</button>"
        "  <label style='margin-left:12px;display:inline-flex;align-items:center;gap:6px;color:#2c3e50;font-weight:bold;'><input type='checkbox' id='autoScrollToggle'>Autoscroll</label>"
        "</div>"
        "<div class='log-container' id='logContainer'>"
        "In attesa di log...<br>"
        "Configura il logging remoto nella pagina <a href='/config'>Configurazione</a> per iniziare a ricevere log."
        "</div></div>"
        "</div>"
        "<script>"
        "const LEVELS = [{v:0,t:'NONE'},{v:1,t:'ERROR'},{v:2,t:'WARN'},{v:3,t:'INFO'},{v:4,t:'DEBUG'},{v:5,t:'VERBOSE'}];"
        "let AUTO_SCROLL = false;"
        "function levelToText(v){const l=LEVELS.find(x=>x.v===v);return l?l.t:v;}"
        "async function setLogLevel(tag, level){try{return fetch('/api/logs/level',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({tag:tag,level:level})}).then(r=>{if(!r.ok)throw r;return r.json();}).then(js=>{loadLevels();if(js.clamped)console.warn('Livello richiesto clamped to',js.applied);return true;}).catch(e=>{console.error(e);return false;});}catch(e){console.error(e);return false;}}"
        "async function loadLevels(){"
        "  try {"
        "    const r = await fetch('/api/logs/levels');"
        "    if (!r.ok) return;"
        "    const data = await r.json();"
        "    const container = document.getElementById('levelsContainer');"
        "    container.innerHTML = '';"
        "    if (data.length === 0) { container.innerHTML = '<em>Nessun tag disponibile</em>'; return; }"
        "    data.forEach(function(item){"
        "      var html = '<label style=\"font-weight:bold;margin-right:6px\">' + item.tag + '</label><select data-tag=\"' + item.tag + '\">';"
        "      LEVELS.forEach(function(l){"
        "        html += '<option value=\"' + l.v + '\"' + (l.v==item.level? ' selected' : '') + '>' + l.t + '</option>';"
        "      });"
        "      html += '</select>';"
        "      var wrapper = document.createElement('div'); wrapper.style.display = 'inline-block'; wrapper.style.marginRight = '10px';"
        "      wrapper.innerHTML = html;"
        "      container.appendChild(wrapper);"
        "    });"
        "    container.querySelectorAll('select').forEach(function(s){"
        "      s.addEventListener('change', async function(e){"
        "        var lv = parseInt(e.target.value); var tg = e.target.getAttribute('data-tag');"
        "        if (await setLogLevel(tg, lv)) { e.target.style.border = '2px solid #27ae60'; setTimeout(function(){ e.target.style.border = ''; }, 600); }"
        "      });"
        "    });"
        "  } catch (e) { console.error(e); }"
        "}"        
        "async function loadLogs(){"
        "try{"
        "const r=await fetch('/api/logs');if(!r.ok)throw new Error('Logs Error');"
        "const logs=await r.json();"
        "const container=document.getElementById('logContainer');"
        "if(logs.length===0){container.innerHTML='Nessun log ricevuto ancora.';return;}"
        "container.innerHTML='';"
        "logs.forEach(log=>{"
        "const entry=document.createElement('div');"
        "entry.className='log-entry log-'+log.level.toLowerCase();"
        "entry.innerHTML=`<span class='log-timestamp'>${log.timestamp}</span><span class='log-level'>[${log.level}]</span><span class='log-tag'>${log.tag}:</span><span class='log-message'>${log.message}</span>`;"
        "container.appendChild(entry);"
        "});"
        "if(AUTO_SCROLL) container.scrollTop=container.scrollHeight;"
        "}catch(e){console.error(e);document.getElementById('logContainer').innerHTML='Errore caricamento log: '+e;}"
        "}"
        "// Client-side log filter + bulk-apply levels (supports wildcards * and ?)\n"
        "var LOG_FILTER_RE = null;\n"
        "function wildcardToRegExp(pat){ if(!pat) return null; var specials='^$.*+?()[]{}|/\\\\'; var s=''; for(var i=0;i<pat.length;i++){ var ch=pat[i]; s += (specials.indexOf(ch)!==-1 ? ('\\\\'+ch) : ch); } s = s.split('\\\\*').join('.*').split('\\\\?').join('.'); try{ return new RegExp(s, 'i'); }catch(e){console.error('Invalid filter',e); return null;} }\n"
        "document.addEventListener('DOMContentLoaded', function(){\n"
        "  var applyBtn = document.getElementById('applyLevelsBtn');\n"
        "  if(applyBtn){ applyBtn.addEventListener('click', async function(){ var container = document.getElementById('levelsContainer'); if(!container) return; var selects = container.querySelectorAll('select[data-tag]'); applyBtn.disabled=true; applyBtn.innerText='Applicazione...'; for(const s of selects){ var tag = s.getAttribute('data-tag'); var lvl = parseInt(s.value); try{ await setLogLevel(tag,lvl); }catch(e){console.error(e);} } applyBtn.disabled=false; applyBtn.innerText='Applica livelli'; }); }\n"
        "  var fbtn = document.getElementById('applyLogFilterBtn'); var cbtn = document.getElementById('clearLogFilterBtn'); var finp = document.getElementById('logFilterInput');\n"
        "  var as=document.getElementById('autoScrollToggle'); if(as){ as.checked=false; as.addEventListener('change', function(){ AUTO_SCROLL = !!as.checked; }); }\n"
        "  if(fbtn && finp){ fbtn.addEventListener('click', function(){ var v=(finp.value||'').trim(); LOG_FILTER_RE = v ? wildcardToRegExp(v) : null; loadLogs(); }); }\n"
        "  if(cbtn && finp){ cbtn.addEventListener('click', function(){ finp.value=''; LOG_FILTER_RE = null; loadLogs(); }); }\n"
        "});\n"
        "(function(){ const _orig = loadLogs; loadLogs = async function(){ try{ const r = await fetch('/api/logs'); if(!r.ok) throw new Error('Logs Error'); const logs = await r.json(); const container = document.getElementById('logContainer'); if(!container) return; const filtered = LOG_FILTER_RE ? logs.filter(l => LOG_FILTER_RE.test(l.message) || LOG_FILTER_RE.test(l.tag) || LOG_FILTER_RE.test(l.level)) : logs; if(filtered.length===0){ container.innerHTML = LOG_FILTER_RE ? 'Nessun log ricevuto che soddisfi il filtro.' : 'Nessun log ricevuto ancora.'; return; } container.innerHTML = ''; filtered.forEach(log=>{ const entry=document.createElement('div'); entry.className='log-entry log-'+log.level.toLowerCase(); entry.innerHTML = `<span class=\'log-timestamp\'>${log.timestamp}</span><span class=\'log-level\'>[${log.level}]</span><span class=\'log-tag\'>${log.tag}:</span><span class=\'log-message\'>${log.message}</span>`; container.appendChild(entry); }); if(AUTO_SCROLL) container.scrollTop = container.scrollHeight; }catch(e){ console.error(e); document.getElementById('logContainer').innerHTML = 'Errore caricamento log: '+e; } }; })();\n"
        "window.addEventListener('load',()=>{loadLevels(); loadLogs();});\n"
        "setInterval(()=>{loadLogs(); loadLevels();},5000);"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
