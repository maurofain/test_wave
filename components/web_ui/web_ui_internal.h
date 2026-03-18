#pragma once

#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "web_ui_profile.h"
#include "webpages_embedded.h"
#include <stdbool.h>

#ifndef WEB_UI_USE_EMBEDDED_PAGES
#define WEB_UI_USE_EMBEDDED_PAGES 0
#endif

/* I18n compact record stored in PSRAM
 * fields: numeric ids for scope/key/section and fixed-size text (32 bytes incl. NUL)
 */
typedef struct {
	uint8_t scope_id;
	uint16_t key_id;
	uint8_t section;
	char text[32];
} i18n_record_t;

/**
 * @file web_ui_internal.h
 * @brief Dichiarazioni interne condivise tra i file del componente Web UI
 *
 * Contiene prototipi non destinati all'uso esterno; la maggior parte delle
 * funzioni sono implementate in altri .c e servono a mantenere il codice
 * organizzato.
 */

// Helper interni esportati tra i file di web_ui (NON API pubblica)
/**
 * @brief Invia l'intestazione HTML standard per le pagine Web UI
 *
 * Gestisce head/meta, stile CSS e navigazione superiore opzionale.
 */
esp_err_t send_head(httpd_req_t *req, const char *title, const char *extra_style, bool show_nav);

// Asset/handler spostato nel modulo comune
esp_err_t logo_get_handler(httpd_req_t *req);

// Gestori log (spostati in components/web_ui/web_ui_logs.c)
esp_err_t api_logs_get(httpd_req_t *req);
esp_err_t api_logs_receive(httpd_req_t *req);
esp_err_t api_logs_options(httpd_req_t *req);
esp_err_t api_logs_levels_get(httpd_req_t *req);  // GET /api/logs/levels
esp_err_t api_logs_set_level(httpd_req_t *req);    // POST /api/logs/level
esp_err_t api_logs_network_get(httpd_req_t *req);  // GET /api/logs/network
esp_err_t api_logs_network_set(httpd_req_t *req);  // POST /api/logs/network
esp_err_t logs_page_handler(httpd_req_t *req);

// Helper di registrazione (usato da web_ui_server.c)
esp_err_t web_ui_register_handlers(httpd_handle_t server);
bool web_ui_is_running(void);

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
esp_err_t files_page_handler(httpd_req_t *req);
esp_err_t api_files_list_get(httpd_req_t *req);
esp_err_t api_files_upload_post(httpd_req_t *req);
esp_err_t api_files_delete_post(httpd_req_t *req);
esp_err_t api_files_download_get(httpd_req_t *req);
esp_err_t api_files_copy_post(httpd_req_t *req);

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
esp_err_t api_cctalk_log_txrx_get(httpd_req_t *req);
esp_err_t api_cctalk_log_txrx_set(httpd_req_t *req);
esp_err_t api_runtime_enable_api_log_get(httpd_req_t *req);
esp_err_t api_runtime_enable_api_log_post(httpd_req_t *req);
esp_err_t api_ui_texts_get(httpd_req_t *req);
esp_err_t api_ui_languages_get(httpd_req_t *req);
esp_err_t api_config_reset(httpd_req_t *req);
esp_err_t api_ntp_sync(httpd_req_t *req);
esp_err_t api_tasks_get(httpd_req_t *req);
esp_err_t api_tasks_save(httpd_req_t *req);
esp_err_t api_tasks_apply(httpd_req_t *req);
esp_err_t api_test_endpoints_handler(httpd_req_t *req);
esp_err_t api_test_handler(httpd_req_t *req);
esp_err_t api_debug_usb_enumerate(httpd_req_t *req);
esp_err_t api_debug_usb_restart(httpd_req_t *req);
esp_err_t api_debug_crash(httpd_req_t *req);
esp_err_t api_debug_restore(httpd_req_t *req);
esp_err_t api_debug_promote_factory(httpd_req_t *req);
esp_err_t api_display_lvgl_test(httpd_req_t *req);

// Reboot handlers
esp_err_t reboot_factory_handler(httpd_req_t *req);
esp_err_t reboot_app_handler(httpd_req_t *req);
esp_err_t reboot_app_last_handler(httpd_req_t *req);
esp_err_t reboot_ota0_handler(httpd_req_t *req);
esp_err_t reboot_ota1_handler(httpd_req_t *req);
esp_err_t maintainer_enable_handler(httpd_req_t *req);

// Task handles and helpers used across web_ui files
extern TaskHandle_t s_rs232_test_handle;
extern TaskHandle_t s_rs485_test_handle;
void uart_test_task(void *arg);

// Test page handlers
esp_err_t test_page_handler(httpd_req_t *req);
esp_err_t led_bar_test_page_handler(httpd_req_t *req);

// Helper spostati in web_ui_common.c e usati da più file
void ip_to_str(esp_netif_t *netif, char *out, size_t len);
esp_err_t perform_ota(const char *url);

// Gestione pagine HTML/JS su filesystem (SPIFFS/SD) + helper external-only
esp_err_t webpages_bootstrap(void);
esp_err_t webpages_try_send_external(httpd_req_t *req, const char *relative_path, const char *content_type);
esp_err_t webpages_send_external_or_error(httpd_req_t *req, const char *relative_path, const char *content_type);
const char *webpages_source_name(void);
void webpages_localized_cache_invalidate(void);

// Profilo/UI feature flags (factory/app) per visibilità e accessibilità endpoint
bool web_ui_feature_enabled(web_ui_feature_t feature);
const char *web_ui_profile_view_label(void);
void web_ui_factory_features_override_set(bool enabled);
bool web_ui_factory_features_override_get(void);
void web_ui_i18n_cache_invalidate(void);

/* Load full i18n dictionary for `language` into PSRAM. Returns pointer to
 * an array of `i18n_record_t` and fills `out_count`. Caller must free with
 * `i18n_free_dictionary_psram()`.
 */
i18n_record_t *i18n_load_full_dictionary_psram(const char *language, size_t *out_count);
void i18n_free_dictionary_psram(i18n_record_t *records);

/* Concatenate sections from PSRAM-loaded dictionary for a given numeric ids.
 * Returns a heap-allocated string (malloc) or NULL. Caller frees with free().
 */
char *i18n_concat_from_psram(uint8_t scope_id, uint16_t key_id);

/* Load language dictionary into PSRAM and keep it resident until invalidated.
 * Returns ESP_OK on success. Freed by `web_ui_i18n_cache_invalidate()`.
 */
esp_err_t web_ui_i18n_load_language_psram(const char *language);

// Error handler esposto per la registrazione (moved to pages)
esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t error);
