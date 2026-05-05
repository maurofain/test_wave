#include "tasks.h"
#include "init.h"
#include "esp_log.h"
#include "esp_system.h"
#include "led_strip.h"
#include "led.h"
#include "device_config.h"
#include "digital_io.h"
#include "audio_player.h"
#include "sht40.h"
#include "pwm.h"
#include "esp_lcd_touch.h"
#include "web_ui.h"
#include "web_ui_programs.h"
#include "fsm.h"
#include "lvgl_panel.h"
#include "usb_cdc_scanner.h" // Scanner QR
#include "rs232.h"
#include "rs232_epaper.h"
#include "cctalk.h"          // cctalk_driver_init + cctalk_task_run
#include "mdb.h"             // mdb_init + mdb_engine_run
#include "mdb_cashless.h"
#include "sd_card.h"         // sd_card_init_monitor + sd_card_monitor_run
#include "http_services.h"
#include "modbus_relay.h"
#include "freertos/idf_additions.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include "cJSON.h"

#ifndef DNA_LED_STRIP
#define DNA_LED_STRIP 0
#endif
#ifndef DNA_SHT40
#define DNA_SHT40 0
#endif
#ifndef DNA_IO_EXPANDER
#define DNA_IO_EXPANDER 0
#endif

static const char *TAG = "TASKS";
static float s_temperature = 0.0f;
static float s_humidity = 0.0f;
static const uint8_t DIGITAL_IO_FIRST_CHANNEL_ID = 1U;
static const uint32_t DIGITAL_IO_POLL_DEFAULT_MS = 50U;
static const uint8_t PROGRAM_OUTPUT_VERIFY_MAX_ATTEMPTS = 3U;
static const uint32_t PROGRAM_OUTPUT_VERIFY_WAIT_MS = 100U;
static volatile bool s_program_outputs_verify_error_active = false;

#define SCANNER_QR_COOLDOWN_DEFAULT_MS 10000U
#define SCANNER_QR_COOLDOWN_MIN_MS 500U
#define SCANNER_QR_COOLDOWN_MAX_MS 60000U
#define FSM_LANGUAGE_RETURN_DEFAULT_MS 10000U
#define FSM_LANGUAGE_RETURN_MIN_MS 1000U
#define FSM_LANGUAGE_RETURN_MAX_MS 600000U
static volatile bool s_scanner_cooldown_active = false;
static volatile bool s_scanner_reenable_pending = false;
static volatile TickType_t s_scanner_cooldown_until = 0;
static volatile uint32_t s_scanner_cooldown_active_ms = SCANNER_QR_COOLDOWN_DEFAULT_MS;
static volatile uint32_t s_scanner_reenable_attempts = 0;
static http_services_customer_t s_last_customer = {0};
static bool s_last_customer_available = false;

#define OOS_REASON_KEY_LEN 64
#define OOS_REASON_FALLBACK_LEN 128
#define OOS_HEALTH_CHECK_MS 1000U
#define OOS_RETRY_MS 30000U

typedef struct {
    bool valid;
    agn_id_t agent;
    char reason_key[OOS_REASON_KEY_LEN];
    char reason_fallback[OOS_REASON_FALLBACK_LEN];
} tasks_oos_cause_t;

static tasks_oos_cause_t s_oos_requested = {0};
static portMUX_TYPE s_oos_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_oos_runtime_active = false;
static volatile bool s_modbus_hard_inhibit = false;
static volatile bool s_oos_modbus_recovery_test_active = false;

static void publish_program_payment_event(const fsm_ctx_t *ctx, const fsm_input_event_t *source_event);
static http_services_payment_type_t tasks_payment_type_from_session_source(fsm_session_source_t source);
static bool tasks_publish_card_vend_result_event(bool approved, int32_t amount_cents, const char *source_tag);

static bool tasks_is_out_of_service_state(void)
{
    return s_oos_runtime_active;
}

static void tasks_set_out_of_service_runtime(bool active)
{
    s_oos_runtime_active = active;
}

static bool tasks_is_modbus_hard_inhibited(void)
{
    bool inhibited = false;
    portENTER_CRITICAL(&s_oos_lock);
    inhibited = s_modbus_hard_inhibit;
    portEXIT_CRITICAL(&s_oos_lock);
    return inhibited;
}

static void tasks_set_modbus_hard_inhibit(bool inhibit)
{
    portENTER_CRITICAL(&s_oos_lock);
    s_modbus_hard_inhibit = inhibit;
    if (!inhibit) {
        s_oos_modbus_recovery_test_active = false;
    }
    portEXIT_CRITICAL(&s_oos_lock);
}

static bool tasks_is_modbus_recovery_test_active(void)
{
    bool active = false;
    portENTER_CRITICAL(&s_oos_lock);
    active = s_oos_modbus_recovery_test_active;
    portEXIT_CRITICAL(&s_oos_lock);
    return active;
}

static void tasks_set_modbus_recovery_test_active(bool active)
{
    portENTER_CRITICAL(&s_oos_lock);
    s_oos_modbus_recovery_test_active = active;
    if (active) {
        s_modbus_hard_inhibit = true;
    }
    portEXIT_CRITICAL(&s_oos_lock);
}

static bool tasks_is_program_active(void)
{
    fsm_ctx_t snap = {0};
    if (!fsm_runtime_snapshot(&snap)) {
        return false;
    }
    return (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED);
}

static bool tasks_is_modbus_runtime_blocked(void)
{
    return tasks_is_out_of_service_state() ||
           tasks_is_modbus_hard_inhibited() ||
           tasks_is_program_active();
}

bool digital_io_modbus_runtime_allowed(void)
{
    return !tasks_is_modbus_runtime_blocked();
}

bool usb_cdc_scanner_runtime_allowed(void)
{
    return !tasks_is_out_of_service_state() &&
           !tasks_is_program_active();
}

static const char *tasks_agent_name(agn_id_t agent)
{
    switch (agent) {
        case AGN_ID_RS485: return "RS485";
        case AGN_ID_HTTP_SERVICES: return "HTTP_SERVICES";
        case AGN_ID_CCTALK: return "CCTALK";
        case AGN_ID_USB_CDC_SCANNER: return "USB_SCANNER";
        case AGN_ID_AUDIO: return "AUDIO";
        case AGN_ID_FSM: return "FSM";
        default: return "UNKNOWN";
    }
}

static void tasks_oos_set(tasks_oos_cause_t *dst,
                          agn_id_t agent,
                          const char *reason_key,
                          const char *reason_fallback)
{
    if (!dst) {
        return;
    }
    dst->valid = true;
    dst->agent = agent;
    snprintf(dst->reason_key,
             sizeof(dst->reason_key),
             "%s",
             (reason_key && reason_key[0] != '\0') ? reason_key : "out_of_service_reason_generic");
    snprintf(dst->reason_fallback,
             sizeof(dst->reason_fallback),
             "%s",
             (reason_fallback && reason_fallback[0] != '\0') ? reason_fallback : "Errore sistema");
}

static bool tasks_oos_take_requested(tasks_oos_cause_t *out)
{
    bool has = false;
    portENTER_CRITICAL(&s_oos_lock);
    if (s_oos_requested.valid) {
        if (out) {
            *out = s_oos_requested;
        }
        s_oos_requested.valid = false;
        has = true;
    }
    portEXIT_CRITICAL(&s_oos_lock);
    return has;
}

static void tasks_oos_clear_requested(void)
{
    portENTER_CRITICAL(&s_oos_lock);
    memset(&s_oos_requested, 0, sizeof(s_oos_requested));
    portEXIT_CRITICAL(&s_oos_lock);
}

void tasks_request_out_of_service(agn_id_t agent_id,
                                  const char *reason_key,
                                  const char *reason_fallback)
{
    portENTER_CRITICAL(&s_oos_lock);
    tasks_oos_set(&s_oos_requested, agent_id, reason_key, reason_fallback);
    if (agent_id == AGN_ID_RS485) {
        s_modbus_hard_inhibit = true;
    }
    portEXIT_CRITICAL(&s_oos_lock);
}

static bool tasks_netif_has_ip(esp_netif_t *netif)
{
    if (!netif) {
        return false;
    }

    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return false;
    }

    return (ip_info.ip.addr != 0U);
}

static bool tasks_network_available(void)
{
    esp_netif_t *ap = NULL;
    esp_netif_t *sta = NULL;
    esp_netif_t *eth = NULL;
    init_get_netifs(&ap, &sta, &eth);
    (void)ap;
    return tasks_netif_has_ip(sta) || tasks_netif_has_ip(eth);
}

static bool tasks_probe_modbus_bootstrap(const device_config_t *cfg)
{
    if (!cfg) {
        return false;
    }

    if (!(cfg->sensors.rs485_enabled && cfg->modbus.enabled)) {
        return true;
    }

    bool probe_ok = false;
    tasks_set_modbus_recovery_test_active(true);

    if (modbus_relay_init() != ESP_OK) {
        goto done;
    }

    uint8_t bits[MODBUS_RELAY_MAX_BYTES] = {0};
    uint16_t relay_count = cfg->modbus.relay_count;
    uint16_t input_count = cfg->modbus.input_count;
    if (relay_count > MODBUS_RELAY_MAX_POINTS) {
        relay_count = MODBUS_RELAY_MAX_POINTS;
    }
    if (input_count > MODBUS_RELAY_MAX_POINTS) {
        input_count = MODBUS_RELAY_MAX_POINTS;
    }

    ESP_LOGD(TAG,
             "[M] Health Modbus probe: slave=%u relay_count=%u input_count=%u",
             (unsigned)cfg->modbus.slave_id,
             (unsigned)relay_count,
             (unsigned)input_count);

    if (relay_count > 0) {
        if (modbus_relay_read_coils(cfg->modbus.slave_id,
                                    cfg->modbus.relay_start,
                                    relay_count,
                                    bits,
                                    sizeof(bits)) != ESP_OK) {
            goto done;
        }
        ESP_LOGD(TAG,
                 "[M] Health Modbus lettura relay OK (start=%u count=%u)",
                 (unsigned)cfg->modbus.relay_start,
                 (unsigned)relay_count);
    }

    if (input_count > 0) {
        if (modbus_relay_read_discrete_inputs(cfg->modbus.slave_id,
                                              cfg->modbus.input_start,
                                              input_count,
                                              bits,
                                              sizeof(bits)) != ESP_OK) {
            goto done;
        }
        ESP_LOGD(TAG,
                 "[M] Health Modbus lettura input OK (start=%u count=%u)",
                 (unsigned)cfg->modbus.input_start,
                 (unsigned)input_count);
    }

    probe_ok = true;

done:
    tasks_set_modbus_recovery_test_active(false);
    if (probe_ok) {
        tasks_set_modbus_hard_inhibit(false);
    } else {
        tasks_set_modbus_hard_inhibit(true);
    }
    return probe_ok;
}

