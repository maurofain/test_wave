#include "http_services.h"
#include "keepalive_task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern bool send_http_log;
#include <sys/time.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "cJSON.h"
#include "device_config.h"
#include "esp_http_client.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#define HTTP_SERVICES_LOG_TO_UI

/* web_ui_add_log is used only when HTTP_SERVICES_LOG_TO_UI is enabled; declare here to
   avoid adding a component dependency (web_ui already depends on http_services). */
#ifdef HTTP_SERVICES_LOG_TO_UI
extern void web_ui_add_log(const char *level, const char *tag, const char *message);
#endif

static const char *TAG = "HTTP_SERVICES";

/* Debug: inibisce i POST verso il server remoto/cloud */
#ifndef DNA_SERVER_POST
#define DNA_SERVER_POST 0
#endif

#ifndef CONFIG_HTTP_PENDING_BUFFER_MAX
#define CONFIG_HTTP_PENDING_BUFFER_MAX 4096
#endif

#ifndef CONFIG_HTTP_PENDING_ENDPOINT
#define CONFIG_HTTP_PENDING_ENDPOINT "/api/device/logs"
#endif

#define HTTP_SERVICES_AUTH_TOKEN_MAX_LEN 2048

// Global variable to store the token
char g_auth_token[HTTP_SERVICES_AUTH_TOKEN_MAX_LEN] = {0};
static bool s_remote_online = false;
static bool __attribute__((unused)) s_http_services_initial_token_done = false;
static bool s_http_buffering_enabled = false;
static bool s_http_buffer_flushed = false;
static char *s_http_pending_json = NULL;
static size_t s_http_pending_len = 0;
static size_t s_http_pending_cap = 0;

static esp_err_t http_services_login_if_needed(bool force_refresh);
static bool http_services_cfg_remote_enabled(const device_config_t *cfg);
static esp_err_t remote_post(const char *remote_path, const char *body, const char *auth_header,
                             char **out_resp, int *out_status, size_t *out_len);

static bool http_services_ftp_enabled(const device_config_t *cfg)
{
    return (cfg != NULL) &&
           cfg->ftp.enabled &&
           (cfg->ftp.server[0] != '\0') &&
           (cfg->ftp.user[0] != '\0');
}

static esp_err_t http_services_mkdir_if_missing(const char *path)
{
    if (!path || path[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "[C] mkdir fallita per %s (errno=%d)", path, errno);
    return ESP_FAIL;
}

static esp_err_t http_services_ensure_parent_dirs(const char *dir_path)
{
    if (!dir_path || dir_path[0] != '/') {
        return ESP_ERR_INVALID_ARG;
    }

    char tmp[256] = {0};
    strncpy(tmp, dir_path, sizeof(tmp) - 1);

    char *cursor = tmp + 1;
    while (*cursor) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (tmp[0] != '\0') {
                esp_err_t mk_err = http_services_mkdir_if_missing(tmp);
                if (mk_err != ESP_OK) {
                    return mk_err;
                }
            }
            *cursor = '/';
        }
        cursor++;
    }

    return http_services_mkdir_if_missing(tmp);
}

static int ftp_read_line(int sock, char *buf, size_t buf_len)
{
    if (!buf || buf_len < 2) {
        return -1;
    }

    size_t used = 0;
    while (used + 1 < buf_len) {
        char ch = 0;
        int r = recv(sock, &ch, 1, 0);
        if (r <= 0) {
            break;
        }
        buf[used++] = ch;
        if (ch == '\n') {
            break;
        }
    }

    buf[used] = '\0';
    return (int)used;
}

static esp_err_t ftp_read_response_code(int sock, int *out_code, char *out_line, size_t out_line_len)
{
    if (!out_code) {
        return ESP_ERR_INVALID_ARG;
    }

    char line[256] = {0};
    if (ftp_read_line(sock, line, sizeof(line)) <= 0) {
        return ESP_FAIL;
    }

    if (out_line && out_line_len > 0) {
        strncpy(out_line, line, out_line_len - 1);
        out_line[out_line_len - 1] = '\0';
    }

    if (strlen(line) < 3 || line[0] < '0' || line[0] > '9') {
        return ESP_FAIL;
    }

    *out_code = (line[0] - '0') * 100 + (line[1] - '0') * 10 + (line[2] - '0');
    return ESP_OK;
}

static esp_err_t ftp_send_command(int sock, const char *fmt, ...)
{
    char cmd[320] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, args);
    va_end(args);

    size_t len = strlen(cmd);
    int written = send(sock, cmd, len, 0);
    if (written != (int)len) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t ftp_open_tcp(const char *host, uint16_t port, int *out_sock)
{
    if (!host || !out_sock) {
        return ESP_ERR_INVALID_ARG;
    }

    char port_str[8] = {0};
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    int gai_ret = getaddrinfo(host, port_str, &hints, &res);
    if (gai_ret != 0 || !res) {
        ESP_LOGW(TAG, "[C] FTP DNS fallito host=%s port=%s", host, port_str);
        return ESP_FAIL;
    }

    int sock = -1;
    for (struct addrinfo *it = res; it != NULL; it = it->ai_next) {
        sock = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (sock < 0) {
            continue;
        }
        if (connect(sock, it->ai_addr, it->ai_addrlen) == 0) {
            break;
        }
        close(sock);
        sock = -1;
    }

    freeaddrinfo(res);

    if (sock < 0) {
        return ESP_FAIL;
    }

    *out_sock = sock;
    return ESP_OK;
}

static void ftp_split_host_port(const char *server, char *host_out, size_t host_len, uint16_t *port_out)
{
    if (!server || !host_out || host_len == 0 || !port_out) {
        return;
    }

    *port_out = 21;
    strncpy(host_out, server, host_len - 1);

    char *colon = strrchr(host_out, ':');
    if (colon && *(colon + 1) != '\0') {
        int port_val = atoi(colon + 1);
        if (port_val > 0 && port_val <= 65535) {
            *port_out = (uint16_t)port_val;
            *colon = '\0';
        }
    }
}

