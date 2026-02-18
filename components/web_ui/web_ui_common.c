#include "web_ui_internal.h"
#include "device_config.h"
#include "app_version.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include <lwip/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *HTML_STYLE_NAV = 
    "nav{background:#000;padding:10px;display:flex;justify-content:center;gap:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}"
    "nav a{color:white;text-decoration:none;padding:8px 15px;border-radius:4px;background:#2c3e50;font-weight:bold;font-size:14px;transition:.2s}"
    "nav a:hover{background:#3498db}";

// Nota: questa funzione è usata da diverse pagine; non è più `static` perché
// sarà condivisa tra i file del componente web_ui dopo lo split.
esp_err_t send_head(httpd_req_t *req, const char *title, const char *extra_style, bool show_nav) {
    // Get current time
    time_t now = time(NULL);
    struct tm timeinfo;
    char time_str[20] = "Time not set";
    if (now != (time_t)-1) {
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
    }

    const bool is_emulator_page =
        (req && strncmp(req->uri, "/emulator", 9) == 0 &&
         (req->uri[9] == '\0' || req->uri[9] == '?'));
    const char *emu_button = is_emulator_page
        ? "<a href='/' style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>Home</a>"
        : "<a href='#' onclick=\"return window.goProtectedPath('/emulator');\" style='margin-left:12px;padding:6px 10px;background:#8e44ad;color:white;text-decoration:none;border-radius:6px;font-size:14px;font-weight:bold;'>Emulatore</a>";

    const bool show_tasks = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TASKS);
    const bool show_test = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TEST);
    const char *nav_tasks = show_tasks ? "<a href='/tasks'>📋 Task</a>" : "";
    const char *nav_test = show_test ? "<a href='/test'>🔧 Test</a>" : "";
    char nav_html[768] = {0};
    snprintf(nav_html, sizeof(nav_html),
             "<nav><a href='/'>🏠 Home</a><a href='/config'>⚙️ Config</a><a href='/stats'>📈 Statistiche</a>%s<a href='/logs'>📋 Log</a>%s<a href='/ota'>🔄 OTA</a></nav>",
             nav_tasks,
             nav_test);

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
        "window.askProtectedPassword=function(path){return new Promise(function(resolve){var ov=document.createElement('div');ov.style='position:fixed;inset:0;background:rgba(0,0,0,.45);display:flex;align-items:center;justify-content:center;z-index:99999;';"
        "var box=document.createElement('div');box.style='background:#fff;border-radius:10px;padding:18px;min-width:320px;max-width:90vw;box-shadow:0 8px 24px rgba(0,0,0,.25);font-family:Arial';"
        "box.innerHTML='<h3 style=\"margin:0 0 10px 0;color:#2c3e50\">Password richiesta</h3><p style=\"margin:0 0 10px 0;color:#566573\">Inserisci password per continuare</p><input id=\"pw_masked_input\" type=\"password\" autocomplete=\"current-password\" style=\"width:100%%;padding:8px;border:1px solid #ccc;border-radius:6px;box-sizing:border-box\"><div style=\"display:flex;justify-content:flex-end;gap:8px;margin-top:12px\"><button id=\"pw_cancel_btn\" style=\"padding:8px 12px;border:none;border-radius:6px;background:#7f8c8d;color:#fff;cursor:pointer\">Annulla</button><button id=\"pw_ok_btn\" style=\"padding:8px 12px;border:none;border-radius:6px;background:#8e44ad;color:#fff;cursor:pointer\">Continua</button></div>';"
        "ov.appendChild(box);document.body.appendChild(ov);"
        "var input=box.querySelector('#pw_masked_input');var ok=box.querySelector('#pw_ok_btn');var cancel=box.querySelector('#pw_cancel_btn');"
        "function close(v){try{document.body.removeChild(ov);}catch(e){} resolve(v);}"
        "ok.onclick=function(){var pwd=input.value||'';if(!pwd){input.focus();return;} close(path+'?pwd='+encodeURIComponent(pwd));};"
        "cancel.onclick=function(){close(null);};"
        "input.addEventListener('keydown',function(e){if(e.key==='Enter'){ok.click();} if(e.key==='Escape'){cancel.click();}});"
        "setTimeout(function(){input.focus();},0);});};"
        "window.goProtectedPath=function(path){window.askProtectedPassword(path).then(function(url){if(url){window.location.href=url;}});return false;};})();</script>", title, show_nav?HTML_STYLE_NAV:"", extra_style?extra_style:"", title, device_config_get_running_app_name(), time_str, emu_button, APP_VERSION, APP_DATE, show_nav?nav_html:"");
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
        "window.askProtectedPassword=function(path){return new Promise(function(resolve){var ov=document.createElement('div');ov.style='position:fixed;inset:0;background:rgba(0,0,0,.45);display:flex;align-items:center;justify-content:center;z-index:99999;';"
        "var box=document.createElement('div');box.style='background:#fff;border-radius:10px;padding:18px;min-width:320px;max-width:90vw;box-shadow:0 8px 24px rgba(0,0,0,.25);font-family:Arial';"
        "box.innerHTML='<h3 style=\"margin:0 0 10px 0;color:#2c3e50\">Password richiesta</h3><p style=\"margin:0 0 10px 0;color:#566573\">Inserisci password per continuare</p><input id=\"pw_masked_input\" type=\"password\" autocomplete=\"current-password\" style=\"width:100%%;padding:8px;border:1px solid #ccc;border-radius:6px;box-sizing:border-box\"><div style=\"display:flex;justify-content:flex-end;gap:8px;margin-top:12px\"><button id=\"pw_cancel_btn\" style=\"padding:8px 12px;border:none;border-radius:6px;background:#7f8c8d;color:#fff;cursor:pointer\">Annulla</button><button id=\"pw_ok_btn\" style=\"padding:8px 12px;border:none;border-radius:6px;background:#8e44ad;color:#fff;cursor:pointer\">Continua</button></div>';"
        "ov.appendChild(box);document.body.appendChild(ov);"
        "var input=box.querySelector('#pw_masked_input');var ok=box.querySelector('#pw_ok_btn');var cancel=box.querySelector('#pw_cancel_btn');"
        "function close(v){try{document.body.removeChild(ov);}catch(e){} resolve(v);}"
        "ok.onclick=function(){var pwd=input.value||'';if(!pwd){input.focus();return;} close(path+'?pwd='+encodeURIComponent(pwd));};"
        "cancel.onclick=function(){close(null);};"
        "input.addEventListener('keydown',function(e){if(e.key==='Enter'){ok.click();} if(e.key==='Escape'){cancel.click();}});"
        "setTimeout(function(){input.focus();},0);});};"
        "window.goProtectedPath=function(path){window.askProtectedPassword(path).then(function(url){if(url){window.location.href=url;}});return false;};})();</script>", title, show_nav?HTML_STYLE_NAV:"", extra_style?extra_style:"", title, device_config_get_running_app_name(), time_str, emu_button, APP_VERSION, APP_DATE, show_nav?nav_html:"");
    httpd_resp_sendstr_chunk(req, buf);
    free(buf);
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