static bool tasks_health_check(agn_id_t focus_agent, tasks_oos_cause_t *out_cause)
{
    const device_config_t *cfg = device_config_get();
    if (!cfg) {
        return false;
    }

    if ((focus_agent == AGN_ID_NONE || focus_agent == AGN_ID_RS485) &&
        (cfg->sensors.rs485_enabled && cfg->modbus.enabled)) {
        if (!tasks_probe_modbus_bootstrap(cfg)) {
            init_agent_status_set(AGN_ID_RS485, 0, INIT_AGENT_ERR_RUNTIME_FAILED);
            tasks_oos_set(out_cause,
                          AGN_ID_RS485,
                          "out_of_service_reason_modbus",
                          "Errore ModBus");
            return true;
        }
        init_agent_status_set(AGN_ID_RS485, 1, INIT_AGENT_ERR_NONE);
    }

    if ((focus_agent == AGN_ID_NONE || focus_agent == AGN_ID_CCTALK || focus_agent == AGN_ID_USB_CDC_SCANNER)) {
        bool cctalk_ok = cfg->sensors.cctalk_enabled && cctalk_driver_is_acceptor_enabled() && cctalk_driver_is_acceptor_online();
        bool scanner_ok = cfg->scanner.enabled && usb_cdc_scanner_is_connected();
        if (!cctalk_ok && !scanner_ok) {
            init_agent_status_set(AGN_ID_CCTALK, 0, INIT_AGENT_ERR_RUNTIME_FAILED);
            init_agent_status_set(AGN_ID_USB_CDC_SCANNER, 0, INIT_AGENT_ERR_RUNTIME_FAILED);
            tasks_oos_set(out_cause,
                          AGN_ID_CCTALK,
                          "out_of_service_reason_credit_systems",
                          "Nessun sistema di acquisizione credito attivo");
            return true;
        }

        if (!cfg->sensors.cctalk_enabled) {
            init_agent_status_set(AGN_ID_CCTALK, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
        } else {
            init_agent_status_set(AGN_ID_CCTALK,
                                  cctalk_ok ? 1 : 0,
                                  cctalk_ok ? INIT_AGENT_ERR_NONE : INIT_AGENT_ERR_RUNTIME_FAILED);
        }

        if (!cfg->scanner.enabled) {
            init_agent_status_set(AGN_ID_USB_CDC_SCANNER, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
        } else {
            init_agent_status_set(AGN_ID_USB_CDC_SCANNER,
                                  scanner_ok ? 1 : 0,
                                  scanner_ok ? INIT_AGENT_ERR_NONE : INIT_AGENT_ERR_RUNTIME_FAILED);
        }
    }

    if (focus_agent == AGN_ID_NONE || focus_agent == AGN_ID_HTTP_SERVICES) {
        if (http_services_is_remote_enabled()) {
            if (!tasks_network_available()) {
#if DNA_HTTP_SERVICES_NON_BLOCKING
                init_agent_status_set(AGN_ID_HTTP_SERVICES, 1, INIT_AGENT_ERR_NETWORK_NO_IP);
                ESP_LOGW(TAG, "[M] [HTTP_SVC] Rete non disponibile ma non bloccante");
#else
                init_agent_status_set(AGN_ID_HTTP_SERVICES, 0, INIT_AGENT_ERR_NETWORK_NO_IP);
                tasks_oos_set(out_cause,
                              AGN_ID_HTTP_SERVICES,
                              "out_of_service_reason_network_down",
                              "Rete non disponibile");
                return true;
#endif
            }

            esp_err_t sync_err = http_services_sync_runtime_state(false);
            if (sync_err != ESP_OK || !http_services_has_auth_token()) {
#if DNA_HTTP_SERVICES_NON_BLOCKING
                init_agent_status_set(AGN_ID_HTTP_SERVICES, 1, INIT_AGENT_ERR_REMOTE_LOGIN_FAILED);
                ESP_LOGW(TAG, "[M] [HTTP_SVC] Token remoto non disponibile ma non bloccante");
#else
                init_agent_status_set(AGN_ID_HTTP_SERVICES, 0, INIT_AGENT_ERR_REMOTE_LOGIN_FAILED);
                tasks_oos_set(out_cause,
                              AGN_ID_HTTP_SERVICES,
                              "out_of_service_reason_remote_token",
                              "Errore richiesta token remoto");
                return true;
#endif
            }

            if (!http_services_is_remote_online()) {
#if DNA_HTTP_SERVICES_NON_BLOCKING
                init_agent_status_set(AGN_ID_HTTP_SERVICES, 1, INIT_AGENT_ERR_RUNTIME_FAILED);
                ESP_LOGW(TAG, "[M] [HTTP_SVC] Server remoto non raggiungibile ma non bloccante");
#else
                init_agent_status_set(AGN_ID_HTTP_SERVICES, 0, INIT_AGENT_ERR_RUNTIME_FAILED);
                tasks_oos_set(out_cause,
                              AGN_ID_HTTP_SERVICES,
                              "out_of_service_reason_remote_unreachable",
                              "Server remoto non raggiungibile");
                return true;
#endif
            }

            init_agent_status_set(AGN_ID_HTTP_SERVICES, 1, INIT_AGENT_ERR_NONE);
        } else {
            init_agent_status_set(AGN_ID_HTTP_SERVICES, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
        }
    }

    return false;
}

typedef struct {
    TaskHandle_t requester_task;
    esp_err_t result;
    bool bool_value;
    digital_io_snapshot_t snapshot;
    bool completed;
    bool abandoned;
} tasks_digital_io_agent_result_t;

static portMUX_TYPE s_digital_io_agent_lock = portMUX_INITIALIZER_UNLOCKED;

const char *tasks_err_to_name(esp_err_t err)
{
    switch (err) {
        case TASKS_ERR_FSM_QUEUE_NOT_READY:
            return "TASKS_ERR_FSM_QUEUE_NOT_READY";
        case TASKS_ERR_PROGRAM_DISABLED:
            return "TASKS_ERR_PROGRAM_DISABLED";
        case TASKS_ERR_FSM_SNAPSHOT_UNAVAILABLE:
            return "TASKS_ERR_FSM_SNAPSHOT_UNAVAILABLE";
        case TASKS_ERR_PROGRAM_STATE_CONFLICT:
            return "TASKS_ERR_PROGRAM_STATE_CONFLICT";
        case TASKS_ERR_PROGRAM_CREDIT_INSUFFICIENT:
            return "TASKS_ERR_PROGRAM_CREDIT_INSUFFICIENT";
        default:
            return esp_err_to_name(err);
    }
}

bool tasks_program_outputs_verify_error_active(void)
{
    return s_program_outputs_verify_error_active;
}

static bool tasks_is_fsm_context(void)
{
    const char *task_name = pcTaskGetName(NULL);
    return (task_name != NULL && strcmp(task_name, "fsm") == 0);
}

static bool tasks_publish_io_process_event(agn_id_t sender,
                                           action_id_t action,
                                           uint32_t value_u32,
                                           uint32_t aux_u32,
                                           const char *text)
{
    if (!fsm_event_queue_init(0)) {
        return false;
    }

    fsm_input_event_t event = {
        .from = sender,
        .to = {AGN_ID_IO_PROCESS},
        .action = action,
        .type = FSM_INPUT_EVENT_NONE,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = value_u32,
        .aux_u32 = aux_u32,
        .data_ptr = NULL,
        .text = {0},
    };

    if (text && text[0] != '\0') {
        snprintf(event.text, sizeof(event.text), "%s", text);
    }

    return fsm_event_publish(&event, pdMS_TO_TICKS(20));
}

static bool tasks_publish_cctalk_control_event(action_id_t action)
{
    if (action != ACTION_ID_CCTALK_START && action != ACTION_ID_CCTALK_STOP) {
        return false;
    }

    if (!fsm_event_queue_init(0)) {
        return false;
    }

    fsm_input_event_t event = {
        .from = AGN_ID_FSM,
        .to = {AGN_ID_CCTALK},
        .action = action,
        .type = FSM_INPUT_EVENT_NONE,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = 0,
        .aux_u32 = 0,
        .data_ptr = NULL,
        .text = {0},
    };

    return fsm_event_publish(&event, pdMS_TO_TICKS(30));
}

static uint8_t tasks_find_program_id_for_input(uint8_t input_id)
{
    const device_config_t *cfg = device_config_get();
    if (!cfg || !digital_io_input_is_touch_mappable(input_id)) {
        return 0U;
    }

    size_t button_limit = cfg->num_programs;
    if (button_limit == 0U || button_limit > DEVICE_TOUCH_BUTTON_MAX) {
        button_limit = DEVICE_TOUCH_BUTTON_MAX;
    }

    uint8_t matched_program_id = 0U;
    for (size_t button_index = 0; button_index < button_limit; ++button_index) {
        uint8_t mapped_input = cfg->touch_button_map.button_to_input[button_index];
        if (mapped_input != input_id) {
            continue;
        }

        if (matched_program_id != 0U) {
            ESP_LOGW(TAG,
                     "[M] IN%02u associato a piu' programmi (%u e %u): input ignorato",
                     (unsigned)input_id,
                     (unsigned)matched_program_id,
                     (unsigned)(button_index + 1U));
            return 0U;
        }

        matched_program_id = (uint8_t)(button_index + 1U);
    }

    return matched_program_id;
}

static void tasks_display_epaper_program_status(const web_ui_program_entry_t *entry,
                                               bool is_active_program);

static void tasks_process_other_digital_input_rising(uint8_t input_id)
{
    char input_code[24] = {0};
    (void)digital_io_input_get_code(input_id, input_code, sizeof(input_code));

    if (!tasks_publish_io_process_event(AGN_ID_DIGITAL_IO,
                                        ACTION_ID_DIGITAL_IO_INPUT_RISING,
                                        (uint32_t)input_id,
                                        0U,
                                        input_code)) {
        ESP_LOGW(TAG,
                 "[M] Publish verso io_process fallito per ingresso %s",
                 input_code[0] != '\0' ? input_code : "digital_input");
        return;
    }

    ESP_LOGI(TAG,
             "[M] Fronte 0->1 su %s inoltrato a io_process",
             input_code[0] != '\0' ? input_code : "digital_input");
}

esp_err_t tasks_publish_program_button_action(uint8_t program_id, agn_id_t sender)
{
    if (program_id == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!fsm_event_queue_init(0)) {
        return TASKS_ERR_FSM_QUEUE_NOT_READY;
    }

    const web_ui_program_entry_t *entry = web_ui_program_find_by_id(program_id);
    if (!entry) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!entry->enabled) {
        return TASKS_ERR_PROGRAM_DISABLED;
    }

    fsm_ctx_t snap = {0};
    if (!fsm_runtime_snapshot(&snap)) {
        return TASKS_ERR_FSM_SNAPSHOT_UNAVAILABLE;
    }

    fsm_input_event_t event = {
        .from = sender,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_NONE,
        .type = FSM_INPUT_EVENT_NONE,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = 0,
        .aux_u32 = 0,
        .data_ptr = NULL,
        .text = {0},
    };

    snprintf(event.text, sizeof(event.text), "%s", entry->name);

    bool is_running_or_paused = (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED);
    bool is_active_program = is_running_or_paused &&
                             entry->name[0] != '\0' &&
                             strcmp(entry->name, snap.running_program_name) == 0;
    int32_t effective_credit = (snap.credit_cents > 0)
                                   ? snap.credit_cents
                                   : (snap.ecd_coins + snap.vcd_coins);

    if (is_active_program) {
        event.action = ACTION_ID_PROGRAM_PAUSE_TOGGLE;
        event.type = FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE;
        event.value_i32 = (int32_t)program_id;
    } else {
        if (snap.state != FSM_STATE_CREDIT && !is_running_or_paused) {
            return TASKS_ERR_PROGRAM_STATE_CONFLICT;
        }
        if ((snap.state == FSM_STATE_CREDIT || is_running_or_paused) &&
            effective_credit < (int32_t)entry->price_units) {
            return TASKS_ERR_PROGRAM_CREDIT_INSUFFICIENT;
        }

        event.action = ACTION_ID_PROGRAM_SELECTED;
        event.type = is_running_or_paused ? FSM_INPUT_EVENT_PROGRAM_SWITCH
                                          : FSM_INPUT_EVENT_PROGRAM_SELECTED;
        event.value_i32 = (int32_t)entry->price_units;
        event.value_u32 = (uint32_t)entry->pause_max_suspend_sec * 1000U;
        event.aux_u32 = (uint32_t)entry->duration_sec * 1000U;
    }

    if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
        return ESP_ERR_TIMEOUT;
    }

    if (device_config_get()->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232) {
        tasks_display_epaper_program_status(entry, is_active_program);
    }

    return ESP_OK;
}

static esp_err_t tasks_execute_digital_io_action(action_id_t action,
                                                 uint32_t value_u32,
                                                 uint32_t aux_u32,
                                                 bool *out_bool,
                                                 digital_io_snapshot_t *out_snapshot)
{
    bool modbus_blocked = tasks_is_modbus_runtime_blocked();

    if (modbus_blocked) {
        uint8_t channel_id = (uint8_t)value_u32;
        bool is_modbus_output = (channel_id >= DIGITAL_IO_FIRST_MODBUS_OUTPUT) &&
                                (channel_id <= DIGITAL_IO_OUTPUT_COUNT);
        bool is_modbus_input = (channel_id >= DIGITAL_IO_FIRST_MODBUS_INPUT) &&
                               (channel_id <= DIGITAL_IO_INPUT_COUNT);

        if ((action == ACTION_ID_DIGITAL_IO_SET_OUTPUT && is_modbus_output) ||
            (action == ACTION_ID_DIGITAL_IO_GET_OUTPUT && is_modbus_output) ||
            (action == ACTION_ID_DIGITAL_IO_GET_INPUT && is_modbus_input) ||
            action == ACTION_ID_DIGITAL_IO_GET_SNAPSHOT) {
            return DIGITAL_IO_ERR_MODBUS_DISABLED;
        }
    }

    if (!modbus_blocked) {
        esp_err_t init_err = digital_io_init();
        if (init_err != ESP_OK) {
            return init_err;
        }
    }

    switch (action) {
        case ACTION_ID_DIGITAL_IO_SET_OUTPUT:
            return digital_io_set_output((uint8_t)value_u32, aux_u32 != 0U);

        case ACTION_ID_DIGITAL_IO_GET_OUTPUT:
            if (!out_bool) {
                return ESP_ERR_INVALID_ARG;
            }
            return digital_io_get_output((uint8_t)value_u32, out_bool);

        case ACTION_ID_DIGITAL_IO_GET_INPUT:
            if (!out_bool) {
                return ESP_ERR_INVALID_ARG;
            }
            return digital_io_get_input((uint8_t)value_u32, out_bool);

        case ACTION_ID_DIGITAL_IO_GET_SNAPSHOT:
            if (!out_snapshot) {
                return ESP_ERR_INVALID_ARG;
            }
            return digital_io_get_snapshot(out_snapshot);

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

static bool tasks_handle_digital_io_agent_event(const fsm_input_event_t *event)
{
    if (!event) {
        return false;
    }

    switch (event->action) {
        case ACTION_ID_DIGITAL_IO_INPUT_RISING: {
            uint8_t input_id = (uint8_t)event->value_u32;
            if (input_id < DIGITAL_IO_FIRST_CHANNEL_ID || input_id > DIGITAL_IO_INPUT_COUNT) {
                ESP_LOGW(TAG, "[M] Evento fronte digitale con input non valido: %u", (unsigned)input_id);
                return true;
            }

            uint8_t program_id = tasks_find_program_id_for_input(input_id);
            if (program_id != 0U) {
                esp_err_t publish_err = tasks_publish_program_button_action(program_id, AGN_ID_DIGITAL_IO);
                if (publish_err != ESP_OK) {
                    ESP_LOGW(TAG,
                             "[M] IN%02u -> programma %u non pubblicato: %s",
                             (unsigned)input_id,
                             (unsigned)program_id,
                             tasks_err_to_name(publish_err));
                } else {
                    ESP_LOGI(TAG,
                             "[M] IN%02u associato al programma %u: pubblicata azione equivalente al touch",
                             (unsigned)input_id,
                             (unsigned)program_id);
                }
            } else {
                tasks_process_other_digital_input_rising(input_id);
            }

            return true;
        }
        case ACTION_ID_DIGITAL_IO_SET_OUTPUT:
        case ACTION_ID_DIGITAL_IO_GET_OUTPUT:
        case ACTION_ID_DIGITAL_IO_GET_INPUT:
        case ACTION_ID_DIGITAL_IO_GET_SNAPSHOT:
            break;
        default:
            return false;
    }

    bool bool_value = false;
    digital_io_snapshot_t snapshot = {0};
    uint8_t output_id = (uint8_t)event->value_u32;
    bool output_value = (event->aux_u32 != 0U);
    esp_err_t op_err = tasks_execute_digital_io_action(event->action,
                                                         event->value_u32,
                                                         event->aux_u32,
                                                         &bool_value,
                                                         &snapshot);

    if (event->action == ACTION_ID_DIGITAL_IO_SET_OUTPUT &&
        op_err == ESP_OK &&
        digital_io_output_is_io_process_signal(output_id)) {
        char output_code[24] = {0};
        (void)digital_io_output_get_code(output_id, output_code, sizeof(output_code));
        if (!tasks_publish_io_process_event(event->from,
                                            ACTION_ID_DIGITAL_IO_SET_OUTPUT,
                                            (uint32_t)output_id,
                                            output_value ? 1U : 0U,
                                            output_code)) {
            ESP_LOGW(TAG,
                     "[M] Publish output %s verso io_process fallito",
                     output_code[0] != '\0' ? output_code : "digital_output");
        }
    }

    tasks_digital_io_agent_result_t *result = (tasks_digital_io_agent_result_t *)event->data_ptr;
    if (result) {
        TaskHandle_t requester_task = NULL;
        bool should_free = false;

        result->result = op_err;
        result->bool_value = bool_value;
        result->snapshot = snapshot;

        portENTER_CRITICAL(&s_digital_io_agent_lock);
        result->completed = true;
        should_free = result->abandoned;
        if (!should_free) {
            requester_task = result->requester_task;
        }
        portEXIT_CRITICAL(&s_digital_io_agent_lock);

        if (requester_task) {
            xTaskNotifyGive(requester_task);
        } else if (should_free) {
            free(result);
        }
    }

    return true;
}

static esp_err_t tasks_dispatch_digital_io_agent_request(action_id_t action,
                                                         uint32_t value_u32,
                                                         uint32_t aux_u32,
                                                         bool *out_bool,
                                                         digital_io_snapshot_t *out_snapshot,
                                                         TickType_t timeout_ticks)
{
    switch (action) {
        case ACTION_ID_DIGITAL_IO_SET_OUTPUT:
        case ACTION_ID_DIGITAL_IO_GET_OUTPUT:
        case ACTION_ID_DIGITAL_IO_GET_INPUT:
        case ACTION_ID_DIGITAL_IO_GET_SNAPSHOT:
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    /* Le operazioni di sola lettura non devono passare dalla mailbox FSM:
     * sotto polling web possono saturare la coda senza necessità.
     * Eseguiamo quindi GET_* direttamente sul componente digital_io. */
    if (action == ACTION_ID_DIGITAL_IO_GET_OUTPUT ||
        action == ACTION_ID_DIGITAL_IO_GET_INPUT ||
        action == ACTION_ID_DIGITAL_IO_GET_SNAPSHOT) {
        return tasks_execute_digital_io_action(action,
                                               value_u32,
                                               aux_u32,
                                               out_bool,
                                               out_snapshot);
    }

    if (tasks_is_fsm_context()) {
        return tasks_execute_digital_io_action(action,
                                               value_u32,
                                               aux_u32,
                                               out_bool,
                                               out_snapshot);
    }

    if (!fsm_event_queue_init(0)) {
        return TASKS_ERR_FSM_QUEUE_NOT_READY;
    }

    tasks_digital_io_agent_result_t *result = (tasks_digital_io_agent_result_t *)calloc(1, sizeof(*result));
    if (!result) {
        return ESP_ERR_NO_MEM;
    }

    *result = (tasks_digital_io_agent_result_t){
        .requester_task = xTaskGetCurrentTaskHandle(),
        .result = ESP_ERR_TIMEOUT,
        .bool_value = false,
        .snapshot = {0},
        .completed = false,
        .abandoned = false,
    };

    fsm_input_event_t event = {
        .from = AGN_ID_WEB_UI,
        .to = {AGN_ID_FSM},
        .action = action,
        .type = FSM_INPUT_EVENT_NONE,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = value_u32,
        .aux_u32 = aux_u32,
        .data_ptr = result,
        .text = {0},
    };

    (void)ulTaskNotifyTake(pdTRUE, 0);

    TickType_t publish_timeout = pdMS_TO_TICKS(20);
    if (!fsm_event_publish(&event, publish_timeout)) {
        free(result);
        return ESP_ERR_TIMEOUT;
    }

    TickType_t wait_timeout = (timeout_ticks > 0) ? timeout_ticks : pdMS_TO_TICKS(200);
    if (ulTaskNotifyTake(pdTRUE, wait_timeout) == 0) {
        bool completed = false;

        portENTER_CRITICAL(&s_digital_io_agent_lock);
        completed = result->completed;
        if (!completed) {
            result->abandoned = true;
            result->requester_task = NULL;
        }
        portEXIT_CRITICAL(&s_digital_io_agent_lock);

        if (!completed) {
            return ESP_ERR_TIMEOUT;
        }

        (void)ulTaskNotifyTake(pdTRUE, 0);
    }

    esp_err_t final_result = result->result;
    if (final_result == ESP_OK) {
        if (out_bool) {
            *out_bool = result->bool_value;
        }
        if (out_snapshot) {
            *out_snapshot = result->snapshot;
        }
    }

    free(result);
    return final_result;
}

esp_err_t tasks_digital_io_set_output_via_agent(uint8_t output_id, bool value, TickType_t timeout_ticks)
{
    return tasks_dispatch_digital_io_agent_request(ACTION_ID_DIGITAL_IO_SET_OUTPUT,
                                                   (uint32_t)output_id,
                                                   value ? 1U : 0U,
                                                   NULL,
                                                   NULL,
                                                   timeout_ticks);
}

esp_err_t tasks_digital_io_get_output_via_agent(uint8_t output_id, bool *out_value, TickType_t timeout_ticks)
{
    if (!out_value) {
        return ESP_ERR_INVALID_ARG;
    }

    return tasks_dispatch_digital_io_agent_request(ACTION_ID_DIGITAL_IO_GET_OUTPUT,
                                                   (uint32_t)output_id,
                                                   0U,
                                                   out_value,
                                                   NULL,
                                                   timeout_ticks);
}

esp_err_t tasks_digital_io_get_input_via_agent(uint8_t input_id, bool *out_value, TickType_t timeout_ticks)
{
    if (!out_value) {
        return ESP_ERR_INVALID_ARG;
    }

    return tasks_dispatch_digital_io_agent_request(ACTION_ID_DIGITAL_IO_GET_INPUT,
                                                   (uint32_t)input_id,
                                                   0U,
                                                   out_value,
                                                   NULL,
                                                   timeout_ticks);
}

esp_err_t tasks_digital_io_get_snapshot_via_agent(digital_io_snapshot_t *out_snapshot, TickType_t timeout_ticks)
{
    if (!out_snapshot) {
        return ESP_ERR_INVALID_ARG;
    }

    return tasks_dispatch_digital_io_agent_request(ACTION_ID_DIGITAL_IO_GET_SNAPSHOT,
                                                   0U,
                                                   0U,
                                                   NULL,
                                                   out_snapshot,
                                                   timeout_ticks);
}

static const web_ui_program_entry_t *tasks_find_running_program_entry(const fsm_ctx_t *ctx)
{
    if (!ctx || ctx->running_program_name[0] == '\0') {
        return NULL;
    }

    return web_ui_program_find_by_name(ctx->running_program_name);
}

static esp_err_t tasks_verify_program_outputs_state(const web_ui_program_entry_t *entry)
{
    if (!entry) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t output_id = DIGITAL_IO_FIRST_CHANNEL_ID;
         output_id <= DIGITAL_IO_OUTPUT_COUNT;
         ++output_id) {
        bool expected = ((entry->relay_mask >> (output_id - 1U)) & 0x01U) != 0U;
        bool actual = false;
        esp_err_t err = tasks_digital_io_get_output_via_agent(output_id,
                                                               &actual,
                                                               pdMS_TO_TICKS(120));
        if (err != ESP_OK) {
            ESP_LOGW(TAG,
                     "[M] Verifica output fallita su OUT%u: %s",
                     (unsigned)output_id,
                     esp_err_to_name(err));
            return err;
        }

        if (actual != expected) {
            ESP_LOGW(TAG,
                     "[M] Mismatch output OUT%u atteso=%u letto=%u",
                     (unsigned)output_id,
                     expected ? 1U : 0U,
                     actual ? 1U : 0U);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static void tasks_apply_running_program_outputs(const fsm_ctx_t *ctx)
{
    const web_ui_program_entry_t *entry = tasks_find_running_program_entry(ctx);
    if (!entry) {
        ESP_LOGW(TAG,
                 "[M] Nessuna configurazione programma trovata per '%s': output non aggiornati",
                 (ctx && ctx->running_program_name[0] != '\0') ? ctx->running_program_name : "");
        return;
    }

    s_program_outputs_verify_error_active = false;

    for (uint8_t attempt = 1; attempt <= PROGRAM_OUTPUT_VERIFY_MAX_ATTEMPTS; ++attempt) {
        esp_err_t apply_err = web_ui_program_apply_outputs(entry);
        if (apply_err != ESP_OK) {
            ESP_LOGW(TAG,
                     "[M] Applicazione output programma '%s' tentativo %u/%u fallita: %s",
                     entry->name,
                     (unsigned)attempt,
                     (unsigned)PROGRAM_OUTPUT_VERIFY_MAX_ATTEMPTS,
                     esp_err_to_name(apply_err));
            if (attempt == PROGRAM_OUTPUT_VERIFY_MAX_ATTEMPTS) {
                s_program_outputs_verify_error_active = true;
            }
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(PROGRAM_OUTPUT_VERIFY_WAIT_MS));
        esp_err_t verify_err = tasks_verify_program_outputs_state(entry);
        if (verify_err == ESP_OK) {
            ESP_LOGI(TAG,
                     "[M] Output programma '%s' verificati (tentativo %u/%u, mask=0x%04x)",
                     entry->name,
                     (unsigned)attempt,
                     (unsigned)PROGRAM_OUTPUT_VERIFY_MAX_ATTEMPTS,
                     (unsigned)entry->relay_mask);
            s_program_outputs_verify_error_active = false;
            return;
        }

        ESP_LOGW(TAG,
                 "[M] Verifica output programma '%s' fallita al tentativo %u/%u: %s",
                 entry->name,
                 (unsigned)attempt,
                 (unsigned)PROGRAM_OUTPUT_VERIFY_MAX_ATTEMPTS,
                 esp_err_to_name(verify_err));

        if (attempt == PROGRAM_OUTPUT_VERIFY_MAX_ATTEMPTS) {
            s_program_outputs_verify_error_active = true;
        }
    }
}

static esp_err_t tasks_read_local_inputs_mask(uint16_t *out_mask)
{
    if (!out_mask) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t mask = 0U;
    for (uint8_t input_id = DIGITAL_IO_FIRST_CHANNEL_ID;
         input_id <= DIGITAL_IO_LOCAL_INPUT_COUNT;
         ++input_id) {
        bool state = false;
        esp_err_t err = digital_io_get_input(input_id, &state);
        if (err != ESP_OK) {
            return err;
        }
        if (state) {
            mask |= (uint16_t)(1U << (input_id - 1U));
        }
    }

    *out_mask = mask;
    return ESP_OK;
}

static void digital_io_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t period_ticks = (param && param->period_ticks > 0)
                                ? param->period_ticks
                                : pdMS_TO_TICKS(DIGITAL_IO_POLL_DEFAULT_MS);
    uint16_t previous_inputs_mask = 0U;
    bool previous_valid = false;

    while (true) {
        vTaskDelayUntil(&last_wake, period_ticks);

        if (tasks_is_modbus_runtime_blocked()) {
            previous_valid = false;
            continue;
        }

        esp_err_t init_err = digital_io_init();
        if (init_err != ESP_OK) {
            ESP_LOGW(TAG, "[M] Init digital_io fallita nel poll task: %s", esp_err_to_name(init_err));
            previous_valid = false;
            continue;
        }

        uint16_t inputs_mask = 0U;
        esp_err_t snapshot_err = tasks_read_local_inputs_mask(&inputs_mask);
        if (snapshot_err != ESP_OK) {
            // Gestione differenziata per i nuovi codici di errore
            switch (snapshot_err) {
                case DIGITAL_IO_ERR_CONFIG_NOT_READY:
                    ESP_LOGD(TAG, "[M] Snapshot digital_io attesa config (non critico)");
                    break;
                case DIGITAL_IO_ERR_LOCAL_IO_DISABLED:
                    ESP_LOGD(TAG, "[M] I/O locali disabilitati (non critico)");
                    break;
                case DIGITAL_IO_ERR_MODBUS_DISABLED:
                    ESP_LOGD(TAG, "[M] Modbus disabilitato (non critico)");
                    break;
                default:
                    ESP_LOGW(TAG, "[M] Snapshot digital_io fallito: %s", esp_err_to_name(snapshot_err));
                    break;
            }
            previous_valid = false;
            continue;
        }

        if (!previous_valid) {
            previous_inputs_mask = inputs_mask;
            previous_valid = true;
            continue;
        }

        uint16_t rising_mask = (uint16_t)((~previous_inputs_mask) & inputs_mask);
        previous_inputs_mask = inputs_mask;
        if (rising_mask == 0U) {
            continue;
        }

        if ((rising_mask & (1U << (DIGITAL_IO_INPUT_SERVICE_SWITCH - 1U))) != 0U) {
            ESP_LOGI(TAG, "[M] IN04 release rilevata: boot FACTORY");
            device_config_reboot_factory();
        }

        for (uint8_t input_id = DIGITAL_IO_FIRST_CHANNEL_ID; input_id <= DIGITAL_IO_LOCAL_INPUT_COUNT; ++input_id) {
            uint16_t input_mask = (uint16_t)(1U << (input_id - 1U));
            if ((rising_mask & input_mask) == 0U) {
                continue;
            }

            uint8_t program_id = tasks_find_program_id_for_input(input_id);
            if (program_id != 0U) {
                char input_code[24] = {0};
                (void)digital_io_input_get_code(input_id, input_code, sizeof(input_code));
                esp_err_t publish_err = tasks_publish_program_button_action(program_id, AGN_ID_DIGITAL_IO);
                if (publish_err != ESP_OK) {
                    ESP_LOGW(TAG,
                             "[M] %s -> programma %u non pubblicato: %s",
                             input_code[0] != '\0' ? input_code : "digital_input",
                             (unsigned)program_id,
                             tasks_err_to_name(publish_err));
                } else {
                    ESP_LOGI(TAG,
                             "[M] %s associato al programma %u: pubblicata azione equivalente al touch",
                             input_code[0] != '\0' ? input_code : "digital_input",
                             (unsigned)program_id);
                }
                continue;
            }

            tasks_process_other_digital_input_rising(input_id);
        }
    }
}

static void io_process_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;

    while (true) {
        fsm_input_event_t event;
        if (!fsm_event_receive(&event, AGN_ID_IO_PROCESS, param->period_ticks)) {
            continue;
        }

        switch (event.action) {
            case ACTION_ID_DIGITAL_IO_INPUT_RISING: {
                uint8_t input_id = (uint8_t)event.value_u32;
                char input_code[24] = {0};
                (void)digital_io_input_get_code(input_id, input_code, sizeof(input_code));
                ESP_LOGI(TAG,
                         "[M] io_process: ricevuto fronte 0->1 su %s, nessuna azione implementata",
                         input_code[0] != '\0' ? input_code : event.text);
                break;
            }

            case ACTION_ID_DIGITAL_IO_SET_OUTPUT: {
                uint8_t output_id = (uint8_t)event.value_u32;
                bool value = (event.aux_u32 != 0U);
                char output_code[24] = {0};
                (void)digital_io_output_get_code(output_id, output_code, sizeof(output_code));
                ESP_LOGI(TAG,
                         "[M] io_process: ricevuto stato %s=%u, nessuna azione implementata",
                         output_code[0] != '\0' ? output_code : event.text,
                         value ? 1U : 0U);
                break;
            }

            default:
                ESP_LOGW(TAG,
                         "[M] io_process: evento digital_io non gestito action=%d",
                         (int)event.action);
                break;
        }
    }
}


/**
 * @brief Ottiene il timeout in millisecondi per la restituzione della lingua.
 *
 * @return uint32_t Il timeout in millisecondi.
 */
static uint32_t fsm_get_language_return_timeout_ms(void)
{
    uint32_t timeout_ms = FSM_LANGUAGE_RETURN_DEFAULT_MS;
    device_config_t *cfg = device_config_get();
    if (cfg) {
        if (cfg->timeouts.idle_before_ads_ms > 0U) {
            timeout_ms = cfg->timeouts.idle_before_ads_ms;
        } else if (cfg->timeouts.exit_programs_ms > 0U) {
            timeout_ms = cfg->timeouts.exit_programs_ms;
        }
    }

    if (timeout_ms < FSM_LANGUAGE_RETURN_MIN_MS) {
        timeout_ms = FSM_LANGUAGE_RETURN_MIN_MS;
    }
    if (timeout_ms > FSM_LANGUAGE_RETURN_MAX_MS) {
        timeout_ms = FSM_LANGUAGE_RETURN_MAX_MS;
    }

    return timeout_ms;
}


/**
 * @brief Applica la gestione del timeout per il ritorno alla lingua.
 * 
 * @param [in] fsm Puntatore al contesto del finite state machine.
 * @return Nessun valore di ritorno.
 */
static void fsm_apply_language_return_timeout(fsm_ctx_t *fsm)
{
    if (!fsm) {
        return;
    }

    uint32_t timeout_ms = fsm_get_language_return_timeout_ms();
    if (fsm->splash_screen_time_ms == timeout_ms) {
        return;
    }

    fsm->splash_screen_time_ms = timeout_ms;
    if (fsm->inactivity_ms > (timeout_ms + 5000U)) {
        fsm->inactivity_ms = timeout_ms + 5000U;
    }

    ESP_LOGI(TAG, "[M] Timeout inattività ritorno lingua impostato a %lu ms", (unsigned long)timeout_ms);
}


/**
 * @brief Ottiene la temperatura corrente.
 * 
 * @return float La temperatura in gradi Celsius.
 */
float tasks_get_temperature(void) { return s_temperature; }

/**
 * @brief Ottiene l'umidità del sistema.
 * 
 * @return float La umidità corrente del sistema, espressa in percento.
 */
float tasks_get_humidity(void) { return s_humidity; }

/**
 * @brief Pubblica un evento KEY verso la FSM.
 *
 * Hook da richiamare quando verrà implementato il pulsante fisico dedicato.
 *
 * @return true se l'evento è stato accodato correttamente.
 * @return false se la coda FSM non è pronta o piena.
 */
bool tasks_publish_key_event(void)
{
    if (!fsm_event_queue_init(0)) {
        ESP_LOGW(TAG, "[M] Hook pulsante: coda FSM non disponibile");
        return false;
    }

    fsm_input_event_t ev = {
        .from = AGN_ID_AUX_GPIO,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_USER_ACTIVITY,
        .type = FSM_INPUT_EVENT_KEY,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = 0,
        .aux_u32 = 0,
        .text = {0},
    };

    if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
        ESP_LOGW(TAG, "[M] Hook pulsante: publish evento KEY fallito");
        return false;
    }

    ESP_LOGI(TAG, "[M] Hook pulsante: evento KEY pubblicato");
    return true;
}

/**
 * @brief Pubblica un evento CARD_CREDIT verso la FSM.
 *
 * Hook da usare per integrazione futura del lettore chip card/MDB cashless.
 *
 * @param vcd_amount_cents Credito virtuale da accreditare (centesimi).
 * @param source_tag Etichetta sorgente opzionale per tracciabilità.
 * @return true se l'evento è stato accodato correttamente.
 * @return false se input non valido o coda non disponibile.
 */
static void tasks_display_credit_if_epaper(int32_t credit_cents);

bool tasks_publish_card_credit_event(int32_t vcd_amount_cents, const char *source_tag)
{
    if (vcd_amount_cents <= 0) {
        ESP_LOGW(TAG, "[M] Hook card: importo non valido (%ld)", (long)vcd_amount_cents);
        return false;
    }

    if (!fsm_event_queue_init(0)) {
        ESP_LOGW(TAG, "[M] Hook card: coda FSM non disponibile");
        return false;
    }

    fsm_input_event_t ev = {
        .from = AGN_ID_MDB,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PAYMENT_ACCEPTED,
        .type = FSM_INPUT_EVENT_CARD_CREDIT,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = vcd_amount_cents,
        .value_u32 = 0,
        .aux_u32 = 0,
        .text = {0},
    };

    if (source_tag && source_tag[0] != '\0') {
        strncpy(ev.text, source_tag, sizeof(ev.text) - 1);
    } else {
        strncpy(ev.text, "card_credit_hook", sizeof(ev.text) - 1);
    }

    if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
        ESP_LOGW(TAG, "[M] Hook card: publish CARD_CREDIT fallito");
        return false;
    }

    ESP_LOGI(TAG, "[M] Hook card: CARD_CREDIT pubblicato (%ld)", (long)vcd_amount_cents);

    tasks_display_credit_if_epaper(vcd_amount_cents);

    return true;
}

static bool tasks_publish_card_vend_result_event(bool approved, int32_t amount_cents, const char *source_tag)
{
    if (!fsm_event_queue_init(0)) {
        ESP_LOGW(TAG, "[M] Hook card vend: coda FSM non disponibile");
        return false;
    }

    fsm_input_event_t ev = {
        .from = AGN_ID_MDB,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_NONE,
        .type = approved ? FSM_INPUT_EVENT_CARD_VEND_APPROVED : FSM_INPUT_EVENT_CARD_VEND_DENIED,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = amount_cents,
        .value_u32 = 0,
        .aux_u32 = 0,
        .text = {0},
    };

    if (source_tag && source_tag[0] != '\0') {
        strncpy(ev.text, source_tag, sizeof(ev.text) - 1);
    } else {
        strncpy(ev.text, approved ? "card_vend_approved" : "card_vend_denied", sizeof(ev.text) - 1);
    }

    if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
        ESP_LOGW(TAG,
                 "[M] Hook card vend: publish %s fallito",
                 approved ? "CARD_VEND_APPROVED" : "CARD_VEND_DENIED");
        return false;
    }

    ESP_LOGI(TAG,
             "[M] Hook card vend: %s pubblicato (%ld)",
             approved ? "CARD_VEND_APPROVED" : "CARD_VEND_DENIED",
             (long)amount_cents);
    return true;
}

bool tasks_request_card_vend(int32_t amount_cents, uint16_t item_number)
{
    if (amount_cents <= 0 || amount_cents > UINT16_MAX) {
        ESP_LOGW(TAG, "[M] Richiesta vend card non valida (%ld)", (long)amount_cents);
        return false;
    }

    ESP_LOGI(TAG, "[M] [CARD_VEND] invio VEND_REQUEST MDB amount=%ld item=0x%04X",
             (long)amount_cents,
             (unsigned)item_number);

    if (!mdb_cashless_request_program_vend((uint16_t)amount_cents, item_number)) {
        ESP_LOGW(TAG, "[M] Richiesta VEND_REQUEST MDB fallita (%ld)", (long)amount_cents);
        return false;
    }

    ESP_LOGI(TAG, "[M] Richiesta VEND_REQUEST MDB accodata (%ld)", (long)amount_cents);
    return true;
}

bool tasks_request_card_vend_success(int32_t approved_amount_cents)
{
    if (approved_amount_cents <= 0 || approved_amount_cents > UINT16_MAX) {
        ESP_LOGW(TAG, "[M] Richiesta vend success card non valida (%ld)", (long)approved_amount_cents);
        return false;
    }

    if (!mdb_cashless_confirm_vend_success((uint16_t)approved_amount_cents)) {
        ESP_LOGW(TAG, "[M] Richiesta VEND_SUCCESS MDB fallita (%ld)", (long)approved_amount_cents);
        return false;
    }

    ESP_LOGI(TAG, "[M] Richiesta VEND_SUCCESS MDB accodata (%ld)", (long)approved_amount_cents);
    return true;
}

bool tasks_request_card_session_complete(void)
{
    if (!mdb_cashless_complete_active_session()) {
        ESP_LOGW(TAG, "[M] Richiesta SESSION_COMPLETE MDB fallita");
        return false;
    }

    ESP_LOGI(TAG, "[M] Richiesta SESSION_COMPLETE MDB accodata");
    return true;
}

static bool tasks_is_card_session_removed(void)
{
    if (mdb_cashless_get_device_count() == 0) {
        return false;
    }

    const mdb_cashless_device_t *device = mdb_cashless_get_device(0);
    if (!device) {
        return false;
    }

     /* Il campo present indica la presenza del lettore cashless sul bus MDB,
         non la presenza del tag NFC. Dopo la rimozione del tag il lettore può
         entrare prima in stato ENDING (SESSION_CANCEL) e solo dopo chiudere
         formalmente la sessione con session_open == false. */
     return device->present &&
              (!device->session_open ||
                device->session_state == MDB_CASHLESS_SESSION_ENDING ||
                device->last_response_code == MDB_CASHLESS_RESP_SESSION_CANCEL ||
                device->last_response_code == MDB_CASHLESS_RESP_END_SESSION);
}

esp_err_t tasks_publish_play_audio(const char *audio_path, agn_id_t sender)
{
    if (!audio_path || audio_path[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (!fsm_event_queue_init(0)) {
        ESP_LOGW(TAG, "[M] PLAY_AUDIO non pubblicato: coda FSM non disponibile");
        return TASKS_ERR_FSM_QUEUE_NOT_READY;
    }

    char normalized_path[FSM_EVENT_TEXT_MAX_LEN] = {0};
    if (strncmp(audio_path, "/spiffs/", 8) == 0) {
        snprintf(normalized_path, sizeof(normalized_path), "%s", audio_path);
    } else if (audio_path[0] == '/') {
        snprintf(normalized_path, sizeof(normalized_path), "/spiffs%s", audio_path);
    } else {
        snprintf(normalized_path, sizeof(normalized_path), "/spiffs/%s", audio_path);
    }

    if (normalized_path[0] == '\0' || strlen(normalized_path) >= sizeof(((fsm_input_event_t *)0)->text)) {
        return ESP_ERR_INVALID_SIZE;
    }

    fsm_input_event_t event = {
        .from = (sender != AGN_ID_NONE) ? sender : AGN_ID_WEB_UI,
        .to = {AGN_ID_AUDIO},
        .action = ACTION_ID_PLAY_AUDIO,
        .type = FSM_INPUT_EVENT_NONE,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = 0,
        .aux_u32 = 0,
        .data_ptr = NULL,
        .text = {0},
    };

    strncpy(event.text, normalized_path, sizeof(event.text) - 1);
    /* [M] In alcuni passaggi UI la mailbox puo' essere temporaneamente contesa:
     * eseguiamo pochi retry brevi per non perdere prompt critici. */
    bool published = false;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        if (fsm_event_publish(&event, pdMS_TO_TICKS(100))) {
            published = true;
            break;
        }
        ESP_LOGW(TAG,
                 "[M] PLAY_AUDIO publish fallito (tentativo %d/3): %s",
                 attempt,
                 normalized_path);
        if (attempt < 3) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    if (!published) {
        ESP_LOGW(TAG, "[M] PLAY_AUDIO scartato dopo retry: %s", normalized_path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[M] PLAY_AUDIO pubblicato: %s", normalized_path);
    return ESP_OK;
}

/* Wrapper: crea il task allocando lo stack in DRAM interna o PSRAM
 * in base al campo stack_caps del descrittore. */

/**
 * @brief Crea un nuovo task.
 *
 * @param [in] t Puntatore ai parametri del task da creare.
 * @return BaseType_t Valore di ritorno che indica il successo o l'errore dell'operazione.
 */
static BaseType_t task_create(task_param_t *t)
{
    return xTaskCreatePinnedToCoreWithCaps(
        t->task_fn, t->name, t->stack_words, t->arg,
        t->priority, &t->handle, t->core_id, t->stack_caps);
}

/* ============================================================
 * ws2812_task — implementazione REALE (hardware WS2812/RMT)
 * Attiva quando DNA_LED_STRIP == 0
 * ============================================================ */
#ifndef DNA_LED_STRIP
#define DNA_LED_STRIP 0
#endif

#if DNA_LED_STRIP == 0

/**
 * @brief Gestisce il task per la comunicazione con il dispositivo WS2812.
 *
 * Questa funzione si occupa di inviare i dati di colore ai LED WS2812
 * in base alle istruzioni ricevute.
 *
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questo contesto).
 */
static void ws2812_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;

    TickType_t period_ticks = (param && param->period_ticks > 0)
                                ? param->period_ticks
                                : pdMS_TO_TICKS(40);
    TickType_t last_wake = xTaskGetTickCount();
    fsm_state_t last_state = FSM_STATE_IDLE;
    uint8_t last_progress = 255U;
    bool last_prefine = false;
    uint32_t last_count = 0U;

    const uint8_t idle_r = 0U;
    const uint8_t idle_g = 0U;
    const uint8_t idle_b = 48U;
    const uint8_t run_r = 0U;
    const uint8_t run_g = 100U;
    const uint8_t run_b = 0U;
    const uint8_t prefine_r = 64U;
    const uint8_t prefine_g = 0U;
    const uint8_t prefine_b = 64U;
    const uint32_t idle_effect_cycle_ms = 10000U;
    uint32_t clear_after_idle_until_ms = 0U;
    const uint32_t finish_blink_toggle_ms = 250U;
    bool finish_blink_active = false;
    bool finish_blink_on = false;
    uint8_t finish_blink_toggles_left = 0U;
    uint32_t finish_blink_next_toggle_ms = 0U;

    while (true) {
        vTaskDelayUntil(&last_wake, period_ticks);

        device_config_t *cfg = device_config_get();
        if (!cfg || !cfg->sensors.led_enabled) {
            continue;
        }

        if (led_init() != ESP_OK) {
            continue;
        }

        uint32_t led_count = led_get_count();
        if (led_count == 0U) {
            continue;
        }

        fsm_ctx_t snap = {0};
        bool has_snap = fsm_runtime_snapshot(&snap);

        fsm_state_t state = has_snap ? snap.state : FSM_STATE_IDLE;
        bool running_or_paused = has_snap && (state == FSM_STATE_RUNNING || state == FSM_STATE_PAUSED);
        bool idle_effect_state = !running_or_paused;
        bool last_running_or_paused = (last_state == FSM_STATE_RUNNING || last_state == FSM_STATE_PAUSED);
        bool last_idle_effect_state = (last_state != FSM_STATE_RUNNING && last_state != FSM_STATE_PAUSED);
        bool prefine_active = running_or_paused && snap.pre_fine_ciclo_active;
        uint8_t progress = 0U;
        uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());

        if (running_or_paused && snap.running_target_ms > 0U) {
            uint32_t pct = (uint32_t)((snap.running_elapsed_ms * 100U) / snap.running_target_ms);
            if (pct > 100U) {
                pct = 100U;
            }
            progress = (uint8_t)pct;
        }

        if (last_running_or_paused && !running_or_paused) {
            finish_blink_active = true;
            finish_blink_on = false;
            finish_blink_toggles_left = 10U;
            finish_blink_next_toggle_ms = now_ms;
        }

        if (finish_blink_active) {
            if (now_ms >= finish_blink_next_toggle_ms) {
                finish_blink_on = !finish_blink_on;
                if (finish_blink_on) {
                    (void)led_fill_color(255U, 255U, 255U);
                } else {
                    (void)led_clear();
                }

                finish_blink_next_toggle_ms = now_ms + finish_blink_toggle_ms;
                if (finish_blink_toggles_left > 0U) {
                    --finish_blink_toggles_left;
                }
                if (finish_blink_toggles_left == 0U) {
                    finish_blink_active = false;
                    last_state = (fsm_state_t)(-1);
                    last_progress = 255U;
                    last_prefine = false;
                    last_count = 0U;
                }
            }

            last_state = state;
            last_progress = progress;
            last_prefine = prefine_active;
            last_count = led_count;
            continue;
        }

        if (last_idle_effect_state && !idle_effect_state) {
            clear_after_idle_until_ms = now_ms + 500U;
            (void)led_clear();
            last_state = state;
            last_progress = progress;
            last_prefine = prefine_active;
            last_count = led_count;
            continue;
        }

        if (clear_after_idle_until_ms != 0U && now_ms < clear_after_idle_until_ms) {
            (void)led_clear();
            continue;
        }

        if (state == last_state &&
            progress == last_progress &&
            prefine_active == last_prefine &&
            led_count == last_count &&
            !idle_effect_state) {
            continue;
        }

        if (idle_effect_state) {
            uint32_t slot_ms = idle_effect_cycle_ms / led_count;
            if (slot_ms == 0U) {
                slot_ms = 1U;
            }

            uint32_t cycle_pos = now_ms % idle_effect_cycle_ms;
            uint32_t active_led_index = cycle_pos / slot_ms;
            if (active_led_index >= led_count) {
                active_led_index = led_count - 1U;
            }

            uint32_t led_phase_ms = cycle_pos % slot_ms;

            for (uint32_t i = 0; i < led_count; ++i) {
                uint32_t brightness_pct;
                if (i == active_led_index) {
                    // Fading logic for the active LED: 50% -> 10% -> 90% -> 50%
                    uint32_t half_phase_ms = slot_ms / 2U;
                    if (half_phase_ms == 0U) half_phase_ms = 1U;

                    if (led_phase_ms < half_phase_ms) {
                        // First half: 50% up to 100%
                        brightness_pct = 50U + ((50U * led_phase_ms) / half_phase_ms);
                    } else {
                        // Second half: 100% down to 10%
                        uint32_t second_half_progress = led_phase_ms - half_phase_ms;
                        uint32_t second_half_duration = slot_ms - half_phase_ms;
                        if (second_half_duration == 0U) second_half_duration = 1U;
                        brightness_pct = 100U - ((90U * second_half_progress) / second_half_duration);
                    }
                } else {
                    // Inactive LEDs are at a constant 10%
                    brightness_pct = 10U;
                }
                uint8_t blue = (uint8_t)(((uint32_t)idle_b * brightness_pct) / 100U);
                (void)led_set_pixel(i, idle_r, idle_g, blue);
            }
            (void)led_refresh();
        } else {
            uint32_t leds_on_target = (led_count * progress) / 100U;

            uint8_t on_r = prefine_active ? prefine_r : run_r;
            uint8_t on_g = prefine_active ? prefine_g : run_g;
            uint8_t on_b = prefine_active ? prefine_b : run_b;

            for (uint32_t i = 0; i < led_count; ++i) {
                if (i < leds_on_target) {
                    // This LED is fully on
                    (void)led_set_pixel(i, on_r, on_g, on_b);
                } else if (i == leds_on_target && progress < 100) {
                    // This is the currently progressing LED, let's fade it in
                    uint32_t progress_per_led = 100 / led_count;
                    uint32_t led_start_progress = i * progress_per_led;
                    uint32_t led_progress_slice = progress - led_start_progress;
                    
                    uint32_t brightness_pct = 10 + (led_progress_slice * 90 / progress_per_led);
                    if (brightness_pct > 100) brightness_pct = 100;
                    if (brightness_pct < 10) brightness_pct = 10;

                    uint8_t r = (uint8_t)(((uint32_t)on_r * brightness_pct) / 100U);
                    uint8_t g = (uint8_t)(((uint32_t)on_g * brightness_pct) / 100U);
                    uint8_t b = (uint8_t)(((uint32_t)on_b * brightness_pct) / 100U);
                    (void)led_set_pixel(i, r, g, b);
                } else {
                    // This LED is off
                    (void)led_set_pixel(i, 0U, 0U, 0U);
                }
            }
            (void)led_refresh();
        }

        last_state = state;
        last_progress = progress;
        last_prefine = prefine_active;
        last_count = led_count;
    }
}
#endif /* DNA_LED_STRIP == 0 */

/* ============================================================
 * ws2812_task — implementazione MOCK (nessun hardware)
 * Attiva quando DNA_LED_STRIP == 1
 * ============================================================ */
#if defined(DNA_LED_STRIP) && (DNA_LED_STRIP == 1)

/**
 * @brief Gestisce la task per la comunicazione con il dispositivo WS2812.
 * 
 * Questa funzione si occupa di inviare i dati di colore ai LED WS2812.
 * 
 * @param arg Puntatore agli argomenti passati alla task.
 */
static void ws2812_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    ESP_LOGI(TAG, "[MOCK] ws2812_task avviata: DNA_LED_STRIP=1, nessun hardware RMT/WS2812");
    while (true) {
        vTaskDelay(param->period_ticks);
    }
}
#endif /* DNA_LED_STRIP == 1 */

// Task scheletro (solo struttura, logica da implementare)


/**
 * @brief Gestisce il task per l'accesso all'EEPROM.
 * 
 * Questa funzione viene eseguita come task e si occupa di leggere e scrivere
 * dati sull'EEPROM in base alle richieste ricevute.
 * 
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 * @return void Nessun valore di ritorno.
 */
static void eeprom_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task di espansione I/O.
 *
 * Questa funzione si occupa di gestire il task di espansione I/O, elaborando i dati ricevuti e aggiornando lo stato del sistema.
 *
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 * @return Nessun valore di ritorno.
 */
static void io_expander_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task per la comunicazione con il sensore SHT40.
 * 
 * Questa funzione si occupa di iniziare la misurazione del temperatura e
 * dell'umidità, di leggere i dati e di inviarli al server.
 * 
 * @param arg Puntatore agli argomenti del task.
 */
static void sht40_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        if (device_config_get()->sensors.temperature_enabled && sht40_is_ready()) {
            float t, h;
            if (sht40_read(&t, &h) == ESP_OK) {
                s_temperature = t;
                s_humidity = h;
                ESP_LOGD(TAG, "SHT40: T=%.1f C, RH=%.1f %%", t, h);

                const device_config_t *cfg = device_config_get();
                bool pwm1_active = false;
                bool pwm2_active = false;

                if (h > cfg->sensors.pwm1_humidity_threshold) {
                    pwm1_active = cfg->sensors.pwm1_enabled;
                    pwm2_active = cfg->sensors.pwm2_enabled;
                } else {
                    if (cfg->sensors.pwm2_enabled) {
                        if (t >= cfg->sensors.pwm2_fan_threshold || t < cfg->sensors.pwm1_heater_threshold) {
                            pwm2_active = true;
                        }
                    }
                }

                if (cfg->sensors.pwm1_enabled) {
                    pwm_set_duty(0, pwm1_active ? 100 : 0);
                } else {
                    pwm_set_duty(0, 0);
                }
                if (cfg->sensors.pwm2_enabled) {
                    pwm_set_duty(1, pwm2_active ? 100 : 0);
                } else {
                    pwm_set_duty(1, 0);
                }
            }
        }
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void scanner_on_barcode_cb(const char *barcode);
static void normalize_barcode_text(const char *input, char *output, size_t output_len);

/** @brief Procesa una singola linea ricevuta da RS232 e la inoltra come barcode scanner.
 *
 *  @param line Linea di testo ricevuta da RS232.
 */
static void rs232_process_scanner_line(const char *line)
{
    if (!line || line[0] == '\0') {
        return;
    }

    char normalized[FSM_EVENT_TEXT_MAX_LEN] = {0};
    normalize_barcode_text(line, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return;
    }

    ESP_LOGI(TAG, "[M] RS232 scanner line: %s", normalized);
    scanner_on_barcode_cb(normalized);
}

/** @brief Gestisce il task per la comunicazione RS232.
 *
 *  Questa funzione legge i dati ricevuti sulla porta RS232 e interpreta le
 *  stringhe ricevute come codici scanner provenienti dal modulo E-Paper/QR.
 *
 *  @param arg Puntatore a dati aggiuntivi (task_param_t*).
 */
static void rs232_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    uint8_t buf[128];
    char line_buf[FSM_EVENT_TEXT_MAX_LEN];
    size_t line_idx = 0;

    while (true) {
        if (rs232_epaper_is_enabled()) {
            int read_len = rs232_receive(buf, sizeof(buf), 20);
            if (read_len > 0) {
                for (int i = 0; i < read_len; ++i) {
                    uint8_t c = buf[i];
                    if (c == '\r' || c == '\n') {
                        if (line_idx > 0) {
                            line_buf[line_idx] = '\0';
                            rs232_process_scanner_line(line_buf);
                            line_idx = 0;
                        }
                        continue;
                    }
                    if (line_idx + 1 < sizeof(line_buf)) {
                        line_buf[line_idx++] = (char)c;
                    } else {
                        ESP_LOGW(TAG, "[M] RS232 linea troppo lunga: scarto dati fino al prossimo terminatore");
                        line_idx = 0;
                    }
                }
            } else if (read_len < 0) {
                ESP_LOGW(TAG, "[M] Errore lettura RS232: %d", read_len);
                line_idx = 0;
            }
        } else {
            if (line_idx > 0) {
                line_idx = 0;
            }
        }
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/** @brief Gestisce il task per la comunicazione RS485.
 *  
 *  Questa funzione gestisce il task dedicato alla comunicazione RS485. 
 *  Si occupa di inviare e ricevere dati tramite la linea RS485.
 *  
 *  @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 *  
 *  @return Nessun valore di ritorno.
 */
static void rs485_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    esp_err_t last_init_err = ESP_OK;
    esp_err_t last_poll_err = ESP_OK;
    bool bootstrap_probe_done = false;

    while (true) {
        bool modbus_hard_inhibit = tasks_is_modbus_hard_inhibited();
        bool modbus_recovery_test_active = tasks_is_modbus_recovery_test_active();

        if (modbus_hard_inhibit) {
            if (!modbus_recovery_test_active && modbus_relay_is_running()) {
                (void)modbus_relay_deinit();
                ESP_LOGI(TAG, "[M] Modbus fermato: inibizione hard attiva");
            }
            last_init_err = ESP_OK;
            last_poll_err = ESP_OK;
            bootstrap_probe_done = false;
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(200));
            continue;
        }

        if (tasks_is_out_of_service_state()) {
            if (modbus_relay_is_running()) {
                (void)modbus_relay_deinit();
                ESP_LOGI(TAG, "[M] Modbus fermato: stato OUT_OF_SERVICE");
            }
            last_init_err = ESP_OK;
            last_poll_err = ESP_OK;
            bootstrap_probe_done = false;
            vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(200));
            continue;
        }

        device_config_t *cfg = device_config_get();
        bool should_run_modbus = false;
        uint32_t delay_ms = (uint32_t)pdTICKS_TO_MS(param->period_ticks);

        if (cfg) {
            should_run_modbus = (cfg->sensors.rs485_enabled && cfg->modbus.enabled);
            if (cfg->modbus.poll_ms > 0U) {
                delay_ms = cfg->modbus.poll_ms;
            }
        }

        if (delay_ms < 20U) {
            delay_ms = 20U;
        }

        if (should_run_modbus) {
            esp_err_t init_err = modbus_relay_init();
            if (init_err != ESP_OK) {
                if (init_err != last_init_err) {
                    ESP_LOGW(TAG, "[M] Modbus init RS485 fallita: %s", esp_err_to_name(init_err));
                    last_init_err = init_err;
                }
            } else {
                last_init_err = ESP_OK;

                if (!bootstrap_probe_done && cfg) {
                    uint8_t bits[MODBUS_RELAY_MAX_BYTES] = {0};
                    uint16_t relay_count = cfg->modbus.relay_count;
                    uint16_t input_count = cfg->modbus.input_count;

                    if (relay_count > MODBUS_RELAY_MAX_POINTS) {
                        relay_count = MODBUS_RELAY_MAX_POINTS;
                    }
                    if (input_count > MODBUS_RELAY_MAX_POINTS) {
                        input_count = MODBUS_RELAY_MAX_POINTS;
                    }

                    ESP_LOGI(TAG,
                             "[M] Bootstrap Modbus probe: slave=%u relay_count=%u input_count=%u",
                             (unsigned)cfg->modbus.slave_id,
                             (unsigned)relay_count,
                             (unsigned)input_count);

                    if (relay_count > 0U) {
                        esp_err_t read_relay_err = modbus_relay_read_coils(cfg->modbus.slave_id,
                                                                           cfg->modbus.relay_start,
                                                                           relay_count,
                                                                           bits,
                                                                           sizeof(bits));
                        if (read_relay_err != ESP_OK) {
                            ESP_LOGW(TAG,
                                     "[M] Bootstrap Modbus lettura relay fallita: %s",
                                     esp_err_to_name(read_relay_err));
                        } else {
                            ESP_LOGI(TAG,
                                     "[M] Bootstrap Modbus lettura relay OK (start=%u count=%u)",
                                     (unsigned)cfg->modbus.relay_start,
                                     (unsigned)relay_count);
                        }
                    }

                    if (input_count > 0U) {
                        esp_err_t read_input_err = modbus_relay_read_discrete_inputs(cfg->modbus.slave_id,
                                                                                      cfg->modbus.input_start,
                                                                                      input_count,
                                                                                      bits,
                                                                                      sizeof(bits));
                        if (read_input_err != ESP_OK) {
                            ESP_LOGW(TAG,
                                     "[M] Bootstrap Modbus lettura input fallita: %s",
                                     esp_err_to_name(read_input_err));
                        } else {
                            ESP_LOGI(TAG,
                                     "[M] Bootstrap Modbus lettura input OK (start=%u count=%u)",
                                     (unsigned)cfg->modbus.input_start,
                                     (unsigned)input_count);
                        }
                    }

                    bootstrap_probe_done = true;
                }

                esp_err_t poll_err = modbus_relay_poll_once();
                if (poll_err != ESP_OK && poll_err != last_poll_err) {
                    ESP_LOGW(TAG, "[M] Modbus poll RS485 fallita: %s", esp_err_to_name(poll_err));
                    last_poll_err = poll_err;
                } else if (poll_err == ESP_OK) {
                    last_poll_err = ESP_OK;
                }
            }
        } else if (modbus_relay_is_running()) {
            (void)modbus_relay_deinit();
            last_init_err = ESP_OK;
            last_poll_err = ESP_OK;
            bootstrap_probe_done = false;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay_ms));
    }
}


