#include "web_ui.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_err.h"

#define TAG "WEB_UI"

static httpd_handle_t s_server = NULL;

// Forward declaration: implemented in web_ui.c (same component)
esp_err_t web_ui_register_handlers(httpd_handle_t server);

esp_err_t web_ui_init(void)
{
    if (s_server != NULL) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_APP_HTTP_PORT;
    config.max_uri_handlers = 64;  // aumentato per ospitare tutte le URI + future estensioni
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "[C] Avvio server HTTP sulla porta %d", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) return ret;

    // Delego la registrazione degli handler al modulo "web_ui"
    ret = web_ui_register_handlers(s_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Registrazione handler fallita");
        httpd_stop(s_server);
        s_server = NULL;
        return ret;
    }

    return ESP_OK;
}

void web_ui_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
