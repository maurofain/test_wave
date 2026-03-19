#include "web_ui_internal.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "sd_card.h"
#include "cJSON.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TAG "WEB_UI_FILES"

/**
 * @brief Traduce l'identificatore storage nel path base del filesystem.
 *
 * @param storage Nome storage richiesto (`spiffs`, `sd`, `sdcard`).
 * @param base_path Puntatore di output al path base risolto.
 * @return true se lo storage è riconosciuto, false in caso contrario.
 */
static bool storage_to_base_path(const char *storage, const char **base_path)
{
    if (!storage || !base_path) {
        return false;
    }
    if (strcmp(storage, "spiffs") == 0) {
        *base_path = "/spiffs";
        return true;
    }
    if (strcmp(storage, "sd") == 0 || strcmp(storage, "sdcard") == 0) {
        *base_path = "/sdcard";
        return true;
    }
    return false;
}

/**
 * @brief Verifica che il nome file non contenga path traversal o separatori.
 *
 * @param name Nome file da validare.
 * @return true se il nome è considerato sicuro, false altrimenti.
 */
static bool is_safe_filename(const char *name)
{
    if (!name || name[0] == '\0') {
        return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return false;
    }
    for (const char *p = name; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            return false;
        }
    }
    return true;
}

/**
 * @brief Estrae un parametro dalla query string della richiesta HTTP.
 *
 * @param req Richiesta HTTP in ingresso.
 * @param key Nome della chiave da leggere.
 * @param out Buffer di output per il valore.
 * @param out_len Dimensione del buffer di output.
 * @return ESP_OK se trovato, errore ESP-IDF in caso di assenza/fallimento.
 */
static esp_err_t get_query_value(httpd_req_t *req, const char *key, char *out, size_t out_len)
{
    if (!req || !key || !out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    out[0] = '\0';

    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    char *query = malloc(qlen + 1);
    if (!query) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = httpd_req_get_url_query_str(req, query, qlen + 1);
    if (ret == ESP_OK) {
        ret = httpd_query_key_value(query, key, out, out_len);
    }

    free(query);
    return ret;
}

/**
 * @brief Renderizza la pagina web del mini file manager.
 *
 * La pagina include elenco file, upload, download, eliminazione multipla
 * e visualizzazione inline con formattazione JSON lato client.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la risposta viene inviata correttamente.
 */
esp_err_t files_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "files.html", "text/html; charset=utf-8");
}

/**
 * @brief Restituisce la lista file della root dello storage selezionato.
 *
 * Query supportata: `storage=spiffs|sd|sdcard`.
 * La risposta include dimensione dei file e spazio usato/totale quando disponibile.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK in caso di risposta inviata; errore HTTP in caso di parametri/storage non validi.
 */
esp_err_t api_files_list_get(httpd_req_t *req)
{
    char storage[16] = {0};
    if (get_query_value(req, "storage", storage, sizeof(storage)) != ESP_OK) {
        snprintf(storage, sizeof(storage), "spiffs");
    }

    const char *base_path = NULL;
    if (!storage_to_base_path(storage, &base_path)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"error\":\"invalid_storage\"}");
    }

    bool is_sd = (strcmp(storage, "sd") == 0 || strcmp(storage, "sdcard") == 0);
    DIR *dir = is_sd ? sd_card_opendir(base_path) : opendir(base_path);
    if (!dir) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"error\":\"cannot_open_storage\"}");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    cJSON_AddStringToObject(root, "storage", storage);
    cJSON_AddItemToObject(root, "files", files);

    uint64_t sum_file_bytes = 0;

    struct dirent *de;
    while ((de = (is_sd ? sd_card_readdir(dir) : readdir(dir))) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        char full_path[320];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, de->d_name);

        struct stat st;
        if ((is_sd ? sd_card_stat(full_path, &st) : stat(full_path, &st)) != 0) {
            continue;
        }
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", de->d_name);
        cJSON_AddNumberToObject(item, "size", (double)st.st_size);
        cJSON_AddItemToArray(files, item);
        sum_file_bytes += (uint64_t)st.st_size;
    }
    if (is_sd) sd_card_closedir(dir); else closedir(dir);

    uint64_t total_bytes = 0;
    uint64_t used_bytes = sum_file_bytes;
    if (strcmp(storage, "spiffs") == 0) {
        size_t total = 0;
        size_t used = 0;
        if (esp_spiffs_info(NULL, &total, &used) == ESP_OK) {
            total_bytes = (uint64_t)total;
            used_bytes = (uint64_t)used;
        }
    } else if (strcmp(storage, "sd") == 0 || strcmp(storage, "sdcard") == 0) {
        uint64_t total_kb = sd_card_get_total_size();
        uint64_t used_kb = sd_card_get_used_size();
        if (total_kb > 0) {
            total_bytes = total_kb * 1024ULL;
            used_bytes = used_kb * 1024ULL;
        }
    }

    cJSON_AddNumberToObject(root, "used_bytes", (double)used_bytes);
    cJSON_AddNumberToObject(root, "total_bytes", (double)total_bytes);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"error\":\"oom\"}");
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_sendstr(req, json);
    free(json);
    return ret;
}