/**
 * @brief Gestisce il task per la gestione del database.
 *
 * Questa funzione viene eseguita come task e si occupa della gestione
 * dei dati del database, come la lettura, la scrittura e la sincronizzazione.
 *
 * @param arg Puntatore a dati aggiuntivi passati al task.
 */
static void mdb_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task del Finite State Machine (FSM).
 *
 * Questa funzione è responsabile della gestione del task del FSM. Viene eseguita in un contesto di sistema operativo e gestisce lo stato corrente del sistema.
 *
 * @param arg Puntatore a dati aggiuntivi passati al task. In questo caso, non viene utilizzato.
 * @return Nessun valore di ritorno.
 */
static void fsm_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    fsm_ctx_t fsm;
    fsm_init(&fsm);
    device_config_t *cfg = device_config_get();
    if (cfg) {
        if (cfg->timeouts.idle_before_ads_ms > 0U) {
            fsm.splash_screen_time_ms = cfg->timeouts.idle_before_ads_ms;
        } else if (cfg->timeouts.exit_programs_ms > 0U) {
            fsm.splash_screen_time_ms = cfg->timeouts.exit_programs_ms;
        }
        if (cfg->timeouts.ad_rotation_ms > 0U) {
            fsm.ads_rotation_ms = cfg->timeouts.ad_rotation_ms;
        }
        if (cfg->timeouts.credit_reset_timeout_ms > 0U) {
            fsm.credit_reset_timeout_ms = cfg->timeouts.credit_reset_timeout_ms;
        }
        fsm.ads_enabled = cfg->display.ads_enabled;
        fsm.state = fsm.ads_enabled ? FSM_STATE_ADS : FSM_STATE_CREDIT;
    }
    fsm_apply_language_return_timeout(&fsm);
    TickType_t prev_tick = xTaskGetTickCount();

    if (!fsm_event_queue_init(0)) {
        ESP_LOGE(TAG, "[FSM] Event queue init failed");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "[FSM] Task started in state=%s", fsm_state_to_string(fsm.state));
    if (cfg && cfg->display.enabled) {
        if (fsm.state != FSM_STATE_ADS) {
            lvgl_panel_show_main_page();
        }
    }
    fsm_runtime_publish(&fsm);

    /* contatore per il log "alive" ogni 10 secondi */
    uint32_t alive_ms = 0;
    static const uint32_t ALIVE_INTERVAL_MS = 10000;
    bool cctalk_forced_stop_for_vcd = false;
    bool cctalk_forced_stop_for_program = false;
    bool scanner_forced_off_for_program = false;
    bool mdb_forced_reset_for_program = false;
    TickType_t next_health_check = xTaskGetTickCount();
    TickType_t next_oos_retry = 0;
    tasks_oos_cause_t active_oos = {0};

    while (true) {
        fsm_input_event_t event;
        bool event_received = false;
        bool changed = false;
        fsm_state_t state_before = fsm.state;

        fsm_apply_language_return_timeout(&fsm);

        /* #6 fix: receive first (blocca fino a period_ticks = 100ms), poi
         * misura l'elapsed così l'attesa è inclusa nel delta. In questo modo
         * fsm_tick() riceve il tempo realmente trascorso, non quello
         * dell'iterazione *precedente*. */
        if (fsm_event_receive(&event, AGN_ID_FSM, param->period_ticks)) {
            event_received = true;
            if (tasks_handle_digital_io_agent_event(&event)) {
                changed = false;
            } else {
                changed = fsm_handle_input_event(&fsm, &event);
            }
        }

        TickType_t now_tick = xTaskGetTickCount();
        uint32_t elapsed_ms = (uint32_t)pdTICKS_TO_MS(now_tick - prev_tick);
        prev_tick = now_tick;

        changed = fsm_tick(&fsm, elapsed_ms) || changed;

        if (fsm.state != FSM_STATE_OUT_OF_SERVICE) {
            tasks_oos_cause_t requested = {0};
            if (tasks_oos_take_requested(&requested)) {
                fsm_input_event_t oos_event = {
                    .from = AGN_ID_NONE,
                    .to = {AGN_ID_FSM},
                    .action = ACTION_ID_SYSTEM_ERROR,
                    .type = FSM_INPUT_EVENT_NONE,
                    .timestamp_ms = (uint32_t)pdTICKS_TO_MS(now_tick),
                    .value_i32 = (int32_t)requested.agent,
                    .value_u32 = 0,
                    .aux_u32 = 0,
                    .data_ptr = NULL,
                    .text = {0},
                };
                snprintf(oos_event.text, sizeof(oos_event.text), "%s", requested.reason_key);
                changed = fsm_handle_input_event(&fsm, &oos_event) || changed;
                active_oos = requested;
                next_oos_retry = now_tick + pdMS_TO_TICKS(OOS_RETRY_MS);
            }
        }

        if (fsm.state == FSM_STATE_OUT_OF_SERVICE) {
            tasks_set_out_of_service_runtime(true);
            if ((int32_t)(now_tick - next_oos_retry) >= 0) {
                tasks_oos_cause_t retry_failure = {0};
                agn_id_t focus = (agn_id_t)fsm.out_of_service_agent;
                bool still_failed = tasks_health_check(focus, &retry_failure);
                if (still_failed) {
                    active_oos = retry_failure;
                    next_oos_retry = now_tick + pdMS_TO_TICKS(OOS_RETRY_MS);
                    ESP_LOGW(TAG,
                             "[M] OUT_OF_SERVICE retry: ancora KO agent=%s reason=%s",
                             tasks_agent_name(active_oos.agent),
                             active_oos.reason_key);
                } else {
                    ESP_LOGW(TAG,
                             "[M] OUT_OF_SERVICE risolto (agent=%s): ritorno in RUN",
                             tasks_agent_name((agn_id_t)fsm.out_of_service_agent));

                    fsm_input_event_t run_event = {
                        .from = AGN_ID_NONE,
                        .to = {AGN_ID_FSM},
                        .action = ACTION_ID_SYSTEM_RUN,
                        .type = FSM_INPUT_EVENT_NONE,
                        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(now_tick),
                        .value_i32 = 0,
                        .value_u32 = 0,
                        .aux_u32 = 0,
                        .data_ptr = NULL,
                        .text = {0},
                    };
                    changed = fsm_handle_input_event(&fsm, &run_event) || changed;
                    if (fsm.state != FSM_STATE_OUT_OF_SERVICE) {
                        tasks_oos_clear_requested();
                        tasks_set_out_of_service_runtime(false);

                        if (cfg && cfg->sensors.cctalk_enabled) {
                            if (tasks_publish_cctalk_control_event(ACTION_ID_CCTALK_START)) {
                                ESP_LOGI(TAG, "[M] Gettoniera CCTALK riabilitata dopo uscita OUT_OF_SERVICE");
                            } else {
                                ESP_LOGW(TAG, "[M] Riabilitazione gettoniera CCTALK fallita dopo uscita OUT_OF_SERVICE");
                            }
                        }

                        if (cfg && cfg->scanner.enabled) {
                            esp_err_t setup_err = usb_cdc_scanner_send_setup_command();
                            esp_err_t on_err = usb_cdc_scanner_send_on_command();
                            if (setup_err == ESP_OK && on_err == ESP_OK) {
                                ESP_LOGI(TAG, "[M] Scanner riabilitato dopo uscita OUT_OF_SERVICE");
                            } else {
                                ESP_LOGW(TAG,
                                         "[M] Riabilitazione scanner fallita dopo uscita OUT_OF_SERVICE (setup=%s on=%s)",
                                         esp_err_to_name(setup_err),
                                         esp_err_to_name(on_err));
                            }
                        }

                        memset(&active_oos, 0, sizeof(active_oos));
                        next_health_check = now_tick + pdMS_TO_TICKS(OOS_HEALTH_CHECK_MS);
                        next_oos_retry = 0;
                        ESP_LOGI(TAG, "[M] Uscita OUT_OF_SERVICE completata");
                    } else {
                        ESP_LOGW(TAG, "[M] Uscita OUT_OF_SERVICE fallita: stato ancora OOS dopo SYSTEM_RUN");
                    }
                }
            }
        } else if ((int32_t)(now_tick - next_health_check) >= 0) {
            tasks_set_out_of_service_runtime(false);
            tasks_oos_cause_t detected = {0};
            if (tasks_health_check(AGN_ID_NONE, &detected)) {
                fsm_input_event_t oos_event = {
                    .from = AGN_ID_NONE,
                    .to = {AGN_ID_FSM},
                    .action = ACTION_ID_SYSTEM_ERROR,
                    .type = FSM_INPUT_EVENT_NONE,
                    .timestamp_ms = (uint32_t)pdTICKS_TO_MS(now_tick),
                    .value_i32 = (int32_t)detected.agent,
                    .value_u32 = 0,
                    .aux_u32 = 0,
                    .data_ptr = NULL,
                    .text = {0},
                };
                snprintf(oos_event.text, sizeof(oos_event.text), "%s", detected.reason_key);
                changed = fsm_handle_input_event(&fsm, &oos_event) || changed;
                active_oos = detected;
                next_oos_retry = now_tick + pdMS_TO_TICKS(OOS_RETRY_MS);
                ESP_LOGE(TAG,
                         "[M] FSM in OUT_OF_SERVICE: agent=%s reason=%s",
                         tasks_agent_name(active_oos.agent),
                         active_oos.reason_key);
            }
            next_health_check = now_tick + pdMS_TO_TICKS(OOS_HEALTH_CHECK_MS);
        } else {
            tasks_set_out_of_service_runtime(false);
        }

        if ((fsm.state == FSM_STATE_RUNNING || fsm.state == FSM_STATE_PAUSED) &&
            fsm.session_source == FSM_SESSION_SOURCE_CARD &&
            !fsm.card_vend_pending &&
            tasks_is_card_session_removed()) {
            if (!fsm.card_session_removed_during_run) {
                fsm.card_session_removed_during_run = true;
                fsm.card_session_complete_required = false;
                fsm.vcd_coins = 0;
                fsm.vcd_cents_residual = 0;
                fsm.credit_cents = fsm.ecd_coins;
                changed = true;
                ESP_LOGI(TAG,
                         "[M] Tag card rimosso durante programma: VCD residuo azzerato, fine ciclo senza autorepeat");
            }
        }

        if (fsm.state == FSM_STATE_CREDIT &&
            fsm.session_source == FSM_SESSION_SOURCE_CARD &&
            !fsm.card_vend_pending &&
            tasks_is_card_session_removed()) {
            if (fsm.card_session_complete_required) {
                fsm.card_session_complete_required = false;
                ESP_LOGI(TAG, "[M] Sessione card gia' chiusa dal lettore: skip SESSION_COMPLETE tardivo");
            }

            fsm.card_session_removed_during_run = false;

            fsm_input_event_t card_removed_event = {
                .from = AGN_ID_MDB,
                .to = {AGN_ID_FSM},
                .action = ACTION_ID_CREDIT_ENDED,
                .type = FSM_INPUT_EVENT_CREDIT_ENDED,
                .timestamp_ms = (uint32_t)pdTICKS_TO_MS(now_tick),
                .value_i32 = 0,
                .value_u32 = 0,
                .aux_u32 = 1,
                .data_ptr = NULL,
                .text = {0},
            };
            strncpy(card_removed_event.text, "nfc_removed", sizeof(card_removed_event.text) - 1);

            if (fsm_event_publish(&card_removed_event, pdMS_TO_TICKS(20))) {
                changed = true;
                ESP_LOGI(TAG, "[M] NFC rimosso dopo stop programma: evento CREDIT_ENDED inviato");
            }

            /* Non forzare un reset del dispositivo cashless su normale chiusura sessione,
               lasciamo il driver MDB gestire il ritorno a stato idle e la successiva
               reinserzione del tag senza bip di ripristino. */
        }

        bool vcd_locked_session =
            (fsm.session_mode == FSM_SESSION_MODE_VIRTUAL_LOCKED) &&
            (fsm.session_source == FSM_SESSION_SOURCE_QR ||
             fsm.session_source == FSM_SESSION_SOURCE_CARD) &&
            !fsm.allow_additional_payments;
        bool active_program_session =
            (fsm.state == FSM_STATE_RUNNING || fsm.state == FSM_STATE_PAUSED);

        if (fsm.state != FSM_STATE_OUT_OF_SERVICE && cfg && cfg->sensors.cctalk_enabled) {
            /* [M] La gettoniera CCtalk deve rimanere attiva durante RUN/PAUSE.
               La disabilitiamo solo in sessione VCD bloccata. */
            bool cctalk_stop_needed = vcd_locked_session;

            if (cctalk_stop_needed && !cctalk_forced_stop_for_vcd && !cctalk_forced_stop_for_program) {
                if (tasks_publish_cctalk_control_event(ACTION_ID_CCTALK_STOP)) {
                    cctalk_forced_stop_for_program = active_program_session;
                    if (active_program_session) {
                        ESP_LOGI(TAG, "[M] Gettoniera CCTALK disabilitata durante programma attivo");
                    } else {
                        cctalk_forced_stop_for_vcd = true;
                        ESP_LOGI(TAG, "[M] Gettoniera CCTALK disabilitata durante sessione VCD");
                    }
                } else {
                    ESP_LOGW(TAG, "[M] Richiesta stop gettoniera CCTALK non pubblicata (sessione VCD)");
                }
            } else if (!cctalk_stop_needed && (cctalk_forced_stop_for_vcd || cctalk_forced_stop_for_program) &&
                       (fsm.state == FSM_STATE_CREDIT || fsm.state == FSM_STATE_ADS || fsm.state == FSM_STATE_IDLE)) {
                if (tasks_publish_cctalk_control_event(ACTION_ID_CCTALK_START)) {
                    cctalk_forced_stop_for_vcd = false;
                    cctalk_forced_stop_for_program = false;
                    ESP_LOGI(TAG, "[M] Gettoniera CCTALK riabilitata dopo sessione VCD");
                } else {
                    ESP_LOGW(TAG, "[M] Richiesta start gettoniera CCTALK non pubblicata (sessione VCD)");
                }
            }
        } else {
            cctalk_forced_stop_for_vcd = false;
            cctalk_forced_stop_for_program = false;
        }

        if (fsm.state != FSM_STATE_OUT_OF_SERVICE && cfg && cfg->scanner.enabled) {
            if (active_program_session && !scanner_forced_off_for_program) {
                esp_err_t off_err = usb_cdc_scanner_send_off_command();
                if (off_err == ESP_OK) {
                    scanner_forced_off_for_program = true;
                    ESP_LOGI(TAG, "[M] Scanner QR disabilitato durante programma attivo");
                } else {
                    ESP_LOGW(TAG, "[M] Richiesta off scanner non pubblicata durante programma attivo: %s", esp_err_to_name(off_err));
                }
            } else if (!active_program_session && scanner_forced_off_for_program &&
                       (fsm.state == FSM_STATE_CREDIT || fsm.state == FSM_STATE_ADS || fsm.state == FSM_STATE_IDLE)) {
                esp_err_t setup_err = usb_cdc_scanner_send_setup_command();
                esp_err_t on_err = usb_cdc_scanner_send_on_command();
                if (setup_err == ESP_OK && on_err == ESP_OK) {
                    scanner_forced_off_for_program = false;
                    ESP_LOGI(TAG, "[M] Scanner QR riabilitato dopo fine programma");
                } else {
                    ESP_LOGW(TAG,
                             "[M] Riabilitazione scanner fallita dopo fine programma (setup=%s on=%s)",
                             esp_err_to_name(setup_err),
                             esp_err_to_name(on_err));
                }
            }
        } else {
            scanner_forced_off_for_program = false;
        }

        if (fsm.state != FSM_STATE_OUT_OF_SERVICE && cfg && cfg->sensors.mdb_enabled && cfg->mdb.cashless_en) {
            bool mdb_session_open = false;
            size_t mdb_count = mdb_cashless_get_device_count();
            for (size_t i = 0; i < mdb_count; ++i) {
                const mdb_cashless_device_t *device = mdb_cashless_get_device(i);
                if (device && device->session_open) {
                    mdb_session_open = true;
                    break;
                }
            }

            bool mdb_stop_needed = active_program_session && !mdb_session_open;
            if (mdb_stop_needed && !mdb_forced_reset_for_program) {
                for (size_t i = 0; i < mdb_count; ++i) {
                    mdb_cashless_reset_device(i);
                }
                mdb_forced_reset_for_program = true;
                ESP_LOGI(TAG, "[M] MDB cashless disabilitato durante programma attivo");
            } else if (!mdb_stop_needed && mdb_forced_reset_for_program &&
                       (fsm.state == FSM_STATE_CREDIT || fsm.state == FSM_STATE_ADS || fsm.state == FSM_STATE_IDLE)) {
                for (size_t i = 0; i < mdb_count; ++i) {
                    mdb_cashless_reset_device(i);
                }
                mdb_forced_reset_for_program = false;
                ESP_LOGI(TAG, "[M] MDB cashless riabilitato dopo fine programma");
            }
        } else {
            mdb_forced_reset_for_program = false;
        }

        if (event_received &&
            ((event.type == FSM_INPUT_EVENT_PROGRAM_SELECTED &&
              state_before == FSM_STATE_CREDIT &&
              fsm.state == FSM_STATE_RUNNING) ||
             (event.type == FSM_INPUT_EVENT_PROGRAM_SWITCH &&
              (state_before == FSM_STATE_RUNNING || state_before == FSM_STATE_PAUSED) &&
              fsm.state == FSM_STATE_RUNNING))) {
            tasks_apply_running_program_outputs(&fsm);
        }

        /* log alive ogni ALIVE_INTERVAL_MS */
        alive_ms += elapsed_ms;
        if (alive_ms >= ALIVE_INTERVAL_MS) {
            alive_ms = 0;
            char now_buf[24] = "--/--/---- --:--:--";
            time_t now = time(NULL);
            if (now != (time_t)-1) {
                struct tm ti;
                localtime_r(&now, &ti);
                strftime(now_buf, sizeof(now_buf), "%d/%m/%Y %H:%M:%S", &ti);
            }

            ESP_LOGI(TAG, "[FSM] Alive: now=%s state=%s credit=%ldc heap=%lu",
                     now_buf,
                     fsm_state_to_string(fsm.state),
                     (long)fsm.credit_cents,
                     (unsigned long)esp_get_free_heap_size());
        }

        bool left_program_state = (state_before == FSM_STATE_RUNNING || state_before == FSM_STATE_PAUSED) &&
                                  (fsm.state == FSM_STATE_IDLE || fsm.state == FSM_STATE_CREDIT);
        bool forced_stop_requested = event_received &&
                                     event.type == FSM_INPUT_EVENT_PROGRAM_STOP &&
                                     event.aux_u32 == 0U;
        bool credit_forced_end = event_received &&
                                 event.type == FSM_INPUT_EVENT_CREDIT_ENDED;
        bool natural_cycle_end = left_program_state && !forced_stop_requested && !credit_forced_end;

        if (left_program_state) {
            tasks_apply_n_run();

            if (cfg && cfg->sensors.cctalk_enabled &&
                (cctalk_forced_stop_for_program || cctalk_forced_stop_for_vcd)) {
                if (tasks_publish_cctalk_control_event(ACTION_ID_CCTALK_START)) {
                    cctalk_forced_stop_for_program = false;
                    cctalk_forced_stop_for_vcd = false;
                    ESP_LOGI(TAG, "[M] Gettoniera CCTALK riabilitata al termine del programma");
                } else {
                    ESP_LOGW(TAG, "[M] Richiesta start CCTALK non pubblicata al termine del programma");
                }
            }

            if (cfg && cfg->scanner.enabled && scanner_forced_off_for_program) {
                esp_err_t setup_err = usb_cdc_scanner_send_setup_command();
                esp_err_t on_err = usb_cdc_scanner_send_on_command();
                if (setup_err == ESP_OK && on_err == ESP_OK) {
                    scanner_forced_off_for_program = false;
                    ESP_LOGI(TAG, "[M] Scanner QR riabilitato al termine del programma");
                } else {
                    ESP_LOGW(TAG,
                             "[M] Riabilitazione scanner al termine del programma fallita (setup=%s on=%s)",
                             esp_err_to_name(setup_err),
                             esp_err_to_name(on_err));
                }
            }

            bool hardware_session_open = false;
            if (cfg && cfg->sensors.mdb_enabled && cfg->mdb.cashless_en &&
                mdb_cashless_get_device_count() > 0) {
                const mdb_cashless_device_t *device = mdb_cashless_get_device(0);
                if (device && device->session_open) {
                    hardware_session_open = true;
                }
            }
            bool keep_cashless_active = (cfg && cfg->sensors.mdb_enabled && cfg->mdb.cashless_en &&
                                         ((fsm.state == FSM_STATE_CREDIT &&
                                           fsm.session_source == FSM_SESSION_SOURCE_CARD) ||
                                          hardware_session_open));
            if (cfg && cfg->sensors.mdb_enabled && cfg->mdb.cashless_en) {
                if (keep_cashless_active) {
                    ESP_LOGI(TAG, "[M] Mantengo cashless attivo: sessione CARD ancora valida o hardware session open");
                } else {
                    size_t mdb_count = mdb_cashless_get_device_count();
                    for (size_t i = 0; i < mdb_count; ++i) {
                        mdb_cashless_reset_device(i);
                    }
                    ESP_LOGI(TAG, "[M] MDB cashless reattivato dopo fine programma");
                }
            }

            if (state_before == FSM_STATE_RUNNING || state_before == FSM_STATE_PAUSED) {
                ESP_LOGI(TAG, "[M] PROGRAMMA TERMINATO: %s", fsm.running_program_name[0] ? fsm.running_program_name : "<programma>");
            }

            esp_err_t clear_err = web_ui_program_clear_outputs();
            if (clear_err == ESP_OK) {
                fsm_append_message("Reset relay eseguito in uscita da RUNNING/PAUSED");
                ESP_LOGI(TAG,
                         "[M] Reset relay applicato (stop_forzato=%d, credito_finito=%d, fine_ciclo=%d)",
                         forced_stop_requested ? 1 : 0,
                         credit_forced_end ? 1 : 0,
                         natural_cycle_end ? 1 : 0);
            } else {
                ESP_LOGW(TAG,
                         "[M] Reset relay fallito in uscita da programma: %s",
                         esp_err_to_name(clear_err));
            }
        }

        if ((state_before != fsm.state) && fsm.state == FSM_STATE_OUT_OF_SERVICE && cfg) {
            if (cfg->sensors.cctalk_enabled) {
                (void)tasks_publish_cctalk_control_event(ACTION_ID_CCTALK_STOP);
            }
            if (cfg->scanner.enabled) {
                (void)usb_cdc_scanner_send_off_command();
            }
        }

        if ((state_before != fsm.state) && cfg && cfg->display.enabled) {
            bool defer_page_switch_after_program_end =
                ((state_before == FSM_STATE_RUNNING || state_before == FSM_STATE_PAUSED) &&
                 (fsm.state == FSM_STATE_ADS || fsm.state == FSM_STATE_IDLE));

            if (defer_page_switch_after_program_end) {
                ESP_LOGI(TAG,
                         "[M] Rimando cambio pagina LVGL (%s) per mostrare MDR a fine ciclo",
                         (fsm.state == FSM_STATE_ADS) ? "ADS" : "IDLE");
            } else if (fsm.state == FSM_STATE_ADS) {
                lvgl_panel_show_ads_page();
            } else if (fsm.state == FSM_STATE_IDLE) {
                if (cfg->display.ads_enabled) {
                    lvgl_panel_show_ads_page();
                } else {
                    lvgl_panel_show_main_page();
                }
            } else if (fsm.state == FSM_STATE_CREDIT &&
                       (state_before == FSM_STATE_ADS ||
                        state_before == FSM_STATE_OUT_OF_SERVICE)) {
                lvgl_panel_show_main_page();
            } else if (fsm.state == FSM_STATE_OUT_OF_SERVICE) {
                const char *agent_name = tasks_agent_name((agn_id_t)fsm.out_of_service_agent);
                const char *reason_key = fsm.out_of_service_reason[0]
                                             ? fsm.out_of_service_reason
                                             : (active_oos.reason_key[0]
                                                    ? active_oos.reason_key
                                                    : "out_of_service_reason_generic");
                const char *reason_fallback = active_oos.reason_fallback[0]
                                                  ? active_oos.reason_fallback
                                                  : "Errore sistema";
                lvgl_panel_show_out_of_service_reason(reason_key,
                                                      reason_fallback,
                                                      agent_name);
            }
        }

        if (event_received &&
            ((event.type == FSM_INPUT_EVENT_PROGRAM_SELECTED &&
              state_before == FSM_STATE_CREDIT &&
              fsm.state == FSM_STATE_RUNNING) ||
             (event.type == FSM_INPUT_EVENT_CARD_VEND_APPROVED &&
              state_before == FSM_STATE_CREDIT &&
              fsm.state == FSM_STATE_RUNNING))) {
            ESP_LOGI(TAG, "****************************************");
            ESP_LOGI(TAG, "[M] AVVIO PROGRAMMA: %s",
                     fsm.running_program_name[0] ? fsm.running_program_name : "<nome non disponibile>");
            ESP_LOGI(TAG, "[M] Credito residuo dopo avvio: %ld coin", (long)fsm.credit_cents);
            ESP_LOGI(TAG, "[M] Stato FSM dopo avvio: %s", fsm_state_to_string(fsm.state));
            ESP_LOGI(TAG, "****************************************");
        }

        if (event_received &&
            ((event.type == FSM_INPUT_EVENT_PROGRAM_SELECTED &&
              state_before == FSM_STATE_CREDIT &&
              fsm.state == FSM_STATE_RUNNING) ||
                         (event.type == FSM_INPUT_EVENT_CARD_VEND_APPROVED &&
                            state_before == FSM_STATE_CREDIT &&
                            fsm.state == FSM_STATE_RUNNING) ||
             (event.type == FSM_INPUT_EVENT_PROGRAM_SWITCH &&
              (state_before == FSM_STATE_RUNNING || state_before == FSM_STATE_PAUSED) &&
              fsm.state == FSM_STATE_RUNNING))) {
            publish_program_payment_event(&fsm, &event);
        }

        if (changed) {
            ESP_LOGI(TAG, "****************************************");
            ESP_LOGI(TAG, "[FSM] State=%s credit=%ldcoin", fsm_state_to_string(fsm.state), (long)fsm.credit_cents);
            ESP_LOGI(TAG, "****************************************");
        }

        fsm_runtime_publish(&fsm);

        /* give other tasks/idle a chance – prevent task watchdog trigger when
         * mailbox is busy or FSM has a burst of events. 100 ms period may not
         * be enough to break the CPU monopoly. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


/**
 * @brief Gestisce il task per la gestione del touchscreen.
 *
 * Questa funzione si occupa di monitorare e interpretare gli eventi del touchscreen,
 * aggiornando lo stato dell'interfaccia utente di conseguenza.
 *
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 * @return Nessun valore di ritorno.
 */
static void touchscreen_task(void *arg)
{
    esp_lcd_touch_handle_t touch_handle = (esp_lcd_touch_handle_t)arg;
    TickType_t last_wake = xTaskGetTickCount();
    
    if (!touch_handle) {
        ESP_LOGE("TOUCH", "Touch handle is NULL, task terminating");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI("TOUCH", "Touch task started with valid handle");
    
    /* Memorizza il tocco precedente per evitare log ripetuti quando non cambia nulla */
    bool prev_present = false;
    esp_lcd_touch_point_data_t prev = {0};
    const int MOVEMENT_THRESHOLD = 3; /* pixel */

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(50)); // Polling ogni 50 ms

        // Legge i dati del touchscreen
        esp_lcd_touch_read_data(touch_handle);

        // Get touch points using new API
        esp_lcd_touch_point_data_t touch_data[1];
        uint8_t touch_points = 0;

        esp_lcd_touch_get_data(touch_handle, touch_data, &touch_points, 1);

        if (touch_points > 0) {
            bool significant = !prev_present ||
                abs((int)touch_data[0].x - (int)prev.x) >= MOVEMENT_THRESHOLD ||
                abs((int)touch_data[0].y - (int)prev.y) >= MOVEMENT_THRESHOLD ||
                touch_data[0].track_id != prev.track_id ||
                touch_data[0].strength != prev.strength;

            if (significant) {
                ESP_LOGI("TOUCH", "Touch detected: x=%d, y=%d, strength=%d, track_id=%d", 
                         touch_data[0].x, touch_data[0].y, touch_data[0].strength, touch_data[0].track_id);
                {
                    fsm_input_event_t ev = {
                        .from = AGN_ID_TOUCH,
                        .to = {AGN_ID_FSM},
                        .action = ACTION_ID_NONE,
                        .type = FSM_INPUT_EVENT_TOUCH,
                        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
                        .value_i32 = 0,
                        .value_u32 = 0,
                        .aux_u32 = 0,
                        .text = {0},
                    };
                    (void)fsm_event_publish(&ev, 0);
                }
                prev = touch_data[0];
                prev_present = true;
            }
        } else {
            if (prev_present) {
                ESP_LOGI("TOUCH", "Touch released (last x=%d, y=%d, track_id=%d)", prev.x, prev.y, prev.track_id);
                prev_present = false;
                prev.x = prev.y = prev.strength = prev.track_id = 0;
            }
        }
    }
}


