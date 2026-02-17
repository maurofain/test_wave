#include "http_services.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "cJSON.h"

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

    if (strcmp(password, expected_md5) != 0) {
        const char *resp = "{\"iserror\":true,\"codeerror\":401,\"deserror\":\"Unauthorized\"}";
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_OK;
    }

    // Generate and store the token (store raw token value; clients add the "Bearer " prefix)
    snprintf(g_auth_token, sizeof(g_auth_token), "%s", serial);  // simple token for local testing

    char resp_buf[512];
    snprintf(resp_buf, sizeof(resp_buf), "{\"iserror\":false,\"access_token\":\"%s\"}", g_auth_token);
    esp_err_t err = httpd_resp_send(req, resp_buf, strlen(resp_buf));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send login response: %s", esp_err_to_name(err));
        return err;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

/* POST /api/keepalive - Maintain connection alive */
static esp_err_t api_keepalive_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/keepalive");

    if (g_auth_token[0] == '\0') {
        const char *resp = "{\"iserror\":true,\"codeerror\":401,\"deserror\":\"Unauthorized\"}";
        httpd_resp_set_type(req, "application/json");
        esp_err_t err = httpd_resp_send(req, resp, strlen(resp));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send keepalive response: %s", esp_err_to_name(err));
            return err;
        }
        return ESP_FAIL;  // Return an error code to indicate unauthorized access
    }

    // Prepare the response buffer
    char resp_buf[512];
    struct tm now_tm;
    time_t now = time(NULL);
    gmtime_r(&now, &now_tm); /* use UTC */

    /* Build ISO timestamp and include token */
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S.000Z", &now_tm);
    snprintf(resp_buf, sizeof(resp_buf), "{\"timestamp\": \"%s\",\"access_token\":\"%s\"}", ts, g_auth_token);

    // Set the response content type and send the response
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, resp_buf, strlen(resp_buf));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send keepalive response: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

/* Helper: validate Date header + Authorization: Bearer <token> */
static bool require_date_and_auth(httpd_req_t *req)
{
    /* Date required */
    size_t date_len = httpd_req_get_hdr_value_len(req, "Date");
    if (date_len == 0) {
        const char *resp = "{\"iserror\":true,\"codeerror\":400,\"deserror\":\"missing_date\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return false;
    }
    char *date_hdr = malloc(date_len + 1);
    if (!date_hdr) {
        const char *resp = "{\"iserror\":true,\"codeerror\":500,\"deserror\":\"server_error\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return false;
    }
    if (httpd_req_get_hdr_value_str(req, "Date", date_hdr, date_len + 1) != ESP_OK) {
        free(date_hdr);
        const char *resp = "{\"iserror\":true,\"codeerror\":400,\"deserror\":\"missing_date\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return false;
    }

    /* Authorization: Bearer <token> required */
    size_t auth_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (auth_len == 0) {
        free(date_hdr);
        const char *resp = "{\"iserror\":true,\"codeerror\":401,\"deserror\":\"Unauthorized\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return false;
    }
    char *auth_hdr = malloc(auth_len + 1);
    if (!auth_hdr) {
        free(date_hdr);
        const char *resp = "{\"iserror\":true,\"codeerror\":500,\"deserror\":\"server_error\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return false;
    }
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_hdr, auth_len + 1) != ESP_OK || strncmp(auth_hdr, "Bearer ", 7) != 0) {
        free(date_hdr);
        free(auth_hdr);
        const char *resp = "{\"iserror\":true,\"codeerror\":401,\"deserror\":\"Unauthorized\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return false;
    }

    /* Verify token matches the one issued by /api/login */
    char *token = auth_hdr + 7;
    if (g_auth_token[0] == '\0' || strcmp(token, g_auth_token) != 0) {
        free(date_hdr);
        free(auth_hdr);
        const char *resp = "{\"iserror\":true,\"codeerror\":401,\"deserror\":\"Unauthorized\"}";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, resp, strlen(resp));
        return false;
    }

    free(date_hdr);
    free(auth_hdr);
    return true;
}

/* --- Server-side test implementations for the documented endpoints --- */

static esp_err_t api_getimages_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getimages");
    if (!require_date_and_auth(req)) return ESP_OK;

    /* In a real implementation this would consult storage or a DB. Return sample response. */
    const char *resp = "{\"iserror\":false,\"files\":[\"splash.jpg\",\"logo.jpg\"],\"server\":\"ftp.example.com\",\"user\":\"ftpuser\",\"password\":\"ftppass\",\"path\":\"/images\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t api_getconfig_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getconfig");
    if (!require_date_and_auth(req)) return ESP_OK;

    const char *resp = "{\"iserror\":false,\"files\":[\"device.cfg\"],\"server\":\"ftp.example.com\",\"user\":\"cfguser\",\"password\":\"cfgpass\",\"path\":\"/config\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t api_gettranslations_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/gettranslations");
    if (!require_date_and_auth(req)) return ESP_OK;

    const char *resp = "{\"iserror\":false,\"files\":[\"it.json\",\"en.json\"],\"server\":\"ftp.example.com\",\"user\":\"truser\",\"password\":\"trpass\",\"path\":\"/translations\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t api_getfirmware_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getfirmware");
    if (!require_date_and_auth(req)) return ESP_OK;

    const char *resp = "{\"iserror\":false,\"files\":[\"v1.2.3.bin\"],\"server\":\"ftp.example.com\",\"user\":\"fwuser\",\"password\":\"fwpass\",\"path\":\"/firmware\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

