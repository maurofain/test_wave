#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * Register HTTP API handlers implemented by the http_services component.
 * Currently provides: POST /api/login
 */
esp_err_t http_services_register_handlers(httpd_handle_t server);