static esp_err_t ftp_parse_pasv(const char *line, char *ip_out, size_t ip_len, uint16_t *port_out)
{
    if (!line || !ip_out || ip_len < 16 || !port_out) {
        return ESP_ERR_INVALID_ARG;
    }

    int h1 = 0, h2 = 0, h3 = 0, h4 = 0, p1 = 0, p2 = 0;
    const char *p = strchr(line, '(');
    if (!p) {
        return ESP_FAIL;
    }

    if (sscanf(p, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        return ESP_FAIL;
    }

    snprintf(ip_out, ip_len, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port_out = (uint16_t)((p1 << 8) | p2);
    return ESP_OK;
}

static const char *http_services_filename_from_remote(const char *remote_name)
{
    if (!remote_name || remote_name[0] == '\0') {
        return "";
    }
    const char *slash = strrchr(remote_name, '/');
    return slash ? (slash + 1) : remote_name;
}

static esp_err_t ftp_download_one_file(int ctrl_sock,
                                       const char *remote_dir,
                                       const char *remote_name,
                                       const char *local_dir)
{
    if (!remote_name || !local_dir) {
        return ESP_ERR_INVALID_ARG;
    }

    char pasv_line[256] = {0};
    if (ftp_send_command(ctrl_sock, "PASV\r\n") != ESP_OK) {
        return ESP_FAIL;
    }

    int code = 0;
    if (ftp_read_response_code(ctrl_sock, &code, pasv_line, sizeof(pasv_line)) != ESP_OK || code != 227) {
        ESP_LOGW(TAG, "[C] FTP PASV fallita: %s", pasv_line);
        return ESP_FAIL;
    }

    char data_ip[32] = {0};
    uint16_t data_port = 0;
    if (ftp_parse_pasv(pasv_line, data_ip, sizeof(data_ip), &data_port) != ESP_OK) {
        ESP_LOGW(TAG, "[C] FTP PASV parse fallita: %s", pasv_line);
        return ESP_FAIL;
    }

    int data_sock = -1;
    if (ftp_open_tcp(data_ip, data_port, &data_sock) != ESP_OK) {
        ESP_LOGW(TAG, "[C] FTP connessione data fallita %s:%u", data_ip, (unsigned)data_port);
        return ESP_FAIL;
    }

    char remote_full[320] = {0};
    if (remote_name[0] == '/') {
        snprintf(remote_full, sizeof(remote_full), "%s", remote_name);
    } else if (remote_dir && remote_dir[0] != '\0') {
        if (remote_dir[strlen(remote_dir) - 1] == '/') {
            snprintf(remote_full, sizeof(remote_full), "%s%s", remote_dir, remote_name);
        } else {
            snprintf(remote_full, sizeof(remote_full), "%s/%s", remote_dir, remote_name);
        }
    } else {
        snprintf(remote_full, sizeof(remote_full), "%s", remote_name);
    }

    if (ftp_send_command(ctrl_sock, "RETR %s\r\n", remote_full) != ESP_OK) {
        close(data_sock);
        return ESP_FAIL;
    }

    char retr_line[256] = {0};
    if (ftp_read_response_code(ctrl_sock, &code, retr_line, sizeof(retr_line)) != ESP_OK ||
        (code != 150 && code != 125)) {
        ESP_LOGW(TAG, "[C] FTP RETR rifiutato (%d): %s", code, retr_line);
        close(data_sock);
        return ESP_FAIL;
    }

    const char *filename = http_services_filename_from_remote(remote_name);
    if (filename[0] == '\0') {
        close(data_sock);
        return ESP_ERR_INVALID_ARG;
    }

    char local_path[320] = {0};
    snprintf(local_path, sizeof(local_path), "%s/%s", local_dir, filename);

    FILE *fp = fopen(local_path, "wb");
    if (!fp) {
        ESP_LOGW(TAG, "[C] Apertura file locale fallita: %s", local_path);
        close(data_sock);
        return ESP_FAIL;
    }

    char buf[1024];
    while (1) {
        int rr = recv(data_sock, buf, sizeof(buf), 0);
        if (rr <= 0) {
            break;
        }
        if (fwrite(buf, 1, (size_t)rr, fp) != (size_t)rr) {
            fclose(fp);
            close(data_sock);
            return ESP_FAIL;
        }
    }

    fclose(fp);
    close(data_sock);

    char done_line[256] = {0};
    if (ftp_read_response_code(ctrl_sock, &code, done_line, sizeof(done_line)) != ESP_OK ||
        (code != 226 && code != 250)) {
        ESP_LOGW(TAG, "[C] FTP completamento RETR fallito (%d): %s", code, done_line);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[C] FTP file scaricato: %s -> %s", remote_full, local_path);
    return ESP_OK;
}

static esp_err_t ftp_download_filelist(const char *server,
                                       const char *user,
                                       const char *password,
                                       const char *remote_dir,
                                       const char *local_dir,
                                       cJSON *files_array)
{
    if (!server || !user || !password || !local_dir || !files_array || !cJSON_IsArray(files_array)) {
        return ESP_ERR_INVALID_ARG;
    }

    char host[128] = {0};
    uint16_t port = 21;
    ftp_split_host_port(server, host, sizeof(host), &port);

    int ctrl_sock = -1;
    if (ftp_open_tcp(host, port, &ctrl_sock) != ESP_OK) {
        ESP_LOGW(TAG, "[C] FTP connessione controllo fallita %s:%u", host, (unsigned)port);
        return ESP_FAIL;
    }

    int code = 0;
    char line[256] = {0};
    esp_err_t err = ftp_read_response_code(ctrl_sock, &code, line, sizeof(line));
    if (err != ESP_OK || code != 220) {
        ESP_LOGW(TAG, "[C] FTP greeting non valido (%d): %s", code, line);
        close(ctrl_sock);
        return ESP_FAIL;
    }

    if (ftp_send_command(ctrl_sock, "USER %s\r\n", user) != ESP_OK) {
        close(ctrl_sock);
        return ESP_FAIL;
    }
    err = ftp_read_response_code(ctrl_sock, &code, line, sizeof(line));
    if (err != ESP_OK || (code != 230 && code != 331)) {
        ESP_LOGW(TAG, "[C] FTP USER fallito (%d): %s", code, line);
        close(ctrl_sock);
        return ESP_FAIL;
    }

    if (code == 331) {
        if (ftp_send_command(ctrl_sock, "PASS %s\r\n", password) != ESP_OK) {
            close(ctrl_sock);
            return ESP_FAIL;
        }
        err = ftp_read_response_code(ctrl_sock, &code, line, sizeof(line));
        if (err != ESP_OK || code != 230) {
            ESP_LOGW(TAG, "[C] FTP PASS fallito (%d): %s", code, line);
            close(ctrl_sock);
            return ESP_FAIL;
        }
    }

    if (ftp_send_command(ctrl_sock, "TYPE I\r\n") != ESP_OK) {
        close(ctrl_sock);
        return ESP_FAIL;
    }
    err = ftp_read_response_code(ctrl_sock, &code, line, sizeof(line));
    if (err != ESP_OK || code != 200) {
        ESP_LOGW(TAG, "[C] FTP TYPE I fallito (%d): %s", code, line);
        close(ctrl_sock);
        return ESP_FAIL;
    }

    cJSON *item = NULL;
    esp_err_t global_err = ESP_OK;
    cJSON_ArrayForEach(item, files_array) {
        const char *remote_name = NULL;
        if (cJSON_IsString(item) && item->valuestring) {
            remote_name = item->valuestring;
        } else if (cJSON_IsObject(item)) {
            cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
            if (cJSON_IsString(name) && name->valuestring) {
                remote_name = name->valuestring;
            }
        }

        if (!remote_name || remote_name[0] == '\0') {
            continue;
        }

        if (ftp_download_one_file(ctrl_sock, remote_dir, remote_name, local_dir) != ESP_OK) {
            global_err = ESP_FAIL;
        }
    }

    ftp_send_command(ctrl_sock, "QUIT\r\n");
    ftp_read_response_code(ctrl_sock, &code, line, sizeof(line));
    close(ctrl_sock);
    return global_err;
}

static void http_services_sync_ftp_for_endpoint(const char *remote_path, const char *local_dir)
{
    device_config_t *cfg = device_config_get();
    if (!http_services_ftp_enabled(cfg)) {
        return;
    }

    if (!remote_path || !local_dir) {
        return;
    }

    if (http_services_login_if_needed(false) != ESP_OK) {
        ESP_LOGW(TAG, "[C] FTP agent skip: login non disponibile per %s", remote_path);
        return;
    }

    char *resp = NULL;
    int status = 0;
    size_t resp_len = 0;
    if (remote_post(remote_path, "{}", NULL, &resp, &status, &resp_len) != ESP_OK || !resp) {
        if (resp) {
            free(resp);
        }
        ESP_LOGW(TAG, "[C] FTP agent skip: risposta remota non disponibile (%s)", remote_path);
        return;
    }

    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "[C] FTP agent skip: HTTP %d su %s", status, remote_path);
        free(resp);
        return;
    }

    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) {
        ESP_LOGW(TAG, "[C] FTP agent skip: JSON non valido per %s", remote_path);
        return;
    }

    cJSON *iserror = cJSON_GetObjectItemCaseSensitive(root, "iserror");
    if (cJSON_IsBool(iserror) && cJSON_IsTrue(iserror)) {
        ESP_LOGW(TAG, "[C] FTP agent skip: risposta iserror=true per %s", remote_path);
        cJSON_Delete(root);
        return;
    }

    cJSON *files = cJSON_GetObjectItemCaseSensitive(root, "files");
    if (!files || !cJSON_IsArray(files)) {
        cJSON_Delete(root);
        return;
    }

    char ftp_server[sizeof(cfg->ftp.server)] = {0};
    char ftp_user[sizeof(cfg->ftp.user)] = {0};
    char ftp_password[sizeof(cfg->ftp.password)] = {0};
    char ftp_path[sizeof(cfg->ftp.path)] = {0};

    cJSON *server_item = cJSON_GetObjectItemCaseSensitive(root, "server");
    cJSON *user_item = cJSON_GetObjectItemCaseSensitive(root, "user");
    cJSON *password_item = cJSON_GetObjectItemCaseSensitive(root, "password");
    cJSON *path_item = cJSON_GetObjectItemCaseSensitive(root, "path");

    strncpy(ftp_server,
            (cJSON_IsString(server_item) && server_item->valuestring) ? server_item->valuestring : cfg->ftp.server,
            sizeof(ftp_server) - 1);
    strncpy(ftp_user,
            (cJSON_IsString(user_item) && user_item->valuestring) ? user_item->valuestring : cfg->ftp.user,
            sizeof(ftp_user) - 1);
    strncpy(ftp_password,
            (cJSON_IsString(password_item) && password_item->valuestring) ? password_item->valuestring : cfg->ftp.password,
            sizeof(ftp_password) - 1);
    strncpy(ftp_path,
            (cJSON_IsString(path_item) && path_item->valuestring) ? path_item->valuestring : cfg->ftp.path,
            sizeof(ftp_path) - 1);

    if (ftp_server[0] == '\0' || ftp_user[0] == '\0') {
        ESP_LOGW(TAG, "[C] FTP agent skip: credenziali incomplete per %s", remote_path);
        cJSON_Delete(root);
        return;
    }

    if (http_services_ensure_parent_dirs(local_dir) != ESP_OK) {
        ESP_LOGW(TAG, "[C] FTP agent: directory locale non disponibile: %s", local_dir);
        cJSON_Delete(root);
        return;
    }

    esp_err_t dl_err = ftp_download_filelist(ftp_server, ftp_user, ftp_password, ftp_path, local_dir, files);
    if (dl_err != ESP_OK) {
        ESP_LOGW(TAG, "[C] FTP agent: download con errori su %s", remote_path);
    } else {
        ESP_LOGI(TAG, "[C] FTP agent: download completato per %s", remote_path);
    }

    cJSON_Delete(root);
}

// Temporary implementation of validate_token

/**
 * @brief Valida un token.
 * 
 * Questa funzione verifica la validità di un token fornito come input.
 * 
 * @param [in] token Puntatore al token da validare.
 * @return true se il token è valido, false altrimenti.
 */
bool validate_token(const char *token) {
    // Add actual token validation logic here
    return token != NULL && strlen(token) > 0;
}

/**
 * @brief Verifica se il server remoto è abilitato e configurato.
 */
static void http_services_buffer_append(const char *json_payload)
{
    if (!s_http_buffering_enabled || !json_payload || json_payload[0] == '\0') {
        return;
    }

    size_t add_len = strlen(json_payload);
    size_t needed = s_http_pending_len + add_len + 1;
    if (needed > CONFIG_HTTP_PENDING_BUFFER_MAX) {
        /* drop oldest data by resetting */
        s_http_pending_len = 0;
        needed = add_len + 1;
    }
    if (needed > s_http_pending_cap) {
        size_t new_cap = s_http_pending_cap ? s_http_pending_cap : 512;
        while (new_cap < needed) {
            new_cap *= 2;
            if (new_cap > CONFIG_HTTP_PENDING_BUFFER_MAX) {
                new_cap = CONFIG_HTTP_PENDING_BUFFER_MAX;
                break;
            }
        }
        char *tmp = realloc(s_http_pending_json, new_cap);
        if (!tmp) {
            s_http_pending_len = 0;
            return;
        }
        s_http_pending_json = tmp;
        s_http_pending_cap = new_cap;
    }
    memcpy(s_http_pending_json + s_http_pending_len, json_payload, add_len);
    s_http_pending_len += add_len;
    s_http_pending_json[s_http_pending_len] = '\0';
}

static esp_err_t __attribute__((unused)) http_services_buffer_flush(void)
{
    if (!s_http_pending_json || s_http_pending_len == 0) {
        s_http_buffer_flushed = true;
        return ESP_OK;
    }

    device_config_t *cfg = device_config_get();
    if (!cfg || !http_services_cfg_remote_enabled(cfg)) {
        return ESP_ERR_INVALID_STATE;
    }

    char *body = malloc(s_http_pending_len + 32);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(body, s_http_pending_json, s_http_pending_len);
    body[s_http_pending_len] = '\0';

    char *resp = NULL;
    int status = 0;
    size_t resp_len = 0;
    esp_err_t err = remote_post(CONFIG_HTTP_PENDING_ENDPOINT, body, NULL, &resp, &status, &resp_len);
    free(body);
    if (resp) {
        free(resp);
    }
    if (err == ESP_OK) {
        s_http_pending_len = 0;
        s_http_buffer_flushed = true;
    }
    return err;
}

static bool http_services_cfg_remote_enabled(const device_config_t *cfg)
{
    return (cfg != NULL) && cfg->server.enabled && (cfg->server.url[0] != '\0');
}

/**
 * @brief Aggiorna lo stato ONLINE/OFFLINE del server remoto.
 */
