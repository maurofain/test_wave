/**
 * web_ui_logs_component.c
 * 
 * Handler endpoints for selective component logging:
 * - POST /api/logs/component/{COMPONENT}
 * - GET /api/logs/component/{COMPONENT}/status
 */

#include "web_ui.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "WEB_LOGS_COMPONENT";

/**
 * Send JSON error response
 */
static esp_err_t web_ui_send_json_error(httpd_req_t *req, int status_code, const char *error_msg) {
    cJSON *err = cJSON_CreateObject();
    if (!err) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }
    
    cJSON_AddNumberToObject(err, "status", status_code);
    cJSON_AddStringToObject(err, "error", error_msg);
    
    char *err_str = cJSON_Print(err);
    cJSON_Delete(err);
    
    if (!err_str) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, NULL, 0);
        return ESP_FAIL;
    }
    
    char status_buf[32] = {0};
    snprintf(status_buf, sizeof(status_buf), "%d %s", status_code, 
             status_code == 400 ? "Bad Request" : 
             status_code == 404 ? "Not Found" :
             status_code == 500 ? "Internal Server Error" : "Error");
    
    httpd_resp_set_status(req, status_buf);
    httpd_resp_set_type(req, "application/json");
    int ret = httpd_resp_send(req, err_str, strlen(err_str));
    free(err_str);
    
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

/**
 * Map component short names to TAG strings
 */
typedef struct {
    const char *short_name;
    const char *tag;
    esp_log_level_t default_level;
} component_log_t;

static const component_log_t LOG_COMPONENTS[] = {
    {"HTTP_SERVICES", "HTTP_SERVICES", ESP_LOG_INFO},
    {"lvgl", "lvgl_panel", ESP_LOG_INFO},
    {"io_expander", "io_expander", ESP_LOG_INFO},
    {NULL, NULL, ESP_LOG_NONE}
};

/**
 * Lookup component by short name
 */
static const component_log_t *find_component(const char *name) {
    if (!name) return NULL;
    for (int i = 0; LOG_COMPONENTS[i].short_name; i++) {
        if (strcmp(LOG_COMPONENTS[i].short_name, name) == 0) {
            return &LOG_COMPONENTS[i];
        }
    }
    return NULL;
}

/**
 * GET /api/logs/component/{COMPONENT}/status
 * 
 * Returns: {"enabled": true/false}
 * enabled=true quando il livello di log è >= ESP_LOG_INFO
 */
esp_err_t api_logs_component_status_get(httpd_req_t *req) {
    // Extract component name from URI: /api/logs/component/{COMPONENT}/status
    const char *uri = req->uri;  // "/api/logs/component/{COMPONENT}/status"
    
    // Parse: skip "/api/logs/component/" (20 chars)
    const char *comp_start = uri + 20;
    const char *comp_end = strchr(comp_start, '/');
    if (!comp_end) comp_end = comp_start + strlen(comp_start);
    
    int comp_len = comp_end - comp_start;
    if (comp_len <= 0 || comp_len > 63) {
        return web_ui_send_json_error(req, 400, "Invalid component name");
    }
    
    char comp_name[64] = {0};
    strncpy(comp_name, comp_start, comp_len);
    
    // Lookup component
    const component_log_t *comp = find_component(comp_name);
    if (!comp) {
        return web_ui_send_json_error(req, 404, "Component not found");
    }
    
    // Get current log level
    esp_log_level_t current_level = esp_log_level_get(comp->tag);
    bool enabled = (current_level >= ESP_LOG_INFO);
    
    // Return JSON response
    cJSON *root = cJSON_CreateObject();
    if (!root) return web_ui_send_json_error(req, 500, "JSON creation failed");
    
    cJSON_AddBoolToObject(root, "enabled", enabled);
    cJSON_AddStringToObject(root, "component", comp_name);
    cJSON_AddStringToObject(root, "tag", comp->tag);
    
    char *response_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    if (!response_str) {
        return web_ui_send_json_error(req, 500, "JSON print failed");
    }
    
    httpd_resp_set_type(req, "application/json");
    int ret = httpd_resp_send(req, response_str, strlen(response_str));
    free(response_str);
    
    ESP_LOGD(TAG, "[C] GET /api/logs/component/%s/status -> enabled=%d", comp_name, enabled);
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

/**
 * POST /api/logs/component/{COMPONENT}
 * 
 * Expects: {"enabled": true/false}
 * Sets log level to ESP_LOG_INFO (enabled) or ESP_LOG_NONE (disabled)
 */
esp_err_t api_logs_component_post(httpd_req_t *req) {
    // Extract component name from URI
    const char *uri = req->uri;
    const char *comp_start = uri + 20;  // skip "/api/logs/component/"
    const char *comp_end = strchr(comp_start, '/');
    if (!comp_end) comp_end = comp_start + strlen(comp_start);
    
    int comp_len = comp_end - comp_start;
    if (comp_len <= 0 || comp_len > 63) {
        return web_ui_send_json_error(req, 400, "Invalid component name");
    }
    
    char comp_name[64] = {0};
    strncpy(comp_name, comp_start, comp_len);
    
    // Lookup component
    const component_log_t *comp = find_component(comp_name);
    if (!comp) {
        return web_ui_send_json_error(req, 404, "Component not found");
    }
    
    // Parse request body
    char buf[256] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return web_ui_send_json_error(req, 400, "No body received");
    }
    buf[ret] = '\0';
    
    // Parse JSON
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return web_ui_send_json_error(req, 400, "Invalid JSON");
    }
    
    cJSON *enabled_obj = cJSON_GetObjectItem(root, "enabled");
    cJSON_Delete(root);
    
    if (!enabled_obj || !cJSON_IsBool(enabled_obj)) {
        return web_ui_send_json_error(req, 400, "Missing or invalid 'enabled' field");
    }
    
    bool enable = enabled_obj->valueint != 0;
    
    // Set log level
    esp_log_level_t new_level = enable ? ESP_LOG_INFO : ESP_LOG_NONE;
    esp_log_level_set(comp->tag, new_level);
    
    // Verify
    esp_log_level_t verified = esp_log_level_get(comp->tag);
    bool actually_enabled = (verified >= ESP_LOG_INFO);
    
    // Return JSON response
    cJSON *resp = cJSON_CreateObject();
    if (!resp) return web_ui_send_json_error(req, 500, "JSON creation failed");
    
    cJSON_AddBoolToObject(resp, "enabled", actually_enabled);
    cJSON_AddStringToObject(resp, "component", comp_name);
    
    char *response_str = cJSON_Print(resp);
    cJSON_Delete(resp);
    
    if (!response_str) {
        return web_ui_send_json_error(req, 500, "JSON print failed");
    }
    
    httpd_resp_set_type(req, "application/json");
    int send_ret = httpd_resp_send(req, response_str, strlen(response_str));
    free(response_str);
    
    ESP_LOGI(TAG, "[C] POST /api/logs/component/%s -> enabled=%d", comp_name, actually_enabled);
    return send_ret == 0 ? ESP_OK : ESP_FAIL;
}
