#include "web_ui_internal.h"
#include "esp_http_server.h"
#include "esp_log.h"

#define TAG "WEB_UI_REG"

// Registrazione di tutte le URI e degli handler - centralizzata
esp_err_t web_ui_register_handlers(httpd_handle_t server)
{
    httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
    httpd_register_uri_handler(server, &uri_root);
    
    httpd_uri_t uri_logo = {.uri = "/logo.jpg", .method = HTTP_GET, .handler = logo_get_handler};
    httpd_register_uri_handler(server, &uri_logo);
    
    httpd_uri_t uri_status = {.uri = "/status", .method = HTTP_GET, .handler = status_get_handler};
    httpd_register_uri_handler(server, &uri_status);
    
    httpd_uri_t uri_ota_get = {.uri = "/ota", .method = HTTP_GET, .handler = ota_get_handler};
    httpd_register_uri_handler(server, &uri_ota_get);
    httpd_uri_t uri_ota_post = {.uri = "/ota", .method = HTTP_POST, .handler = ota_post_handler};
    httpd_register_uri_handler(server, &uri_ota_post);
    httpd_uri_t uri_ota_upload = {.uri = "/ota/upload", .method = HTTP_POST, .handler = ota_upload_handler};
    httpd_register_uri_handler(server, &uri_ota_upload);

    httpd_uri_t uri_config = {.uri = "/config", .method = HTTP_GET, .handler = config_page_handler};
    httpd_register_uri_handler(server, &uri_config);
    
    httpd_uri_t uri_stats = {.uri = "/stats", .method = HTTP_GET, .handler = stats_page_handler};
    httpd_register_uri_handler(server, &uri_stats);
    
    httpd_uri_t uri_tasks = {.uri = "/tasks", .method = HTTP_GET, .handler = tasks_page_handler};
    httpd_register_uri_handler(server, &uri_tasks);
    
    httpd_uri_t uri_test = {.uri = "/test", .method = HTTP_GET, .handler = test_page_handler};
    httpd_register_uri_handler(server, &uri_test);
    
    httpd_uri_t uri_httpservices = {.uri = "/httpservices", .method = HTTP_GET, .handler = httpservices_page_handler};
    httpd_register_uri_handler(server, &uri_httpservices);
    
    httpd_uri_t uri_logs = {.uri = "/logs", .method = HTTP_GET, .handler = logs_page_handler};
    httpd_register_uri_handler(server, &uri_logs);

    // API routes
    httpd_uri_t uri_api_get = {.uri = "/api/config", .method = HTTP_GET, .handler = api_config_get};
    httpd_register_uri_handler(server, &uri_api_get);
    httpd_uri_t uri_api_save = {.uri = "/api/config/save", .method = HTTP_POST, .handler = api_config_save};
    httpd_register_uri_handler(server, &uri_api_save);
    httpd_uri_t uri_api_backup = {.uri = "/api/config/backup", .method = HTTP_POST, .handler = api_config_backup};
    httpd_register_uri_handler(server, &uri_api_backup);
    httpd_uri_t uri_api_reset = {.uri = "/api/config/reset", .method = HTTP_POST, .handler = api_config_reset};
    httpd_register_uri_handler(server, &uri_api_reset);
    httpd_uri_t uri_api_ntp_sync = {.uri = "/api/ntp/sync", .method = HTTP_POST, .handler = api_ntp_sync};
    httpd_register_uri_handler(server, &uri_api_ntp_sync);

    httpd_uri_t uri_api_tasks = {.uri = "/api/tasks", .method = HTTP_GET, .handler = api_tasks_get};
    httpd_register_uri_handler(server, &uri_api_tasks);
    httpd_uri_t uri_api_tasks_save = {.uri = "/api/tasks/save", .method = HTTP_POST, .handler = api_tasks_save};
    httpd_register_uri_handler(server, &uri_api_tasks_save);
    httpd_uri_t uri_api_tasks_apply = {.uri = "/api/tasks/apply", .method = HTTP_POST, .handler = api_tasks_apply};
    httpd_register_uri_handler(server, &uri_api_tasks_apply);

    httpd_uri_t uri_api_test = {.uri = "/api/test/*", .method = HTTP_POST, .handler = api_test_handler};
    httpd_register_uri_handler(server, &uri_api_test);

    // logs API (handlers live in web_ui_logs.c)
    httpd_uri_t uri_api_logs_get = {.uri = "/api/logs", .method = HTTP_GET, .handler = api_logs_get};
    httpd_register_uri_handler(server, &uri_api_logs_get);
    httpd_uri_t uri_api_logs_receive = {.uri = "/api/logs/receive", .method = HTTP_POST, .handler = api_logs_receive};
    httpd_register_uri_handler(server, &uri_api_logs_receive);
    httpd_uri_t uri_api_logs_levels = {.uri = "/api/logs/levels", .method = HTTP_GET, .handler = api_logs_levels_get};
    httpd_register_uri_handler(server, &uri_api_logs_levels);
    httpd_uri_t uri_api_logs_set_level = {.uri = "/api/logs/level", .method = HTTP_POST, .handler = api_logs_set_level};
    httpd_register_uri_handler(server, &uri_api_logs_set_level);
    httpd_uri_t uri_api_logs_options = {.uri = "/api/logs/*", .method = HTTP_OPTIONS, .handler = api_logs_options};
    httpd_register_uri_handler(server, &uri_api_logs_options);

    // debug/usb
    httpd_uri_t uri_api_debug_usb = {.uri = "/api/debug/usb/enumerate", .method = HTTP_GET, .handler = api_debug_usb_enumerate};
    httpd_register_uri_handler(server, &uri_api_debug_usb);
    httpd_uri_t uri_api_debug_usb_restart = {.uri = "/api/debug/usb/restart", .method = HTTP_POST, .handler = api_debug_usb_restart};
    httpd_register_uri_handler(server, &uri_api_debug_usb_restart);

    // error handler
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found_handler);

    // reboot
    httpd_uri_t uri_reboot_factory = {.uri = "/reboot/factory", .method = HTTP_GET, .handler = reboot_factory_handler};
    httpd_register_uri_handler(server, &uri_reboot_factory);
    httpd_uri_t uri_reboot_app = {.uri = "/reboot/app", .method = HTTP_GET, .handler = reboot_app_handler};
    httpd_register_uri_handler(server, &uri_reboot_app);

    ESP_LOGI(TAG, "All web_ui handlers registered");
    return ESP_OK;
}