static void http_services_set_online(bool online, const char *reason)
{
    if (s_remote_online == online) {
        return;
    }
    s_remote_online = online;
    ESP_LOGI(TAG,
             "[C] Stato server remoto: %s (%s)",
             online ? "ONLINE" : "OFFLINE",
             reason ? reason : "n/a");
}

/**
 * @brief Azzera il token remoto memorizzato.
 */
static void http_services_clear_token(const char *reason)
{
    if (g_auth_token[0] == '\0') {
        return;
    }
    g_auth_token[0] = '\0';
    ESP_LOGI(TAG, "[C] Token remoto azzerato (%s)", reason ? reason : "n/a");
}

/**
 * @brief Controlla se i servizi HTTP sono abilitati per l'accesso remoto.
 *
 * Questa funzione verifica se i servizi HTTP sono configurati per consentire
 * l'accesso remoto.
 *
 * @return true se i servizi HTTP sono abilitati per l'accesso remoto, false altrimenti.
 */
bool http_services_is_remote_enabled(void)
{
    const device_config_t *cfg = device_config_get();
    return http_services_cfg_remote_enabled(cfg);
}

/**
 * @brief Controlla se il servizio remoto HTTP è online.
 * 
 * @return true se il servizio remoto HTTP è online, false altrimenti.
 * @param [in] Nessun parametro di input.
 */
bool http_services_is_remote_online(void)
{
    if (!http_services_is_remote_enabled()) {
        return false;
    }
    return s_remote_online;
}

/**
 * @brief Controlla se il servizio HTTP ha un token di autenticazione.
 *
 * @return true se il servizio HTTP ha un token di autenticazione, false altrimenti.
 */
bool http_services_has_auth_token(void)
{
    return validate_token(g_auth_token);
}

/**
 * @brief Sincronizza lo stato di runtime dei servizi HTTP.
 *
 * Questa funzione sincronizza lo stato di runtime dei servizi HTTP.
 * Se il parametro force_login è true, la funzione forza il login prima di sincronizzare lo stato.
 *
 * @param [in] force_login Flag booleano che indica se forzare il login.
 * @return esp_err_t Codice di errore.
 */
esp_err_t http_services_sync_runtime_state(bool force_login)
{
    device_config_t *cfg = device_config_get();
    if (!http_services_cfg_remote_enabled(cfg)) {
        if (keepalive_task_is_running()) {
            esp_err_t keepalive_stop_err = keepalive_task_stop();
            if (keepalive_stop_err != ESP_OK) {
                ESP_LOGW(TAG,
                         "[C] Arresto keepalive fallito durante disable remoto: %s",
                         esp_err_to_name(keepalive_stop_err));
            }
        }
        http_services_clear_token("server_disabled");
        http_services_set_online(false, "server_disabled");
        return ESP_OK;
    }

    if (!keepalive_task_is_running()) {
        esp_err_t keepalive_start_err = keepalive_task_start();
        if (keepalive_start_err != ESP_OK) {
            ESP_LOGW(TAG,
                     "[C] Avvio keepalive task fallito: %s",
                     esp_err_to_name(keepalive_start_err));
        }
    }

    if (!force_login && validate_token(g_auth_token)) {
        http_services_set_online(true, "token_cached");
        return ESP_OK;
    }

    if (force_login) {
        http_services_clear_token("force_login");
    }

    esp_err_t login_err = http_services_login_if_needed(true);
    if (login_err != ESP_OK) {
        ESP_LOGW(TAG,
                 "[C] Sync runtime server remoto fallita: %s",
                 esp_err_to_name(login_err));
        return login_err;
    }

    http_services_set_online(true, "login_ok");
    return ESP_OK;
}

esp_err_t http_services_keepalive(const char *status,
                                  const char *inputstates,
                                  const char *outputstates,
                                  int32_t temperature,
                                  int32_t humidity,
                                  http_services_keepalive_response_t *out)
{
    if (!inputstates || !outputstates || !out) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    esp_err_t token_err = http_services_login_if_needed(false);
    if (token_err != ESP_OK) {
        out->common.iserror = true;
        out->common.codeerror = -10;
        snprintf(out->common.deserror,
                 sizeof(out->common.deserror),
                 "login_failed:%s",
                 esp_err_to_name(token_err));
        return token_err;
    }

    cJSON *req = cJSON_CreateObject();
    if (!req) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(req, "status", (status && status[0] != '\0') ? status : "OK");
    cJSON_AddStringToObject(req, "inputstates", inputstates);
    cJSON_AddStringToObject(req, "outputstates", outputstates);
    cJSON_AddNumberToObject(req, "temperature", temperature);
    cJSON_AddNumberToObject(req, "humidity", humidity);

    cJSON *subdevices = cJSON_AddArrayToObject(req, "subdevices");
    if (!subdevices) {
        cJSON_Delete(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON *qrc = cJSON_CreateObject();
    cJSON *prn1 = cJSON_CreateObject();
    if (!qrc || !prn1) {
        if (qrc) cJSON_Delete(qrc);
        if (prn1) cJSON_Delete(prn1);
        cJSON_Delete(req);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(qrc, "code", "QRC");
    cJSON_AddStringToObject(qrc, "status", "OK");
    cJSON_AddItemToArray(subdevices, qrc);

    cJSON_AddStringToObject(prn1, "code", "PRN1");
    cJSON_AddStringToObject(prn1, "status", "OK");
    cJSON_AddItemToArray(subdevices, prn1);

    char *body = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "[C] keepalive: body=%s", body);

    char *resp = NULL;
    int status_code = 0;
    size_t resp_len = 0;
    esp_err_t err = ESP_FAIL;

    for (int attempt = 0; attempt < 2; ++attempt) {
        err = remote_post("/api/keepalive", body, NULL, &resp, &status_code, &resp_len);
        if (err != ESP_OK) {
            break;
        }

        if ((status_code == 401 || status_code == 403) && attempt == 0) {
            ESP_LOGW(TAG, "[C] keepalive non autorizzato (HTTP %d), rinnovo token", status_code);
            http_services_clear_token("keepalive_unauthorized");
            if (resp) {
                free(resp);
                resp = NULL;
            }
            token_err = http_services_login_if_needed(true);
            if (token_err != ESP_OK) {
                err = token_err;
                break;
            }
            continue;
        }

        break;
    }

    free(body);

    if (err != ESP_OK) {
        out->common.iserror = true;
        out->common.codeerror = -1;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "%s", esp_err_to_name(err));
        if (resp) {
            free(resp);
        }
        return err;
    }

    if (status_code < 200 || status_code >= 300) {
        out->common.iserror = true;
        out->common.codeerror = status_code;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "http_%d", status_code);
        if (resp) {
            free(resp);
        }
        return ESP_FAIL;
    }

    if (!resp) {
        out->common.iserror = true;
        out->common.codeerror = -2;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "no_response");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) {
        out->common.iserror = true;
        out->common.codeerror = -3;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "json_parse_error");
        return ESP_FAIL;
    }

    cJSON *je = cJSON_GetObjectItemCaseSensitive(root, "iserror");
    out->common.iserror = cJSON_IsTrue(je);
    cJSON *jce = cJSON_GetObjectItemCaseSensitive(root, "codeerror");
    if (cJSON_IsNumber(jce)) {
        out->common.codeerror = (int32_t)jce->valueint;
    }
    cJSON *jde = cJSON_GetObjectItemCaseSensitive(root, "deserror");
    if (cJSON_IsString(jde) && jde->valuestring) {
        snprintf(out->common.deserror, sizeof(out->common.deserror), "%s", jde->valuestring);
    }
    cJSON *jdt = cJSON_GetObjectItemCaseSensitive(root, "datetime");
    if (cJSON_IsString(jdt) && jdt->valuestring) {
        snprintf(out->datetime, sizeof(out->datetime), "%s", jdt->valuestring);
    }

    cJSON *activities = cJSON_GetObjectItemCaseSensitive(root, "activities");
    if (cJSON_IsArray(activities)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, activities) {
            if (out->activity_count >= HTTP_SERVICES_MAX_ACTIVITIES) {
                break;
            }
            http_services_activity_t *activity = &out->activities[out->activity_count];
            cJSON *jaid = cJSON_GetObjectItemCaseSensitive(item, "activityid");
            cJSON *jcode = cJSON_GetObjectItemCaseSensitive(item, "code");
            cJSON *jparams = cJSON_GetObjectItemCaseSensitive(item, "parameters");

            if (cJSON_IsNumber(jaid)) {
                activity->activityid = (int32_t)jaid->valueint;
            }
            if (cJSON_IsString(jcode) && jcode->valuestring) {
                snprintf(activity->code, sizeof(activity->code), "%s", jcode->valuestring);
            }
            if (cJSON_IsString(jparams) && jparams->valuestring) {
                snprintf(activity->parameters, sizeof(activity->parameters), "%s", jparams->valuestring);
            }
            out->activity_count++;
        }
    }

    cJSON_Delete(root);
    return out->common.iserror ? ESP_FAIL : ESP_OK;
}

/**
 * @brief Salva in modo sicuro il token JWT remoto nel buffer globale.
 *
 * @param [in] token Token da salvare.
 * @param [in] source Origine logica del token (solo per log diagnostico).
 * @return ESP_OK se salvato correttamente, errore in caso di token non valido/troncabile.
 */
static esp_err_t store_auth_token(const char *token, const char *source)
{
    if (!token || token[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    size_t token_len = strlen(token);
    if (token_len >= sizeof(g_auth_token)) {
        g_auth_token[0] = '\0';
        ESP_LOGE(TAG,
                 "[C] access_token troppo lungo (%u >= %u) da %s",
                 (unsigned)token_len,
                 (unsigned)sizeof(g_auth_token),
                 source ? source : "unknown");
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(g_auth_token, token, token_len + 1);
    ESP_LOGI(TAG,
             "[C] access_token salvato da %s (len=%u)",
             source ? source : "unknown",
             (unsigned)token_len);
    return ESP_OK;
}

/* Read the whole request body into a NUL-terminated buffer (caller frees) */
static char *read_request_body(httpd_req_t *req)
{
    if (!req) return NULL;
    int total_len = req->content_len;
    if (total_len <= 0) return NULL;

    char *buf = malloc(total_len + 1);
    if (!buf) return NULL;

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) {
            free(buf);
            return NULL;
        }
        received += ret;
    }
    buf[received] = '\0';
    return buf;
}

/* Format full date-time string in ISO format */
static void format_full_datetime(char *out, size_t len)
{
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm); /* use UTC for the "Z" suffix */
    strftime(out, len, "%Y-%m-%dT%H:%M:%S.000Z", &tm);
}