/** @brief Gestisce la task LVGL.
 *  @param arg Argomento della task, non utilizzato.
 *  @return Nessun valore di ritorno. */
static void lvgl_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task per l'aggiornamento del tempo NTP.
 *
 * Questa funzione viene eseguita in un task separato e si occupa di sincronizzare
 * l'orologio del sistema con un server NTP.
 *
 * @param arg Puntatore a dati di input (non utilizzato in questa funzione).
 * @return Nessun valore di ritorno.
 */
static void ntp_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;

    ESP_LOGI(TAG, "[NTP] NTP task started, period: %d seconds", 600);

    /* Attende che la rete sia pronta prima del primo tentativo.
     * L'Ethernet/Wi-Fi può impiegare alcuni secondi per ottenere un IP. */
    vTaskDelay(pdMS_TO_TICKS(30000));

    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        device_config_t *cfg = device_config_get();

        if (cfg->ntp_enabled) {
            ESP_LOGI(TAG, "[NTP] Checking NTP sync...");
            esp_err_t ret = init_sync_ntp();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "[NTP] NTP sync request sent (completamento asincrono)");
            } else {
                ESP_LOGW(TAG, "[NTP] NTP sync request failed");
            }
        } else {
            ESP_LOGD(TAG, "[NTP] NTP disabled in configuration");
        }

        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

