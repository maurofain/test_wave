#include "web_ui.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "WEB_UI"

static httpd_handle_t s_server = NULL;

static void web_ui_load_http_task_config(UBaseType_t *priority, BaseType_t *core_id, size_t *stack_size)
{
    FILE *f = fopen("/spiffs/tasks.csv", "r");
    if (!f) {
        return;
    }

    char line[192];
    bool skip_header = true;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (skip_header) {
            skip_header = false;
            continue;
        }

        char name[64] = {0};
        char state[16] = {0};
        int prio = -1;
        int core = -1;
        int period_ms = 0;
        int stack_words = 0;

        if (sscanf(line, "%63[^,],%15[^,],%d,%d,%d,%d",
                   name, state, &prio, &core, &period_ms, &stack_words) != 6) {
            continue;
        }

        if (strcmp(name, "http_server") != 0) {
            continue;
        }

        if (prio >= 1) {
            *priority = (UBaseType_t)prio;
        }
        if (core >= 0) {
            *core_id = (BaseType_t)core;
        }
        if (stack_words >= 2048) {
            *stack_size = (size_t)stack_words * sizeof(StackType_t);
        }
        break;
    }

    fclose(f);
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
    config.max_uri_handlers = 64;  // aumentato per ospitare tutte le URI + future estensioni
    config.stack_size = 8192;
    config.lru_purge_enable = true;
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
