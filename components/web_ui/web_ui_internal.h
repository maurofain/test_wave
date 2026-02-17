#pragma once

#include "esp_http_server.h"
#include "esp_err.h"
#include <stdbool.h>

// Helper interni esportati tra i file di web_ui (NON API pubblica)
esp_err_t send_head(httpd_req_t *req, const char *title, const char *extra_style, bool show_nav);

// Asset/handler spostato nel modulo comune
esp_err_t logo_get_handler(httpd_req_t *req);

// Gestori log (spostati in components/web_ui/web_ui_logs.c)
esp_err_t api_logs_get(httpd_req_t *req);
esp_err_t api_logs_receive(httpd_req_t *req);
esp_err_t api_logs_options(httpd_req_t *req);
esp_err_t api_logs_levels_get(httpd_req_t *req);  // GET /api/logs/levels
esp_err_t api_logs_set_level(httpd_req_t *req);    // POST /api/logs/level
esp_err_t logs_page_handler(httpd_req_t *req);

// Helper di registrazione (usato da web_ui_server.c)
esp_err_t web_ui_register_handlers(httpd_handle_t server);