/* Simple in-memory counters for test IDs */
static int s_next_payment_id = 1000;
static int s_next_activity_id = 1;

static esp_err_t api_payment_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/payment");
    if (!require_date_and_auth(req)) return ESP_OK;

    char *body = read_request_body(req);
    (void)body; /* for now we don't persist */

    char resp[256];
    time_t now = time(NULL);
    struct tm now_tm; gmtime_r(&now, &now_tm);
    char ts[64]; strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S.000Z", &now_tm);
    int paymentid = __atomic_fetch_add(&s_next_payment_id, 1, __ATOMIC_RELAXED);
    snprintf(resp, sizeof(resp), "{\"iserror\":false,\"datetime\":\"%s\",\"paymentid\":%d}", ts, paymentid);

    if (body) free(body);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t api_serviceused_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/serviceused");
    if (!require_date_and_auth(req)) return ESP_OK;

    char *body = read_request_body(req);
    (void)body;

    char resp[192];
    time_t now = time(NULL);
    struct tm now_tm; gmtime_r(&now, &now_tm);
    char ts[64]; strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S.000Z", &now_tm);
    snprintf(resp, sizeof(resp), "{\"iserror\":false,\"datetime\":\"%s\"}", ts);

    if (body) free(body);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t api_paymentoffline_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/paymentoffline");
    if (!require_date_and_auth(req)) return ESP_OK;

    char *body = read_request_body(req);
    (void)body;

    char resp[256];
    time_t now = time(NULL);
    struct tm now_tm; gmtime_r(&now, &now_tm);
    char ts[64]; strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S.000Z", &now_tm);
    int paymentid = __atomic_fetch_add(&s_next_payment_id, 1, __ATOMIC_RELAXED);
    snprintf(resp, sizeof(resp), "{\"iserror\":false,\"datetime\":\"%s\",\"paymentid\":%d}", ts, paymentid);

    if (body) free(body);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t api_getcustomers_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getcustomers");
    if (!require_date_and_auth(req)) return ESP_OK;

    char *body = read_request_body(req);
    const char *code = "*";
    if (body) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            cJSON *jcode = cJSON_GetObjectItemCaseSensitive(root, "Code");
            if (cJSON_IsString(jcode) && jcode->valuestring) code = jcode->valuestring;
            cJSON_Delete(root);
        }
    }

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp, "iserror", false);
    cJSON *arr = cJSON_CreateArray();

    if (strcmp(code, "*") == 0) {
        cJSON *c1 = cJSON_CreateObject();
        cJSON_AddStringToObject(c1, "code", "CUST001");
        cJSON_AddStringToObject(c1, "telephone", "+390123456");
        cJSON_AddStringToObject(c1, "email", "demo@example.com");
        cJSON_AddStringToObject(c1, "name", "Mario");
        cJSON_AddStringToObject(c1, "surname", "Rossi");
        cJSON_AddNumberToObject(c1, "amount", 5000);
        cJSON_AddBoolToObject(c1, "new", false);
        cJSON_AddItemToArray(arr, c1);
    } else {
        cJSON *c1 = cJSON_CreateObject();
        cJSON_AddStringToObject(c1, "code", code);
        cJSON_AddStringToObject(c1, "telephone", "");
        cJSON_AddStringToObject(c1, "email", "");
        cJSON_AddStringToObject(c1, "name", "");
        cJSON_AddStringToObject(c1, "surname", "");
        cJSON_AddNumberToObject(c1, "amount", 0);
        cJSON_AddBoolToObject(c1, "new", true);
        cJSON_AddItemToArray(arr, c1);
    }

    cJSON_AddItemToObject(resp, "customers", arr);
    char *out = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, out, strlen(out));

    if (body) free(body);
    free(out);
    cJSON_Delete(resp);
    return ESP_OK;
}

static esp_err_t api_getoperators_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/getoperators");
    if (!require_date_and_auth(req)) return ESP_OK;

    /* Return a small operators list for testing */
    const char *resp = "{\"iserror\":false,\"operator\":[{\"code\":\"OP01\",\"username\":\"admin\",\"pin\":\"1234\",\"password\":\"pwd\"}]}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

static esp_err_t api_activity_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/activity");
    if (!require_date_and_auth(req)) return ESP_OK;

    char *body = read_request_body(req);
    (void)body;

    char resp[192];
    time_t now = time(NULL);
    struct tm now_tm; gmtime_r(&now, &now_tm);
    char ts[64]; strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S.000Z", &now_tm);
    int activityid = __atomic_fetch_add(&s_next_activity_id, 1, __ATOMIC_RELAXED);
    snprintf(resp, sizeof(resp), "{\"iserror\":false,\"datetime\":\"%s\",\"activityid\":%d}", ts, activityid);

    if (body) free(body);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
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