/*
 * Optional: forward verbose HTTP logs into the web UI `/logs` buffer.
 * Controlled at compile-time with `-DHTTP_SERVICES_LOG_TO_UI`.
 * When enabled, long messages are chunked to fit the `web_ui` store.
 */
#ifdef HTTP_SERVICES_LOG_TO_UI

/**
 * @brief Registra un blocco di log in modo chunked.
 *
 * @param [in] level Livello di log (ad esempio, "INFO", "WARNING", "ERROR").
 * @param [in] tag Etichetta associata al log.
 * @param [in] label Etichetta del blocco di log.
 * @param [in] msg Messaggio di log da registrare.
 */
static void webui_log_chunked(const char *level, const char *tag, const char *label, const char *msg)
{
    if (!msg) return;
    const size_t CHUNK = 200; /* web_ui stores ~255 chars per message */
    size_t len = strlen(msg);
    if (len == 0) {
        web_ui_add_log(level, tag, label);
        return;
    }
    char buf[256];
    size_t offset = 0;
    int part = 1;
    while (offset < len) {
        size_t n = len - offset;
        if (n > CHUNK) n = CHUNK;
        if (label && label[0] != '\0')
            snprintf(buf, sizeof(buf), "%s (part %d): %.*s", label, part, (int)n, msg + offset);
        else
            snprintf(buf, sizeof(buf), "part %d: %.*s", part, (int)n, msg + offset);
        web_ui_add_log(level, tag, buf);
        offset += n;
        part++;
    }
}
#endif

/* small accumulator type used by the http client event handler */
#if !DNA_SERVER_POST
typedef struct { char *buf; size_t len; size_t cap; } http_acc_t;
#endif

/* Helper: log incoming httpd request (method/uri, selected headers and body) */
static void log_httpd_request(httpd_req_t *req, const char *body)
{
    if (!req) return;
    ESP_LOGI(TAG, ">>> HTTP IN: uri=%s content_len=%d", req->uri?req->uri:"(null)", (int)req->content_len);

    const char *hdr_names[] = {"Host","User-Agent","Content-Type","Date","X-Request-Date","X-Date","Authorization","Accept","Accept-Encoding","Cache-Control","Connection","Content-Length"};
    bool date_found = false;
    for (size_t i = 0; i < sizeof(hdr_names)/sizeof(hdr_names[0]); ++i) {
        size_t vlen = httpd_req_get_hdr_value_len(req, hdr_names[i]);
        if (vlen > 0) {
            char *v = malloc(vlen + 1);
            if (v) {
                if (httpd_req_get_hdr_value_str(req, hdr_names[i], v, vlen + 1) == ESP_OK) {
                    ESP_LOGI(TAG, "Header: %s: %s", hdr_names[i], v);
#ifdef HTTP_SERVICES_LOG_TO_UI
                    web_ui_add_log("INFO", TAG, v);
#endif
                    if (strcasecmp(hdr_names[i], "Date") == 0 || strcasecmp(hdr_names[i], "X-Request-Date") == 0 || strcasecmp(hdr_names[i], "X-Date") == 0) {
                        date_found = true;
                    }
                }
                free(v);
            }
        }
    }

    /* If client didn't provide a Date or equivalent header, log a generated Date so it's visible */
    if (!date_found) {
        char gen_date[64];
        format_full_datetime(gen_date, sizeof(gen_date));
        ESP_LOGI(TAG, "Header: Date (generated): %s", gen_date);
#ifdef HTTP_SERVICES_LOG_TO_UI
        web_ui_add_log("INFO", TAG, gen_date);
#endif
    }

    if (body && body[0] != '\0') {
        size_t blen = strlen(body);
        const size_t MAX_LOG = 1024;
        if (blen <= MAX_LOG) ESP_LOGI(TAG, "Body: %s", body);
        else {
            char *tmp = strndup(body, MAX_LOG);
            ESP_LOGI(TAG, "Body (truncated %u/%u): %s ...", (unsigned)MAX_LOG, (unsigned)blen, tmp);
            free(tmp);
        }
#ifdef HTTP_SERVICES_LOG_TO_UI
        webui_log_chunked("INFO", TAG, "Body", body);
#endif
    } else {
        ESP_LOGI(TAG, "Body: <empty>");
#ifdef HTTP_SERVICES_LOG_TO_UI
        web_ui_add_log("INFO", TAG, "<empty body>");
#endif
    }
}

static esp_err_t __attribute__((unused)) remote_post_pending(const char *remote_path, const char *body)
{
    (void)remote_path;
    if (!body) {
        return ESP_ERR_INVALID_ARG;
    }
    http_services_buffer_append(body);
    return ESP_OK;
}

/* --- Proxy implementations forwarding to remote server (uses device_config.server.url + credentials) --- */

#if !DNA_SERVER_POST
/* HTTP client event handler accumulator: collects HTTP_EVENT_ON_DATA into a dynamic buffer
   so responses delivered via the event path (chunked or already-copied-perform) are not lost.
*/
static esp_err_t http_client_event_accumulator(esp_http_client_event_t *evt)
{
    http_acc_t *acc = (http_acc_t *)evt->user_data;
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (!acc || evt->data_len <= 0) break;
        if (acc->len + evt->data_len + 1 > acc->cap) {
            size_t newcap = acc->cap ? acc->cap * 2 : 2048;
            while (newcap < acc->len + evt->data_len + 1) newcap *= 2;
            char *p = realloc(acc->buf, newcap);
            if (!p) { free(acc->buf); acc->buf = NULL; acc->len = acc->cap = 0; return ESP_ERR_NO_MEM; }
            acc->buf = p; acc->cap = newcap;
        }
        memcpy(acc->buf + acc->len, evt->data, evt->data_len);
        acc->len += evt->data_len;
        acc->buf[acc->len] = '\0';
        break;
    default:
        break;
    }
    return ESP_OK;
}
#endif

/* Helper: perform HTTP POST to remote server (returns dynamically allocated response in out_resp)
   and reports the number of bytes read via out_len (may differ from strlen when binary/NULs are present). */
static esp_err_t remote_post(const char *remote_path, const char *body, const char *auth_header, char **out_resp, int *out_status, size_t *out_len)
{
#if DNA_SERVER_POST
    (void)body;
    (void)auth_header;
    if (out_resp) {
        *out_resp = strdup("{\"iserror\":true,\"codeerror\":503,\"deserror\":\"debug_post_disabled\"}");
    }
    if (out_status) {
        *out_status = 503;
    }
    if (out_len) {
        *out_len = 0;
    }
    ESP_LOGW(TAG, "POST remoto inibito da DNA_SERVER_POST (path=%s)", remote_path ? remote_path : "(null)");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!remote_path) return ESP_ERR_INVALID_ARG;
    device_config_t *cfg = device_config_get();
    if (!http_services_cfg_remote_enabled(cfg)) {
        http_services_clear_token("remote_disabled");
        http_services_set_online(false, "remote_disabled");
        if (out_resp) {
            *out_resp = strdup("{\"iserror\":true,\"codeerror\":503,\"deserror\":\"remote_disabled\"}");
        }
        if (out_status) {
            *out_status = 503;
        }
        if (out_len) {
            *out_len = 0;
        }
        ESP_LOGW(TAG,
                 "[C] POST remoto bloccato: server disabilitato (path=%s)",
                 remote_path ? remote_path : "(null)");
        return ESP_ERR_INVALID_STATE;
    }

    /* build URL (avoid double slashes) */
    const char *base = cfg->server.url;
    char url[384];
    size_t bl = strlen(base);
    if (bl > 0 && base[bl-1] == '/' && remote_path[0] == '/') snprintf(url, sizeof(url), "%s%s", base, remote_path + 1);
    else if (bl > 0 && base[bl-1] == '/' ) snprintf(url, sizeof(url), "%s%s", base, remote_path);
    else if (remote_path[0] == '/') snprintf(url, sizeof(url), "%s%s", base, remote_path);
    else snprintf(url, sizeof(url), "%s/%s", base, remote_path);

    /* accumulator used by event handler to collect ON_DATA events */
    http_acc_t acc = { .buf = NULL, .len = 0, .cap = 0 };

    esp_http_client_config_t cfg_http = {
        .url = url,
        .timeout_ms = 5000,  /* [C] Ridotto da 15s a 5s per riconoscimento veloce disconnessioni */
        .skip_cert_common_name_check = true,
        /* increase internal buffers so large response headers (or many headers) fit */
        .buffer_size = 8192,
        .buffer_size_tx = 8192,
    };
    /* attach event handler + user_data so esp_http_client_perform() delivers body via events */
    cfg_http.event_handler = http_client_event_accumulator;
    cfg_http.user_data = &acc;

    /* perform up to two attempts when we see incomplete/chunked errors */
    esp_err_t err = ESP_FAIL;
    int status = 0;
    const int max_attempts = 2;
    esp_http_client_handle_t client = NULL;
    /* diagnostic: server-reported content-length (set after perform) */
    long resp_content_len = -1;

    /* response buffer that will be filled inside the attempt loop */
    char *resp = NULL;
    size_t total = 0;
    bool use_fallback_headers = true;
    char *generated_auth_header = NULL;

    if ((!auth_header || auth_header[0] == '\0') && g_auth_token[0] != '\0') {
        size_t token_len = strlen(g_auth_token);
        size_t needed_len = strlen("Bearer ") + token_len + 1;
        generated_auth_header = malloc(needed_len);
        if (!generated_auth_header) {
            return ESP_ERR_NO_MEM;
        }
        snprintf(generated_auth_header, needed_len, "Bearer %s", g_auth_token);
    }

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        if (client) {
            esp_http_client_cleanup(client);
            client = NULL;
            vTaskDelay(pdMS_TO_TICKS(150));
        }

        client = esp_http_client_init(&cfg_http);
        if (!client) { err = ESP_ERR_NO_MEM; break; }

        esp_http_client_set_method(client, HTTP_METHOD_POST);

        /* only required headers for the remote server: Content-Type and Date
           on fallback attempt we will also force Connection: close and Accept-Encoding: identity */
        const char hdr_content_type[40] = "application/json";
        esp_http_client_set_header(client, "Content-Type", hdr_content_type);

        char date_hdr[64] = "2026-01-23T13:25:13.218763+01:00";
        esp_http_client_set_header(client, "Date", date_hdr);

        if (use_fallback_headers) {
            esp_http_client_set_header(client, "Connection", "close");
            esp_http_client_set_header(client, "Accept-Encoding", "identity");
            ESP_LOGI(TAG, "Using fallback headers: Connection: close, Accept-Encoding: identity (attempt %d)", attempt + 1);
        }

        const char *hdr_authorization = NULL;
        if (auth_header && strlen(auth_header) > 0) {
            hdr_authorization = auth_header;
        } else if (generated_auth_header) {
            hdr_authorization = generated_auth_header;
        }
        if (hdr_authorization) {
            esp_http_client_set_header(client, "Authorization", hdr_authorization);
        }

        const char *post_body = body ? body : "{}";
        esp_http_client_set_post_field(client, post_body, strlen(post_body));

        /* LOG: outgoing request (URL, headers we set, body summary) */
        ESP_LOGI(TAG, ">>> HTTP OUT: POST %s (attempt %d)", url, attempt + 1);
