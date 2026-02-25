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

static const char *HTML_STYLE_NAV = 
    "nav{background:#000;padding:10px;display:flex;justify-content:center;gap:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}"
    "nav a{color:white;text-decoration:none;padding:8px 15px;border-radius:4px;background:#2c3e50;font-weight:bold;font-size:14px;transition:.2s}"
    "nav a:hover{background:#3498db}";

static const char *TAG = "WEB_UI_COMMON";

#define I18N_CACHE_MAX_ENTRIES 8

typedef struct {
    char lang[8];
    char scope[16];
    char *table_json;
} i18n_cache_entry_t;

static i18n_cache_entry_t s_i18n_cache[I18N_CACHE_MAX_ENTRIES];
static size_t s_i18n_cache_next_slot = 0;

static char *dup_cstr(const char *src)
{
    if (!src) {
        return NULL;
    }
    size_t len = strlen(src);
    char *dst = malloc(len + 1);
    if (!dst) {
        return NULL;
    }
    memcpy(dst, src, len + 1);
    return dst;
}

static const char *i18n_scope_for_uri(const char *uri)
{
    if (!uri || uri[0] == '\0') {
        return "p_runtime";
    }

    if (strncmp(uri, "/config/programs", 16) == 0) {
        return "p_programs";
    }
    if (strncmp(uri, "/config", 7) == 0) {
        return "p_config";
    }
    if (strncmp(uri, "/emulator", 9) == 0) {
        return "p_emulator";
    }
    if (strncmp(uri, "/logs", 5) == 0) {
        return "p_logs";
    }
    if (strncmp(uri, "/test", 5) == 0) {
        return "p_test";
    }

    return "p_runtime";
}

static bool i18n_scope_allowed(const char *scope, const char *page_scope)
{
    if (!scope || !page_scope) {
        return false;
    }

    if (strcmp(scope, "nav") == 0 ||
        strcmp(scope, "header") == 0 ||
        strcmp(scope, "lvgl") == 0) {
        return true;
    }

    return strcmp(scope, page_scope) == 0;
}

static char *i18n_cache_get(const char *lang, const char *page_scope)
{
    if (!lang || !page_scope) {
        return NULL;
    }

    for (size_t i = 0; i < I18N_CACHE_MAX_ENTRIES; i++) {
        i18n_cache_entry_t *entry = &s_i18n_cache[i];
        if (!entry->table_json) {
            continue;
        }
        if (strcmp(entry->lang, lang) == 0 && strcmp(entry->scope, page_scope) == 0) {
            return dup_cstr(entry->table_json);
        }
    }

    return NULL;
}

