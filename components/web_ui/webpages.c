#include "web_ui_internal.h"
#include "sd_card.h"
#include "esp_log.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef WEB_UI_PAGE_SOURCE
#define WEB_UI_PAGE_SOURCE 0
#endif

#ifndef WEB_UI_EXPORT_ON_BOOT
#define WEB_UI_EXPORT_ON_BOOT 0
#endif

#define WEB_PAGES_SPIFFS_BASE "/spiffs/www"
#define WEB_PAGES_SD_BASE "/sdcard/www"

static const char *TAG = "WEB_PAGES";

typedef struct {
    const char *filename;
    const char *content;
} webpage_seed_t;

#if WEB_UI_EXPORT_ON_BOOT == 1
#define SEED_INDEX_HTML \
    "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Home</title></head>" \
    "<body style='font-family:Arial;margin:20px'>" \
    "<h2>MH1001 - index.html (esterno)</h2>" \
    "<p>Questa pagina arriva da storage esterno. Personalizzala come preferisci.</p>" \
    "<ul>" \
    "<li><a href='/config'>Configurazione</a></li>" \
    "<li><a href='/stats'>Statistiche</a></li>" \
    "<li><a href='/files'>File Manager</a></li>" \
    "<li><a href='/logs'>Logs</a></li>" \
    "<li><a href='/api'>API</a></li>" \
    "</ul>" \
    "<pre id='st'>Caricamento /status...</pre>" \
    "<script>fetch('/status',{cache:'no-store'}).then(r=>r.json()).then(j=>{document.getElementById('st').textContent=JSON.stringify(j,null,2)}).catch(e=>{document.getElementById('st').textContent='Errore: '+e});</script>" \
    "</body></html>"

#define SEED_CONFIG_HTML \
    "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Config</title></head>" \
    "<body style='font-family:Arial;margin:20px'>" \
    "<h2>config.html (esterno)</h2>" \
    "<p>Inserisci qui la tua versione HTML/JS della pagina configurazione.</p>" \
    "<p><a href='/'>Torna home</a></p>" \
    "</body></html>"

#define SEED_STATS_HTML \
    "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Stats</title></head>" \
    "<body style='font-family:Arial;margin:20px'>" \
    "<h2>stats.html (esterno)</h2>" \
    "<p>Inserisci qui la tua pagina statistiche.</p>" \
    "<p><a href='/'>Torna home</a></p>" \
    "</body></html>"

#define SEED_TASKS_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Tasks</title></head><body style='font-family:Arial;margin:20px'><h2>tasks.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"
#define SEED_TEST_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Test</title></head><body style='font-family:Arial;margin:20px'><h2>test.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"
#define SEED_FILES_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Files</title></head><body style='font-family:Arial;margin:20px'><h2>files.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"
#define SEED_LOGS_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Logs</title></head><body style='font-family:Arial;margin:20px'><h2>logs.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"
#define SEED_EMULATOR_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Emulator</title></head><body style='font-family:Arial;margin:20px'><h2>emulator.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"
#define SEED_HTTPSERVICES_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 HTTP Services</title></head><body style='font-family:Arial;margin:20px'><h2>httpservices.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"
#define SEED_PROGRAMS_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 Programs</title></head><body style='font-family:Arial;margin:20px'><h2>programs.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"
#define SEED_OTA_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 OTA</title></head><body style='font-family:Arial;margin:20px'><h2>ota.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"
#define SEED_API_HTML "<!doctype html><html><head><meta charset='utf-8'><title>MH1001 API</title></head><body style='font-family:Arial;margin:20px'><h2>api.html (esterno)</h2><p><a href='/'>Home</a></p></body></html>"

static const webpage_seed_t SEED_PAGES[] = {
    {"index.html", SEED_INDEX_HTML},
    {"config.html", SEED_CONFIG_HTML},
    {"stats.html", SEED_STATS_HTML},
    {"tasks.html", SEED_TASKS_HTML},
    {"test.html", SEED_TEST_HTML},
    {"files.html", SEED_FILES_HTML},
    {"logs.html", SEED_LOGS_HTML},
    {"emulator.html", SEED_EMULATOR_HTML},
    {"httpservices.html", SEED_HTTPSERVICES_HTML},
    {"programs.html", SEED_PROGRAMS_HTML},
    {"ota.html", SEED_OTA_HTML},
    {"api.html", SEED_API_HTML},
};
#endif

static const char *webpages_source_name_impl(void)
{
#if WEB_UI_PAGE_SOURCE == 1
    return "SPIFFS";
#elif WEB_UI_PAGE_SOURCE == 2
    return "SD";
#else
    return "EMBEDDED";
#endif
}

const char *webpages_source_name(void)
{
    return webpages_source_name_impl();
}

static bool webpages_external_enabled(void)
{
#if WEB_UI_PAGE_SOURCE == 1 || WEB_UI_PAGE_SOURCE == 2
    return true;
#else
    return false;
#endif
}

static const char *webpages_base_path(void)
{
#if WEB_UI_PAGE_SOURCE == 1
    return WEB_PAGES_SPIFFS_BASE;
#elif WEB_UI_PAGE_SOURCE == 2
    return WEB_PAGES_SD_BASE;
#else
    return NULL;
#endif
}

static bool webpages_storage_ready(void)
{
#if WEB_UI_PAGE_SOURCE == 2
    return sd_card_is_mounted();
#else
    return true;
#endif
}