/**
 * @brief Carica un file binario nello storage selezionato.
 *
 * Query richieste: `storage` e `name`.
 * Il body viene scritto in streaming per chunk fino al completamento.
 *
 * @param req Richiesta HTTP POST con payload binario.
 * @return ESP_OK se l'upload termina correttamente; errore HTTP in caso di fallimento.
 */
esp_err_t api_files_upload_post(httpd_req_t *req)
{
    char storage[16] = {0};
    char name[128] = {0};
    if (get_query_value(req, "storage", storage, sizeof(storage)) != ESP_OK ||
        get_query_value(req, "name", name, sizeof(name)) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "missing_query");
    }

    const char *base_path = NULL;
    if (!storage_to_base_path(storage, &base_path) || !is_safe_filename(name)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_params");
    }

    char full_path[320];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);

    bool is_sd = (strcmp(base_path, "/sdcard") == 0);
    FILE *f = is_sd ? sd_card_fopen(full_path, "wb") : fopen(full_path, "wb");
    if (!f) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "open_failed");
    }

    int remaining = req->content_len;
    char buf[1024];
    while (remaining > 0) {
        int chunk = remaining > (int)sizeof(buf) ? (int)sizeof(buf) : remaining;
        int r = httpd_req_recv(req, buf, chunk);
        if (r <= 0) {
            if (is_sd) sd_card_fclose(f); else fclose(f);
            remove(full_path);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_sendstr(req, "recv_failed");
        }
        size_t w = is_sd ? sd_card_fwrite(f, buf, (size_t)r) : fwrite(buf, 1, (size_t)r, f);
        if (w != (size_t)r) {
            if (is_sd) sd_card_fclose(f); else fclose(f);
            remove(full_path);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_sendstr(req, "write_failed");
        }
        remaining -= r;
    }

    if (is_sd) sd_card_fclose(f); else fclose(f);
    return httpd_resp_sendstr(req, "ok");
}

/**
 * @brief Elimina un file dallo storage selezionato.
 *
 * Body JSON richiesto: `{ "storage": "...", "name": "..." }`.
 *
 * @param req Richiesta HTTP POST con body JSON.
 * @return ESP_OK se la risposta viene inviata; errore HTTP in caso di parametri o file non valido.
 */
esp_err_t api_files_delete_post(httpd_req_t *req)
{
    char body[256];
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_body");
    }
    body[len] = '\0';

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_json");
    }

    const cJSON *storage = cJSON_GetObjectItemCaseSensitive(root, "storage");
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!cJSON_IsString(storage) || !cJSON_IsString(name)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "missing_fields");
    }

    const char *base_path = NULL;
    if (!storage_to_base_path(storage->valuestring, &base_path) || !is_safe_filename(name->valuestring)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_params");
    }

    char full_path[320];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name->valuestring);
    cJSON_Delete(root);

    if (remove(full_path) != 0) {
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_sendstr(req, "delete_failed");
    }

    return httpd_resp_sendstr(req, "ok");
}

/**
 * @brief Eroga il contenuto di un file come download.
 *
 * Query richieste: `storage` e `name`.
 * La risposta imposta `Content-Disposition: attachment` e invia il file a chunk.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se il file viene inviato completamente; ESP_FAIL su interruzioni di stream.
 */
