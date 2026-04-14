#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "component_status.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HTTP_SERVICES_TEXT_LEN_SHORT 64
#define HTTP_SERVICES_TEXT_LEN_MEDIUM 128
#define HTTP_SERVICES_TEXT_LEN_LONG 256
#define HTTP_SERVICES_MAX_FILES 32
#define HTTP_SERVICES_MAX_ACTIVITIES 16

typedef enum {
	HTTP_SERVICES_PAID_SERVICE_CWASH = 0,
	HTTP_SERVICES_PAID_SERVICE_CGATE,
	HTTP_SERVICES_PAID_SERVICE_CLANE,
	HTTP_SERVICES_PAID_SERVICE_CCLEAN,
} http_services_paid_service_code_t;

typedef enum {
	HTTP_SERVICES_PAYMENT_TYPE_CASH = 0,
	HTTP_SERVICES_PAYMENT_TYPE_CREC,
	HTTP_SERVICES_PAYMENT_TYPE_BCOIN,
	HTTP_SERVICES_PAYMENT_TYPE_COIN,
	HTTP_SERVICES_PAYMENT_TYPE_SATI,
	HTTP_SERVICES_PAYMENT_TYPE_VOUC,
	HTTP_SERVICES_PAYMENT_TYPE_CASHL,
} http_services_payment_type_t;

typedef struct {
	bool iserror;
	int32_t codeerror;
	char deserror[HTTP_SERVICES_TEXT_LEN_MEDIUM];
} http_services_common_response_t;

typedef struct {
	int32_t activityid;
	char code[HTTP_SERVICES_TEXT_LEN_SHORT];
	char parameters[HTTP_SERVICES_TEXT_LEN_LONG];
} http_services_activity_t;

typedef struct {
	http_services_common_response_t common;
	char datetime[40];
	size_t activity_count;
	http_services_activity_t activities[HTTP_SERVICES_MAX_ACTIVITIES];
} http_services_keepalive_response_t;

typedef struct {
	http_services_common_response_t common;
	size_t file_count;
	char files[HTTP_SERVICES_MAX_FILES][HTTP_SERVICES_TEXT_LEN_SHORT];
	char server[HTTP_SERVICES_TEXT_LEN_MEDIUM];
	char user[HTTP_SERVICES_TEXT_LEN_SHORT];
	char password[HTTP_SERVICES_TEXT_LEN_SHORT];
	char path[HTTP_SERVICES_TEXT_LEN_MEDIUM];
} http_services_filelist_response_t;

typedef struct {
	http_services_common_response_t common;
	char datetime[40];
	int32_t paymentid;
} http_services_payment_response_t;

typedef struct {
	http_services_common_response_t common;
	char datetime[40];
} http_services_serviceused_response_t;

typedef struct {
	bool valid;
	char code[HTTP_SERVICES_TEXT_LEN_SHORT];
	char telephone[HTTP_SERVICES_TEXT_LEN_SHORT];
	char email[HTTP_SERVICES_TEXT_LEN_MEDIUM];
	char name[HTTP_SERVICES_TEXT_LEN_SHORT];
	char surname[HTTP_SERVICES_TEXT_LEN_SHORT];
	int32_t amount;
	bool is_new;
} http_services_customer_t;

typedef struct {
	http_services_common_response_t common;
	size_t customer_count;
	http_services_customer_t customers[HTTP_SERVICES_MAX_FILES];
} http_services_getcustomers_response_t;

typedef struct {
	bool valid;
	char code[HTTP_SERVICES_TEXT_LEN_SHORT];
	char username[HTTP_SERVICES_TEXT_LEN_SHORT];
	char pin[HTTP_SERVICES_TEXT_LEN_SHORT];
	char password[HTTP_SERVICES_TEXT_LEN_SHORT];
	char operativity[HTTP_SERVICES_TEXT_LEN_SHORT];
} http_services_operator_t;

typedef struct {
	http_services_common_response_t common;
	size_t operator_count;
	http_services_operator_t operators[HTTP_SERVICES_MAX_FILES];
} http_services_getoperators_response_t;

const char *http_services_paid_service_code_to_string(http_services_paid_service_code_t code);
http_services_paid_service_code_t http_services_paid_service_code_from_string(const char *code_or_label);
const char *http_services_payment_type_to_string(http_services_payment_type_t type);

/**
 * Register HTTP API handlers implemented by the http_services component.
 * Provides proxy handlers for POST routes under /api/ used by cloud services.
 */
esp_err_t http_services_register_handlers(httpd_handle_t server);

/**
 * @brief Indica se il server remoto è abilitato da configurazione runtime.
 */
bool http_services_is_remote_enabled(void);

/**
 * @brief Indica se il server remoto risulta ONLINE (ultimo login/chiamata OK).
 */
bool http_services_is_remote_online(void);

/**
 * @brief Indica se è presente un token JWT remoto in memoria.
 */
bool http_services_has_auth_token(void);

/**
 * @brief Restituisce lo stato sintetico del componente HTTP services.
 */
device_component_status_t http_services_get_component_status(void);

/**
 * @brief Notifica al componente HTTP services un cambio di disponibilita' rete.
 *
 * Quando available e' false il componente passa immediatamente OFFLINE senza
 * attendere il prossimo keepalive o errore HTTP.
 */
void http_services_notify_network_available(bool available, const char *reason);

/**
 * @brief Sincronizza lo stato runtime HTTP services con la config corrente.
 *
 * Quando force_login è true, invalida il token corrente e forza un login remoto
 * se il server è abilitato.
 */
esp_err_t http_services_sync_runtime_state(bool force_login);

/**
 * Call remote /api/getcustomers and parse the response.
 *
 * @param code       Customer code to look up, or "*" for all. Must not be NULL.
 * @param telephone  Optional telephone filter — pass empty string "" if unused.
 * @param out        Output struct filled on success. Must not be NULL.
 * @return ESP_OK on HTTP 200 + valid JSON, ESP_ERR_* otherwise.
 */
esp_err_t http_services_getcustomers(const char *code, const char *telephone,
                                     http_services_getcustomers_response_t *out);

/**
 * Call remote /api/payment with payload generated from runtime context.
 *
 * @param customer      Optional customer context (NULL -> empty customer fields).
 * @param amount        Payment amount (>= 0).
 * @param service_code  Service code normalizzato secondo la specifica server.
 * @param payment_type  Payment type codificato secondo la specifica server.
 * @param out           Output response struct. Must not be NULL.
 * @return ESP_OK on HTTP 200 + valid JSON response, ESP_ERR_* otherwise.
 */
esp_err_t http_services_payment(const http_services_customer_t *customer,
								int32_t amount,
								http_services_paid_service_code_t service_code,
								http_services_payment_type_t payment_type,
								http_services_payment_response_t *out);

/**
 * Call remote /api/keepalive with current machine status.
 *
 * @param status        Device status string (fallback: "OK").
 * @param inputstates   Input bitmap string.
 * @param outputstates  Output bitmap string.
 * @param temperature   Temperature in hundredths.
 * @param humidity      Humidity in hundredths.
 * @param out           Output response struct. Must not be NULL.
 * @return ESP_OK on HTTP 200 + valid JSON response, ESP_ERR_* otherwise.
 */
esp_err_t http_services_keepalive(const char *status,
                                  const char *inputstates,
                                  const char *outputstates,
                                  int32_t temperature,
                                  int32_t humidity,
                                  http_services_keepalive_response_t *out);