#ifdef HTTP_SERVICES_LOG_TO_UI
        web_ui_add_log("INFO", TAG, url);
#endif
        {
            /* log the Date value actually sent */
            ESP_LOGI(TAG, "OUT Header: Date: %s", date_hdr);
#ifdef HTTP_SERVICES_LOG_TO_UI
            web_ui_add_log("INFO", TAG, date_hdr);
#endif
        }
        if (hdr_authorization) {
            ESP_LOGI(TAG, "OUT Header: Authorization: %s", hdr_authorization);
            ESP_LOGI(TAG, "[C] OUT Authorization length=%u", (unsigned)strlen(hdr_authorization));
        }
        ESP_LOGI(TAG, "OUT Header: Content-Type: %s", hdr_content_type);

#ifdef HTTP_SERVICES_LOG_TO_UI
        if (hdr_authorization) webui_log_chunked("INFO", TAG, "OUT Authorization", hdr_authorization);
        webui_log_chunked("INFO", TAG, "OUT Header: Content-Type", hdr_content_type);
#endif

        if (post_body && post_body[0] != '\0') {
            size_t pbl = strlen(post_body); size_t MAX_LOG = 1024;
            if (pbl <= MAX_LOG) ESP_LOGI(TAG, "OUT Body: %s", post_body); else { char *tmp = strndup(post_body, MAX_LOG); ESP_LOGI(TAG, "OUT Body (truncated %u/%u): %s ...", (unsigned)MAX_LOG, (unsigned)pbl, tmp); free(tmp); }
#ifdef HTTP_SERVICES_LOG_TO_UI
            webui_log_chunked("INFO", TAG, "OUT Body", post_body);
#endif
        } else {
            ESP_LOGI(TAG, "OUT Body: <empty>");
#ifdef HTTP_SERVICES_LOG_TO_UI
            web_ui_add_log("INFO", TAG, "OUT Body: <empty>");
#endif
        }

        /* perform (riduce rumore log da parser chunked del client IDF) */
        esp_log_level_t prev_http_client_level = esp_log_level_get("HTTP_CLIENT");
        esp_log_level_t prev_esp_http_client_level = esp_log_level_get("esp_http_client");
        esp_log_level_t prev_esp_tls_level = esp_log_level_get("esp_tls");
        esp_log_level_set("HTTP_CLIENT", ESP_LOG_NONE);
        esp_log_level_set("esp_http_client", ESP_LOG_NONE);
        esp_log_level_set("esp_tls", ESP_LOG_WARN);
        err = esp_http_client_perform(client);
        status = esp_http_client_get_status_code(client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_http_client_perform returned %s (attempt %d)", esp_err_to_name(err), attempt + 1);
        }
        /* ripristina i livelli precedenti */
        esp_log_level_set("HTTP_CLIENT", prev_http_client_level);
        esp_log_level_set("esp_http_client", prev_esp_http_client_level);
        esp_log_level_set("esp_tls", prev_esp_tls_level);

        /* log response headers we care about (may help debug chunking issues).
           esp_http_client_get_header() returns a pointer to the client's internal header value — DO NOT free it. */
        char *hdr = NULL;
        if (esp_http_client_get_header(client, "Transfer-Encoding", &hdr) == ESP_OK && hdr) { ESP_LOGI(TAG, "RESP Header: Transfer-Encoding: %s", hdr); hdr = NULL; }
        if (esp_http_client_get_header(client, "Content-Length", &hdr) == ESP_OK && hdr) { ESP_LOGI(TAG, "RESP Header: Content-Length: %s", hdr); hdr = NULL; }
        if (esp_http_client_get_header(client, "Content-Encoding", &hdr) == ESP_OK && hdr) { ESP_LOGI(TAG, "RESP Header: Content-Encoding: %s", hdr); hdr = NULL; }
        if (esp_http_client_get_header(client, "Connection", &hdr) == ESP_OK && hdr) { ESP_LOGI(TAG, "RESP Header: Connection: %s", hdr); hdr = NULL; }
        if (esp_http_client_get_header(client, "Server", &hdr) == ESP_OK && hdr) { ESP_LOGI(TAG, "RESP Header: Server: %s", hdr); hdr = NULL; }
        if (esp_http_client_get_header(client, "Content-Type", &hdr) == ESP_OK && hdr) { ESP_LOGI(TAG, "RESP Header: Content-Type: %s", hdr); hdr = NULL; }

        /* diagnostic: check reported content-length from the client library */
        resp_content_len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "RESP reported Content-Length (esp_http_client_get_content_length) = %ld", resp_content_len);
#ifdef HTTP_SERVICES_LOG_TO_UI
        char clbuf[64]; snprintf(clbuf, sizeof(clbuf), "RESP reported Content-Length: %ld", resp_content_len); web_ui_add_log("INFO", TAG, clbuf);
#endif

        /* If event-accumulator collected data during perform(), prefer it (covers chunked/on-data path
           and cases where perform() already handed data to the internal event handler). */
        if (acc.len > 0 && acc.buf) {
            resp = acc.buf; /* transfer ownership to resp */
            total = acc.len;
            acc.buf = NULL;
            ESP_LOGI(TAG, "Response collected via event handler: %u bytes", (unsigned)total);
        } else {
            /* read response (streaming, unknown length) */
            char *tmp_resp = malloc(1);
            if (!tmp_resp) { esp_http_client_cleanup(client); err = ESP_ERR_NO_MEM; break; }
            tmp_resp[0] = '\0';
            size_t tmp_total = 0;
            char rbuf[512];
            int rr = 0;
            int chunk = 0;

            /* first-pass read */
            while ((rr = esp_http_client_read(client, rbuf, sizeof(rbuf))) > 0) {
                ESP_LOGD(TAG, "esp_http_client_read chunk %d => %d bytes", chunk, rr);
                if (chunk == 0) {
                    int sample_n = rr < 128 ? rr : 128;
                    char sample[129];
                    memcpy(sample, rbuf, sample_n);
                    sample[sample_n] = '\0';
                    ESP_LOGD(TAG, "First RESP chunk sample: %s", sample);
    #ifdef HTTP_SERVICES_LOG_TO_UI
                    webui_log_chunked("DEBUG", TAG, "First RESP chunk", sample);
    #endif
                }
                char *tmp = realloc(tmp_resp, tmp_total + rr + 1);
                if (!tmp) { free(tmp_resp); tmp_resp = NULL; break; }
                tmp_resp = tmp;
                memcpy(tmp_resp + tmp_total, rbuf, rr);
                tmp_total += rr;
                tmp_resp[tmp_total] = '\0';
                chunk++;
            }
            if (rr < 0) {
                ESP_LOGW(TAG, "esp_http_client_read returned %d", rr);
    #ifdef HTTP_SERVICES_LOG_TO_UI
                char ebuf[64]; snprintf(ebuf, sizeof(ebuf), "esp_http_client_read error: %d", rr); web_ui_add_log("WARN", TAG, ebuf);
    #endif
            }

            /* if we read nothing but the server reported a positive Content-Length, try a small read-retry loop
               (some servers / network conditions can cause a race where body arrives shortly after perform()) */
            if (tmp_total == 0 && resp_content_len > 0) {
                ESP_LOGW(TAG, "No bytes read but server reports content-length=%ld — doing additional read retries", resp_content_len);
                for (int rtry = 0; rtry < 3 && tmp_total == 0; ++rtry) {
                    vTaskDelay(pdMS_TO_TICKS(100 * (rtry + 1)));
                    ESP_LOGD(TAG, "Read-retry %d: attempting esp_http_client_read()", rtry + 1);
                    while ((rr = esp_http_client_read(client, rbuf, sizeof(rbuf))) > 0) {
                        ESP_LOGD(TAG, "esp_http_client_read(retry) chunk %d => %d bytes", chunk, rr);
                        char *tmp = realloc(tmp_resp, tmp_total + rr + 1);
                        if (!tmp) { free(tmp_resp); tmp_resp = NULL; break; }
                        tmp_resp = tmp;
                        memcpy(tmp_resp + tmp_total, rbuf, rr);
                        tmp_total += rr;
                        tmp_resp[tmp_total] = '\0';
                        chunk++;
                    }
                    if (rr < 0) {
                        ESP_LOGW(TAG, "esp_http_client_read (retry) returned %d", rr);
                        break;
                    }
                }
            }

            /* Diagnostic: mismatch between reported content-length and bytes actually read */
            if (resp_content_len >= 0 && (size_t)resp_content_len != tmp_total) {
                ESP_LOGW(TAG, "Content-Length mismatch: server reported=%ld but client read=%u bytes", resp_content_len, (unsigned)tmp_total);
    #ifdef HTTP_SERVICES_LOG_TO_UI
                char mismatch[128]; snprintf(mismatch, sizeof(mismatch), "Content-Length mismatch: server=%ld client=%u", resp_content_len, (unsigned)tmp_total); web_ui_add_log("WARN", TAG, mismatch);
    #endif
                /* retry with fallback headers if we haven't tried them yet */
                if (tmp_total == 0 && attempt + 1 < max_attempts && !use_fallback_headers) {
                    ESP_LOGW(TAG, "Retrying with fallback headers because server reported a body but none was read (attempt %d)", attempt + 2);
                    free(tmp_resp);
                    tmp_resp = NULL;
                    use_fallback_headers = true;
                    esp_http_client_cleanup(client);
                    client = NULL;
                    vTaskDelay(pdMS_TO_TICKS(150));
                    continue; /* next attempt will set Connection: close + Accept-Encoding: identity */
                }
            }

            /* accept the response we read */
            resp = tmp_resp;
            total = tmp_total;
        }

        /* otherwise break and use the data */
        break;
    }

    if (!client) {
        if (generated_auth_header) {
            free(generated_auth_header);
        }
        return ESP_ERR_NO_MEM;
    }

    /* LOG: response summary */
    ESP_LOGI(TAG, "<<< HTTP RESP: status=%d content_len=%u", status, (unsigned)total);
