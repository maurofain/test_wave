#include "web_ui.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "WEB_UI"

static httpd_handle_t s_server = NULL;

static void web_ui_load_http_task_config(UBaseType_t *priority, BaseType_t *core_id, size_t *stack_size)
{
    FILE *f = fopen("/spiffs/tasks.json", "r");
    if (!f) {
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 32768) {
        fclose(f);
        return;
    }

    char *buf = malloc((size_t)file_size + 1);
    if (!buf) {
        fclose(f);
        return;
    }
    fread(buf, 1, (size_t)file_size, f);
    fclose(f);
    buf[file_size] = '\0';

    cJSON *arr = cJSON_Parse(buf);
    free(buf);
    if (!arr || !cJSON_IsArray(arr)) {
        if (arr) cJSON_Delete(arr);
        return;
    }

    cJSON *obj;
    cJSON_ArrayForEach(obj, arr) {
        cJSON *jname = cJSON_GetObjectItem(obj, "n");
        if (!jname || !cJSON_IsString(jname)) continue;
        if (strcmp(jname->valuestring, "http_server") != 0) continue;

        cJSON *jprio  = cJSON_GetObjectItem(obj, "p");
        cJSON *jcore  = cJSON_GetObjectItem(obj, "c");
        cJSON *jstack = cJSON_GetObjectItem(obj, "w");

        if (jprio  && cJSON_IsNumber(jprio)  && jprio->valueint  >= 1)    *priority  = (UBaseType_t)jprio->valueint;
        if (jcore  && cJSON_IsNumber(jcore)  && jcore->valueint  >= 0)    *core_id   = (BaseType_t)jcore->valueint;
        if (jstack && cJSON_IsNumber(jstack) && jstack->valueint >= 2048) *stack_size = (size_t)jstack->valueint * sizeof(StackType_t);
        break;
    }

    cJSON_Delete(arr);
}

// Forward declaration: implemented in web_ui.c (same component)
esp_err_t web_ui_register_handlers(httpd_handle_t server);

esp_err_t web_ui_init(void)
{
    if (s_server != NULL) {
        ESP_LOGI(TAG, "[C] web_ui_init: server già attivo");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_APP_HTTP_PORT;
    config.max_uri_handlers = 100;  /* alzato da 64: con http_services siamo ~68 handler, il vecchio limite causava 404 sulle route registrate dopo il limite */
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 3;   /* sec: libera socket inattivi più velocemente (default ~5s) */
    config.send_wait_timeout = 3;   /* sec: idem lato send */
    config.uri_match_fn = httpd_uri_match_wildcard;
    web_ui_load_http_task_config(&config.task_priority, &config.core_id, &config.stack_size);

    ESP_LOGI(TAG, "[C] Avvio server HTTP: port=%d ctrl_port=%d max_uri=%d stack=%d",
             config.server_port, config.ctrl_port, config.max_uri_handlers, config.stack_size);
    ESP_LOGI(TAG, "[C] HTTP task config: priority=%u core=%d stack=%u",
             (unsigned)config.task_priority, (int)config.core_id, (unsigned)config.stack_size);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] httpd_start fallita: %s", esp_err_to_name(ret));
        return ret;
    }

    // Delego la registrazione degli handler al modulo "web_ui"
    ret = web_ui_register_handlers(s_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Registrazione handler fallita: %s", esp_err_to_name(ret));
        httpd_stop(s_server);
        s_server = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "[C] Web UI pronta");

    return ESP_OK;
}

void web_ui_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}

bool web_ui_is_running(void)
{
    return (s_server != NULL);
}