static void i18n_cache_put(const char *lang, const char *page_scope, const char *table_json)
{
    if (!lang || !page_scope || !table_json) {
        return;
    }

    char *copy = dup_cstr(table_json);
    if (!copy) {
        return;
    }

    for (size_t i = 0; i < I18N_CACHE_MAX_ENTRIES; i++) {
        i18n_cache_entry_t *entry = &s_i18n_cache[i];
        if (entry->table_json && strcmp(entry->lang, lang) == 0 && strcmp(entry->scope, page_scope) == 0) {
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

void web_ui_i18n_cache_invalidate(void)
{
    for (size_t i = 0; i < I18N_CACHE_MAX_ENTRIES; i++) {
        free(s_i18n_cache[i].table_json);
        s_i18n_cache[i].table_json = NULL;
        s_i18n_cache[i].lang[0] = '\0';
        s_i18n_cache[i].scope[0] = '\0';
    }
    s_i18n_cache_next_slot = 0;
}

static void add_table_mapping(cJSON *table, const char *key, const char *value)
{
    if (!table || !key || !value || key[0] == '\0') {
        return;
    }
    if (!cJSON_GetObjectItemCaseSensitive(table, key)) {
        cJSON_AddStringToObject(table, key, value);
    }
}

static char *build_i18n_table_json(const char *records_json, const char *base_records_json)
{
    if (!records_json) {
        return NULL;
    }

    cJSON *records = cJSON_Parse(records_json);
    if (!records || !cJSON_IsArray(records)) {
        if (records) {
            cJSON_Delete(records);
        }
        return NULL;
    }

    cJSON *table = cJSON_CreateObject();
    if (!table) {
        cJSON_Delete(records);
        return NULL;
    }

    cJSON *base_lookup = NULL;
    if (base_records_json && base_records_json[0] != '\0') {
        cJSON *base_records = cJSON_Parse(base_records_json);
        if (base_records && cJSON_IsArray(base_records)) {
            base_lookup = cJSON_CreateObject();
            if (base_lookup) {
                cJSON *b_item = NULL;
                cJSON_ArrayForEach(b_item, base_records) {
                    if (!cJSON_IsObject(b_item)) {
                        continue;
                    }
                    cJSON *b_scope = cJSON_GetObjectItem(b_item, "scope");
                    cJSON *b_key = cJSON_GetObjectItem(b_item, "key");
                    cJSON *b_text = cJSON_GetObjectItem(b_item, "text");
                    if (!cJSON_IsString(b_scope) || !b_scope->valuestring ||
                        !cJSON_IsString(b_key) || !b_key->valuestring ||
                        !cJSON_IsString(b_text) || !b_text->valuestring) {
                        continue;
                    }
                    char b_scoped_key[96] = {0};
                    snprintf(b_scoped_key, sizeof(b_scoped_key), "%s.%s", b_scope->valuestring, b_key->valuestring);
                    add_table_mapping(base_lookup, b_scoped_key, b_text->valuestring);
                }
            }
        }
        if (base_records) {
            cJSON_Delete(base_records);
        }
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
        add_table_mapping(table, scoped_key, text->valuestring);

        add_table_mapping(table, key->valuestring, text->valuestring);

        if (base_lookup) {
            cJSON *b_text = cJSON_GetObjectItemCaseSensitive(base_lookup, scoped_key);
            if (cJSON_IsString(b_text) && b_text->valuestring && strcmp(b_text->valuestring, text->valuestring) != 0) {
                add_table_mapping(table, b_text->valuestring, text->valuestring);

                const char *start = b_text->valuestring;
                while (*start && isspace((unsigned char)*start)) {
                    start++;
                }
                const char *end = b_text->valuestring + strlen(b_text->valuestring);
                while (end > start && isspace((unsigned char)end[-1])) {
                    end--;
                }
                size_t trim_len = (size_t)(end - start);
                if (trim_len > 0 && (start != b_text->valuestring || end != b_text->valuestring + strlen(b_text->valuestring))) {
                    char *trimmed = malloc(trim_len + 1);
                    if (trimmed) {
                        memcpy(trimmed, start, trim_len);
                        trimmed[trim_len] = '\0';
                        add_table_mapping(table, trimmed, text->valuestring);
                        free(trimmed);
                    }
                }
            }
        }
    }

    char *out = cJSON_PrintUnformatted(table);
    if (base_lookup) {
        cJSON_Delete(base_lookup);
    }
    cJSON_Delete(table);
    cJSON_Delete(records);
    return out;
}

static char *filter_i18n_records_json_for_scope(const char *records_json, const char *page_scope)
{
    if (!records_json || !page_scope) {
        return NULL;
    }

    cJSON *root = cJSON_Parse(records_json);
    if (!root || !cJSON_IsArray(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        return NULL;
    }

    cJSON *filtered = cJSON_CreateArray();
    if (!filtered) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (!cJSON_IsObject(item)) {
            continue;
        }

        cJSON *scope = cJSON_GetObjectItem(item, "scope");
        if (!cJSON_IsString(scope) || !scope->valuestring) {
            continue;
        }

        if (!i18n_scope_allowed(scope->valuestring, page_scope)) {
            continue;
        }

        cJSON *copy = cJSON_Duplicate(item, 1);
        if (!copy) {
            cJSON_Delete(filtered);
            cJSON_Delete(root);
            return NULL;
        }
        cJSON_AddItemToArray(filtered, copy);
    }

    char *out = cJSON_PrintUnformatted(filtered);
    cJSON_Delete(filtered);
    cJSON_Delete(root);
    return out;
}

static char *escape_script_end_tag(const char *src)
{
    if (!src) {
        return NULL;
    }

    const char *needle = "</script";
    const char *replacement = "<\\/script";
    const size_t needle_len = strlen(needle);
    const size_t repl_len = strlen(replacement);

    size_t count = 0;
    const char *p = src;
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += needle_len;
    }

    if (count == 0) {
        char *copy = malloc(strlen(src) + 1);
        if (!copy) {
            return NULL;
        }
        strcpy(copy, src);
        return copy;
    }

    size_t src_len = strlen(src);
    size_t out_len = src_len + (count * (repl_len - needle_len));
    char *out = malloc(out_len + 1);
    if (!out) {
        return NULL;
    }

    const char *in = src;
    char *dst = out;
    while ((p = strstr(in, needle)) != NULL) {
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

static esp_err_t send_i18n_runtime_script(httpd_req_t *req)
{
    const char *lang = device_config_get_ui_language();
    const char *page_scope = i18n_scope_for_uri(req ? req->uri : NULL);

    char *table_json = i18n_cache_get(lang, page_scope);
    if (!table_json) {
        char *texts_json = device_config_get_ui_texts_records_json(lang);
        if (!texts_json) {
            return ESP_ERR_NO_MEM;
        }

        char *filtered_texts_json = filter_i18n_records_json_for_scope(texts_json, page_scope);
        free(texts_json);
        if (!filtered_texts_json) {
            return ESP_ERR_NO_MEM;
        }

        char *base_filtered_json = NULL;
        if (strcmp(lang, "it") != 0) {
            char *base_json = device_config_get_ui_texts_records_json("it");
            if (base_json) {
                base_filtered_json = filter_i18n_records_json_for_scope(base_json, page_scope);
                free(base_json);
            }
        }

        table_json = build_i18n_table_json(filtered_texts_json, base_filtered_json);
        free(filtered_texts_json);
        if (base_filtered_json) {
            free(base_filtered_json);
        }
        if (!table_json) {
            return ESP_ERR_NO_MEM;
        }

        i18n_cache_put(lang, page_scope, table_json);
    }

    char *safe_texts_json = escape_script_end_tag(table_json);
    free(table_json);
    if (!safe_texts_json) {
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
    if (needed < 0) {
        free(safe_texts_json);
        return ESP_FAIL;
    }

    char *script = malloc((size_t)needed + 1);
    if (!script) {
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
esp_err_t send_head(httpd_req_t *req, const char *title, const char *extra_style, bool show_nav) {
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
    if (now != (time_t)-1) {
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
    if (emu_needed < 0) {
        return ESP_FAIL;
    }
    char *emu_button = malloc((size_t)emu_needed + 1);
    if (!emu_button) {
        return ESP_ERR_NO_MEM;
    }
    if (is_emulator_page) {
        snprintf(emu_button, (size_t)emu_needed + 1,
                 "<a href='/' style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>%s</a>",
                 txt_home);
    } else {
        snprintf(emu_button, (size_t)emu_needed + 1,
                 "<a href='#' onclick=\"return window.goProtectedPath('/emulator');\" style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>%s</a>",
                 txt_emulator);
    }

    const bool show_tasks = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TASKS);
    const bool show_test = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TEST);
    char txt_nav_home[24] = {0};
    char txt_nav_config[24] = {0};
    char txt_nav_stats[24] = {0};
    char txt_nav_tasks[24] = {0};
    char txt_nav_logs[24] = {0};
    char txt_nav_test[24] = {0};
    char txt_nav_ota[24] = {0};
    device_config_get_ui_text_scoped("nav", "home", "Home", txt_nav_home, sizeof(txt_nav_home));
    device_config_get_ui_text_scoped("nav", "config", "Config", txt_nav_config, sizeof(txt_nav_config));
    device_config_get_ui_text_scoped("nav", "stats", "Statistiche", txt_nav_stats, sizeof(txt_nav_stats));
    device_config_get_ui_text_scoped("nav", "tasks", "Task", txt_nav_tasks, sizeof(txt_nav_tasks));
    device_config_get_ui_text_scoped("nav", "logs", "Log", txt_nav_logs, sizeof(txt_nav_logs));
    device_config_get_ui_text_scoped("nav", "test", "Test", txt_nav_test, sizeof(txt_nav_test));
    device_config_get_ui_text_scoped("nav", "ota", "OTA", txt_nav_ota, sizeof(txt_nav_ota));

    char nav_tasks[96] = {0};
    char nav_test[96] = {0};
    if (show_tasks) {
        snprintf(nav_tasks, sizeof(nav_tasks), "<a href='/tasks'>📋 %s</a>", txt_nav_tasks);
    }
    if (show_test) {
        snprintf(nav_test, sizeof(nav_test), "<a href='/test'>🔧 %s</a>", txt_nav_test);
    }
    int nav_needed = snprintf(NULL, 0,
             "<nav><a href='/'>🏠 %s</a><a href='/config'>⚙️ %s</a><a href='/stats'>📈 %s</a>%s<a href='/logs'>📋 %s</a>%s<a href='/ota'>🔄 %s</a></nav>",
             txt_nav_home,
             txt_nav_config,
             txt_nav_stats,
             nav_tasks,
             txt_nav_logs,
             nav_test,
             txt_nav_ota);
    if (nav_needed < 0) {
        free(emu_button);
        return ESP_FAIL;
    }
    char *nav_html = malloc((size_t)nav_needed + 1);
    if (!nav_html) {
        free(emu_button);
        return ESP_ERR_NO_MEM;
    }
    snprintf(nav_html, (size_t)nav_needed + 1,
             "<nav><a href='/'>🏠 %s</a><a href='/config'>⚙️ %s</a><a href='/stats'>📈 %s</a>%s<a href='/logs'>📋 %s</a>%s<a href='/ota'>🔄 %s</a></nav>",
             txt_nav_home,
             txt_nav_config,
             txt_nav_stats,
             nav_tasks,
             txt_nav_logs,
             nav_test,
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
        "})();</script>", safe_title, show_nav?HTML_STYLE_NAV:"", safe_extra_style, safe_title, device_config_get_running_app_name(), time_str, emu_button, APP_VERSION, APP_DATE, show_nav?nav_html:"");
    if (needed < 0) {
        free(nav_html);
        free(emu_button);
        return ESP_FAIL;
    }

    char *buf = malloc((size_t)needed + 1);
    if (!buf) {
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
        "})();</script>", safe_title, show_nav?HTML_STYLE_NAV:"", safe_extra_style, safe_title, device_config_get_running_app_name(), time_str, emu_button, APP_VERSION, APP_DATE, show_nav?nav_html:"");
    esp_err_t send_ret = httpd_resp_sendstr_chunk(req, buf);
    free(buf);
    free(nav_html);
    free(emu_button);

    if (send_ret != ESP_OK) {
        if (send_ret != ESP_ERR_HTTPD_RESP_SEND) {
            ESP_LOGW(TAG, "send_head: errore invio header: %s", esp_err_to_name(send_ret));
        }
        return ESP_OK;
    }

    esp_err_t i18n_ret = send_i18n_runtime_script(req);
    if (i18n_ret != ESP_OK && i18n_ret != ESP_ERR_HTTPD_RESP_SEND) {
        ESP_LOGW(TAG, "send_head: errore script i18n: %s", esp_err_to_name(i18n_ret));
    }
    return ESP_OK;
}

// Sposto qui la response per /logo.jpg (era in web_ui.c). Rendendola visibile
// tramite header interno possiamo registrarla dagli altri file.
esp_err_t logo_get_handler(httpd_req_t *req)
{
    // Carica logo da filesystem (se presente) oppure restituisce 204
    FILE *f = fopen("/spiffs/logo.jpg", "rb");
    if (!f) {
        httpd_resp_set_status(req, "204 No Content");
        return httpd_resp_send(req, NULL, 0);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }
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
    if (!netif || !out) { if(out && len>0) out[0]='\0'; return; }
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
        ip4addr_ntoa_r((const ip4_addr_t *)&info.ip, out, len);
    } else {
        if(len>0) out[0]='\0';
    }
}

esp_err_t perform_ota(const char *url)
{
    if (!url || strlen(url) == 0) return ESP_ERR_INVALID_ARG;
    ESP_LOGI("WEB_UI", "Avvio OTA da %s", url);
    esp_http_client_config_t client_cfg = {.url = url, .timeout_ms = 15000, .cert_pem = NULL, .skip_cert_common_name_check = true};
    esp_https_ota_config_t ota_cfg = {.http_config = &client_cfg};
    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI("WEB_UI", "OTA riuscito. Riavvio in corso...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    return ret;
}

static bool is_factory_runtime(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    return (running && running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY);
}

const char *web_ui_profile_view_label(void)
{
    return is_factory_runtime() ? "Factory View" : "App View";
}

bool web_ui_feature_enabled(web_ui_feature_t feature)
{
    bool is_factory = is_factory_runtime();
    return web_ui_scope_allows(web_ui_feature_scope(feature), is_factory);
}