#ifdef HTTP_SERVICES_LOG_TO_UI
    char stbuf[64]; snprintf(stbuf, sizeof(stbuf), "RESP status=%d content_len=%u", status, (unsigned)total);
    web_ui_add_log("INFO", TAG, stbuf);
#endif
    if (resp && total > 0) {
        size_t MAX_SHOW = 2048;
        if (total <= MAX_SHOW) ESP_LOGI(TAG, "RESP Body: %s", resp);
        else {
            char *tmp = strndup(resp, MAX_SHOW);
            ESP_LOGI(TAG, "RESP Body (truncated %u/%u): %s ...", (unsigned)MAX_SHOW, (unsigned)total, tmp);
            free(tmp);
        }
#ifdef HTTP_SERVICES_LOG_TO_UI
        webui_log_chunked("INFO", TAG, "RESP Body", resp);
#endif
    } else {
        ESP_LOGI(TAG, "RESP Body: <empty>");
#ifdef HTTP_SERVICES_LOG_TO_UI
        web_ui_add_log("INFO", TAG, "RESP Body: <empty>");
#endif
    }

    esp_http_client_cleanup(client);

    /* If we got a partial body but the HTTP status is 2xx, treat as success (some servers reply chunked and close early). */
    if (err != ESP_OK && total > 0 && status >= 200 && status < 300) {
        ESP_LOGW(TAG, "Partial response tolerated: status=%d bytes=%u esp_err=%s", status, (unsigned)total, esp_err_to_name(err));
        err = ESP_OK;
    }

    /* report length to caller so caller can forward exact byte count (handles embedded NULs/binary) */
    if (out_len) *out_len = total;

    if (err == ESP_OK && status >= 200 && status < 300) {
        http_services_set_online(true, "http_ok");
    } else {
        http_services_set_online(false, "http_error");
        if (status == 401 || status == 403) {
            http_services_clear_token("http_unauthorized");
        }
    }

    if (out_resp) *out_resp = resp; else if (resp) free(resp);
    if (out_status) *out_status = status;
    if (generated_auth_header) {
        free(generated_auth_header);
    }
    return err;
#endif
}

/**
 * @brief Invia una richiesta HTTPD remota disabilitata.
 *
 * @param req Puntatore alla richiesta HTTPD.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t send_remote_disabled_httpd(httpd_req_t *req)
{
    const char *resp = "{\"iserror\":true,\"codeerror\":503,\"deserror\":\"remote_disabled\"}";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, strlen(resp));
}

/* Helper: forward incoming POST to remote_path (uses request body or config credentials for login) */
static esp_err_t forward_post(httpd_req_t *req, const char *remote_path, const char *override_body, bool override_with_config_credentials)
{
    if (!http_services_is_remote_enabled()) {
        ESP_LOGW(TAG,
                 "[C] Richiesta locale bloccata: server remoto disabilitato (path=%s)",
                 remote_path ? remote_path : "(null)");
        return send_remote_disabled_httpd(req);
    }

    char *incoming_body = NULL;
    
    if (!override_body) incoming_body = read_request_body(req);
    const char *body_to_send = override_body ? override_body : (incoming_body ? incoming_body : "{}");

    /* Debug: log all input parameters for this forward_post call */
    ESP_LOGI(TAG, "forward_post START: remote_path=%s override_with_config_credentials=%d override_body_present=%d req_uri=%s req_content_len=%d",
             remote_path ? remote_path : "(null)",
             override_with_config_credentials ? 1 : 0,
             override_body ? 1 : 0,
             req && req->uri ? req->uri : "(null)",
             (int)(req ? req->content_len : 0));
    ESP_LOGI(TAG, "forward_post: body_to_send_len=%u", (unsigned)(body_to_send ? strlen(body_to_send) : 0));
    if (body_to_send && body_to_send[0] != '\0') {
        const size_t SAMPLE = 128;
        char sample[SAMPLE + 1];
        size_t n = strlen(body_to_send) < SAMPLE ? strlen(body_to_send) : SAMPLE;
        memcpy(sample, body_to_send, n);
        sample[n] = '\0';
        ESP_LOGD(TAG, "forward_post: body_to_send_sample: %s", sample);
#ifdef HTTP_SERVICES_LOG_TO_UI
        webui_log_chunked("DEBUG", TAG, "IN body_to_send (sample)", sample);
#endif
    }

    /* LOG incoming request */
    log_httpd_request(req, incoming_body);
    ESP_LOGI(TAG, "Forwarding to remote path: %s", remote_path);

    bool force_refresh_login = (remote_path != NULL) && (strcmp(remote_path, "/api/login") != 0);
    if (force_refresh_login) {
        esp_err_t login_err = http_services_login_if_needed(true);
        if (login_err != ESP_OK) {
            if (incoming_body) free(incoming_body);
            ESP_LOGE(TAG, "[C] Login forzato pre-chiamata fallito per %s: %s",
                     remote_path,
                     esp_err_to_name(login_err));
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req,
                                   "{\"iserror\":true,\"codeerror\":401,\"deserror\":\"login_refresh_failed\"}",
                                   -1);
        }
    }

    /* try to forward Authorization header from incoming request */
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    char *auth_hdr = NULL;
    if (auth_len > 0) {
        auth_hdr = malloc(auth_len + 1);
        if (auth_hdr) httpd_req_get_hdr_value_str(req, "Authorization", auth_hdr, auth_len + 1);
    }

    /* Debug: show whether Authorization header was provided */
    ESP_LOGI(TAG, "forward_post: Authorization header length=%d", (int)auth_len);
    if (auth_hdr && auth_hdr[0] != '\0') {
        ESP_LOGD(TAG, "forward_post: Authorization header value: %s", auth_hdr);
#ifdef HTTP_SERVICES_LOG_TO_UI
        webui_log_chunked("DEBUG", TAG, "IN Authorization", auth_hdr);
#endif
    }

    /* special case: if override_with_config_credentials == true, build login body from config */
    char login_body[256];
    const char *send_body = body_to_send;
    if (override_with_config_credentials) {
        device_config_t *cfg = device_config_get();
        snprintf(login_body, sizeof(login_body), "{\"serial\":\"%s\",\"password\":\"%s\"}", cfg->server.serial, cfg->server.password);
        send_body = login_body;
        ESP_LOGI(TAG, "Using config credentials for login (serial=%s)", cfg->server.serial);
        ESP_LOGD(TAG, "Login body: %s", send_body);
#ifdef HTTP_SERVICES_LOG_TO_UI
        webui_log_chunked("INFO", TAG, "Login body", send_body);
#endif
    }

    char *remote_resp = NULL;
    int status = 0;
    size_t remote_len = 0;
    const char *auth_to_send = auth_hdr;
    if (force_refresh_login) {
        auth_to_send = NULL;
        if (auth_hdr && auth_hdr[0] != '\0') {
            ESP_LOGI(TAG, "[C] Authorization inbound ignorata: uso token aggiornato da /api/login");
        }
    }

    esp_err_t err = remote_post(remote_path, send_body, auth_to_send, &remote_resp, &status, &remote_len);

    if (auth_hdr) free(auth_hdr);
    if (incoming_body) free(incoming_body);

    if (err != ESP_OK) {
        const char *err_resp = "{\"iserror\":true,\"codeerror\":502,\"deserror\":\"remote_error\"}";
        httpd_resp_set_type(req, "application/json");
        if (remote_resp) {
            ESP_LOGE(TAG, "Forward to %s failed: %s — remote partial response: %s", remote_path, esp_err_to_name(err), remote_resp);
            free(remote_resp);
        } else {
            ESP_LOGE(TAG, "Forward to %s failed: %s — no remote response body", remote_path, esp_err_to_name(err));
        }
        return httpd_resp_send(req, err_resp, strlen(err_resp));
    }

    ESP_LOGI(TAG, "Forward to %s returned HTTP %d (bytes_read=%u)", remote_path, status, (unsigned)remote_len);
    if (remote_resp) ESP_LOGI(TAG, "forward_post: strlen(remote_resp)=%u", (unsigned)strlen(remote_resp));
    if (remote_len != 0 && remote_resp && remote_len != strlen(remote_resp)) {
        ESP_LOGW(TAG, "forward_post: byte-count != strlen(remote_resp) — forwarding exact byte count to client");
    }

    /* on login, store access_token locally if provided */
    if (strcmp(remote_path, "/api/login") == 0 && remote_resp) {
        cJSON *rj = cJSON_Parse(remote_resp);
        if (rj) {
            cJSON *at = cJSON_GetObjectItemCaseSensitive(rj, "access_token");
            if (cJSON_IsString(at) && at->valuestring) {
                esp_err_t token_store_err = store_auth_token(at->valuestring, "forward_post/login");
                if (token_store_err != ESP_OK) {
                    ESP_LOGE(TAG, "[C] Salvataggio access_token fallito: %s", esp_err_to_name(token_store_err));
                }
#ifdef HTTP_SERVICES_LOG_TO_UI
                if (token_store_err == ESP_OK) {
                    webui_log_chunked("INFO", TAG, "Stored access_token", g_auth_token);
                }
#endif
            }
            cJSON_Delete(rj);
        }
    }

    httpd_resp_set_type(req, "application/json");
    if (remote_resp) {
        size_t send_len = (remote_len > 0) ? remote_len : strlen(remote_resp);
        httpd_resp_send(req, remote_resp, send_len);
        free(remote_resp);
    } else {
        httpd_resp_send(req, "{}", 2);
    }
    return ESP_OK;
}

