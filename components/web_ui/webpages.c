#include "web_ui_internal.h"
#include "sd_card.h"
#include "device_config.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifndef WEB_UI_PAGE_SOURCE
#define WEB_UI_PAGE_SOURCE 0
#endif

#ifndef WEB_UI_EXPORT_ON_BOOT
#define WEB_UI_EXPORT_ON_BOOT 0
#endif

#define WEB_PAGES_SPIFFS_BASE "/spiffs/www"
#define WEB_PAGES_SD_BASE "/sdcard/www"
#define I18N_V2_FILE_PATH "/spiffs/i18n_v2.json"
#define WEB_LOCALIZED_CACHE_MAX 24

static const char *TAG = "WEB_PAGES";

typedef struct {
    const char *filename;
    const char *content;
} webpage_seed_t;

typedef struct {
    char lang[8];
    char relative_path[64];
    char *content;
    size_t length;
} localized_page_cache_entry_t;

static localized_page_cache_entry_t s_page_cache[WEB_LOCALIZED_CACHE_MAX];
static size_t s_page_cache_next_slot = 0;
static cJSON *s_i18n_v2_root = NULL;

static void *psram_malloc(size_t size)
{
#ifdef MALLOC_CAP_SPIRAM
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) {
        return p;
    }
#endif
    return malloc(size);
}

static void psram_free(void *p)
{
#ifdef MALLOC_CAP_SPIRAM
    heap_caps_free(p);
#else
    free(p);
#endif
}

static void page_cache_entry_clear(localized_page_cache_entry_t *entry)
{
    if (!entry) {
        return;
    }
    if (entry->content) {
        psram_free(entry->content);
        entry->content = NULL;
    }
    entry->length = 0;
    entry->lang[0] = '\0';
    entry->relative_path[0] = '\0';
}

void webpages_localized_cache_invalidate(void)
{
    for (size_t i = 0; i < WEB_LOCALIZED_CACHE_MAX; ++i) {
        page_cache_entry_clear(&s_page_cache[i]);
    }
    s_page_cache_next_slot = 0;

    if (s_i18n_v2_root) {
        cJSON_Delete(s_i18n_v2_root);
        s_i18n_v2_root = NULL;
    }
}

static bool is_html_path(const char *relative_path)
{
    if (!relative_path) {
        return false;
    }
    const char *ext = strrchr(relative_path, '.');
    if (!ext) {
        return false;
    }
    return strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0;
}

static bool extract_page_name(const char *relative_path, char *out, size_t out_len)
{
    if (!relative_path || !out || out_len < 2) {
        return false;
    }

    const char *name = strrchr(relative_path, '/');
    name = name ? name + 1 : relative_path;
    if (!name[0]) {
        return false;
    }

    size_t len = 0;
    while (name[len] && name[len] != '.' && len < out_len - 1) {
        out[len] = name[len];
        len++;
    }
    out[len] = '\0';
    return len > 0;
}

static char *read_text_file(const char *path, size_t *out_len)
{
    if (out_len) {
        *out_len = 0;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(buf);
        return NULL;
    }
    buf[n] = '\0';
    if (out_len) {
        *out_len = n;
    }
    return buf;
}

static bool js_token_is_safe(const char *token)
{
    if (!token || !token[0]) {
        return false;
    }
    if (token[0] == '/' || strstr(token, "..") != NULL || strchr(token, '\\') != NULL) {
        return false;
    }
    if (!strstr(token, ".js")) {
        return false;
    }
    return true;
}

static bool append_chunk(char **buf, size_t *len, size_t *cap, const char *src, size_t src_len)
{
    if (!buf || !len || !cap || !src) {
        return false;
    }
    size_t needed = *len + src_len + 1;
    if (needed > *cap) {
        size_t next_cap = (*cap == 0) ? 1024 : *cap;
        while (next_cap < needed) {
            next_cap *= 2;
        }
        char *next = realloc(*buf, next_cap);
        if (!next) {
            return false;
        }
        *buf = next;
        *cap = next_cap;
    }
    memcpy(*buf + *len, src, src_len);
    *len += src_len;
    (*buf)[*len] = '\0';
    return true;
}