esp_err_t api_files_download_get(httpd_req_t *req)
{
    char storage[16] = {0};
    char name[128] = {0};
    char inline_mode[8] = {0};
    if (get_query_value(req, "storage", storage, sizeof(storage)) != ESP_OK ||
        get_query_value(req, "name", name, sizeof(name)) != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "missing_query");
    }

    const char *base_path = NULL;
    if (!storage_to_base_path(storage, &base_path) || !is_safe_filename(name)) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_params");
    }

    char full_path[320];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, name);

    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        httpd_resp_set_status(req, "404 Not Found");
        return httpd_resp_sendstr(req, "not_found");
    }

    bool is_sd = (strcmp(base_path, "/sdcard") == 0);
    FILE *f = is_sd ? sd_card_fopen(full_path, "rb") : fopen(full_path, "rb");
    if (!f) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "open_failed");
    }

    bool inline_requested = (get_query_value(req, "inline", inline_mode, sizeof(inline_mode)) == ESP_OK &&
                             strcmp(inline_mode, "1") == 0);
    if (inline_requested) {
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
    } else {
        httpd_resp_set_type(req, "application/octet-stream");
        char cd[196];
        snprintf(cd, sizeof(cd), "attachment; filename=\"%s\"", name);
        httpd_resp_set_hdr(req, "Content-Disposition", cd);
    }

    char buf[1024];
    size_t n = 0;
    while ((n = (is_sd ? sd_card_fread(f, buf, sizeof(buf)) : fread(buf, 1, sizeof(buf), f))) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) {
            if (is_sd) sd_card_fclose(f); else fclose(f);
            return ESP_FAIL;
        }
    }
    if (is_sd) sd_card_fclose(f); else fclose(f);

    return httpd_resp_send_chunk(req, NULL, 0);
}

/* -----------------------------------------------------------------------
 * POST /api/files/copy
 * Body JSON: {"from":"spiffs","to":"sdcard"}   (o viceversa)
 * Copia tutti i file img*.jpg dalla sorgente alla destinazione.
 * ----------------------------------------------------------------------- */
esp_err_t api_files_copy_post(httpd_req_t *req)
{
    char buf[256];
    int recv = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (recv <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "empty_body");
    }
    buf[recv] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "invalid_json");
    }

    cJSON *from_j = cJSON_GetObjectItem(root, "from");
    cJSON *to_j   = cJSON_GetObjectItem(root, "to");
    if (!from_j || !to_j || !from_j->valuestring || !to_j->valuestring) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "missing_fields");
    }

    const char *src_path = NULL;
    const char *dst_path = NULL;
    if (!storage_to_base_path(from_j->valuestring, &src_path) ||
        !storage_to_base_path(to_j->valuestring,   &dst_path)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "unknown_storage");
    }

    bool src_is_sd = (strcmp(src_path, "/sdcard") == 0);
    bool dst_is_sd = (strcmp(dst_path, "/sdcard") == 0);
    cJSON_Delete(root);

    /* Apri directory sorgente */
    DIR *dir = src_is_sd ? sd_card_opendir(src_path) : opendir(src_path);
    if (!dir) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "opendir_failed");
    }

    int copied = 0, failed = 0;
    struct dirent *ent;
    char ibuf[2048];

    while ((ent = readdir(dir)) != NULL) {
        /* Filtra img*.jpg */
        const char *n = ent->d_name;
        size_t nlen = strlen(n);
        if (ent->d_type != DT_REG ||
            strncmp(n, "img", 3) != 0 ||
            nlen < 8 ||
            strcmp(n + nlen - 4, ".jpg") != 0) {
            continue;
        }

        char src_fp[320], dst_fp[320];
        snprintf(src_fp, sizeof(src_fp), "%s/%s", src_path, n);
        snprintf(dst_fp, sizeof(dst_fp), "%s/%s", dst_path, n);

        FILE *fsrc = src_is_sd ? sd_card_fopen(src_fp, "rb") : fopen(src_fp, "rb");
        if (!fsrc) { failed++; continue; }

        FILE *fdst = dst_is_sd ? sd_card_fopen(dst_fp, "wb") : fopen(dst_fp, "wb");
        if (!fdst) {
            if (src_is_sd) sd_card_fclose(fsrc); else fclose(fsrc);
            failed++;
            continue;
        }

        size_t nr;
        bool ok = true;
        while ((nr = (src_is_sd ? sd_card_fread(fsrc, ibuf, sizeof(ibuf))
                                 : fread(ibuf, 1, sizeof(ibuf), fsrc))) > 0) {
            size_t nw = dst_is_sd ? sd_card_fwrite(fdst, ibuf, nr)
                                  : fwrite(ibuf, 1, nr, fdst);
            if (nw != nr) { ok = false; break; }
        }

        if (src_is_sd) sd_card_fclose(fsrc); else fclose(fsrc);
        if (dst_is_sd) sd_card_fclose(fdst); else fclose(fdst);

        if (!dst_is_sd && !ok) { remove(dst_fp); }
        if (ok) copied++; else failed++;
    }

    if (src_is_sd) sd_card_closedir(dir); else closedir(dir);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "copied", copied);
    cJSON_AddNumberToObject(resp, "failed", failed);
    char *out = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_sendstr(req, out ? out : "{\"copied\":0,\"failed\":0}");
    if (out) free(out);
    return ret;
}