static const char *mime_from_filename(const char *name)
{
    if (!name) {
        return "text/plain; charset=utf-8";
    }
    const char *ext = strrchr(name, '.');
    if (!ext) {
        return "text/plain; charset=utf-8";
    }
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return "text/html; charset=utf-8";
    }
    if (strcmp(ext, ".js") == 0) {
        return "application/javascript; charset=utf-8";
    }
    if (strcmp(ext, ".css") == 0) {
        return "text/css; charset=utf-8";
    }
    if (strcmp(ext, ".json") == 0) {
        return "application/json; charset=utf-8";
    }
    return "text/plain; charset=utf-8";
}

static bool is_relative_path_safe(const char *relative_path)
{
    if (!relative_path || relative_path[0] == '\0') {
        return false;
    }
    if (relative_path[0] == '/') {
        return false;
    }
    if (strstr(relative_path, "..") != NULL) {
        return false;
    }
    if (strchr(relative_path, '\\') != NULL) {
        return false;
    }
    return true;
}

static esp_err_t send_file_as_chunks(httpd_req_t *req, const char *full_path, const char *content_type)
{
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    httpd_resp_set_type(req, content_type ? content_type : mime_from_filename(full_path));

    char buf[1024];
    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n > 0) {
            esp_err_t send_err = httpd_resp_send_chunk(req, buf, n);
            if (send_err != ESP_OK) {
                fclose(f);
                return send_err;
            }
        }
        if (n < sizeof(buf)) {
            if (ferror(f)) {
                fclose(f);
                return ESP_FAIL;
            }
            break;
        }
    }

    fclose(f);
    return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t webpages_try_send_external(httpd_req_t *req, const char *relative_path, const char *content_type)
{
    if (!webpages_external_enabled()) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!webpages_storage_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!is_relative_path_safe(relative_path)) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *base = webpages_base_path();
    if (!base) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    char full_path[256];
    int n = snprintf(full_path, sizeof(full_path), "%s/%s", base, relative_path);
    if (n <= 0 || n >= (int)sizeof(full_path)) {
        return ESP_ERR_INVALID_ARG;
    }

    return send_file_as_chunks(req, full_path, content_type);
}

esp_err_t webpages_send_external_or_error(httpd_req_t *req, const char *relative_path, const char *content_type)
{
    esp_err_t ret = webpages_try_send_external(req, relative_path, content_type);
    if (ret == ESP_OK) {
        return ESP_OK;
    }

    if (!req) {
        return ret;
    }

    if (ret == ESP_ERR_NOT_FOUND) {
        httpd_resp_set_status(req, "404 Non Trovato");
        return httpd_resp_send(req, "Pagina web non trovata su storage esterno", HTTPD_RESP_USE_STRLEN);
    }

    if (ret == ESP_ERR_INVALID_STATE) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "Storage non pronto per pagine web esterne", HTTPD_RESP_USE_STRLEN);
    }

    if (ret == ESP_ERR_NOT_SUPPORTED) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Sorgente pagine esterna non abilitata", HTTPD_RESP_USE_STRLEN);
    }

    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_send(req, "Errore caricamento pagina esterna", HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ensure_dir(const char *path)
{
    if (!path) {
        return ESP_ERR_INVALID_ARG;
    }
    if (mkdir(path, 0755) == 0 || errno == EEXIST) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

#if WEB_UI_EXPORT_ON_BOOT == 1
static esp_err_t write_file_if_missing(const char *path, const char *content)
{
    FILE *check = fopen(path, "rb");
    if (check) {
        fclose(check);
        return ESP_OK;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        return ESP_FAIL;
    }

    size_t len = strlen(content ? content : "");
    size_t wr = fwrite(content ? content : "", 1, len, f);
    fclose(f);
    return (wr == len) ? ESP_OK : ESP_FAIL;
}
#endif

esp_err_t webpages_bootstrap(void)
{
    ESP_LOGI(TAG, "[C] Web pages source: %s", webpages_source_name_impl());

    if (!webpages_external_enabled()) {
        return ESP_OK;
    }

    if (!webpages_storage_ready()) {
        ESP_LOGW(TAG, "[C] Storage non pronto per pagine esterne (%s)", webpages_source_name_impl());
        return ESP_ERR_INVALID_STATE;
    }

    const char *base = webpages_base_path();
    esp_err_t dir_err = ensure_dir(base);
    if (dir_err != ESP_OK) {
        ESP_LOGE(TAG, "[C] Impossibile creare directory web: %s", base ? base : "?");
        return dir_err;
    }

#if WEB_UI_EXPORT_ON_BOOT == 1
    for (size_t i = 0; i < sizeof(SEED_PAGES) / sizeof(SEED_PAGES[0]); ++i) {
        char out_path[256];
        int n = snprintf(out_path, sizeof(out_path), "%s/%s", base, SEED_PAGES[i].filename);
        if (n <= 0 || n >= (int)sizeof(out_path)) {
            continue;
        }
        esp_err_t wr_err = write_file_if_missing(out_path, SEED_PAGES[i].content);
        if (wr_err != ESP_OK) {
            ESP_LOGW(TAG, "[C] Export seed fallito: %s", out_path);
        }
    }
    ESP_LOGI(TAG, "[C] Export pagine seed completato in %s", base);
#endif

    return ESP_OK;
}
