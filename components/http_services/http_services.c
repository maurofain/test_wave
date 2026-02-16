#include "http_services.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "cJSON.h"
#include "device_config.h"
#include "esp_http_client.h"
#define HTTP_SERVICES_LOG_TO_UI

/* web_ui_add_log is used only when HTTP_SERVICES_LOG_TO_UI is enabled; declare here to
   avoid adding a component dependency (web_ui already depends on http_services). */
#ifdef HTTP_SERVICES_LOG_TO_UI
extern void web_ui_add_log(const char *level, const char *tag, const char *message);
#endif

static const char *TAG = "HTTP_SERVICES";

// Global variable to store the token
char g_auth_token[256] = {0};

/* Helper: compute MD5 hex (lowercase) of input */
static void md5_hex(const unsigned char *input, size_t ilen, char *out_hex, size_t out_hex_len)
{
    // Implementation of md5_hex function
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

/* Format date used by services_http.md: MMYYYYdd (month 2 digits, year 4 digits, day 2 digits) */
static void format_date_mm_yyyy_dd(char *out, size_t len)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    snprintf(out, len, "%02d%04d%02d", tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_mday);
}

/* Format full date-time string in ISO format */
static void format_full_datetime(char *out, size_t len)
{
    time_t now = time(NULL);
    struct tm tm;
    gmtime_r(&now, &tm); /* use UTC for the "Z" suffix */
    strftime(out, len, "%Y-%m-%dT%H:%M:%S.000Z", &tm);
}

/* Format ISO8601 with microseconds and local timezone offset.
   Example: 2026-01-23T13:25:13.218763+01:00
   Falls back to UTC Z format on error. */
static void format_iso8601_local_us_tz(char *out, size_t len)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        /* fallback to UTC Z format */
        format_full_datetime(out, len);
        return;
    }

    time_t sec = tv.tv_sec;
    struct tm local_tm;
    if (!localtime_r(&sec, &local_tm)) {
        format_full_datetime(out, len);
        return;
    }

    /* compute offset between local time and UTC robustly using tm fields */
    struct tm utc_tm;
    gmtime_r(&sec, &utc_tm);

    /* day difference (handles day/year roll-over) */
    int year_diff = local_tm.tm_year - utc_tm.tm_year;
    long day_diff = year_diff * 365L + (local_tm.tm_yday - utc_tm.tm_yday);

    long offset_seconds = day_diff * 86400L
                        + (local_tm.tm_hour - utc_tm.tm_hour) * 3600L
                        + (local_tm.tm_min  - utc_tm.tm_min)  * 60L
                        + (local_tm.tm_sec  - utc_tm.tm_sec);

    char sign = '+';
    if (offset_seconds < 0) { sign = '-'; offset_seconds = -offset_seconds; }
    int off_h = (int)(offset_seconds / 3600);
    int off_m = (int)((offset_seconds % 3600) / 60);
    int usec = (int)tv.tv_usec;

    snprintf(out, len, "%04d-%02d-%02dT%02d:%02d:%02d.%06d%c%02d:%02d",
             local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday,
             local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec, usec,
             sign, off_h, off_m);
}

/*
 * Optional: forward verbose HTTP logs into the web UI `/logs` buffer.
 * Controlled at compile-time with `-DHTTP_SERVICES_LOG_TO_UI`.
 * When enabled, long messages are chunked to fit the `web_ui` store.
 */
#ifdef HTTP_SERVICES_LOG_TO_UI
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
        if (blen <= MAX_LOG) {
            ESP_LOGI(TAG, "Body: %s", body);
#ifdef HTTP_SERVICES_LOG_TO_UI
            web_ui_add_log("INFO", TAG, body);
#endif
        } else {
            char *tmp = strndup(body, MAX_LOG);
            ESP_LOGI(TAG, "Body (truncated %u/%u): %s ...", (unsigned)MAX_LOG, (unsigned)blen, tmp);
#ifdef HTTP_SERVICES_LOG_TO_UI
            web_ui_add_log("INFO", TAG, tmp);
            web_ui_add_log("INFO", TAG, "<body truncated in UI log>");
#endif
            free(tmp);
        }
    } else {
        ESP_LOGI(TAG, "Body: <empty>");
#ifdef HTTP_SERVICES_LOG_TO_UI
        web_ui_add_log("INFO", TAG, "<empty body>");
#endif
    }
}




/* --- Proxy implementations forwarding to remote server (uses device_config.server.url + credentials) --- */