/**
 * @brief Gestisce la richiesta POST per l'autenticazione.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_login_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/login -> proxy to remote server");
    return forward_post(req, "/api/login", NULL, true);
}

/**
 * @brief Gestisce la richiesta POST per mantenere attiva la connessione.
 *
 * @param req Puntatore alla richiesta HTTPD.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_keepalive_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/keepalive -> proxy to remote server");
    return forward_post(req, "/api/keepalive", NULL, false);
}

/**
 * @brief Gestisce la richiesta POST per ottenere immagini.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_getimages_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/getimages -> proxy to remote server");
    esp_err_t ret = forward_post(req, "/api/getimages", NULL, false);
    if (ret == ESP_OK) {
        http_services_sync_ftp_for_endpoint("/api/getimages", "/spiffs");
    }
    return ret;
}

/**
 * @brief Gestisce la richiesta POST per ottenere la configurazione.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_getconfig_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/getconfig -> proxy to remote server");
    esp_err_t ret = forward_post(req, "/api/getconfig", NULL, false);
    if (ret == ESP_OK) {
        http_services_sync_ftp_for_endpoint("/api/getconfig", "/spiffs/config");
    }
    return ret;
}

/**
 * @brief Gestisce la richiesta POST per ottenere le traduzioni.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_gettranslations_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/gettranslations -> proxy to remote server");
    esp_err_t ret = forward_post(req, "/api/gettranslations", NULL, false);
    if (ret == ESP_OK) {
        http_services_sync_ftp_for_endpoint("/api/gettranslations", "/spiffs");
    }
    return ret;
}

/**
 * @brief Gestisce la richiesta POST per ottenere il firmware.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_getfirmware_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/getfirmware -> proxy to remote server");
    esp_err_t ret = forward_post(req, "/api/getfirmware", NULL, false);
    if (ret == ESP_OK) {
        http_services_sync_ftp_for_endpoint("/api/getfirmware", "/spiffs/firmware");
    }
    return ret;
}

/**
 * @brief Gestisce la richiesta POST per l'API di pagamento.
 *
 * @param req Puntatore alla richiesta HTTPD.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_payment_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/payment -> proxy to remote server");
    return forward_post(req, "/api/payment", NULL, false);
}

/**
 * @brief Gestisce la richiesta POST per l'API serviceused.
 *
 * @param req Puntatore alla richiesta HTTPD.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_serviceused_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/serviceused -> proxy to remote server");
    return forward_post(req, "/api/serviceused", NULL, false);
}

/**
 * @brief Gestisce la richiesta POST per il pagamento offline.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_paymentoffline_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/paymentoffline -> proxy to remote server");
    return forward_post(req, "/api/paymentoffline", NULL, false);
}

/**
 * @brief Verifica se è necessario eseguire il login HTTP e, in caso, esegue l'operazione.
 *
 * @param force_refresh Flag booleano che indica se il login deve essere forzato anche se non è necessario.
 * @return esp_err_t Codice di errore che indica il risultato dell'operazione.
 */
static esp_err_t http_services_login_if_needed(bool force_refresh)
{
    device_config_t *cfg = device_config_get();
    if (!http_services_cfg_remote_enabled(cfg)) {
        http_services_clear_token("login_disabled");
        http_services_set_online(false, "login_disabled");
        ESP_LOGW(TAG, "[C] Login remoto bloccato: server disabilitato o URL mancante");
        return ESP_ERR_INVALID_STATE;
    }

    if (!force_refresh && validate_token(g_auth_token)) {
        http_services_set_online(true, "token_valid");
        return ESP_OK;
    }

    if (!cfg || cfg->server.serial[0] == '\0' || cfg->server.password[0] == '\0') {
        http_services_set_online(false, "login_missing_credentials");
        ESP_LOGE(TAG, "[C] Login non possibile: credenziali server non configurate");
        return ESP_ERR_INVALID_STATE;
    }

    char login_body[256];
    snprintf(login_body,
             sizeof(login_body),
             "{\"serial\":\"%s\",\"password\":\"%s\"}",
             cfg->server.serial,
             cfg->server.password);

    char *resp = NULL;
    int status = 0;
    size_t resp_len = 0;
    esp_err_t err = remote_post("/api/login", login_body, NULL, &resp, &status, &resp_len);
    if (err != ESP_OK) {
        http_services_set_online(false, "login_http_error");
        ESP_LOGE(TAG, "[C] Login remoto fallito: %s", esp_err_to_name(err));
        if (resp) free(resp);
        return err;
    }

    if (status < 200 || status >= 300 || !resp) {
        http_services_set_online(false, "login_http_status");
        ESP_LOGE(TAG, "[C] Login remoto HTTP non valido: status=%d", status);
        if (resp) free(resp);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) {
        http_services_set_online(false, "login_json_invalid");
        ESP_LOGE(TAG, "[C] Login remoto: risposta JSON non valida");
        return ESP_FAIL;
    }

    cJSON *at = cJSON_GetObjectItemCaseSensitive(root, "access_token");
    if (!cJSON_IsString(at) || !at->valuestring || at->valuestring[0] == '\0') {
        cJSON_Delete(root);
        http_services_set_online(false, "login_token_missing");
        ESP_LOGE(TAG, "[C] Login remoto: access_token mancante");
        return ESP_FAIL;
    }

    esp_err_t store_err = store_auth_token(at->valuestring, "login_if_needed");
    cJSON_Delete(root);
    if (store_err != ESP_OK) {
        http_services_set_online(false, "login_token_store_failed");
        return store_err;
    }

    http_services_set_online(true, "login_ok");
    ESP_LOGI(TAG, "[C] Token remoto acquisito (len=%u)", (unsigned)strlen(g_auth_token));
    return ESP_OK;
}

/* =========================================================================
 * Direct C call: getcustomers
 * Builds request body, calls remote_post(), parses response.
 * ========================================================================= */
esp_err_t http_services_getcustomers(const char *code, const char *telephone,
                                     http_services_getcustomers_response_t *out)
{
    if (!code || !out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    esp_err_t token_err = http_services_login_if_needed(true);
    if (token_err != ESP_OK) {
        out->common.iserror = true;
        out->common.codeerror = -10;
        snprintf(out->common.deserror,
                 sizeof(out->common.deserror),
                 "login_failed:%s",
                 esp_err_to_name(token_err));
        return token_err;
    }

    /* --- build request body -------------------------------------------- */
    cJSON *req_json = cJSON_CreateObject();
    if (!req_json) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(req_json, "Code", code);
    // cJSON_AddStringToObject(req_json, "Telephone", telephone ? telephone : "");
    char *body = cJSON_PrintUnformatted(req_json);
    cJSON_Delete(req_json);
    if (!body) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "getcustomers: body=%s", body);

    /* --- call remote server -------------------------------------------- */
    char *resp = NULL;
    int status = 0;
    size_t resp_len = 0;
    esp_err_t err = ESP_FAIL;
    for (int attempt = 0; attempt < 2; ++attempt) {
        err = remote_post("/api/getcustomers", body, NULL, &resp, &status, &resp_len);
        if (err != ESP_OK) {
            break;
        }

        if ((status == 401 || status == 403) && attempt == 0) {
            ESP_LOGW(TAG, "[C] getcustomers non autorizzato (HTTP %d), rinnovo token", status);
            g_auth_token[0] = '\0';
            if (resp) {
                free(resp);
                resp = NULL;
            }
            token_err = http_services_login_if_needed(true);
            if (token_err != ESP_OK) {
                err = token_err;
                break;
            }
            continue;
        }

        break;
    }
    free(body);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "getcustomers: remote_post failed: %s", esp_err_to_name(err));
        out->common.iserror = true;
        out->common.codeerror = -1;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "%s", esp_err_to_name(err));
        if (resp) free(resp);
        return err;
    }
    if (status < 200 || status >= 300) {
        out->common.iserror = true;
        out->common.codeerror = status;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "http_%d", status);
        if (resp) free(resp);
        return ESP_FAIL;
    }

    if (!resp) {
        out->common.iserror = true;
        out->common.codeerror = -2;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "no_response");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "getcustomers: HTTP %d, resp_len=%u", status, (unsigned)resp_len);

    /* --- parse response ------------------------------------------------- */
    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) {
        out->common.iserror = true;
        out->common.codeerror = -3;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "json_parse_error");
        return ESP_FAIL;
    }

    /* common error fields */
    cJSON *je = cJSON_GetObjectItemCaseSensitive(root, "iserror");
    out->common.iserror = cJSON_IsTrue(je);
    cJSON *jce = cJSON_GetObjectItemCaseSensitive(root, "codeerror");
    if (cJSON_IsNumber(jce)) out->common.codeerror = (int32_t)jce->valueint;
    cJSON *jde = cJSON_GetObjectItemCaseSensitive(root, "deserror");
    if (cJSON_IsString(jde) && jde->valuestring)
        snprintf(out->common.deserror, sizeof(out->common.deserror), "%s", jde->valuestring);

    if (out->common.iserror) {
        ESP_LOGW(TAG, "getcustomers: server error %d: %s", out->common.codeerror, out->common.deserror);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    /* customers array */
    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "customers");
    if (cJSON_IsArray(arr)) {
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (out->customer_count >= HTTP_SERVICES_MAX_FILES) break;
            http_services_customer_t *c = &out->customers[out->customer_count];

            cJSON *jv = cJSON_GetObjectItemCaseSensitive(item, "valid");
            c->valid = cJSON_IsTrue(jv);

            cJSON *jcode = cJSON_GetObjectItemCaseSensitive(item, "code");
            if (cJSON_IsString(jcode) && jcode->valuestring)
                snprintf(c->code, sizeof(c->code), "%s", jcode->valuestring);

            cJSON *jtel = cJSON_GetObjectItemCaseSensitive(item, "telephone");
            if (cJSON_IsString(jtel) && jtel->valuestring)
                snprintf(c->telephone, sizeof(c->telephone), "%s", jtel->valuestring);

            cJSON *jemail = cJSON_GetObjectItemCaseSensitive(item, "email");
            if (cJSON_IsString(jemail) && jemail->valuestring)
                snprintf(c->email, sizeof(c->email), "%s", jemail->valuestring);

            cJSON *jname = cJSON_GetObjectItemCaseSensitive(item, "name");
            if (cJSON_IsString(jname) && jname->valuestring)
                snprintf(c->name, sizeof(c->name), "%s", jname->valuestring);

            cJSON *jsur = cJSON_GetObjectItemCaseSensitive(item, "surname");
            if (cJSON_IsString(jsur) && jsur->valuestring)
                snprintf(c->surname, sizeof(c->surname), "%s", jsur->valuestring);

            cJSON *jamt = cJSON_GetObjectItemCaseSensitive(item, "amount");
            if (cJSON_IsNumber(jamt)) c->amount = (int32_t)jamt->valueint;

            cJSON *jnew = cJSON_GetObjectItemCaseSensitive(item, "new");
            c->is_new = cJSON_IsTrue(jnew);

            out->customer_count++;
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "getcustomers: parsed %u customer(s)", (unsigned)out->customer_count);
    return ESP_OK;
}