static char *expand_js_include_markers(const char *html, const char *full_path, size_t *out_len)
{
    if (out_len) {
        *out_len = 0;
    }
    if (!html || !full_path) {
        return NULL;
    }

    char base_dir[256] = {0};
    strncpy(base_dir, full_path, sizeof(base_dir) - 1);
    char *slash = strrchr(base_dir, '/');
    if (!slash) {
        return strdup(html);
    }
    *slash = '\0';

    char *out = NULL;
    size_t out_size = 0;
    size_t out_cap = 0;
    const char *cursor = html;

    while (true) {
        const char *marker = strstr(cursor, "{{JS:");
        if (!marker) {
            if (!append_chunk(&out, &out_size, &out_cap, cursor, strlen(cursor))) {
                free(out);
                return NULL;
            }
            break;
        }

        if (!append_chunk(&out, &out_size, &out_cap, cursor, (size_t)(marker - cursor))) {
            free(out);
            return NULL;
        }

        const char *token_start = marker + 5;
        const char *token_end = strstr(token_start, "}}");
        if (!token_end) {
            if (!append_chunk(&out, &out_size, &out_cap, marker, strlen(marker))) {
                free(out);
                return NULL;
            }
            break;
        }

        size_t token_len = (size_t)(token_end - token_start);
        if (token_len == 0 || token_len >= 120) {
            if (!append_chunk(&out, &out_size, &out_cap, marker, (size_t)(token_end + 2 - marker))) {
                free(out);
                return NULL;
            }
            cursor = token_end + 2;
            continue;
        }

        char token[128] = {0};
        memcpy(token, token_start, token_len);
        token[token_len] = '\0';

        if (!js_token_is_safe(token)) {
            if (!append_chunk(&out, &out_size, &out_cap, marker, (size_t)(token_end + 2 - marker))) {
                free(out);
                return NULL;
            }
            cursor = token_end + 2;
            continue;
        }

        char js_path[384] = {0};
        snprintf(js_path, sizeof(js_path), "%s/%s", base_dir, token);
        size_t js_len = 0;
        char *js_body = read_text_file(js_path, &js_len);
        if (!js_body) {
            ESP_LOGW(TAG, "[C] JS include non trovato: %s", js_path);
            if (!append_chunk(&out, &out_size, &out_cap, marker, (size_t)(token_end + 2 - marker))) {
                free(out);
                return NULL;
            }
            cursor = token_end + 2;
            continue;
        }

        if (!append_chunk(&out, &out_size, &out_cap, "<script>", 8) ||
            !append_chunk(&out, &out_size, &out_cap, js_body, js_len) ||
            !append_chunk(&out, &out_size, &out_cap, "</script>", 9)) {
            free(js_body);
            free(out);
            return NULL;
        }
        free(js_body);
        cursor = token_end + 2;
    }

    if (!out) {
        out = strdup(html);
        if (!out) {
            return NULL;
        }
        out_size = strlen(out);
    }
    if (out_len) {
        *out_len = out_size;
    }
    return out;
}

static cJSON *i18n_v2_root_get(void)
{
    if (s_i18n_v2_root) {
        return s_i18n_v2_root;
    }

    size_t sz = 0;
    char *json = read_text_file(I18N_V2_FILE_PATH, &sz);
    if (!json || sz == 0) {
        if (json) {
            free(json);
        }
        return NULL;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root || !cJSON_IsObject(root)) {
        if (root) {
            cJSON_Delete(root);
        }
        return NULL;
    }
    s_i18n_v2_root = root;
    return s_i18n_v2_root;
}

static const char *entry_get_lang_text(cJSON *entry, const char *lang)
{
    if (!entry || !cJSON_IsObject(entry)) {
        return NULL;
    }

    cJSON *text = cJSON_GetObjectItemCaseSensitive(entry, "text");
    if (!cJSON_IsObject(text)) {
        return NULL;
    }

    cJSON *lang_item = cJSON_GetObjectItemCaseSensitive(text, lang ? lang : "it");
    if (cJSON_IsString(lang_item) && lang_item->valuestring) {
        return lang_item->valuestring;
    }

    cJSON *it_item = cJSON_GetObjectItemCaseSensitive(text, "it");
    if (cJSON_IsString(it_item) && it_item->valuestring) {
        return it_item->valuestring;
    }

    return NULL;
}

static const char *i18n_v2_lookup(const char *page_name, const char *key3, const char *lang)
{
    cJSON *root = i18n_v2_root_get();
    if (!root || !page_name || !key3) {
        return NULL;
    }

    cJSON *web = cJSON_GetObjectItemCaseSensitive(root, "web");
    if (!cJSON_IsObject(web)) {
        return NULL;
    }
    cJSON *page = cJSON_GetObjectItemCaseSensitive(web, page_name);
    if (!cJSON_IsObject(page)) {
        return NULL;
    }
    cJSON *entry = cJSON_GetObjectItemCaseSensitive(page, key3);
    return entry_get_lang_text(entry, lang);
}