// wrapper per lo scanner USB: inizializza il driver e poi entra nella sua routine

/**
 * @brief Recupera il tempo di attesa scanner da configurazione.
 */
static uint32_t scanner_get_cooldown_ms(void)
{
    uint32_t cooldown_ms = SCANNER_QR_COOLDOWN_DEFAULT_MS;
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->scanner.cooldown_ms > 0) {
        cooldown_ms = cfg->scanner.cooldown_ms;
    }

    if (cooldown_ms < SCANNER_QR_COOLDOWN_MIN_MS) {
        cooldown_ms = SCANNER_QR_COOLDOWN_MIN_MS;
    }
    if (cooldown_ms > SCANNER_QR_COOLDOWN_MAX_MS) {
        cooldown_ms = SCANNER_QR_COOLDOWN_MAX_MS;
    }
    return cooldown_ms;
}

/**
 * @brief Verifica se il cooldown scanner è ancora attivo.
 */
static bool scanner_cooldown_is_active(TickType_t now)
{
    if (!s_scanner_cooldown_active) {
        return false;
    }
    return ((int32_t)(now - s_scanner_cooldown_until) < 0);
}


/**
 * @brief Riattiva lo scanner al termine del cooldown.
 */
static void scanner_cooldown_tick(void)
{
    if (tasks_is_out_of_service_state()) {
        (void)usb_cdc_scanner_send_off_command();
        s_scanner_reenable_pending = false;
        s_scanner_cooldown_active = false;
        s_scanner_reenable_attempts = 0;
        return;
    }

    if (!s_scanner_reenable_pending) {
        return;
    }

    TickType_t now = xTaskGetTickCount();
    if ((int32_t)(now - s_scanner_cooldown_until) < 0) {
        return;
    }

    esp_err_t on_err = usb_cdc_scanner_send_on_command();
    if (on_err == ESP_OK) {
        s_scanner_reenable_pending = false;
        s_scanner_cooldown_active = false;
        s_scanner_reenable_attempts = 0;
        ESP_LOGI("SCANNER", "[M] Scanner riattivato dopo cooldown di %lu ms",
                 (unsigned long)s_scanner_cooldown_active_ms);
    } else {
        s_scanner_reenable_attempts++;

        if ((s_scanner_reenable_attempts % 10U) == 1U) {
            ESP_LOGW("SCANNER", "[M] Riattivazione scanner fallita (tentativo=%lu): %s",
                     (unsigned long)s_scanner_reenable_attempts,
                     esp_err_to_name(on_err));
        }

        if (s_scanner_reenable_attempts >= 5U) {
            esp_err_t setup_err = usb_cdc_scanner_send_setup_command();
            if (setup_err == ESP_OK) {
                on_err = usb_cdc_scanner_send_on_command();
                if (on_err == ESP_OK) {
                    s_scanner_reenable_pending = false;
                    s_scanner_cooldown_active = false;
                    s_scanner_reenable_attempts = 0;
                    ESP_LOGI("SCANNER", "[M] Scanner riattivato con fallback setup+on");
                    return;
                }
            }

            if ((s_scanner_reenable_attempts % 10U) == 1U) {
                ESP_LOGW("SCANNER", "[M] Fallback setup+on fallito (setup=%s on=%s)",
                         esp_err_to_name(setup_err),
                         esp_err_to_name(on_err));
            }
        }

        s_scanner_cooldown_until = now + pdMS_TO_TICKS(500);
    }
}