/* =========================================================================
 * Direct C call: payment
 * Builds request body, calls remote_post(), parses response.
 * ========================================================================= */
esp_err_t http_services_payment(const http_services_customer_t *customer,
                                int32_t amount,
                                const char *service_code,
                                http_services_payment_response_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    if (amount < 0) {
        amount = 0;
    }

    const char *service = (service_code && service_code[0] != '\0') ? service_code : "SER1";

    esp_err_t token_err = http_services_login_if_needed(true);
    if (token_err != ESP_OK) {
        out->common.iserror = true;
        out->common.codeerror = -10;
        snprintf(out->common.deserror,
                 sizeof(out->common.deserror),
                 "login_failed:%s",
                 esp_err_to_name(token_err));
        return token_err;
    }

    cJSON *req = cJSON_CreateObject();
    if (!req) {
        return ESP_ERR_NO_MEM;
    }

    cJSON *cust = cJSON_CreateObject();
    if (!cust) {
        cJSON_Delete(req);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(cust, "code", (customer && customer->code[0] != '\0') ? customer->code : "");
    cJSON_AddStringToObject(cust, "telephone", (customer && customer->telephone[0] != '\0') ? customer->telephone : "");
    if (customer && customer->email[0] != '\0') {
        cJSON_AddStringToObject(cust, "email", customer->email);
    } else {
        cJSON_AddNullToObject(cust, "email");
    }
    cJSON_AddStringToObject(cust, "surname", (customer && customer->surname[0] != '\0') ? customer->surname : "");
    cJSON_AddStringToObject(cust, "name", (customer && customer->name[0] != '\0') ? customer->name : "");
    cJSON_AddItemToObject(req, "customer", cust);

    char datetime_iso[40] = {0};
    format_full_datetime(datetime_iso, sizeof(datetime_iso));
    cJSON_AddStringToObject(req, "datetime", datetime_iso);
    cJSON_AddNumberToObject(req, "amount", amount);
    cJSON_AddStringToObject(req, "paymenttype", "CASH");
    cJSON_AddStringToObject(req, "paymentdata", "");

    cJSON *cashreturned = cJSON_CreateArray();
    cJSON_AddItemToObject(req, "cashreturned", cashreturned);

    cJSON *cashentered = cJSON_CreateArray();
    if (!cashentered) {
        cJSON_Delete(req);
        return ESP_ERR_NO_MEM;
    }
    if (amount > 0) {
        cJSON *ce = cJSON_CreateObject();
        if (!ce) {
            cJSON_Delete(req);
            return ESP_ERR_NO_MEM;
        }
        cJSON_AddNumberToObject(ce, "value", amount);
        cJSON_AddNumberToObject(ce, "quantity", 1);
        cJSON_AddNumberToObject(ce, "position", 99);
        cJSON_AddItemToArray(cashentered, ce);
    }
    cJSON_AddItemToObject(req, "cashentered", cashentered);

    cJSON *services = cJSON_CreateArray();
    if (!services) {
        cJSON_Delete(req);
        return ESP_ERR_NO_MEM;
    }
    cJSON *srv = cJSON_CreateObject();
    if (!srv) {
        cJSON_Delete(req);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(srv, "code", service);
    cJSON_AddNumberToObject(srv, "amount", amount);
    cJSON_AddNumberToObject(srv, "quantity", 1);
    cJSON_AddBoolToObject(srv, "used", true);
    cJSON_AddBoolToObject(srv, "recharge", false);
    cJSON_AddItemToArray(services, srv);
    cJSON_AddItemToObject(req, "services", services);

    char *body = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    if (!body) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "payment: body=%s", body);

    char *resp = NULL;
    int status = 0;
    size_t resp_len = 0;
    esp_err_t err = remote_post("/api/payment", body, NULL, &resp, &status, &resp_len);
    free(body);

    if (err != ESP_OK) {
        out->common.iserror = true;
        out->common.codeerror = -1;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "%s", esp_err_to_name(err));
        if (resp) free(resp);
        return err;
    }

    if (status < 200 || status >= 300) {
        out->common.iserror = true;
        out->common.codeerror = status;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "http_%d", status);
        if (resp) free(resp);
        return ESP_FAIL;
    }

    if (!resp) {
        out->common.iserror = true;
        out->common.codeerror = -2;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "no_response");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(resp);
    free(resp);
    if (!root) {
        out->common.iserror = true;
        out->common.codeerror = -3;
        snprintf(out->common.deserror, sizeof(out->common.deserror), "json_parse_error");
        return ESP_FAIL;
    }

    cJSON *je = cJSON_GetObjectItemCaseSensitive(root, "iserror");
    out->common.iserror = cJSON_IsTrue(je);
    cJSON *jce = cJSON_GetObjectItemCaseSensitive(root, "codeerror");
    if (cJSON_IsNumber(jce)) out->common.codeerror = (int32_t)jce->valueint;
    cJSON *jde = cJSON_GetObjectItemCaseSensitive(root, "deserror");
    if (cJSON_IsString(jde) && jde->valuestring) {
        snprintf(out->common.deserror, sizeof(out->common.deserror), "%s", jde->valuestring);
    }

    cJSON *jdt = cJSON_GetObjectItemCaseSensitive(root, "datetime");
    if (cJSON_IsString(jdt) && jdt->valuestring) {
        snprintf(out->datetime, sizeof(out->datetime), "%s", jdt->valuestring);
    }

    cJSON *jpid = cJSON_GetObjectItemCaseSensitive(root, "paymentid");
    if (cJSON_IsNumber(jpid)) {
        out->paymentid = (int32_t)jpid->valueint;
    }

    cJSON_Delete(root);
    return out->common.iserror ? ESP_FAIL : ESP_OK;
}


/**
 * @brief Gestisce la richiesta POST per ottenere i clienti.
 *
 * @param req Puntatore alla richiesta HTTPD.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_getcustomers_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/getcustomers -> proxy to remote server");
    return forward_post(req, "/api/getcustomers", NULL, false);
}


/**
 * @brief Gestisce la richiesta POST per ottenere gli operatori.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_getoperators_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/getoperators -> proxy to remote server");
    return forward_post(req, "/api/getoperators", NULL, false);
}


/**
 * @brief Gestisce la richiesta POST per l'attività.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t api_activity_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/activity -> proxy to remote server");
    return forward_post(req, "/api/activity", NULL, false);
}


/** @brief Gestisce la richiesta POST per l'attività del dispositivo.
 *  
 *  @param [in] req Puntatore alla richiesta HTTP.
 *  
 *  @return esp_err_t Codice di errore.
 */
static esp_err_t api_deviceactivity_post(httpd_req_t *req)
{
    if (send_http_log) ESP_LOGI(TAG, "POST /api/deviceactivity -> proxy to remote server");
    return forward_post(req, "/api/deviceactivity", NULL, false);
}



/**
 * @brief Registra i gestori HTTP per il server.
 *
 * Questa funzione registra i gestori HTTP necessari per il server.
 *
 * @param [in] server Handle del server HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t http_services_register_handlers(httpd_handle_t server)
{
    httpd_uri_t uri_login = {
        .uri = "/api/login",
        .method = HTTP_POST,
        .handler = api_login_post,
        .user_ctx = NULL
    };
    
    httpd_uri_t uri_keepalive = {
        .uri = "/api/keepalive",
        .method = HTTP_POST,
        .handler = api_keepalive_post,
        .user_ctx = NULL
    };

    httpd_uri_t uri_getimages = { .uri = "/api/getimages", .method = HTTP_POST, .handler = api_getimages_post, .user_ctx = NULL };
    httpd_uri_t uri_getconfig = { .uri = "/api/getconfig", .method = HTTP_POST, .handler = api_getconfig_post, .user_ctx = NULL };
    httpd_uri_t uri_gettranslations = { .uri = "/api/gettranslations", .method = HTTP_POST, .handler = api_gettranslations_post, .user_ctx = NULL };
    httpd_uri_t uri_getfirmware = { .uri = "/api/getfirmware", .method = HTTP_POST, .handler = api_getfirmware_post, .user_ctx = NULL };

    httpd_uri_t uri_payment = { .uri = "/api/payment", .method = HTTP_POST, .handler = api_payment_post, .user_ctx = NULL };
    httpd_uri_t uri_serviceused = { .uri = "/api/serviceused", .method = HTTP_POST, .handler = api_serviceused_post, .user_ctx = NULL };
    httpd_uri_t uri_paymentoffline = { .uri = "/api/paymentoffline", .method = HTTP_POST, .handler = api_paymentoffline_post, .user_ctx = NULL };

    httpd_uri_t uri_getcustomers = { .uri = "/api/getcustomers", .method = HTTP_POST, .handler = api_getcustomers_post, .user_ctx = NULL };
    httpd_uri_t uri_getoperators = { .uri = "/api/getoperators", .method = HTTP_POST, .handler = api_getoperators_post, .user_ctx = NULL };
    httpd_uri_t uri_activity = { .uri = "/api/activity", .method = HTTP_POST, .handler = api_activity_post, .user_ctx = NULL };
    httpd_uri_t uri_deviceactivity = { .uri = "/api/deviceactivity", .method = HTTP_POST, .handler = api_deviceactivity_post, .user_ctx = NULL };

    httpd_register_uri_handler(server, &uri_login);
    httpd_register_uri_handler(server, &uri_keepalive);

    httpd_register_uri_handler(server, &uri_getimages);
    httpd_register_uri_handler(server, &uri_getconfig);
    httpd_register_uri_handler(server, &uri_gettranslations);
    httpd_register_uri_handler(server, &uri_getfirmware);

    httpd_register_uri_handler(server, &uri_payment);
    httpd_register_uri_handler(server, &uri_serviceused);
    httpd_register_uri_handler(server, &uri_paymentoffline);

    httpd_register_uri_handler(server, &uri_getcustomers);
    httpd_register_uri_handler(server, &uri_getoperators);
    httpd_register_uri_handler(server, &uri_activity);
    httpd_register_uri_handler(server, &uri_deviceactivity);

    ESP_LOGI(TAG, "Registered POST /api/* handlers (http_services)");
    return ESP_OK;
}