/* Helper: perform HTTP POST to remote server (returns dynamically allocated response in out_resp) */
static esp_err_t remote_post(const char *remote_path, const char *body, const char *auth_header, char **out_resp, int *out_status)
{
    if (!remote_path) return ESP_ERR_INVALID_ARG;
    device_config_t *cfg = device_config_get();
    if (!cfg || strlen(cfg->server.url) == 0) return ESP_ERR_INVALID_STATE;

    /* build URL (avoid double slashes) */
    const char *base = cfg->server.url;
    char url[384];
    size_t bl = strlen(base);
    if (bl > 0 && base[bl-1] == '/' && remote_path[0] == '/') snprintf(url, sizeof(url), "%s%s", base, remote_path + 1);
    else if (bl > 0 && base[bl-1] == '/' ) snprintf(url, sizeof(url), "%s%s", base, remote_path);
    else if (remote_path[0] == '/') snprintf(url, sizeof(url), "%s%s", base, remote_path);
    else snprintf(url, sizeof(url), "%s/%s", base, remote_path);

    esp_http_client_config_t cfg_http = {
        .url = url,
        .timeout_ms = 15000,
        .skip_cert_common_name_check = true
    };

    /* perform up to two attempts when we see incomplete/chunked errors */
    esp_err_t err = ESP_FAIL;
    int status = 0;
    const int max_attempts = 2;
    esp_http_client_handle_t client = NULL;
    /* diagnostic: server-reported content-length (set after perform) */
    long resp_content_len = -1;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        if (client) {
            esp_http_client_cleanup(client);
            client = NULL;
            vTaskDelay(pdMS_TO_TICKS(150));
        }

        client = esp_http_client_init(&cfg_http);
        if (!client) { err = ESP_ERR_NO_MEM; break; }

        esp_http_client_set_method(client, HTTP_METHOD_POST);

        /* only required headers for the remote server: Content-Type and Date */
        const char hdr_content_type[40] = "application/json";

        esp_http_client_set_header(client, "Content-Type", hdr_content_type);

        char date_hdr[64] = "2026-01-23T13:25:13.218763+01:00";
        //format_iso8601_local_us_tz(date_hdr, sizeof(date_hdr));
        esp_http_client_set_header(client, "Date", date_hdr);
                                                    
        char abuf[320] = {0};
        const char *hdr_authorization = NULL;
        if (auth_header && strlen(auth_header) > 0) {
            hdr_authorization = auth_header;
        } else if (g_auth_token[0] != '\0') {
            snprintf(abuf, sizeof(abuf), "Bearer %s", g_auth_token);
            hdr_authorization = abuf;
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
        if (hdr_authorization) ESP_LOGI(TAG, "OUT Header: Authorization: %s", hdr_authorization);
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

        /* perform */
        err = esp_http_client_perform(client);
        status = esp_http_client_get_status_code(client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_http_client_perform returned %s (attempt %d)", esp_err_to_name(err), attempt + 1);
        }

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

        /* If we got an ESP_ERR_HTTP_INCOMPLETE_DATA and we still have retry attempts, try again */
        if (err == ESP_ERR_HTTP_INCOMPLETE_DATA && attempt + 1 < max_attempts) {
            ESP_LOGW(TAG, "Incomplete chunked data received — retrying (attempt %d)", attempt + 2);
            continue;
        }

        /* otherwise break and read whatever is available (may be partial) */
        break;
    }

    if (!client) return ESP_ERR_NO_MEM;

    /* read response (streaming, unknown length) */
    char *resp = malloc(1);
    if (!resp) { esp_http_client_cleanup(client); return ESP_ERR_NO_MEM; }
    resp[0] = '\0';
    size_t total = 0;
    char buf[512];
    int r = 0;
    int chunk = 0;
    while ((r = esp_http_client_read(client, buf, sizeof(buf))) > 0) {
        ESP_LOGD(TAG, "esp_http_client_read chunk %d => %d bytes", chunk, r);
        /* log a short sample of the first chunk to help debugging */
        if (chunk == 0) {
            int sample_n = r < 128 ? r : 128;
            char sample[129];
            memcpy(sample, buf, sample_n);
            sample[sample_n] = '\0';
            ESP_LOGD(TAG, "First RESP chunk sample: %s", sample);
#ifdef HTTP_SERVICES_LOG_TO_UI
            webui_log_chunked("DEBUG", TAG, "First RESP chunk", sample);
#endif
        }
        char *tmp = realloc(resp, total + r + 1);
        if (!tmp) { free(resp); resp = NULL; break; }
        resp = tmp;
        memcpy(resp + total, buf, r);
        total += r;
        resp[total] = '\0';
        chunk++;
    }
    if (r < 0) {
        ESP_LOGW(TAG, "esp_http_client_read returned %d", r);
#ifdef HTTP_SERVICES_LOG_TO_UI
        char ebuf[64]; snprintf(ebuf, sizeof(ebuf), "esp_http_client_read error: %d", r); web_ui_add_log("WARN", TAG, ebuf);
#endif
    }

    /* Diagnostic: mismatch between reported content-length and bytes actually read */
    if (resp_content_len >= 0 && (size_t)resp_content_len != total) {
        ESP_LOGW(TAG, "Content-Length mismatch: server reported=%ld but client read=%u bytes", resp_content_len, (unsigned)total);
#ifdef HTTP_SERVICES_LOG_TO_UI
        char mismatch[128]; snprintf(mismatch, sizeof(mismatch), "Content-Length mismatch: server=%ld client=%u", resp_content_len, (unsigned)total); web_ui_add_log("WARN", TAG, mismatch);
#endif
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

    if (out_resp) *out_resp = resp; else if (resp) free(resp);
    if (out_status) *out_status = status;
    return err;
}

/* Helper: forward incoming POST to remote_path (uses request body or config credentials for login) */
static esp_err_t forward_post(httpd_req_t *req, const char *remote_path, const char *override_body, bool override_with_config_credentials)
{
    char *incoming_body = NULL;
    if (!override_body) incoming_body = read_request_body(req);
    const char *body_to_send = override_body ? override_body : (incoming_body ? incoming_body : "{}");

    /* LOG incoming request */
    log_httpd_request(req, incoming_body);
    ESP_LOGI(TAG, "Forwarding to remote path: %s", remote_path);

    /* try to forward Authorization header from incoming request */
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    char *auth_hdr = NULL;
    if (auth_len > 0) {
        auth_hdr = malloc(auth_len + 1);
        if (auth_hdr) httpd_req_get_hdr_value_str(req, "Authorization", auth_hdr, auth_len + 1);
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
    esp_err_t err = remote_post(remote_path, send_body, auth_hdr, &remote_resp, &status);

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

    ESP_LOGI(TAG, "Forward to %s returned HTTP %d", remote_path, status);

    /* on login, store access_token locally if provided */
    if (strcmp(remote_path, "/api/login") == 0 && remote_resp) {
        cJSON *rj = cJSON_Parse(remote_resp);
        if (rj) {
            cJSON *at = cJSON_GetObjectItemCaseSensitive(rj, "access_token");
            if (cJSON_IsString(at) && at->valuestring) {
                snprintf(g_auth_token, sizeof(g_auth_token), "%s", at->valuestring);
                ESP_LOGI(TAG, "Stored access_token (len=%d)", (int)strlen(g_auth_token));
#ifdef HTTP_SERVICES_LOG_TO_UI
                webui_log_chunked("INFO", TAG, "Stored access_token", g_auth_token);
#endif
            }
            cJSON_Delete(rj);
        }
    }

    httpd_resp_set_type(req, "application/json");
    if (remote_resp) {
        httpd_resp_send(req, remote_resp, strlen(remote_resp));
        free(remote_resp);
    } else {
        httpd_resp_send(req, "{}", 2);
    }
    return ESP_OK;
}

static esp_err_t api_login_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/login -> proxy to remote server");
    return forward_post(req, "/api/login", NULL, true);
}

static esp_err_t api_keepalive_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/keepalive -> proxy to remote server");
    return forward_post(req, "/api/keepalive", NULL, false);
}

