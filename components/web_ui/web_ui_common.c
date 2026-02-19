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

static const char *HTML_STYLE_NAV = 
    "nav{background:#000;padding:10px;display:flex;justify-content:center;gap:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}"
    "nav a{color:white;text-decoration:none;padding:8px 15px;border-radius:4px;background:#2c3e50;font-weight:bold;font-size:14px;transition:.2s}"
    "nav a:hover{background:#3498db}";

static esp_err_t send_i18n_runtime_script(httpd_req_t *req)
{
    const char *lang = device_config_get_ui_language();
    char *texts_json = device_config_get_ui_texts_records_json(lang);
    if (!texts_json) {
        return ESP_ERR_NO_MEM;
    }

    const char *script_fmt =
        "<script>(function(){"
        "if(window.__ui_i18n_ready)return;"
        "window.__ui_i18n_ready=true;"
        "const lang='%s';"
        "const records=%s;"
        "const table={};"
        "for(const r of (Array.isArray(records)?records:[])){"
        "if(!r||!r.scope||!r.key)continue;"
        "const k=String(r.scope)+'.'+String(r.key);"
        "table[k]=String(r.text||'');"
        "if(table[String(r.key)]===undefined){table[String(r.key)]=String(r.text||'');}"
        "}"
        "function mapText(t){if(!t)return t;return (table[t]!==undefined&&table[t]!==null)?String(table[t]):t;}"
        "function applyNode(node){if(!node)return;"
        "if(node.nodeType===Node.TEXT_NODE){const v=node.nodeValue;if(!v)return;const tt=v.trim();if(!tt)return;"
        "const tr=mapText(tt);if(tr!==tt){node.nodeValue=v.replace(tt,tr);}return;}"
        "if(node.nodeType!==Node.ELEMENT_NODE)return;"
        "if(node.hasAttribute&&node.hasAttribute('data-i18n')){const k=node.getAttribute('data-i18n');if(k){const tr=mapText(k);if(tr&&tr!==k)node.textContent=tr;}}"
        "const attrs=['placeholder','title','aria-label','value'];"
        "for(const a of attrs){if(node.hasAttribute&&node.hasAttribute(a)){const ov=node.getAttribute(a);const nv=mapText(ov);if(nv!==ov)node.setAttribute(a,nv);}}"
        "for(const c of node.childNodes){applyNode(c);}"
        "}"
        "function apply(root){if(!root)return;applyNode(root);}"
        "window.uiI18n={language:lang,table:table,apply:apply,translate:mapText};"
        "document.addEventListener('DOMContentLoaded',function(){apply(document.body);"
        "const obs=new MutationObserver(function(ms){for(const m of ms){if(m.type==='characterData'){applyNode(m.target);}"
        "for(const n of m.addedNodes){applyNode(n);}}});"
        "obs.observe(document.body,{subtree:true,childList:true,characterData:true});"
        "});"
        "})();</script>";

    int needed = snprintf(NULL, 0, script_fmt, lang, texts_json);
    if (needed < 0) {
        free(texts_json);
        return ESP_FAIL;
    }

    char *script = malloc((size_t)needed + 1);
    if (!script) {
        free(texts_json);
        return ESP_ERR_NO_MEM;
    }

    snprintf(script, (size_t)needed + 1, script_fmt, lang, texts_json);
    free(texts_json);

    esp_err_t ret = httpd_resp_sendstr_chunk(req, script);
    free(script);
    return ret;
}

