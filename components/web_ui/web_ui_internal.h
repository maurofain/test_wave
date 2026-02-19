#pragma once

#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "web_ui_profile.h"
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

// Pagine e handler esposti per la registrazione (moduli runtime + test)
esp_err_t root_get_handler(httpd_req_t *req);
esp_err_t config_page_handler(httpd_req_t *req);
esp_err_t status_get_handler(httpd_req_t *req);
esp_err_t stats_page_handler(httpd_req_t *req);
esp_err_t tasks_page_handler(httpd_req_t *req);
esp_err_t test_page_handler(httpd_req_t *req);
esp_err_t httpservices_page_handler(httpd_req_t *req);
esp_err_t emulator_page_handler(httpd_req_t *req);
esp_err_t emulator_page_handler_local(httpd_req_t *req);
esp_err_t api_emulator_coin_event(httpd_req_t *req);
esp_err_t api_emulator_program_start(httpd_req_t *req);
esp_err_t api_emulator_program_stop(httpd_req_t *req);
esp_err_t api_emulator_program_pause_toggle(httpd_req_t *req);
esp_err_t api_emulator_fsm_messages_get(httpd_req_t *req);
esp_err_t api_index_page_handler(httpd_req_t *req);

// Password UI protection helpers (web_ui_auth_emulator.c)
bool web_ui_has_valid_password(httpd_req_t *req);
esp_err_t web_ui_send_password_required(httpd_req_t *req, const char *title, const char *target);
const char *web_ui_boot_password_get(void);
esp_err_t web_ui_boot_password_set(const char *new_password);

// Program table + virtual relay handlers (web_ui_programs_page.c)
esp_err_t programs_page_handler(httpd_req_t *req);
esp_err_t api_programs_get(httpd_req_t *req);
esp_err_t api_programs_save(httpd_req_t *req);
esp_err_t api_emulator_relay_control(httpd_req_t *req);
esp_err_t api_security_password(httpd_req_t *req);

// OTA handlers
esp_err_t ota_get_handler(httpd_req_t *req);
esp_err_t ota_post_handler(httpd_req_t *req);
esp_err_t ota_upload_handler(httpd_req_t *req);

// API handlers (moduli web_ui.c + web_ui_test_api.c)
esp_err_t api_config_get(httpd_req_t *req);
esp_err_t api_config_save(httpd_req_t *req);
esp_err_t api_config_backup(httpd_req_t *req);
esp_err_t api_ui_texts_get(httpd_req_t *req);
esp_err_t api_config_reset(httpd_req_t *req);
esp_err_t api_ntp_sync(httpd_req_t *req);
esp_err_t api_tasks_get(httpd_req_t *req);
esp_err_t api_tasks_save(httpd_req_t *req);
esp_err_t api_tasks_apply(httpd_req_t *req);
esp_err_t api_test_endpoints_handler(httpd_req_t *req);
esp_err_t api_test_handler(httpd_req_t *req);
esp_err_t api_debug_usb_enumerate(httpd_req_t *req);
esp_err_t api_debug_usb_restart(httpd_req_t *req);

// Reboot handlers
esp_err_t reboot_factory_handler(httpd_req_t *req);
esp_err_t reboot_app_handler(httpd_req_t *req);
esp_err_t reboot_app_last_handler(httpd_req_t *req);
esp_err_t reboot_ota0_handler(httpd_req_t *req);
esp_err_t reboot_ota1_handler(httpd_req_t *req);

// Task handles and helpers used across web_ui files
extern TaskHandle_t s_rs232_test_handle;
extern TaskHandle_t s_rs485_test_handle;
void uart_test_task(void *arg);

// Helper spostati in web_ui_common.c e usati da più file
void ip_to_str(esp_netif_t *netif, char *out, size_t len);
esp_err_t perform_ota(const char *url);

// Profilo/UI feature flags (factory/app) per visibilità e accessibilità endpoint
bool web_ui_feature_enabled(web_ui_feature_t feature);
const char *web_ui_profile_view_label(void);

// Error handler esposto per la registrazione (moved to pages)
esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t error);