static bool parse_placeholder_key(const char *s, size_t max_len, size_t *consumed, char out_key[4])
{
    if (!s || max_len < 7 || !consumed || !out_key) {
        return false;
    }
    if (s[0] != '{' || s[1] != '{') {
        return false;
    }
    if (!isdigit((unsigned char)s[2]) || !isdigit((unsigned char)s[3]) || !isdigit((unsigned char)s[4])) {
        return false;
    }
    if (s[5] != '}' || s[6] != '}') {
        return false;
    }
    out_key[0] = s[2];
    out_key[1] = s[3];
    out_key[2] = s[4];
    out_key[3] = '\0';
    *consumed = 7;
    return true;
}

static char *localize_html_template(const char *html, const char *page_name, const char *lang, size_t *out_len)
{
    if (out_len) {
        *out_len = 0;
    }
    if (!html || !page_name) {
        return NULL;
    }

    const size_t in_len = strlen(html);
    size_t needed = 0;
    for (size_t i = 0; i < in_len;) {
        char key3[4] = {0};
        size_t consumed = 0;
        if (parse_placeholder_key(html + i, in_len - i, &consumed, key3)) {
            const char *tr = i18n_v2_lookup(page_name, key3, lang);
            needed += tr ? strlen(tr) : 0;
            i += consumed;
        } else {
            needed++;
            i++;
        }
    }

    char *out = psram_malloc(needed + 1);
    if (!out) {
        return NULL;
    }

    size_t pos = 0;
    for (size_t i = 0; i < in_len;) {
        char key3[4] = {0};
        size_t consumed = 0;
        if (parse_placeholder_key(html + i, in_len - i, &consumed, key3)) {
            const char *tr = i18n_v2_lookup(page_name, key3, lang);
            if (tr) {
                size_t tl = strlen(tr);
                memcpy(out + pos, tr, tl);
                pos += tl;
            }
            i += consumed;
        } else {
            out[pos++] = html[i++];
        }
    }
    out[pos] = '\0';
    if (out_len) {
        *out_len = pos;
    }
    return out;
}

static localized_page_cache_entry_t *page_cache_find(const char *lang, const char *relative_path)
{
    if (!lang || !relative_path) {
        return NULL;
    }
    for (size_t i = 0; i < WEB_LOCALIZED_CACHE_MAX; ++i) {
        localized_page_cache_entry_t *entry = &s_page_cache[i];
        if (entry->content && strcmp(entry->lang, lang) == 0 && strcmp(entry->relative_path, relative_path) == 0) {
            return entry;
        }
    }
    return NULL;
}

static void page_cache_put(const char *lang, const char *relative_path, char *content, size_t length)
{
    if (!lang || !relative_path || !content || length == 0) {
        if (content) {
            psram_free(content);
        }
        return;
    }

    localized_page_cache_entry_t *existing = page_cache_find(lang, relative_path);
    if (existing) {
        page_cache_entry_clear(existing);
        strncpy(existing->lang, lang, sizeof(existing->lang) - 1);
        strncpy(existing->relative_path, relative_path, sizeof(existing->relative_path) - 1);
        existing->content = content;
        existing->length = length;
        return;
    }

    localized_page_cache_entry_t *slot = &s_page_cache[s_page_cache_next_slot % WEB_LOCALIZED_CACHE_MAX];
    page_cache_entry_clear(slot);
    strncpy(slot->lang, lang, sizeof(slot->lang) - 1);
    strncpy(slot->relative_path, relative_path, sizeof(slot->relative_path) - 1);
    slot->content = content;
    slot->length = length;
    s_page_cache_next_slot = (s_page_cache_next_slot + 1) % WEB_LOCALIZED_CACHE_MAX;
}

static esp_err_t send_html_localized_cached(httpd_req_t *req, const char *full_path, const char *relative_path)
{
    const char *lang = device_config_get_ui_backend_language();
    if (!lang || !lang[0]) {
        lang = "it";
    }

    localized_page_cache_entry_t *cached = page_cache_find(lang, relative_path);
    if (cached) {
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        return httpd_resp_send(req, cached->content, (ssize_t)cached->length);
    }

    size_t raw_len = 0;
    char *raw_html = read_text_file(full_path, &raw_len);
    if (!raw_html || raw_len == 0) {
        if (raw_html) {
            free(raw_html);
        }
        return ESP_ERR_NOT_FOUND;
    }

    size_t expanded_len = 0;
    char *expanded_html = expand_js_include_markers(raw_html, full_path, &expanded_len);
    free(raw_html);
    if (!expanded_html || expanded_len == 0) {
        if (expanded_html) {
            free(expanded_html);
        }
        return ESP_ERR_NO_MEM;
    }

    char page_name[32] = {0};
    if (!extract_page_name(relative_path, page_name, sizeof(page_name))) {
        free(expanded_html);
        return ESP_ERR_INVALID_ARG;
    }

    size_t localized_len = 0;
    char *localized = localize_html_template(expanded_html, page_name, lang, &localized_len);
    free(expanded_html);
    if (!localized || localized_len == 0) {
        if (localized) {
            psram_free(localized);
        }
        return ESP_ERR_NO_MEM;
    }

    page_cache_put(lang, relative_path, localized, localized_len);
    cached = page_cache_find(lang, relative_path);
    if (!cached) {
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, cached->content, (ssize_t)cached->length);
}

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