/**
 * @brief Task dedicato alla riattivazione scanner dopo cooldown.
 */
static void scanner_cooldown_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t period = (param && param->period_ticks > 0) ? param->period_ticks : pdMS_TO_TICKS(100);

    ESP_LOGI("SCANNER", "[M] scanner_cooldown_task avviato (period=%lu ms)",
             (unsigned long)pdTICKS_TO_MS(period));

    while (true) {
        scanner_cooldown_tick();
        vTaskDelay(period);
    }
}

/**
 * @brief Callback chiamata quando viene rilevato un barcode.
 *
 * Questa funzione viene invocata dal sistema quando viene rilevato un barcode.
 * 
 * @param barcode [in] Il codice barcode rilevato.
 * @return void Non restituisce alcun valore.
 */
static bool scanner_extract_clean_barcode(const char *raw_barcode, char *clean_barcode, size_t clean_len)
{
    if (!raw_barcode || !clean_barcode || clean_len == 0) {
        return false;
    }

    clean_barcode[0] = '\0';

    char printable[FSM_EVENT_TEXT_MAX_LEN] = {0};
    size_t printable_idx = 0;
    for (const unsigned char *p = (const unsigned char *)raw_barcode;
         *p != '\0' && printable_idx < sizeof(printable) - 1;
         ++p) {
        if (*p >= 32 && *p <= 126) {
            printable[printable_idx++] = (char)*p;
        }
    }
    printable[printable_idx] = '\0';

    const char *start = printable;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) {
        len--;
    }
    if (len == 0) {
        return false;
    }

    char trimmed[FSM_EVENT_TEXT_MAX_LEN] = {0};
    if (len >= sizeof(trimmed)) {
        len = sizeof(trimmed) - 1;
    }
    memcpy(trimmed, start, len);
    trimmed[len] = '\0';

    bool looks_like_scanner_frame = (strstr(trimmed, "SCNENA") != NULL) ||
                                    (strstr(trimmed, "SCNMOD") != NULL) ||
                                    (strstr(trimmed, "RRDENA") != NULL) ||
                                    (strstr(trimmed, "CIDENA") != NULL) ||
                                    (strstr(trimmed, "RRDDUR") != NULL) ||
                                    (strchr(trimmed, '#') != NULL && strchr(trimmed, ';') != NULL);

    if (!looks_like_scanner_frame) {
        if (len >= clean_len) {
            len = clean_len - 1;
        }
        memcpy(clean_barcode, trimmed, len);
        clean_barcode[len] = '\0';
        return true;
    }

    size_t best_start = 0;
    size_t best_len = 0;
    size_t run_start = 0;
    size_t run_len = 0;
    for (size_t i = 0;; ++i) {
        char c = trimmed[i];
        if (isdigit((unsigned char)c)) {
            if (run_len == 0) {
                run_start = i;
            }
            run_len++;
        } else {
            if (run_len > 0) {
                if (run_len > best_len || (run_len == best_len && run_start >= best_start)) {
                    best_start = run_start;
                    best_len = run_len;
                }
                run_len = 0;
            }
            if (c == '\0') {
                break;
            }
        }
    }

    if (best_len < 6) {
        return false;
    }

    if (best_len >= clean_len) {
        best_len = clean_len - 1;
    }
    memcpy(clean_barcode, &trimmed[best_start], best_len);
    clean_barcode[best_len] = '\0';
    return true;
}


/**
 * @brief Callback chiamata quando viene rilevato un barcode.
 * 
 * Questa funzione viene invocata quando il sistema rileva un barcode.
 * 
 * @param barcode [in] Il codice barcode rilevato.
 * @return void Nessun valore di ritorno.
 */
static void scanner_on_barcode_cb(const char *barcode)
{
    if (tasks_is_out_of_service_state()) {
        ESP_LOGW("SCANNER", "[M] Lettura scanner ignorata: stato OUT_OF_SERVICE");
        return;
    }

    if (!barcode || barcode[0] == '\0') {
        return;
    }

    char clean_barcode[FSM_EVENT_TEXT_MAX_LEN] = {0};
    if (!scanner_extract_clean_barcode(barcode, clean_barcode, sizeof(clean_barcode))) {
        ESP_LOGW("SCANNER", "[M] Barcode scanner non valido/scartato");
        return;
    }

    TickType_t now = xTaskGetTickCount();
    if (scanner_cooldown_is_active(now)) {
        ESP_LOGI("SCANNER", "[M] Barcode ignorato: scanner in cooldown");
        return;
    }

    uint32_t cooldown_ms = scanner_get_cooldown_ms();
    s_scanner_cooldown_active = true;
    s_scanner_reenable_pending = true;
    s_scanner_reenable_attempts = 0;
    s_scanner_cooldown_active_ms = cooldown_ms;
    s_scanner_cooldown_until = now + pdMS_TO_TICKS(cooldown_ms);

    esp_err_t off_err = usb_cdc_scanner_send_off_command();
    if (off_err == ESP_OK) {
        ESP_LOGI("SCANNER", "[M] Scanner spento per %lu ms dopo lettura QR",
                 (unsigned long)cooldown_ms);
    } else {
        ESP_LOGW("SCANNER", "[M] Spegnimento scanner fallito: %s",
                 esp_err_to_name(off_err));
    }

    ESP_LOGI("SCANNER", "[M] Barcode clean: %s", clean_barcode);
    /* Barcode readings: use distinct tag so UI can display readings separately */
    web_ui_add_log("INFO", "SCANNER_DATA", clean_barcode);
    {
        fsm_input_event_t ev = {
            .from = AGN_ID_USB_CDC_SCANNER,
            .to = {AGN_ID_HTTP_SERVICES},
            .action = ACTION_ID_USB_CDC_SCANNER_READ,
            .type = FSM_INPUT_EVENT_QR_SCANNED,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = 0,
            .value_u32 = 0,
            .aux_u32 = 0,
            .text = {0},
        };
        strncpy(ev.text, clean_barcode, sizeof(ev.text)-1);
        (void)fsm_event_publish(&ev, 0);
    }
}


/**
 * @brief Normalizza il testo barcode rimuovendo spazi iniziali/finali.
 */
static void normalize_barcode_text(const char *input, char *output, size_t output_len)
{
    if (!output || output_len == 0) {
        return;
    }
    output[0] = '\0';
    if (!input) {
        return;
    }

    while (*input && isspace((unsigned char)*input)) {
        input++;
    }

    size_t len = strlen(input);
    while (len > 0 && isspace((unsigned char)input[len - 1])) {
        len--;
    }

    if (len >= output_len) {
        len = output_len - 1;
    }

    memcpy(output, input, len);
    output[len] = '\0';
}


/**
 * @brief Pubblica verso FSM il credito ECD ricavato da QR.
 */
static void publish_qr_credit_event(const char *customer_code, int32_t ecd_amount)
{
    if (ecd_amount <= 0) {
        return;
    }

    fsm_input_event_t credit_ev = {
        .from = AGN_ID_HTTP_SERVICES,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PAYMENT_ACCEPTED,
        .type = FSM_INPUT_EVENT_QR_CREDIT,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = ecd_amount,
        .value_u32 = 0,
        .aux_u32 = 0,
        .text = {0},
    };
    if (customer_code && customer_code[0] != '\0') {
        strncpy(credit_ev.text, customer_code, sizeof(credit_ev.text) - 1);
    }

    if (!fsm_event_publish(&credit_ev, pdMS_TO_TICKS(50))) {
        ESP_LOGE(TAG, "[M] Publish QR_CREDIT fallito (customer_code=%s ecd=%ld)",
                 customer_code ? customer_code : "",
                 (long)ecd_amount);
        return;
    }

    ESP_LOGI(TAG, "[M] QR_CREDIT pubblicato (customer_code=%s ecd=%ld)",
             customer_code ? customer_code : "",
             (long)ecd_amount);

    tasks_display_credit_if_epaper(ecd_amount);
}