// Nota: questa funzione è usata da diverse pagine; non è più `static` perché
// sarà condivisa tra i file del componente web_ui dopo lo split.
esp_err_t send_head(httpd_req_t *req, const char *title, const char *extra_style, bool show_nav) {
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
        (req && strncmp(req->uri, "/emulator", 9) == 0 &&
         (req->uri[9] == '\0' || req->uri[9] == '?'));
    char txt_home[32] = {0};
    char txt_emulator[32] = {0};
    device_config_get_ui_text_scoped("nav", "home", "Home", txt_home, sizeof(txt_home));
    device_config_get_ui_text_scoped("nav", "emulator", "Emulatore", txt_emulator, sizeof(txt_emulator));

    char emu_button[256] = {0};
    if (is_emulator_page) {
        snprintf(emu_button, sizeof(emu_button),
                 "<a href='/' style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>%s</a>",
                 txt_home);
    } else {
        snprintf(emu_button, sizeof(emu_button),
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
    char nav_html[768] = {0};
    snprintf(nav_html, sizeof(nav_html),
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
        "<div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>%s [%s] - %s</h1>%s</div>"
        "<div style='text-align:right;font-size:12px;opacity:0.8;'>v%s (%s)</div>"
        "</header>"
        "%s"
        "<script>/* Global fetch wrapper: injects Authorization */"
        "(function(){if(window.__auth_wrapped) return; window.__auth_wrapped=true; const _fetch = window.fetch.bind(window);"
        "window.setAuthToken = function(t){ if(t) localStorage.setItem('httpservices_token', t); else localStorage.removeItem('httpservices_token'); };"
        "window.getAuthToken = function(){ return localStorage.getItem('httpservices_token'); };"
        "window.clearAuthToken = function(){ localStorage.removeItem('httpservices_token'); };"
        "window.fetch = function(input, init){ try{ const token = window.getAuthToken(); if(token){ init = init || {}; if(!init.headers){ init.headers = {'Authorization':'Bearer '+token}; } else if(init.headers instanceof Headers){ if(!init.headers.get('Authorization')) init.headers.set('Authorization','Bearer '+token); } else if(Array.isArray(init.headers)){ let has=false; for(const h of init.headers){ if(h[0].toLowerCase()==='authorization'){ has=true; break; } } if(!has) init.headers.push(['Authorization','Bearer '+token]); } else if(typeof init.headers==='object'){ if(!init.headers['Authorization'] && !init.headers['authorization']) init.headers['Authorization'] = 'Bearer '+token; } } }catch(e){} return _fetch(input, init); };"
        "window.goProtectedPath=function(path){window.location.href=path;return false;};})();</script>", title, show_nav?HTML_STYLE_NAV:"", extra_style?extra_style:"", title, device_config_get_running_app_name(), time_str, emu_button, APP_VERSION, APP_DATE, show_nav?nav_html:"");
    if (needed < 0) {
        return ESP_FAIL;
    }

    char *buf = malloc((size_t)needed + 1);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    snprintf(buf, (size_t)needed + 1,
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>%s</title><style>"
        "body{font-family:Arial;background:#f5f5f5;color:#333;margin:0}header{background:#000;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".container{max-width:1000px;margin:20px auto;padding:0 20px}" 
        "%s %s"
        "</style></head><body>"
        "<header>"
        "<div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>%s [%s] - %s</h1>%s</div>"
        "<div style='text-align:right;font-size:12px;opacity:0.8;'>v%s (%s)</div>"
        "</header>"
        "%s"
        "<script>/* Global fetch wrapper: injects Authorization */"
        "(function(){if(window.__auth_wrapped) return; window.__auth_wrapped=true; const _fetch = window.fetch.bind(window);"
        "window.setAuthToken = function(t){ if(t) localStorage.setItem('httpservices_token', t); else localStorage.removeItem('httpservices_token'); };"
        "window.getAuthToken = function(){ return localStorage.getItem('httpservices_token'); };"
        "window.clearAuthToken = function(){ localStorage.removeItem('httpservices_token'); };"
        "window.fetch = function(input, init){ try{ const token = window.getAuthToken(); if(token){ init = init || {}; if(!init.headers){ init.headers = {'Authorization':'Bearer '+token}; } else if(init.headers instanceof Headers){ if(!init.headers.get('Authorization')) init.headers.set('Authorization','Bearer '+token); } else if(Array.isArray(init.headers)){ let has=false; for(const h of init.headers){ if(h[0].toLowerCase()==='authorization'){ has=true; break; } } if(!has) init.headers.push(['Authorization','Bearer '+token]); } else if(typeof init.headers==='object'){ if(!init.headers['Authorization'] && !init.headers['authorization']) init.headers['Authorization'] = 'Bearer '+token; } } }catch(e){} return _fetch(input, init); };"
        "window.goProtectedPath=function(path){window.location.href=path;return false;};})();</script>", title, show_nav?HTML_STYLE_NAV:"", extra_style?extra_style:"", title, device_config_get_running_app_name(), time_str, emu_button, APP_VERSION, APP_DATE, show_nav?nav_html:"");
    httpd_resp_sendstr_chunk(req, buf);
    free(buf);
    ESP_ERROR_CHECK_WITHOUT_ABORT(send_i18n_runtime_script(req));
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
