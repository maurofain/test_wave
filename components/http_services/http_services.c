#include "http_services.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "cJSON.h"
#include "mbedtls/md5.h"
#include "esp_http_server.h"

static const char *TAG = "HTTP_SERVICES";

/* Helper: compute MD5 hex (lowercase) of input */
static void md5_hex(const unsigned char *input, size_t ilen, char *out_hex, size_t out_hex_len)
{
    unsigned char md[16];
    if (mbedtls_md5(input, ilen, md) != 0) {
        out_hex[0] = '\0';
        return;
    }
    if (out_hex_len < 33) { /* need 32 chars + NUL */
        out_hex[0] = '\0';
        return;
    }
    for (int i = 0; i < 16; ++i) {
        sprintf(&out_hex[i*2], "%02x", md[i]);
    }
    out_hex[32] = '\0';
}

/* Read the whole request body into a NUL-terminated buffer (caller frees) */
static char *read_request_body(httpd_req_t *req)
{
    int content_len = req->content_len;
    if (content_len <= 0) return NULL;
    if (content_len > 8192) content_len = 8192; /* safety cap */
    char *buf = malloc(content_len + 1);
    if (!buf) return NULL;
    int received = 0;
    while (received < content_len) {
        int ret = httpd_req_recv(req, buf + received, content_len - received);
        if (ret <= 0) {
            free(buf);
            return NULL;
        }
        received += ret;
    }
    buf[received] = '\0';
    return buf;
}

/* Format date used by services_http.md: MMYYYYdd (month 2 digits, year 4 digits, day 2 digits)
 * Use device local time; if Date header is provided the server will log it but still
 * compute expected password using device time. */
static void format_date_mm_yyyy_dd(char *out, size_t len)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    snprintf(out, len, "%02d%04d%02d", tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_mday);
}

/* POST /api/login
 * Body: { "serial":"...", "password":"<md5>" }
 * Password expected: MD5(MMYYYYdd + serial) where date is device date (see docs/servizi_http.md)
 */
static esp_err_t api_login_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/login");
    httpd_resp_set_type(req, "application/json");

    char *body = read_request_body(req);
    if (!body) {
        const char *resp = "{\"iserror\":true,\"codeerror\":400,\"deserror\":\"empty_body\"}";
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        const char *resp = "{\"iserror\":true,\"codeerror\":400,\"deserror\":\"invalid_json\"}";
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    const cJSON *jserial = cJSON_GetObjectItemCaseSensitive(root, "serial");
    const cJSON *jpassword = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!cJSON_IsString(jserial) || !cJSON_IsString(jpassword)) {
        const char *resp = "{\"iserror\":true,\"codeerror\":400,\"deserror\":\"missing_fields\"}";
        cJSON_Delete(root);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    const char *serial = jserial->valuestring;
    const char *password = jpassword->valuestring;

    /* Build expected string = date + serial */
    char date_str[32];
    format_date_mm_yyyy_dd(date_str, sizeof(date_str));

    char expected_input[256];
    snprintf(expected_input, sizeof(expected_input), "%s%s", date_str, serial);

    char expected_md5[33];
    md5_hex((const unsigned char *)expected_input, strlen(expected_input), expected_md5, sizeof(expected_md5));

    ESP_LOGD(TAG, "serial=%s date=%s expected_md5=%s provided=%s", serial, date_str, expected_md5, password);

    if (strcasecmp(expected_md5, password) != 0) {
        const char *resp = "{\"iserror\":true,\"codeerror\":401,\"deserror\":\"invalid_credentials\"}";
        cJSON_Delete(root);
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    /* Generate a simple access token (MD5 of serial + current time) */
    char token_input[128];
    snprintf(token_input, sizeof(token_input), "%s:%lld", serial, (long long)time(NULL));
    char access_token[33];
    md5_hex((const unsigned char *)token_input, strlen(token_input), access_token, sizeof(access_token));

    char resp_buf[256];
    snprintf(resp_buf, sizeof(resp_buf), "{\"access_token\": \"%s\"}", access_token);

    cJSON_Delete(root);
    httpd_resp_send(req, resp_buf, strlen(resp_buf));
    return ESP_OK;
}

esp_err_t http_services_register_handlers(httpd_handle_t server)
{
    httpd_uri_t uri_login = {
        .uri = "/api/login",
        .method = HTTP_POST,
        .handler = api_login_post,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_login);
    ESP_LOGI(TAG, "Registered POST /api/login handler");
    return ESP_OK;
}