static void tasks_display_credit_if_epaper(int32_t credit_cents)
{
    const device_config_t *cfg = device_config_get();
    if (cfg && cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232) {
        esp_err_t text_err = rs232_epaper_display_credit_big(credit_cents);
        if (text_err != ESP_OK) {
            ESP_LOGW(TAG, "[M] E-Paper RS232 display credit failed: %s", esp_err_to_name(text_err));
        }
    }
}

static void tasks_display_epaper_program_status(const web_ui_program_entry_t *entry,
                                               bool is_active_program)
{
    if (!entry) {
        return;
    }

    const device_config_t *cfg = device_config_get();
    if (!cfg || cfg->display.type != DEVICE_DISPLAY_TYPE_EPAPER_RS232) {
        return;
    }

    char message[128];
    if (is_active_program) {
        snprintf(message, sizeof(message), "§Programma %s\rPausa attiva", entry->name);
    } else {
        snprintf(message, sizeof(message), "§Avvio %s\rCosto %u", entry->name, (unsigned)entry->price_units);
    }

    esp_err_t text_err = rs232_epaper_display_text(message);
    if (text_err != ESP_OK) {
        ESP_LOGW(TAG, "[M] E-Paper RS232 display program status failed: %s", esp_err_to_name(text_err));
    }
}

/**
 * @brief Pubblica verso HTTP_SERVICES l'evento di pagamento all'attivazione programma.
 */
static void publish_program_payment_event(const fsm_ctx_t *ctx, const fsm_input_event_t *source_event)
{
    if (!ctx) {
        return;
    }

    int32_t payment_amount = (ctx->running_price_units > 0) ? ctx->running_price_units : 0;
    const char *service_code = (ctx->running_program_name[0] != '\0')
                                   ? ctx->running_program_name
                                   : ((source_event && source_event->text[0] != '\0') ? source_event->text : "SER1");

    fsm_session_source_t payment_source = (ctx->payment_credit_source != FSM_SESSION_SOURCE_NONE)
                                           ? ctx->payment_credit_source
                                           : ctx->session_source;

    fsm_input_event_t pay_ev = {
        .from = AGN_ID_FSM,
        .to = {AGN_ID_HTTP_SERVICES},
        .action = ACTION_ID_PROGRAM_SELECTED,
        .type = FSM_INPUT_EVENT_PROGRAM_SELECTED,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = payment_amount,
        .value_u32 = 0,
        .aux_u32 = (uint32_t)payment_source,
        .text = {0},
        .customer_code = {0},
    };
    strncpy(pay_ev.text, service_code, sizeof(pay_ev.text) - 1);
    snprintf(pay_ev.customer_code, sizeof(pay_ev.customer_code), "%s",
             ctx->customer_code[0] ? ctx->customer_code : "");

    if (!fsm_event_publish(&pay_ev, pdMS_TO_TICKS(50))) {
        ESP_LOGE(TAG, "[M] Publish PAYMENT_EVENT fallito (service=%s amount=%ld)",
                 service_code,
                 (long)payment_amount);
        return;
    }

    ESP_LOGI(TAG, "[M] PAYMENT_EVENT pubblicato (service=%s amount=%ld source=%u customer=%s)",
             service_code,
             (long)payment_amount,
             (unsigned)ctx->session_source,
             pay_ev.customer_code[0] ? pay_ev.customer_code : "<none>");
}

static http_services_payment_type_t tasks_payment_type_from_session_source(fsm_session_source_t source)
{
    switch (source) {
        case FSM_SESSION_SOURCE_QR:
            return HTTP_SERVICES_PAYMENT_TYPE_WALLET;
        case FSM_SESSION_SOURCE_CARD:
            return HTTP_SERVICES_PAYMENT_TYPE_CASHL;
        case FSM_SESSION_SOURCE_CCTALK:
            return HTTP_SERVICES_PAYMENT_TYPE_CASH;
        case FSM_SESSION_SOURCE_COIN:
            return HTTP_SERVICES_PAYMENT_TYPE_COIN;
        case FSM_SESSION_SOURCE_TOUCH:
        case FSM_SESSION_SOURCE_KEY:
        case FSM_SESSION_SOURCE_NONE:
        default:
            return HTTP_SERVICES_PAYMENT_TYPE_CASH;
    }
}


/**
 * @brief Task consumer eventi per AGN_ID_HTTP_SERVICES.
 *
 * Riceve barcode scanner da mailbox FSM, interroga il backend (`getcustomers`) e
 * traduce `amount` in credito ECD verso FSM.
 */
static void http_services_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;

    // La mailbox FSM è già inizializzata in init.c, non chiamare qui
    // per evitare race condition durante il boot

    while (true) {
        fsm_input_event_t event;
        if (!fsm_event_receive(&event, AGN_ID_HTTP_SERVICES, param->period_ticks)) {
            continue;
        }

        if (event.type == FSM_INPUT_EVENT_PROGRAM_SELECTED && event.action == ACTION_ID_PROGRAM_SELECTED) {
            const char *service_code = (event.text[0] != '\0') ? event.text : "SER1";
            http_services_paid_service_code_t payment_service = http_services_paid_service_code_from_string(service_code);
            http_services_payment_type_t payment_type = tasks_payment_type_from_session_source((fsm_session_source_t)event.aux_u32);
            int32_t amount = (event.value_i32 > 0) ? event.value_i32 : 0;

            if (payment_type == HTTP_SERVICES_PAYMENT_TYPE_CASH &&
                event.customer_code[0] != '\0' &&
                (fsm_session_source_t)event.aux_u32 == FSM_SESSION_SOURCE_NONE) {
                payment_type = HTTP_SERVICES_PAYMENT_TYPE_WALLET;
                ESP_LOGI(TAG, "[M] payment type overridden to WALL because customer_code present and source NONE");
            }

            ESP_LOGI(TAG, "[M] payment event received service=%s source=%u type=%s amount=%ld customer=%s",
                     service_code,
                     (unsigned)event.aux_u32,
                     http_services_payment_type_to_string(payment_type),
                     (long)amount,
                     event.customer_code[0] ? event.customer_code : "<none>");
            
            /* [C] Usa customer_code dall'evento FSM, altrimenti fallback su s_last_customer */
            http_services_customer_t customer = {0};
            const http_services_customer_t *customer_ptr = NULL;
            
            if (event.customer_code[0] != '\0') {
                customer.valid = true;
                snprintf(customer.code, sizeof(customer.code), "%s", event.customer_code);
                customer_ptr = &customer;
            } else if (s_last_customer_available) {
                customer_ptr = &s_last_customer;
            }

            if (!customer_ptr) {
                ESP_LOGW(TAG, "[M] payment: customer non disponibile, invio campi customer vuoti");
            }

            http_services_payment_response_t pay_resp;
            esp_err_t pay_err = http_services_payment(customer_ptr, amount, payment_service, payment_type, &pay_resp);
            if (pay_err != ESP_OK || pay_resp.common.iserror) {
                ESP_LOGE(TAG,
                         "[M] payment fallita service=%s type=%s amount=%ld err=%s code=%ld des=%s",
                         http_services_paid_service_code_to_string(payment_service),
                         http_services_payment_type_to_string(payment_type),
                         (long)amount,
                         esp_err_to_name(pay_err),
                         (long)pay_resp.common.codeerror,
                         pay_resp.common.deserror);
            } else {
                ESP_LOGI(TAG,
                         "[M] payment OK service=%s type=%s amount=%ld paymentid=%ld",
                         http_services_paid_service_code_to_string(payment_service),
                         http_services_payment_type_to_string(payment_type),
                         (long)amount,
                         (long)pay_resp.paymentid);
            }
            continue;
        }

        if (event.type != FSM_INPUT_EVENT_QR_SCANNED || event.action != ACTION_ID_USB_CDC_SCANNER_READ) {
            continue;
        }

        char barcode[FSM_EVENT_TEXT_MAX_LEN] = {0};
        normalize_barcode_text(event.text, barcode, sizeof(barcode));
        if (barcode[0] == '\0') {
            ESP_LOGW(TAG, "[M] QR scanner: barcode vuoto, evento ignorato");
            continue;
        }

        ESP_LOGI(TAG, "[M] QR scanner: lookup customer code=%s", barcode);

        http_services_getcustomers_response_t gc_resp;
        esp_err_t gc_err = http_services_getcustomers(barcode, "", &gc_resp);
        if (gc_err != ESP_OK) {
            ESP_LOGE(TAG, "[M] getcustomers fallita per barcode=%s (%s)",
                     barcode,
                     esp_err_to_name(gc_err));
            continue;
        }
        if (gc_resp.common.iserror) {
            ESP_LOGW(TAG, "[M] getcustomers errore server barcode=%s code=%ld des=%s",
                     barcode,
                     (long)gc_resp.common.codeerror,
                     gc_resp.common.deserror);
            continue;
        }

        const http_services_customer_t *selected = NULL;
        for (size_t i = 0; i < gc_resp.customer_count; ++i) {
            const http_services_customer_t *candidate = &gc_resp.customers[i];
            if (!candidate->valid) {
                continue;
            }
            if (candidate->code[0] != '\0' && strcmp(candidate->code, barcode) == 0) {
                selected = candidate;
                break;
            }
            if (!selected) {
                selected = candidate;
            }
        }

        if (!selected) {
            ESP_LOGW(TAG, "[M] QR scanner: nessun customer valido per barcode=%s", barcode);
            continue;
        }

        memset(&s_last_customer, 0, sizeof(s_last_customer));
        memcpy(&s_last_customer, selected, sizeof(*selected));
        if (s_last_customer.code[0] == '\0') {
            strncpy(s_last_customer.code, barcode, sizeof(s_last_customer.code) - 1);
        }
        s_last_customer_available = true;

        if (selected->amount <= 0) {
            ESP_LOGI(TAG, "[M] QR scanner: credito non disponibile per code=%s (amount=%ld)",
                     selected->code,
                     (long)selected->amount);
            continue;
        }

        const char *resolved_customer_code = selected->code[0] != '\0'
                                                 ? selected->code
                                                 : barcode;
        publish_qr_credit_event(resolved_customer_code, selected->amount);
    }
}


/**
 * @brief Wrapper per la gestione del task del scanner USB.
 *
 * Questa funzione viene utilizzata per avviare e gestire il task del scanner USB.
 * È responsabile della creazione e del lancio del task che si occupa della scansione dei dispositivi USB.
 *
 * @param arg Puntatore a dati aggiuntivi che possono essere passati al task.
 * @return Nessun valore di ritorno.
 */
static void usb_scanner_task_wrapper(void *arg)
{
    usb_cdc_scanner_config_t cfg = {.on_barcode = scanner_on_barcode_cb};
    usb_cdc_scanner_init(&cfg);
    // usa la task routine del componente scanner che gestisce sia la modalità reale (CDC-ACM) che simulata
    usb_cdc_scanner_task(NULL);
}

/**
 * @brief Task consumer per i messaggi audio in coda FSM.
 *
 * Consuma eventi indirizzati a AGN_ID_AUDIO e, quando riceve ACTION_ID_PLAY_AUDIO,
 * riproduce il file audio presente su SPIFFS.
 */
static void audio_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t period_ticks = (param && param->period_ticks > 0)
                                ? param->period_ticks
                                : pdMS_TO_TICKS(100);

    esp_err_t init_err = audio_player_init();
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG, "[M] Audio init fallita: %s", esp_err_to_name(init_err));
    } else {
        ESP_LOGI(TAG, "[M] Audio task pronta");
    }

    while (true) {
        fsm_input_event_t event = {0};
        if (!fsm_event_receive(&event, AGN_ID_AUDIO, period_ticks)) {
            continue;
        }

        if (event.action != ACTION_ID_PLAY_AUDIO) {
            continue;
        }

        if (event.text[0] == '\0') {
            ESP_LOGW(TAG, "[M] PLAY_AUDIO ignorato: path vuoto");
            continue;
        }

        device_config_t *cfg = device_config_get();
        const char *test_suffix = "/test.wav";
        size_t path_len = strlen(event.text);
        size_t test_suffix_len = strlen(test_suffix);
        bool is_test_wav = (path_len >= test_suffix_len) &&
                           (strcmp(event.text + (path_len - test_suffix_len), test_suffix) == 0);

        /* [M] Con audio disabilitato consentiamo solo il file di test. */
        if (cfg && !cfg->audio.enabled && !is_test_wav) {
            ESP_LOGI(TAG,
                     "[M] PLAY_AUDIO ignorato: audio disabilitato in config (%s)",
                     event.text);
            continue;
        }
        if (cfg && !cfg->audio.enabled && is_test_wav) {
            ESP_LOGI(TAG, "[M] PLAY_AUDIO consentito con audio disabilitato (solo test.wav)");
        }

        uint8_t volume = (cfg != NULL) ? cfg->audio.volume : 75U;
        if (audio_player_set_volume(volume) != ESP_OK) {
            ESP_LOGW(TAG, "[M] Impostazione volume audio fallita (%u)", (unsigned)volume);
        }

        esp_err_t play_err = audio_player_play_file(event.text);
        if (play_err != ESP_OK) {
            ESP_LOGW(TAG,
                     "[M] PLAY_AUDIO fallito (%s): %s",
                     event.text,
                     esp_err_to_name(play_err));
        } else {
            ESP_LOGI(TAG, "[M] PLAY_AUDIO eseguito: %s", event.text);
        }
    }
}

/* Wrapper CCtalk: l'hardware UART è già inizializzato da init.c (cctalk_driver_init).
 * Questo wrapper entra direttamente nel loop di ricezione. */

/**
 * @brief Wrapper per l'engine CCTalk.
 *
 * Questa funzione agisce come un wrapper per l'engine CCTalk, gestendo
 * le operazioni di comunicazione e processamento dei dati.
 *
 * @param arg Puntatore a dati aggiuntivi necessari per l'operazione.
 * @return Nessun valore di ritorno.
 */
static void cctalk_engine_wrapper(void *arg)
{
    cctalk_task_run(NULL);
}

/* Wrapper MDB engine: l'hardware UART è già inizializzato da init.c (mdb_init).
 * Questo wrapper entra direttamente nel loop di polling. */

/**
 * @brief Wrapper per l'engine di gestione del database.
 *
 * Questa funzione agisce come un wrapper per l'engine di gestione del database,
 * permettendo di eseguire operazioni di lettura e scrittura su un database.
 *
 * @param arg Puntatore a un argomento generico che può essere utilizzato per passare
 *            informazioni aggiuntive alla funzione.
 * @return Nessun valore di ritorno.
 */
static void mdb_engine_wrapper(void *arg)
{
    TickType_t period_ticks = (arg != NULL) ? ((task_param_t *)arg)->period_ticks : pdMS_TO_TICKS(20);
    mdb_register_cashless_credit_callback(tasks_publish_card_credit_event);
    mdb_register_cashless_vend_callback(tasks_publish_card_vend_result_event);
    mdb_engine_run((void *)(uintptr_t)period_ticks);
}

/* Wrapper SD monitor: non richiede init hardware preventivo (il task configura
 * autonomamente il GPIO di card-detect). */

/**
 * @brief Wrapper per la funzione di monitoraggio.
 *
 * Questa funzione agisce come un wrapper per la funzione di monitoraggio,
 * permettendo di passare un argomento generico a una funzione di monitoraggio.
        .period_ticks = pdMS_TO_TICKS(20),
 * @param arg Puntatore a un argomento generico da passare alla funzione di monitoraggio.
 * @return Nessun valore di ritorno.
 */
static void sd_monitor_wrapper(void *arg)
{
    sd_card_monitor_run(NULL);
}

