#include "web_ui_internal.h"
#include "device_config.h"
#include "app_version.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *HTML_NAV = "<nav><a href='/'>🏠 Home</a><a href='/config'>⚙️ Config</a><a href='/stats'>📈 Statistiche</a><a href='/tasks'>📋 Task</a><a href='/logs'>📋 Log</a><a href='/test'>🔧 Test</a><a href='/ota'>🔄 OTA</a></nav>";

static const char *HTML_STYLE_NAV = 
    "nav{background:#000;padding:10px;display:flex;justify-content:center;gap:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}"
    "nav a{color:white;text-decoration:none;padding:8px 15px;border-radius:4px;background:#2c3e50;font-weight:bold;font-size:14px;transition:.2s}"
    "nav a:hover{background:#3498db}";

// Nota: questa funzione è usata da diverse pagine; non è più `static` perché
// sarà condivisa tra i file del componente web_ui dopo lo split.
esp_err_t send_head(httpd_req_t *req, const char *title, const char *extra_style, bool show_nav) {
    char *buf = malloc(4096);
    if (!buf) return ESP_ERR_NO_MEM;

    // Get current time
    time_t now = time(NULL);
    struct tm timeinfo;
    char time_str[20] = "Time not set";
    if (now != (time_t)-1) {
        localtime_r(&now, &timeinfo);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", &timeinfo);
    }

    snprintf(buf, 4096,
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>%s</title><style>"
        "body{font-family:Arial;background:#f5f5f5;color:#333;margin:0}header{background:#000;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".container{max-width:1000px;margin:20px auto;padding:0 20px}" 
        "%s %s"
        "</style></head><body>"
        "<header>"
        "<div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>%s [%s] - %s</h1></div>"
        "<div style='text-align:right;font-size:12px;opacity:0.8;'>v%s (%s)</div>"
        "</header>"
        "%s", title, show_nav?HTML_STYLE_NAV:"", extra_style?extra_style:"", title, device_config_get_running_app_name(), time_str, APP_VERSION, APP_DATE, show_nav?HTML_NAV:"");
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