/**
 * @brief Controlla se le pagine esterne sono abilitate.
 *
 * @return true se le pagine esterne sono abilitate, false altrimenti.
 */
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


/**
 * @brief Controlla se lo storage per le pagine web è pronto.
 *
 * Questa funzione verifica se lo storage dedicato alle pagine web è pronto
 * per l'uso. Questo è importante per assicurare che tutte le risorse
 * necessarie siano state correttamente inizializzate e siano accessibili.
 *
 * @return true se lo storage è pronto, false altrimenti.
 */
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


/**
 * @brief Controlla se un percorso relativo è sicuro.
 * 
 * Questa funzione verifica se il percorso relativo fornito è sicuro da utilizzare.
 * Un percorso relativo è considerato sicuro se non inizia con un carattere di barra (/),
 * il che potrebbe indicare un percorso assoluto.
 * 
 * @param [in] relative_path Il percorso relativo da controllare.
 * @return true se il percorso è sicuro, false altrimenti.
 */
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


/**
 * @brief Invia un file in blocchi attraverso una richiesta HTTP.
 *
 * Questa funzione invia il contenuto di un file specificato in blocchi
 * attraverso una richiesta HTTP. Il file viene aperto e letto in piccoli
 * segmenti, che vengono poi inviati al client. Questo approccio è utile
 * per gestire file di grandi dimensioni senza utilizzare troppa memoria.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @param full_path Percorso completo del file da inviare.
 * @param content_type Tipo di contenuto del file.
 * @return esp_err_t Codice di errore.
 */
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


/**
 * @brief Tenta di inviare una pagina web esterna.
 * 
 * Questa funzione cerca di inviare una pagina web esterna al client HTTP.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @param relative_path Percorso relativo alla pagina web da inviare.
 * @param content_type Tipo di contenuto della pagina web.
 * @return esp_err_t Codice di errore.
 */
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

    if (is_html_path(relative_path)) {
        esp_err_t localized_ret = send_html_localized_cached(req, full_path, relative_path);
        if (localized_ret == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "[C] Localized send failed for %s: %s. Fallback raw file.", relative_path, esp_err_to_name(localized_ret));
    }

    return send_file_as_chunks(req, full_path, content_type);
}


/**
 * @brief Invia una pagina web esterna o un errore in base al percorso relativo fornito.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @param relative_path Percorso relativo della pagina web da inviare.
 * @param content_type Tipo di contenuto della pagina web.
 * @return esp_err_t Codice di errore.
 */
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


/**
 * @brief Verifica se una directory esiste già.
 *
 * Non tenta mai di crearla: la directory deve essere pre-caricata in fase di flash.
 *
 * @param [in] path Il percorso della directory da verificare.
 * @return true se la directory esiste, false altrimenti.
 */
static bool is_existing_dir(const char *path)
{
    if (!path) {
        return false;
    }

    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
}

#if WEB_UI_EXPORT_ON_BOOT == 1

/**
 * @brief Scrive il contenuto in un file solo se il file non esiste già.
 *
 * @param [in] path Percorso del file da scrivere.
 * @param [in] content Contenuto da scrivere nel file.
 * @return esp_err_t Errore se la scrittura fallisce, OK se la scrittura è andata a buon fine.
 */
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


/**
 * @brief Avvia il servizio web per gestire le pagine web.
 *
 * Questa funzione inizializza e avvia il servizio web, pronto a gestire le richieste HTTP
 * per le pagine web. Questo include la configurazione del server web, l'elaborazione delle richieste
 * e la gestione delle risposte.
 *
 * @return esp_err_t
 * - ESP_OK: Se l'avvio del servizio web è stato completato con successo.
 * - ESP_FAIL: Se si è verificato un errore durante l'avvio del servizio web.
 */
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
    if (!is_existing_dir(base)) {
        ESP_LOGW(TAG, "[C] Directory web non trovata: %s (deve essere presente da flash)", base ? base : "?");
        return ESP_ERR_NOT_FOUND;
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

    webpages_localized_cache_invalidate();
    return ESP_OK;
}