static esp_err_t api_getimages_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getimages -> proxy to remote server");
    return forward_post(req, "/api/getimages", NULL, false);
}

static esp_err_t api_getconfig_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getconfig -> proxy to remote server");
    return forward_post(req, "/api/getconfig", NULL, false);
}

static esp_err_t api_gettranslations_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/gettranslations -> proxy to remote server");
    return forward_post(req, "/api/gettranslations", NULL, false);
}

static esp_err_t api_getfirmware_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getfirmware -> proxy to remote server");
    return forward_post(req, "/api/getfirmware", NULL, false);
}

static esp_err_t api_payment_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/payment -> proxy to remote server");
    return forward_post(req, "/api/payment", NULL, false);
}

static esp_err_t api_serviceused_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/serviceused -> proxy to remote server");
    return forward_post(req, "/api/serviceused", NULL, false);
}

static esp_err_t api_paymentoffline_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/paymentoffline -> proxy to remote server");
    return forward_post(req, "/api/paymentoffline", NULL, false);
}

static esp_err_t api_getcustomers_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getcustomers -> proxy to remote server");
    return forward_post(req, "/api/getcustomers", NULL, false);
}

static esp_err_t api_getoperators_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getoperators -> proxy to remote server");
    return forward_post(req, "/api/getoperators", NULL, false);
}

static esp_err_t api_activity_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/activity -> proxy to remote server");
    return forward_post(req, "/api/activity", NULL, false);
}


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

    ESP_LOGI(TAG, "Registered POST /api/* handlers (http_services)");
    return ESP_OK;
}
