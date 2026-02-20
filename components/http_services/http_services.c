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

// Temporary implementation of validate_token
bool validate_token(const char *token) {
    // Add actual token validation logic here
    return token != NULL && strlen(token) > 0;
}

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

/* small accumulator type used by the http client event handler */
typedef struct { char *buf; size_t len; size_t cap; } http_acc_t;

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

/* Helper: perform HTTP POST to remote server (returns dynamically allocated response in out_resp)
   and reports the number of bytes read via out_len (may differ from strlen when binary/NULs are present). */
static esp_err_t remote_post(const char *remote_path, const char *body, const char *auth_header, char **out_resp, int *out_status, size_t *out_len)
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

    /* accumulator used by event handler to collect ON_DATA events */
    http_acc_t acc = { .buf = NULL, .len = 0, .cap = 0 };

    esp_http_client_config_t cfg_http = {
        .url = url,
        .timeout_ms = 15000,
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
    bool use_fallback_headers = false;

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
        //format_iso8601_local_us_tz(date_hdr, sizeof(date_hdr));
        esp_http_client_set_header(client, "Date", date_hdr);

        if (use_fallback_headers) {
            esp_http_client_set_header(client, "Connection", "close");
            esp_http_client_set_header(client, "Accept-Encoding", "identity");
            ESP_LOGI(TAG, "Using fallback headers: Connection: close, Accept-Encoding: identity (attempt %d)", attempt + 1);
        }

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

        /* perform (enable verbose http/tls logs briefly to help debug read failures) */
        esp_log_level_set("esp_http_client", ESP_LOG_DEBUG);
        esp_log_level_set("esp_tls", ESP_LOG_DEBUG);
        err = esp_http_client_perform(client);
        status = esp_http_client_get_status_code(client);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_http_client_perform returned %s (attempt %d)", esp_err_to_name(err), attempt + 1);
        }
        /* restore default levels (keep noise low) */
        esp_log_level_set("esp_http_client", ESP_LOG_INFO);
        esp_log_level_set("esp_tls", ESP_LOG_WARN);

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

            /* If we got an ESP_ERR_HTTP_INCOMPLETE_DATA and we still have retry attempts, try again */
            if (err == ESP_ERR_HTTP_INCOMPLETE_DATA && attempt + 1 < max_attempts) {
                ESP_LOGW(TAG, "Incomplete chunked data received — retrying (attempt %d)", attempt + 2);
                if (resp) { free(resp); resp = NULL; total = 0; }
                continue;
            }
        }

        /* otherwise break and use the data */
        break;
    }

    if (!client) return ESP_ERR_NO_MEM;

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
    esp_err_t err = remote_post(remote_path, send_body, auth_hdr, &remote_resp, &status, &remote_len);

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
        size_t send_len = (remote_len > 0) ? remote_len : strlen(remote_resp);
        httpd_resp_send(req, remote_resp, send_len);
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
    ESP_LOGI(TAG, "POST /api/keepalive -> processing request");

    // Extract the Authorization header (dynamic length to support long JWT)
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (auth_len == 0) {
        ESP_LOGE(TAG, "Authorization header missing");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authorization header missing");
        return ESP_FAIL;
    }

    char *auth_header = malloc(auth_len + 1);
    if (!auth_header) {
        ESP_LOGE(TAG, "OOM reading Authorization header");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, auth_len + 1) != ESP_OK) {
        free(auth_header);
        ESP_LOGE(TAG, "Authorization header read failed");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Authorization header missing");
        return ESP_FAIL;
    }

    // Validate the token (example validation logic)
    if (strncmp(auth_header, "Bearer ", 7) != 0 || strlen(auth_header) <= 7) {
        free(auth_header);
        ESP_LOGE(TAG, "Invalid Authorization header format");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid Authorization header");
        return ESP_FAIL;
    }

    const char *token = auth_header + 7; // Skip "Bearer " prefix
    if (!validate_token(token)) { // Assume validate_token is implemented elsewhere
        free(auth_header);
        ESP_LOGE(TAG, "Invalid or expired token");
        httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid or expired token");
        return ESP_FAIL;
    }
    free(auth_header);

    // Buffer to store the incoming JSON payload
    char content[256];
    int content_len = httpd_req_recv(req, content, sizeof(content) - 1);
    if (content_len <= 0) {
        ESP_LOGE(TAG, "Failed to receive request payload");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive payload");
        return ESP_FAIL;
    }
    content[content_len] = '\0'; // Null-terminate the received content

    // Parse the JSON payload
    cJSON *json = cJSON_Parse(content);
    if (!json) {
        ESP_LOGE(TAG, "Invalid JSON payload");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // Extract fields from the JSON payload
    const cJSON *status = cJSON_GetObjectItem(json, "status");
    const cJSON *inputstates = cJSON_GetObjectItem(json, "inputstates");
    const cJSON *outputstates = cJSON_GetObjectItem(json, "outputstates");
    const cJSON *temperature = cJSON_GetObjectItem(json, "temperature");
    const cJSON *humidity = cJSON_GetObjectItem(json, "humidity");
    const cJSON *subdevices = cJSON_GetObjectItem(json, "subdevices");

    if (!status || !cJSON_IsString(status) ||
        !inputstates || !cJSON_IsString(inputstates) ||
        !outputstates || !cJSON_IsString(outputstates) ||
        !temperature || !cJSON_IsNumber(temperature) ||
        !humidity || !cJSON_IsNumber(humidity) ||
        !subdevices || !cJSON_IsArray(subdevices)) {
        ESP_LOGE(TAG, "Missing or invalid fields in JSON payload");
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid fields in JSON");
        return ESP_FAIL;
    }

    // Log the received data
    ESP_LOGI(TAG, "Keepalive status: %s", status->valuestring);
    ESP_LOGI(TAG, "Input states: %s", inputstates->valuestring);
    ESP_LOGI(TAG, "Output states: %s", outputstates->valuestring);
    ESP_LOGI(TAG, "Temperature: %d", temperature->valueint);
    ESP_LOGI(TAG, "Humidity: %d", humidity->valueint);

    cJSON *subdevice = NULL;
    cJSON_ArrayForEach(subdevice, subdevices) {
        const cJSON *code = cJSON_GetObjectItem(subdevice, "code");
        const cJSON *sub_status = cJSON_GetObjectItem(subdevice, "status");
        if (code && cJSON_IsString(code) && sub_status && cJSON_IsString(sub_status)) {
            ESP_LOGI(TAG, "Subdevice code: %s, status: %s", code->valuestring, sub_status->valuestring);
        }
    }

    // Create the response JSON
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "iserror", false);
    cJSON_AddNumberToObject(response, "codeerror", 0);
    cJSON_AddStringToObject(response, "deserror", "");
    cJSON_AddStringToObject(response, "datetime", "2026-01-23T13:25:13.218763+01:00");

    cJSON *activities = cJSON_AddArrayToObject(response, "activities");
    cJSON *activity = cJSON_CreateObject();
    cJSON_AddNumberToObject(activity, "activityid", 1);
    cJSON_AddStringToObject(activity, "code", "example_activity");
    cJSON_AddStringToObject(activity, "parameters", "example_parameters");
    cJSON_AddItemToArray(activities, activity);

    // Send the response
    const char *response_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response_str, strlen(response_str));

    // Clean up
    cJSON_Delete(json);
    cJSON_Delete(response);
    free((void *)response_str);

    return ESP_OK;
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

static esp_err_t api_deviceactivity_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/deviceactivity -> proxy to remote server");
    return forward_post(req, "/api/deviceactivity", NULL, false);
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