static task_param_t s_tasks[] = {
    {
        .name = "ws2812",
        .state = TASK_STATE_RUN,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = ws2812_task,
        .stack_words = 4096,                  /* RISC-V: 4KB; ESP_LOGW in retry loop */
        .stack_caps = MALLOC_CAP_INTERNAL,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "eeprom",
        .state = TASK_STATE_IDLE,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(1000),
        .task_fn = eeprom_task,
        .stack_words = 2048,                  /* RISC-V: StackType_t=1B; ~2KB reali */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "io_expander",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = io_expander_task,
        .stack_words = 2048,                  /* RISC-V: StackType_t=1B; ~2KB reali */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "sht40",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(2000),
        .task_fn = sht40_task,
        .stack_words = 8192,                  /* RISC-V: 8KB; driver I2C + float + ESP_LOG */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "rs232",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(10),
        .task_fn = rs232_task,
        .stack_words = 4096,                  /* RISC-V: 4KB; skeleton con margine */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "rs485",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = rs485_task,
        .stack_words = 4096,                  /* RISC-V: 4KB; skeleton con margine */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "mdb",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(10),
        .task_fn = mdb_task,
        .stack_words = 4096,                  /* RISC-V: 4KB; skeleton con margine */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "fsm",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = fsm_task,
        .stack_words = 32768,                 /* RISC-V: 32KB; logica FSM + ESP_LOG + cJSON */
        .stack_caps = MALLOC_CAP_INTERNAL,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "digital_io",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(50),
        .task_fn = digital_io_task,
        .stack_words = 4096,
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "io_process",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = io_process_task,
        .stack_words = 4096,
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "http_services",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = http_services_task,
        .stack_words = 32768,                 /* RISC-V: 32KB; esp_http_client + JSON parsing */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "scanner_cooldown",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = scanner_cooldown_task,
        .stack_words = 4096,                  /* RISC-V: 4KB; polling cooldown + comando ON scanner */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "audio",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = audio_task,
        .stack_words = 12288,                 /* RISC-V: 12KB; parsing WAV + streaming I2S */
        .stack_caps = MALLOC_CAP_INTERNAL,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "touchscreen",
        .state = TASK_STATE_IDLE,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(20),
        .task_fn = touchscreen_task,
        .stack_words = 8192,                  /* RISC-V: 8KB; polling touch + event publish */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "lvgl",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(16),
        .task_fn = lvgl_task,
        .stack_words = 65536,                 /* RISC-V: 256KB; LVGL stack frame profondo - aumentato per evitare overflow durante rendering */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "ntp",
        .state = TASK_STATE_RUN,
        .priority = 3,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(600000),
        .task_fn = ntp_task,
        .stack_words = 32768,                 /* RISC-V: 32KB; esp_http_client + TLS frame profondi */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "usb_scanner",
        .state = TASK_STATE_IDLE,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(1000),
        .task_fn = usb_scanner_task_wrapper,
        .stack_words = 32768,                 /* RISC-V: 32KB; USB host stack profondo */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        /* Task di ricezione CCtalk. Richiede che cctalk_driver_init() sia già
         * stato chiamato da init.c (avviene quando sensors.rs232_enabled=true).
         * Default: IDLE — abilitare via tasks.csv se la periferica è presente. */
        .name = "cctalk_task",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = cctalk_engine_wrapper,
        .stack_words = 4096,                  /* RISC-V: 4KB; UART rx + snprintf + serial monitor */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        /* Polling engine MDB. Richiede che mdb_init() sia già stato chiamato da
         * init.c (avviene quando sensors.mdb_enabled=true).
         * Default: IDLE — abilitare via tasks.csv se la periferica è presente. */
        .name = "mdb_engine",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = mdb_engine_wrapper,
        .stack_words = 4096,                  /* RISC-V: 4KB; polling state-machine MDB */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        /* Monitor hot-plug SD. Configura autonomamente il GPIO di card-detect e
         * cicla indefinitamente. Avviato sempre (default RUN). */
        .name = "sd_monitor",
        .state = TASK_STATE_RUN,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = sd_monitor_wrapper,
        .stack_words = 4096,                  /* RISC-V: 4KB; gpio_config + poll + ESP_LOG */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
};

// -----------------------------------------------------------------------------
// Caricatore configurazione (CSV da SPIFFS)
// -----------------------------------------------------------------------------

static task_param_t *find_task_by_name(const char *name)
{
    if (name && strcmp(name, "mdb") == 0) {
        name = "mdb_engine";
    }

    for (size_t i = 0; i < sizeof(s_tasks) / sizeof(s_tasks[0]); ++i) {
        if (strcmp(s_tasks[i].name, name) == 0) {
            return &s_tasks[i];
        }
    }
    return NULL;
}

static const char *s_lvgl_test_task_names[] = {
    "ws2812",
    "io_expander",
    "sht40",
    "rs232",
    "rs485",
    "mdb",
    "pwm",
    "usb_scanner",
    "cctalk_task",
    "mdb_engine",
};

static bool s_lvgl_test_tasks_suspended = false;

void tasks_suspend_peripherals_for_lvgl_test(void)
{
    if (s_lvgl_test_tasks_suspended) {
        return;
    }

    for (size_t i = 0; i < sizeof(s_lvgl_test_task_names) / sizeof(s_lvgl_test_task_names[0]); ++i) {
        task_param_t *t = find_task_by_name(s_lvgl_test_task_names[i]);
        if (!t || !t->handle) {
            continue;
        }
        vTaskSuspend(t->handle);
        ESP_LOGI(TAG, "[M] LVGL test: sospesa task %s", t->name);
    }

    s_lvgl_test_tasks_suspended = true;
}

void tasks_resume_peripherals_after_lvgl_test(void)
{
    if (!s_lvgl_test_tasks_suspended) {
        return;
    }

    for (size_t i = 0; i < sizeof(s_lvgl_test_task_names) / sizeof(s_lvgl_test_task_names[0]); ++i) {
        task_param_t *t = find_task_by_name(s_lvgl_test_task_names[i]);
        if (!t || !t->handle) {
            continue;
        }
        vTaskResume(t->handle);
        ESP_LOGI(TAG, "[M] LVGL test: ripresa task %s", t->name);
    }

    s_lvgl_test_tasks_suspended = false;
}


/**
 * @brief Analizza lo stato di una task.
 *
 * Questa funzione prende una stringa che rappresenta lo stato di una task e lo converte in un valore di tipo task_state_t.
 * Se la stringa non è valida, viene restituito lo stato di default.
 *
 * @param [in] s Puntatore alla stringa che rappresenta lo stato della task.
 * @param [in] def Valore di default da restituire se la stringa non è valida.
 * @return Lo stato di tipo task_state_t corrispondente alla stringa fornita, o lo stato di default se la stringa non è valida.
 */
static task_state_t parse_state(const char *s, task_state_t def)
{
    if (!s) return def;
    if (strcasecmp(s, "run") == 0) return TASK_STATE_RUN;
    if (strcasecmp(s, "idle") == 0) return TASK_STATE_IDLE;
    if (strcasecmp(s, "pause") == 0) return TASK_STATE_PAUSE;
    return def;
}


/** @brief Imposta il gestore del touchscreen.
 *  
 *  @param [in] handle Puntatore al gestore del touchscreen.
 *  
 *  @return Nessun valore di ritorno.
 */
void tasks_set_touchscreen_handle(void *handle)
{
    task_param_t *t = find_task_by_name("touchscreen");
    if (t) {
        t->arg = handle;
    }
}


/**
 * @brief Carica la configurazione delle attività da un file.
 *
 * Questa funzione carica la configurazione delle attività dal file specificato
 * dal percorso passato come parametro.
 *
 * @param [in] path Percorso del file da cui caricare la configurazione.
 * @return Nessun valore di ritorno.
 */
void tasks_load_config(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "[M] Impossibile aprire %s; uso configurazioni predefinite", path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 32768) {
        ESP_LOGE(TAG, "[M] tasks.json: dimensione non valida (%ld)", file_size);
        fclose(f);
        return;
    }

    char *buf = malloc((size_t)file_size + 1);
    if (!buf) {
        ESP_LOGE(TAG, "[M] tasks.json: out of memory");
        fclose(f);
        return;
    }
    fread(buf, 1, (size_t)file_size, f);
    fclose(f);
    buf[file_size] = '\0';

    cJSON *arr = cJSON_Parse(buf);
    free(buf);
    if (!arr || !cJSON_IsArray(arr)) {
        ESP_LOGE(TAG, "[M] tasks.json: JSON non valido");
        if (arr) cJSON_Delete(arr);
        return;
    }

    cJSON *obj;
    cJSON_ArrayForEach(obj, arr) {
        cJSON *jname = cJSON_GetObjectItem(obj, "n");
        if (!jname || !cJSON_IsString(jname)) continue;
        const char *name = jname->valuestring;

        task_param_t *t = find_task_by_name(name);
        if (!t) {
            if (strcmp(name, "http_server") == 0) continue;
            ESP_LOGD(TAG, "[M] Task '%s' in JSON non in s_tasks[] — ignorato", name);
            continue;
        }

        cJSON *jstate  = cJSON_GetObjectItem(obj, "s");
        cJSON *jprio   = cJSON_GetObjectItem(obj, "p");
        cJSON *jcore   = cJSON_GetObjectItem(obj, "c");
        cJSON *jperiod = cJSON_GetObjectItem(obj, "m");
        cJSON *jstack  = cJSON_GetObjectItem(obj, "w");
        cJSON *jcaps   = cJSON_GetObjectItem(obj, "k");

        if (jstate && cJSON_IsNumber(jstate)) {
            switch (jstate->valueint) {
                case 1:  t->state = TASK_STATE_RUN;   break;
                case 2:  t->state = TASK_STATE_PAUSE; break;
                default: t->state = TASK_STATE_IDLE;  break;
            }
        }
        if (jprio   && cJSON_IsNumber(jprio))   t->priority     = (UBaseType_t)jprio->valueint;
        if (jcore   && cJSON_IsNumber(jcore))   t->core_id      = (BaseType_t)jcore->valueint;
        if (jperiod && cJSON_IsNumber(jperiod)) t->period_ticks = pdMS_TO_TICKS(jperiod->valueint);
        if (jstack  && cJSON_IsNumber(jstack)) {
            uint32_t val = (uint32_t)jstack->valueint;
            if (val < 512) val = 512;
            t->stack_words = val;
        }
        if (jcaps && cJSON_IsNumber(jcaps)) {
            t->stack_caps = (jcaps->valueint == 1) ? MALLOC_CAP_INTERNAL : MALLOC_CAP_SPIRAM;
        }
    }
    cJSON_Delete(arr);
    ESP_LOGI(TAG, "[M] tasks.json caricato da %s", path);
}

/* ============================================================
 * tasks_set_state_idle_for_mocks
 * Imposta IDLE le task dei moduli con mock DNA_* attivo (= 1).
 * Va chiamata dopo tasks_load_config() e prima di tasks_start_all()
 * in modo che il CSV non possa sovrascrivere la decisione.
 * Aggiungere qui una riga per ogni nuovo mock DNA_*.
 * ============================================================ */

/**
 * @brief Imposta lo stato delle attività su "idle" per i mock.
 *
 * Questa funzione imposta lo stato delle attività su "idle" per i mock.
 * Non ha parametri di input o output.
 * Non restituisce alcun valore.
 */
static void tasks_set_state_idle_for_mocks(void)
{
#if DNA_LED_STRIP == 1
    task_param_t *t = find_task_by_name("ws2812");
    if (t) { t->state = TASK_STATE_IDLE; }
#endif
    /* Nota: sht40_task e io_expander_task hanno mock passthrough
     * (la funzione chiama le API del componente che risponde correttamente),
     * quindi non vanno messi IDLE: la task gira e il mock risponde.
     * Aggiungere qui solo i task che non hanno senso con il mock attivo.
     */
}


/** @brief Avvia tutte le attività del sistema.
 *  
 *  Questa funzione avvia tutte le attività del sistema, preparando il sistema per l'esecuzione.
 *  
 *  @return Nessun valore di ritorno.
 */
void tasks_start_all(void)
{
    tasks_set_state_idle_for_mocks();
    device_config_t *cfg = device_config_get();

    for (size_t i = 0; i < sizeof(s_tasks) / sizeof(s_tasks[0]); ++i) {
        task_param_t *t = &s_tasks[i];

        if (cfg && strcmp(t->name, "usb_scanner") == 0) {
            if (cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232 && cfg->sensors.rs232_enabled) {
                t->state = TASK_STATE_IDLE;
                ESP_LOGI(TAG, "[M] Task %s forzato IDLE (EPAPER_RS232 usa scanner RS232)", t->name);
            } else {
                t->state = cfg->scanner.enabled ? TASK_STATE_RUN : TASK_STATE_IDLE;
            }
        }

        if (cfg && strcmp(t->name, "cctalk_task") == 0) {
            t->state = cfg->sensors.cctalk_enabled
                           ? TASK_STATE_RUN
                           : TASK_STATE_IDLE;
        }

        if (cfg && strcmp(t->name, "rs232") == 0 &&
            cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232 &&
            cfg->sensors.rs232_enabled) {
            t->state = TASK_STATE_RUN;
            ESP_LOGI(TAG, "[M] Task %s forzato RUN (EPAPER_RS232 attivo)", t->name);
        }

        if (cfg && strcmp(t->name, "mdb_engine") == 0) {
            bool coin_runtime_enabled = cfg->mdb.coin_acceptor_en && !cfg->sensors.cctalk_enabled;
            bool mdb_runtime_enabled = cfg->sensors.mdb_enabled &&
                                       (coin_runtime_enabled || cfg->mdb.cashless_en);
            t->state = mdb_runtime_enabled ? TASK_STATE_RUN : TASK_STATE_IDLE;
            ESP_LOGI(TAG,
                     "[M] Task %s forzato %s (mdb=%d coin=%d cashless=%d)",
                     t->name,
                     (t->state == TASK_STATE_RUN) ? "RUN" : "IDLE",
                     cfg->sensors.mdb_enabled ? 1 : 0,
                     coin_runtime_enabled ? 1 : 0,
                     cfg->mdb.cashless_en ? 1 : 0);
        }

        if (strcmp(t->name, "fsm") == 0) {
            ESP_LOGI(TAG, "[M] Task saltato %s (avvio differito post-bootstrap UI)", t->name);
            continue;
        }
        if (strcmp(t->name, "mdb_engine") == 0) {
            ESP_LOGI(TAG, "[M] Task saltato %s (avvio differito alla comparsa schermata ADS/Programmi)", t->name);
            continue;
        }
        // Rispetta la configurazione di display: se headless o EPAPER_RS232 salta lvgl/touchscreen
        if ((strcmp(t->name, "lvgl") == 0 || strcmp(t->name, "touchscreen") == 0)) {
            device_config_t *cfg = device_config_get();
            if (!cfg || !cfg->display.enabled || cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232) {
                ESP_LOGI(TAG, "[M] Task saltato %s (display non compatibile con LVGL/touch)", t->name);
                continue;
            }
        }
        // Salta il task io_expander se il dispositivo non è abilitato in config
        if (strcmp(t->name, "io_expander") == 0 && !device_config_get()->sensors.io_expander_enabled) {
            ESP_LOGI(TAG, "[M] Task saltato %s (I/O expander disabilitato)", t->name);
            continue;
        }
        if (t->state != TASK_STATE_RUN) {
            if (cfg && strcmp(t->name, "mdb_engine") == 0) {
                ESP_LOGW(TAG,
                         "[M] Task saltato %s (stato=%d, mdb=%d coin=%d cashless=%d)",
                         t->name,
                         (int)t->state,
                         cfg->sensors.mdb_enabled ? 1 : 0,
                         (cfg->mdb.coin_acceptor_en && !cfg->sensors.cctalk_enabled) ? 1 : 0,
                         cfg->mdb.cashless_en ? 1 : 0);
                continue;
            }
            ESP_LOGI(TAG, "[M] Task saltato %s (stato=%d)", t->name, (int)t->state);
            continue;
        }
        if (strcmp(t->name, "touchscreen") == 0 && t->arg == NULL) {
            ESP_LOGW(TAG, "[M] Task saltato %s (touch handle non disponibile)", t->name);
            continue;
        }
        ESP_LOGI(TAG, "[M] Avvio task %s (stack=%lu words)...", t->name, (unsigned long)t->stack_words);
        // Non sovrascrivere arg se già impostato (es. touchscreen handle)
        // e non impostare fallback per task che richiedono handle specifici.
        if (t->arg == NULL && strcmp(t->name, "touchscreen") != 0) {
            t->arg = t;
        }
        BaseType_t res = task_create(t);
        if (res != pdPASS) {
            ESP_LOGE(TAG, "[M] Fallimento avvio task %s (stack=%lu caps=0x%lx)",
                     t->name, (unsigned long)t->stack_words, (unsigned long)t->stack_caps);
        }
    }
}

bool tasks_start_named(const char *name)
{
    if (!name || name[0] == '\0') {
        return false;
    }

    task_param_t *t = find_task_by_name(name);
    if (!t) {
        ESP_LOGW(TAG, "[M] Task %s non trovata", name);
        return false;
    }
    if (t->handle != NULL) {
        ESP_LOGI(TAG, "[M] Task %s già avviata", name);
        return true;
    }
    if (t->state != TASK_STATE_RUN) {
        ESP_LOGW(TAG, "[M] Task %s non avviata (stato=%d)", name, (int)t->state);
        return false;
    }
    if ((strcmp(t->name, "lvgl") == 0 || strcmp(t->name, "touchscreen") == 0) &&
        !device_config_get()->display.enabled) {
        ESP_LOGW(TAG, "[M] Task %s non avviata (display disabilitato da config)", name);
        return false;
    }
    if (strcmp(t->name, "io_expander") == 0 && !device_config_get()->sensors.io_expander_enabled) {
        ESP_LOGW(TAG, "[M] Task %s non avviata (I/O expander disabilitato)", name);
        return false;
    }
    if (strcmp(t->name, "touchscreen") == 0 && t->arg == NULL) {
        ESP_LOGW(TAG, "[M] Task %s non avviata (touch handle non disponibile)", name);
        return false;
    }

    if (t->arg == NULL && strcmp(t->name, "touchscreen") != 0) {
        t->arg = t;
    }

    ESP_LOGI(TAG, "[M] Avvio differito task %s (stack=%lu words)...",
             t->name, (unsigned long)t->stack_words);
    BaseType_t res = task_create(t);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "[M] Fallimento avvio task %s (stack=%lu caps=0x%lx)",
                 t->name, (unsigned long)t->stack_words, (unsigned long)t->stack_caps);
        return false;
    }

    return true;
}

/** @brief Esegue tutte le attività pianificate e le esegue.
 * 
 * @param [in/out] Nessun parametro specifico.
 * 
 * @return Nessun valore di ritorno.
 * 
 * Questa funzione si occupa di eseguire tutte le attività pianificate in anticipo.
 * Non richiede parametri di input o output specifici e non restituisce alcun valore.
 */
void tasks_apply_n_run(void)
{
    ESP_LOGI(TAG, "Applicazione nuovi stati task... (display.enabled=%d)", (int)device_config_get()->display.enabled);
    device_config_t *cfg = device_config_get();
    for (size_t i = 0; i < sizeof(s_tasks) / sizeof(s_tasks[0]); ++i) {
        task_param_t *t = &s_tasks[i];

        // Forza IDLE sui task display solo se headless o EPAPER_RS232; altrimenti rispetta il CSV
        if ((strcmp(t->name, "lvgl") == 0 || strcmp(t->name, "touchscreen") == 0)) {
            if (!cfg->display.enabled || cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232) {
                t->state = TASK_STATE_IDLE; // headless/EPAPER: garantiamo che siano disabilitati
                ESP_LOGI(TAG, "[M] Task %s forzato IDLE (display non compatibile con LVGL/touch)", t->name);
            }
            // display abilitato e non EPAPER: stato gestito dal CSV, nessun override
        }
        // Forza stato idle su io_expander se il sensore è disabilitato
        if (strcmp(t->name, "io_expander") == 0 && !cfg->sensors.io_expander_enabled) {
            t->state = TASK_STATE_IDLE;
        }
        
        // Forza stato idle/running su usb_scanner in base alla configurazione
        if (strcmp(t->name, "rs232") == 0 &&
            cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232 &&
            cfg->sensors.rs232_enabled) {
            t->state = TASK_STATE_RUN;
            ESP_LOGI(TAG, "[M] Task %s forzato RUN (EPAPER_RS232 attivo)", t->name);
        }

        if (strcmp(t->name, "usb_scanner") == 0) {
            if (cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232 && cfg->sensors.rs232_enabled) {
                t->state = TASK_STATE_IDLE;
                ESP_LOGI(TAG, "[M] Task %s forzato IDLE (EPAPER_RS232 usa scanner RS232)", t->name);
            } else if (cfg->scanner.enabled) {
                t->state = TASK_STATE_RUN;
                ESP_LOGI(TAG, "[M] Task %s forzato RUN (scanner.enabled=true)", t->name);
            } else {
                t->state = TASK_STATE_IDLE;
                ESP_LOGI(TAG, "[M] Task %s forzato IDLE (scanner.enabled=false)", t->name);
            }
        }

        if (strcmp(t->name, "mdb_engine") == 0) {
            bool coin_runtime_enabled = cfg->mdb.coin_acceptor_en && !cfg->sensors.cctalk_enabled;
            bool mdb_runtime_enabled = cfg->sensors.mdb_enabled &&
                                       (coin_runtime_enabled || cfg->mdb.cashless_en);
            t->state = mdb_runtime_enabled ? TASK_STATE_RUN : TASK_STATE_IDLE;
            ESP_LOGI(TAG,
                     "[M] Task %s forzato %s (mdb=%d coin=%d cashless=%d)",
                     t->name,
                     (t->state == TASK_STATE_RUN) ? "RUN" : "IDLE",
                     cfg->sensors.mdb_enabled ? 1 : 0,
                     coin_runtime_enabled ? 1 : 0,
                     cfg->mdb.cashless_en ? 1 : 0);
        }

        if (t->state == TASK_STATE_RUN) {
            if (t->handle == NULL) {
                if (strcmp(t->name, "touchscreen") == 0 && t->arg == NULL) {
                    ESP_LOGW(TAG, "[M] Task saltato %s (touch handle non disponibile)", t->name);
                    continue;
                }
                ESP_LOGI(TAG, "Avvio task %s (stack=%lu words)...", t->name, (unsigned long)t->stack_words);
                // Non sovrascrivere arg se già impostato (es. touchscreen handle)
                if (t->arg == NULL && strcmp(t->name, "touchscreen") != 0) {
                    t->arg = t;
                }
                xTaskCreatePinnedToCoreWithCaps(t->task_fn, t->name, t->stack_words, t->arg, t->priority, &t->handle, t->core_id, t->stack_caps);
            } else {
                vTaskResume(t->handle);
            }
        } else if (t->state == TASK_STATE_PAUSE) {
            if (t->handle != NULL) {
                vTaskSuspend(t->handle);
            }
        } else if (t->state == TASK_STATE_IDLE) {
            if (t->handle != NULL) {
                vTaskDelete(t->handle);
                t->handle = NULL;
            }
        }
    }
}
