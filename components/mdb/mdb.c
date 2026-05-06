#include "mdb.h"
#include "mdb_cashless.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "device_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG_INIT = "MDB_INIT";
static const char *TAG_IO = "MDB_IO";
static const char *TAG_COIN = "MDB_COIN";
static const char *TAG_CASH = "MDB_CASH";
static const char *TAG_ENGINE = "MDB_ENGINE";

#ifndef DNA_MDB
#define DNA_MDB 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_MDB == 0

#define MDB_UART_PORT   CONFIG_APP_MDB_UART_PORT
#define MDB_TX_GPIO     CONFIG_APP_MDB_TX_GPIO
#define MDB_RX_GPIO     CONFIG_APP_MDB_RX_GPIO
#define MDB_COIN_SETUP_MAX_RETRIES 5
#define MDB_CASHLESS_SETUP_MAX_RETRIES 5

#define MDB_CASHLESS_CMD_RESET      0x00
#define MDB_CASHLESS_CMD_SETUP      0x01
#define MDB_CASHLESS_CMD_POLL       0x02
#define MDB_CASHLESS_CMD_VEND       0x03
#define MDB_CASHLESS_CMD_ENABLE     0x04
#define MDB_CASHLESS_CMD_REVALUE    0x05
#define MDB_CASHLESS_CMD_EXPANSION  0x07

#define MDB_CASHLESS_EXP_REQUEST_ID             0x00
#define MDB_CASHLESS_EXP_REQUEST_FEATURE_ENABLE 0x04
#define MDB_CASHLESS_VEND_REQUEST               0x00
#define MDB_CASHLESS_VEND_SUCCESS               0x02
#define MDB_CASHLESS_VEND_SESSION_COMPLETE      0x04
#define MDB_CASHLESS_REVALUE_REQUEST            0x00
#define MDB_CASHLESS_REVALUE_LIMIT_REQUEST      0x01

static mdb_status_t s_mdb_status = {0};
static uint8_t s_coin_setup_retries = 0;
static uint8_t s_cashless_reset_retries[MDB_CASHLESS_DEVICE_COUNT] = {0};
static uint8_t s_cashless_setup_retries[MDB_CASHLESS_DEVICE_COUNT] = {0};
static uint32_t s_cashless_idle_poll_timeout_count[MDB_CASHLESS_DEVICE_COUNT] = {0};
static uint32_t s_cashless_idle_poll_ack_only_count[MDB_CASHLESS_DEVICE_COUNT] = {0};
static uint32_t s_cashless_idle_poll_error_count[MDB_CASHLESS_DEVICE_COUNT] = {0};
/* [C] Timestamp (tick) in cui il dispositivo ha raggiunto MDB_STATE_ERROR,
 * usato per il recovery automatico dopo MDB_ERROR_RECOVERY_MS. */
static TickType_t s_cashless_error_since_tick[MDB_CASHLESS_DEVICE_COUNT] = {0};
#define MDB_ERROR_RECOVERY_MS 20000U   /* tempo in error prima del reset automatico */
static bool s_mdb_driver_initialized = false;
static bool s_mdb_init_failed = false;
static bool s_mdb_runtime_fault = false;
static mdb_cashless_credit_callback_t s_mdb_cashless_credit_callback = NULL;
static mdb_cashless_vend_callback_t s_mdb_cashless_vend_callback = NULL;

static const uint8_t s_pk4_setup_vmc_caps[] = {0x00, 0x02, 0x00, 0x00, 0x00};
static const uint8_t s_pk4_setup_vmc_maxmin[] = {0x01, 0xFF, 0xFF, 0x00, 0x00};
static const uint8_t s_pk4_request_id_payload[] = {
    0x00,
    'M', 'H', 'I',
    '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '1',
    'E', 'S', 'P', '3', '2', '-', 'P', '4', '-', 'V', 'M', 'C',
    0x00, 0x01,
};
static const uint8_t s_pk4_enable_payload[] = {0x01};

static bool mdb_coin_runtime_enabled(const device_config_t *cfg)
{
    return cfg && cfg->sensors.mdb_enabled && cfg->mdb.coin_acceptor_en &&
           !cfg->sensors.cctalk_enabled;
}

static bool mdb_cashless_runtime_enabled(const device_config_t *cfg)
{
    return cfg && cfg->sensors.mdb_enabled && cfg->mdb.cashless_en;
}

static void mdb_cashless_update_public_status(void);
static void mdb_cashless_sm(size_t device_index);
static void mdb_cashless_dispatch_pending_credit(void);
static void mdb_cashless_dispatch_pending_vend(void);
static bool mdb_cashless_get_active_device_index(size_t *device_index);
static esp_err_t mdb_cashless_send_vend_command(uint8_t device_address,
                                                uint8_t subcommand,
                                                uint16_t amount_cents,
                                                uint16_t item_number,
                                                uint8_t *rx,
                                                size_t rx_size,
                                                size_t *rx_len,
                                                uint32_t timeout_ms);
static esp_err_t mdb_cashless_send_revalue_command(uint8_t device_address,
                                                   uint8_t subcommand,
                                                   uint16_t amount_cents,
                                                   uint8_t *rx,
                                                   size_t rx_size,
                                                   size_t *rx_len,
                                                   uint32_t timeout_ms);
static esp_err_t mdb_cashless_send_simple_command(uint8_t device_address,
                                                  uint8_t command,
                                                  const uint8_t *payload,
                                                  size_t payload_len,
                                                  uint8_t *rx,
                                                  size_t rx_size,
                                                  size_t *rx_len,
                                                  uint32_t timeout_ms);

static const char *mdb_state_to_string(mdb_device_state_t state)
{
    switch (state) {
        case MDB_STATE_INACTIVE: return "inactive";
        case MDB_STATE_INIT_RESET: return "init_reset";
        case MDB_STATE_INIT_SETUP: return "init_setup";
        case MDB_STATE_INIT_EXPANSION: return "init_expansion";
        case MDB_STATE_INIT_ENABLE: return "init_enable";
        case MDB_STATE_IDLE_POLLING: return "idle_polling";
        case MDB_STATE_ERROR: return "error";
        default: return "unknown";
    }
}

static __attribute__((unused)) const char *mdb_cashless_command_to_string(uint8_t command,
                                                                          uint8_t subcommand)
{
    switch (command) {
        case MDB_CASHLESS_CMD_RESET:
            return "reset";
        case MDB_CASHLESS_CMD_SETUP:
            return "setup";
        case MDB_CASHLESS_CMD_POLL:
            return "poll";
        case MDB_CASHLESS_CMD_ENABLE:
            return "enable";
        case MDB_CASHLESS_CMD_EXPANSION:
            if (subcommand == MDB_CASHLESS_EXP_REQUEST_ID) {
                return "exp_request_id";
            }
            if (subcommand == MDB_CASHLESS_EXP_REQUEST_FEATURE_ENABLE) {
                return "exp_feature_enable";
            }
            return "expansion";
        case MDB_CASHLESS_CMD_VEND:
            if (subcommand == MDB_CASHLESS_VEND_REQUEST) {
                return "vend_request";
            }
            if (subcommand == MDB_CASHLESS_VEND_SUCCESS) {
                return "vend_success";
            }
            if (subcommand == MDB_CASHLESS_VEND_SESSION_COMPLETE) {
                return "vend_session_complete";
            }
            return "vend";
        case MDB_CASHLESS_CMD_REVALUE:
            if (subcommand == MDB_CASHLESS_REVALUE_REQUEST) {
                return "revalue_request";
            }
            if (subcommand == MDB_CASHLESS_REVALUE_LIMIT_REQUEST) {
                return "revalue_limit";
            }
            return "revalue";
        default:
            return "unknown";
    }
}

static __attribute__((unused)) void mdb_log_packet_bytes(const char *tag,
                                                         const char *label,
                                                         const uint8_t *data,
                                                         size_t len)
{
    char buffer[256] = {0};
    size_t offset = 0;
    size_t max_bytes = len;

    if (!tag || !label) {
        return;
    }

    if (!data || len == 0U) {
        ESP_LOGI(tag, "[C] [%s] <empty>", label);
        return;
    }

    if (max_bytes > 24U) {
        max_bytes = 24U;
    }

    for (size_t index = 0; index < max_bytes && offset + 4U < sizeof(buffer); ++index) {
        offset += (size_t)snprintf(buffer + offset, sizeof(buffer) - offset, "%02X ", data[index]);
    }

    if (len > max_bytes && offset + 5U < sizeof(buffer)) {
        snprintf(buffer + offset, sizeof(buffer) - offset, "...");
    } else if (offset > 0U) {
        buffer[offset - 1U] = '\0';
    }

    ESP_LOGI(tag, "[C] [%s] len=%u data=%s", label, (unsigned)len, buffer);
}

const mdb_status_t* mdb_get_status(void) {
    return &s_mdb_status;
}

void mdb_register_cashless_credit_callback(mdb_cashless_credit_callback_t callback)
{
    s_mdb_cashless_credit_callback = callback;
}

void mdb_register_cashless_vend_callback(mdb_cashless_vend_callback_t callback)
{
    s_mdb_cashless_vend_callback = callback;
}

static void mdb_cashless_log_idle_poll_diagnostics(size_t device_index,
                                                   const mdb_cashless_device_t *device)
{
    if (!device || device_index >= MDB_CASHLESS_DEVICE_COUNT) {
        return;
    }

    if (s_cashless_idle_poll_timeout_count[device_index] > 0U &&
        (s_cashless_idle_poll_timeout_count[device_index] % 250U) == 0U) {
        ESP_LOGW(TAG_CASH,
                 "[C] [mdb_cashless_sm] dev=%u idle polling: %lu timeout consecutivi (addr=0x%02X state=%s present=%d session_open=%d)",
                 (unsigned)device_index,
                 (unsigned long)s_cashless_idle_poll_timeout_count[device_index],
                 device->address,
                 mdb_state_to_string(device->poll_state),
                 device->present ? 1 : 0,
                 device->session_open ? 1 : 0);
    }

    if (s_cashless_idle_poll_ack_only_count[device_index] > 0U &&
        (s_cashless_idle_poll_ack_only_count[device_index] % 250U) == 0U) {
        ESP_LOGW(TAG_CASH,
                 "[C] [mdb_cashless_sm] dev=%u idle polling: %lu ACK consecutivi senza eventi TAG/sessione",
                 (unsigned)device_index,
                 (unsigned long)s_cashless_idle_poll_ack_only_count[device_index]);
    }

    if (s_cashless_idle_poll_error_count[device_index] > 0U &&
        (s_cashless_idle_poll_error_count[device_index] % 100U) == 0U) {
        ESP_LOGW(TAG_CASH,
                 "[C] [mdb_cashless_sm] dev=%u idle polling: %lu errori consecutivi non-timeout",
                 (unsigned)device_index,
                 (unsigned long)s_cashless_idle_poll_error_count[device_index]);
    }
}

bool mdb_cashless_request_program_vend(uint16_t amount_cents, uint16_t item_number)
{
    size_t device_index = 0;

    if (amount_cents == 0U || !mdb_cashless_get_active_device_index(&device_index)) {
        ESP_LOGW(TAG_CASH,
                 "[C] [mdb_cashless_request_program_vend] richiesta ignorata amount=%u item=0x%04X",
                 (unsigned)amount_cents,
                 (unsigned)item_number);
        return false;
    }

    ESP_LOGI(TAG_CASH,
             "[C] [mdb_cashless_request_program_vend] dev=%u amount=%u item=0x%04X",
             (unsigned)device_index,
             (unsigned)amount_cents,
             (unsigned)item_number);
    return mdb_cashless_prepare_vend_request(device_index, amount_cents, item_number);
}

bool mdb_cashless_confirm_vend_success(uint16_t approved_amount_cents)
{
    size_t device_index = 0;

    if (approved_amount_cents == 0U || !mdb_cashless_get_active_device_index(&device_index)) {
        ESP_LOGW(TAG_CASH,
                 "[C] [mdb_cashless_confirm_vend_success] conferma ignorata amount=%u",
                 (unsigned)approved_amount_cents);
        return false;
    }

    ESP_LOGI(TAG_CASH,
             "[C] [mdb_cashless_confirm_vend_success] dev=%u amount=%u",
             (unsigned)device_index,
             (unsigned)approved_amount_cents);
    return mdb_cashless_prepare_vend_success(device_index, approved_amount_cents);
}

bool mdb_cashless_complete_active_session(void)
{
    size_t device_index = 0;

    if (!mdb_cashless_get_active_device_index(&device_index)) {
        ESP_LOGW(TAG_CASH, "[C] [mdb_cashless_complete_active_session] nessun device cashless attivo");
        return false;
    }

    ESP_LOGI(TAG_CASH,
             "[C] [mdb_cashless_complete_active_session] dev=%u richiesta chiusura sessione",
             (unsigned)device_index);
    return mdb_cashless_request_session_complete(device_index);
}

bool mdb_cashless_request_active_revalue_limit(void)
{
    size_t device_index = 0;

    if (!mdb_cashless_get_active_device_index(&device_index)) {
        ESP_LOGW(TAG_CASH, "[C] [mdb_cashless_request_active_revalue_limit] nessun device cashless attivo");
        return false;
    }

    ESP_LOGI(TAG_CASH,
             "[C] [mdb_cashless_request_active_revalue_limit] dev=%u richiesta limite",
             (unsigned)device_index);
    return mdb_cashless_request_revalue_limit(device_index);
}

bool mdb_cashless_request_active_revalue(uint16_t amount_cents)
{
    size_t device_index = 0;

    if (amount_cents == 0U || !mdb_cashless_get_active_device_index(&device_index)) {
        ESP_LOGW(TAG_CASH,
                 "[C] [mdb_cashless_request_active_revalue] richiesta ignorata amount=%u",
                 (unsigned)amount_cents);
        return false;
    }

    ESP_LOGI(TAG_CASH,
             "[C] [mdb_cashless_request_active_revalue] dev=%u amount=%u",
             (unsigned)device_index,
             (unsigned)amount_cents);
    return mdb_cashless_prepare_revalue(device_index, amount_cents);
}

device_component_status_t mdb_get_component_status(void)
{
    const device_config_t *cfg = device_config_get();
    bool coin_enabled = mdb_coin_runtime_enabled(cfg);
    bool cashless_enabled = mdb_cashless_runtime_enabled(cfg);

    if (cashless_enabled && s_mdb_status.cashless.is_online) {
        return DEVICE_COMPONENT_STATUS_ONLINE;
    }

    if (s_mdb_init_failed || s_mdb_runtime_fault) {
        return DEVICE_COMPONENT_STATUS_OFFLINE;
    }

    if (!cfg || !cfg->sensors.mdb_enabled || (!coin_enabled && !cashless_enabled)) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (!s_mdb_driver_initialized) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    if ((coin_enabled && s_mdb_status.coin.is_online && s_mdb_status.coin.state == MDB_STATE_IDLE_POLLING) ||
        (cashless_enabled && s_mdb_status.cashless.is_online)) {
        return DEVICE_COMPONENT_STATUS_ONLINE;
    }

    return DEVICE_COMPONENT_STATUS_OFFLINE;
}

// Helper per calcolare la parità di un byte (ritorna true se dispari)

/**
 * @brief Calcola la parità di un byte.
 * 
 * Questa funzione calcola la parità di un byte fornito come input.
 * La parità è definita come il numero di bit 1 nel byte. Se il numero di bit 1 è pari, la parità è pari; se è dispari, la parità è dispari.
 * 
 * @param [in] data Byte di cui calcolare la parità.
 * @return true se la parità è pari, false se la parità è dispari.
 */
static bool get_byte_parity(uint8_t data) {
    return __builtin_parity(data);
}

// Helper: Invia ACK

/**
 * @brief Invia un ACK (Acknowledgment) al dispositivo.
 *
 * Questa funzione invia un ACK al dispositivo per confermare la ricezione di un pacchetto.
 *
 * @param [in/out] Nessun parametro specifico.
 * @return Nessun valore di ritorno.
 */
static void mdb_send_ack(void) {
    uint8_t ack = MDB_ACK;
    // ACK è un data byte, quindi 9° bit = 0
    // Per avere 9° bit = 0 con dato 0x00 (parità pari): usiamo EVEN
    uart_set_parity(MDB_UART_PORT, UART_PARITY_EVEN);
    uart_write_bytes(MDB_UART_PORT, &ack, 1);
    uart_wait_tx_done(MDB_UART_PORT, pdMS_TO_TICKS(10));
    // ESP_LOGI(TAG_IO, "[C] [mdb_send_ack] ACK inviato sul bus MDB");
}

// Logica per la Macchina a Stati della Gettoniera (Coin Acceptor)

/**
 * @brief Gestisce lo stato di coin per il sistema di monete.
 * 
 * Questa funzione si occupa di gestire lo stato delle monete inserite nel sistema.
 * 
 * @param [in/out] coin_state Stato corrente delle monete.
 * @return void Nessun valore di ritorno.
 */
static void mdb_coin_sm(void) {
    uint8_t rx[36];
    size_t rx_len;
    esp_err_t ret;
    device_config_t *cfg = device_config_get();

    if (!mdb_coin_runtime_enabled(cfg)) {
        if (s_mdb_status.coin.state != MDB_STATE_INACTIVE || s_mdb_status.coin.is_online) {
            ESP_LOGI(TAG_COIN,
                     "[C] [mdb_coin_sm] gettoniera MDB inattiva: progetto su CCTalk o config disabilitata");
        }
        s_coin_setup_retries = 0;
        s_mdb_status.coin.state = MDB_STATE_INACTIVE;
        s_mdb_status.coin.is_online = false;
        return;
    }

    switch (s_mdb_status.coin.state) {
        case MDB_STATE_INACTIVE:
            if (cfg->mdb.coin_acceptor_en) {
                ESP_LOGI(TAG_COIN,
                         "[C] [mdb_coin_sm] state=%s -> %s",
                         mdb_state_to_string(s_mdb_status.coin.state),
                         mdb_state_to_string(MDB_STATE_INIT_RESET));
                s_mdb_status.coin.state = MDB_STATE_INIT_RESET;
            }
            break;

        case MDB_STATE_INIT_RESET:
            ESP_LOGI(TAG_COIN, "[C] [mdb_coin_sm] invio RESET gettoniera");
            mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_CMD_RESET, NULL, 0);
            vTaskDelay(pdMS_TO_TICKS(500)); // Aspetta reboot periferica
            s_coin_setup_retries = 0;
            s_mdb_runtime_fault = false;
            s_mdb_status.coin.state = MDB_STATE_INIT_SETUP;
            break;

        case MDB_STATE_INIT_SETUP:
            ESP_LOGI(TAG_COIN, "[C] [mdb_coin_sm] invio SETUP gettoniera");
            mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_CMD_SETUP, NULL, 0);
            if (mdb_receive_packet(rx, sizeof(rx), &rx_len, 100) == ESP_OK) {
                if (rx_len >= 23) {
                    s_coin_setup_retries = 0;
                    s_mdb_status.coin.feature_level = rx[0];
                    s_mdb_status.coin.currency_code = (rx[1] << 8) | rx[2];
                    s_mdb_status.coin.scaling_factor = rx[3];
                    s_mdb_status.coin.decimal_places = rx[4];
                    s_mdb_status.coin.coin_routing = (rx[5] << 8) | rx[6];
                    for (int i = 0; i < 16; i++) {
                        s_mdb_status.coin.coin_values[i] = rx[7 + i];
                    }
                    
                    ESP_LOGI(TAG_COIN, "[C] [mdb_coin_sm] setup ok level=%d currency=%04X scaling=%d decimals=%d", 
                             s_mdb_status.coin.feature_level, s_mdb_status.coin.currency_code,
                             s_mdb_status.coin.scaling_factor, s_mdb_status.coin.decimal_places);
                    
                    mdb_send_ack();
                    s_mdb_status.coin.state = MDB_STATE_INIT_ENABLE; // Passa alla fase di abilitazione
                    s_mdb_status.coin.is_online = true;
                    s_mdb_runtime_fault = false;
                } else {
                    ESP_LOGW(TAG_COIN, "[C] [mdb_coin_sm] setup risposta troppo breve len=%u", (unsigned)rx_len);
                    s_coin_setup_retries++;
                }
            } else {
                ESP_LOGW(TAG_COIN,
                         "[C] [mdb_coin_sm] setup senza risposta valida retry=%u/%u",
                         (unsigned)(s_coin_setup_retries + 1U),
                         (unsigned)MDB_COIN_SETUP_MAX_RETRIES);
                s_coin_setup_retries++;
            }

            if (s_coin_setup_retries >= MDB_COIN_SETUP_MAX_RETRIES) {
                if (cfg) {
                    cfg->mdb.coin_acceptor_en = false;
                }
                s_mdb_status.coin.is_online = false;
                s_mdb_status.coin.state = MDB_STATE_INACTIVE;
                s_mdb_runtime_fault = !mdb_cashless_runtime_enabled(cfg);
                ESP_LOGE(TAG_COIN,
                         "[C] [mdb_coin_sm] setup fallito %u volte, gettoniera disabilitata a runtime",
                         (unsigned)s_coin_setup_retries);
            }
            break;

        case MDB_STATE_INIT_ENABLE:
            ESP_LOGI(TAG_COIN, "[C] [mdb_coin_sm] abilito tutti i tipi di moneta");
            {
                uint8_t enable_data[4] = {0xFF, 0xFF, 0xFF, 0xFF}; // Abilita tutto (Accept & Dispense)
                mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_COIN_CMD_COIN_TYPE, enable_data, 4);
            }
            if (mdb_receive_packet(rx, sizeof(rx), &rx_len, 50) == ESP_OK && rx[0] == MDB_ACK) {
                ESP_LOGI(TAG_COIN, "[C] [mdb_coin_sm] gettoniera abilitata");
                s_mdb_status.coin.state = MDB_STATE_IDLE_POLLING;
            }
            break;

        case MDB_STATE_IDLE_POLLING:
            mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_CMD_POLL, NULL, 0);
            ret = mdb_receive_packet(rx, sizeof(rx), &rx_len, 20);
            if (ret == ESP_OK) {
                if (rx_len == 1 && rx[0] == MDB_ACK) {
                    // Tutto OK, nessun evento
                } else {
                    // Dati ricevuti (es. moneta inserita)
                    ESP_LOGI(TAG_COIN, "[C] [mdb_coin_sm] evento gettoniera len=%u", (unsigned)rx_len);
                    mdb_send_ack();
                    
                    // Parsing eventi (solo i più importanti)
                    for (int i=0; i < (int)rx_len-1; i++) { // L'ultimo è il checksum
                        uint8_t b = rx[i];
                        if ((b & 0x80) == 0) { // Moneta accettata
                            uint8_t routing = (b >> 4) & 0x03; // 00: Cassetto, 01: Tubi, 02: Non Usato, 03: Rifiutata
                            uint8_t type = b & 0x0F;
                            uint16_t real_val = s_mdb_status.coin.coin_values[type] * s_mdb_status.coin.scaling_factor;
                            s_mdb_status.coin.credit_cents += real_val;
                            ESP_LOGI(TAG_COIN,
                                     "[C] [mdb_coin_sm] moneta tipo=%d routing=%d credito=+%u cent totale=%u",
                                     type,
                                     routing,
                                     (unsigned)real_val,
                                     (unsigned)s_mdb_status.coin.credit_cents);
                        } else if (b == 0x0B) { // Reset del Changer
                            ESP_LOGW(TAG_COIN, "[C] [mdb_coin_sm] reset gettoniera rilevato, riavvio setup");
                            s_mdb_status.coin.state = MDB_STATE_INIT_SETUP;
                        }
                    }
                }
                s_mdb_status.coin.is_online = true;
                s_mdb_runtime_fault = false;
            } else {
                s_mdb_status.coin.is_online = false;
                ESP_LOGD(TAG_COIN, "[C] [mdb_coin_sm] nessuna risposta dal poll gettoniera");
            }
            break;
            
        default: break;
    }
}

static void mdb_cashless_update_public_status(void)
{
    const mdb_cashless_device_t *active_device = NULL;
    size_t online_devices = 0;

    s_mdb_status.cashless.state = MDB_STATE_INACTIVE;
    s_mdb_status.cashless.is_online = false;
    s_mdb_status.cashless.online_devices = 0;
    s_mdb_status.cashless.active_device_index = 0xFF;
    s_mdb_status.cashless.feature_level = 0;
    s_mdb_status.cashless.last_response_code = 0;
    s_mdb_status.cashless.credit_cents = 0;
    s_mdb_status.cashless.approved_price_cents = 0;
    s_mdb_status.cashless.approved_revalue_cents = 0;
    s_mdb_status.cashless.revalue_limit_cents = 0;
    s_mdb_status.cashless.revalue_status = 0;
    s_mdb_status.cashless.session_open = false;

    for (size_t device_index = 0; device_index < mdb_cashless_get_device_count(); ++device_index) {
        const mdb_cashless_device_t *device = mdb_cashless_get_device(device_index);
        if (!device) {
            continue;
        }

        if (device->present) {
            online_devices++;
            if (!active_device) {
                active_device = device;
                s_mdb_status.cashless.active_device_index = (uint8_t)device_index;
            }
        }
    }

    s_mdb_status.cashless.online_devices = (uint8_t)online_devices;
    s_mdb_status.cashless.is_online = (online_devices > 0U);

    if (active_device) {
        s_mdb_status.cashless.state = active_device->poll_state;
        s_mdb_status.cashless.feature_level = active_device->feature_level;
        s_mdb_status.cashless.last_response_code = active_device->last_response_code;
        s_mdb_status.cashless.credit_cents = active_device->credit_cents;
        s_mdb_status.cashless.approved_price_cents = active_device->approved_price_cents;
        s_mdb_status.cashless.approved_revalue_cents = active_device->approved_revalue_cents;
        s_mdb_status.cashless.revalue_limit_cents = active_device->revalue_limit_cents;
        s_mdb_status.cashless.revalue_status = (uint8_t)active_device->revalue_status;
        s_mdb_status.cashless.session_open = active_device->session_open;
    }
}

static void mdb_cashless_dispatch_pending_credit(void)
{
    size_t device_index = 0;
    uint16_t credit_cents = 0;

    if (!s_mdb_cashless_credit_callback) {
        return;
    }

    if (!mdb_cashless_get_pending_credit_event(&device_index, &credit_cents)) {
        return;
    }

    if (credit_cents == 0U) {
        mdb_cashless_ack_pending_credit_event(device_index);
        return;
    }

    if (s_mdb_cashless_credit_callback((int32_t)credit_cents, "mdb_cashless")) {
        ESP_LOGI(TAG_CASH,
             "[C] [mdb_cashless_dispatch_pending_credit] dev=%u credito=%u cent sincronizzato verso FSM",
                 (unsigned)device_index,
                 (unsigned)credit_cents);
        mdb_cashless_ack_pending_credit_event(device_index);
    }
}

static void mdb_cashless_dispatch_pending_vend(void)
{
    size_t device_index = 0;
    uint16_t amount_cents = 0;
    bool approved = false;

    if (!s_mdb_cashless_vend_callback) {
        return;
    }

    if (!mdb_cashless_get_pending_vend_event(&device_index, &approved, &amount_cents)) {
        return;
    }

    if (s_mdb_cashless_vend_callback(approved, (int32_t)amount_cents, "mdb_cashless")) {
        ESP_LOGI(TAG_CASH,
             "[C] [mdb_cashless_dispatch_pending_vend] dev=%u esito=%s amount=%u",
                 (unsigned)device_index,
                 approved ? "approved" : "denied",
                 (unsigned)amount_cents);
        mdb_cashless_ack_pending_vend_event(device_index);
    }
}

static bool mdb_cashless_get_active_device_index(size_t *device_index)
{
    if (!device_index) {
        return false;
    }

    for (size_t index = 0; index < mdb_cashless_get_device_count(); ++index) {
        const mdb_cashless_device_t *device = mdb_cashless_get_device(index);
        if (!device || !device->present || !device->session_open) {
            continue;
        }

        *device_index = index;
        return true;
    }

    return false;
}

static esp_err_t mdb_cashless_send_vend_command(uint8_t device_address,
                                                uint8_t subcommand,
                                                uint16_t amount_cents,
                                                uint16_t item_number,
                                                uint8_t *rx,
                                                size_t rx_size,
                                                size_t *rx_len,
                                                uint32_t timeout_ms)
{
    uint8_t payload[5] = {0};
    size_t payload_len = 1U;

    payload[0] = subcommand;

    switch (subcommand) {
        case MDB_CASHLESS_VEND_REQUEST:
            payload[1] = (uint8_t)((amount_cents >> 8) & 0xFFU);
            payload[2] = (uint8_t)(amount_cents & 0xFFU);
            payload[3] = (uint8_t)((item_number >> 8) & 0xFFU);
            payload[4] = (uint8_t)(item_number & 0xFFU);
            payload_len = 5U;
            break;

        case MDB_CASHLESS_VEND_SUCCESS:
            /* Il codice del produttore invia VEND_SUCCESS con 0xFF 0xFF. */
            payload[1] = 0xFFU;
            payload[2] = 0xFFU;
            payload_len = 3U;
            break;

        case MDB_CASHLESS_VEND_SESSION_COMPLETE:
            payload_len = 1U;
            break;

        default:
            payload_len = 1U;
            break;
    }

    return mdb_cashless_send_simple_command(device_address,
                                            MDB_CASHLESS_CMD_VEND,
                                            payload,
                                            payload_len,
                                            rx,
                                            rx_size,
                                            rx_len,
                                            timeout_ms);
}

static esp_err_t mdb_cashless_send_revalue_command(uint8_t device_address,
                                                   uint8_t subcommand,
                                                   uint16_t amount_cents,
                                                   uint8_t *rx,
                                                   size_t rx_size,
                                                   size_t *rx_len,
                                                   uint32_t timeout_ms)
{
    uint8_t payload[3] = {0};
    size_t payload_len = 1U;

    payload[0] = subcommand;
    if (subcommand == MDB_CASHLESS_REVALUE_REQUEST) {
        payload[1] = (uint8_t)((amount_cents >> 8) & 0xFFU);
        payload[2] = (uint8_t)(amount_cents & 0xFFU);
        payload_len = 3U;
    }

    return mdb_cashless_send_simple_command(device_address,
                                            MDB_CASHLESS_CMD_REVALUE,
                                            payload,
                                            payload_len,
                                            rx,
                                            rx_size,
                                            rx_len,
                                            timeout_ms);
}

static esp_err_t mdb_cashless_send_simple_command(uint8_t device_address,
                                                  uint8_t command,
                                                  const uint8_t *payload,
                                                  size_t payload_len,
                                                  uint8_t *rx,
                                                  size_t rx_size,
                                                  size_t *rx_len,
                                                  uint32_t timeout_ms)
{
    // ESP_LOGI(TAG_IO,
    //          "[C] [mdb_cashless_send_simple_command] addr=0x%02X cmd=%s(0x%02X) payload_len=%u timeout=%u",
    //          device_address,
    //          mdb_cashless_command_to_string(command, subcommand),
    //          command,
    //          (unsigned)payload_len,
    //          (unsigned)timeout_ms);
    // if (payload && payload_len > 0U) {
    //     mdb_log_packet_bytes(TAG_IO, "mdb_cashless_send_simple_command.tx_payload", payload, payload_len);
    // }

    esp_err_t err = mdb_send_packet(device_address | command, payload, payload_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_IO,
                 "[C] [mdb_cashless_send_simple_command] tx fallita addr=0x%02X cmd=0x%02X err=%s",
                 device_address,
                 command,
                 esp_err_to_name(err));
        return err;
    }

    err = mdb_receive_packet(rx, rx_size, rx_len, timeout_ms);
    if (err != ESP_OK) {
        /* Alcuni timeout in bootstrap o durante il POLL idle sono attesi. */
        bool suppress_timeout_log = (err == ESP_ERR_TIMEOUT &&
                                     (command == MDB_CASHLESS_CMD_POLL ||
                                      command == MDB_CASHLESS_CMD_RESET ||
                                      command == MDB_CASHLESS_CMD_EXPANSION));
        if (!suppress_timeout_log) {
            ESP_LOGW(TAG_IO,
                     "[C] [mdb_cashless_send_simple_command] rx fallita addr=0x%02X cmd=0x%02X err=%s",
                     device_address,
                     command,
                     esp_err_to_name(err));
        }
        return err;
    }

    if (*rx_len > 1U) {
        mdb_send_ack();
    }

    // mdb_log_packet_bytes(TAG_IO, "mdb_cashless_send_simple_command.rx", rx, *rx_len);

    return ESP_OK;
}

static void mdb_cashless_sm(size_t device_index)
{
    const device_config_t *cfg = device_config_get();
    const mdb_cashless_device_t *device = mdb_cashless_get_device(device_index);
    mdb_cashless_device_t *device_rw = NULL;
    uint8_t rx[64] = {0};
    size_t rx_len = 0;
    esp_err_t ret = ESP_OK;
    bool response_ok = false;
    bool parsed_response = false;

    if (!cfg || !device) {
        return;
    }

    device_rw = (mdb_cashless_device_t *)device;

    if (!cfg->mdb.cashless_en) {
        s_cashless_reset_retries[device_index] = 0;
        s_cashless_setup_retries[device_index] = 0;
        if (device->poll_state != MDB_STATE_INACTIVE || device->present) {
            ESP_LOGI(TAG_CASH,
                     "[C] [mdb_cashless_sm] dev=%u cashless disabilitato da config, reset stato runtime",
                     (unsigned)device_index);
        }
        mdb_cashless_reset_device(device_index);
        mdb_cashless_update_public_status();
        return;
    }

    switch (device->poll_state) {
        case MDB_STATE_INACTIVE:
            ESP_LOGI(TAG_CASH,
                     "[C] [mdb_cashless_sm] dev=%u addr=0x%02X state=%s -> %s",
                     (unsigned)device_index,
                     device->address,
                     mdb_state_to_string(device->poll_state),
                     mdb_state_to_string(MDB_STATE_INIT_RESET));
            s_cashless_reset_retries[device_index] = 0;
            s_cashless_setup_retries[device_index] = 0;
            s_cashless_error_since_tick[device_index] = 0;  /* reset timer recovery */
            device_rw->last_response_code = 0;
            s_cashless_idle_poll_timeout_count[device_index] = 0;
            s_cashless_idle_poll_ack_only_count[device_index] = 0;
            s_cashless_idle_poll_error_count[device_index] = 0;
            device_rw->poll_state = MDB_STATE_INIT_RESET;
            break;

        case MDB_STATE_INIT_RESET:
            ESP_LOGI(TAG_CASH,
                     "[C] [mdb_cashless_sm] dev=%u addr=0x%02X invio RESET PK4",
                     (unsigned)device_index,
                     device->address);
            ret = mdb_cashless_send_simple_command(device->address,
                                                   MDB_CASHLESS_CMD_RESET,
                                                   NULL,
                                                   0,
                                                   rx,
                                                   sizeof(rx),
                                                   &rx_len,
                                                   100);
            if (ret == ESP_OK) {
                response_ok = (rx_len > 0U);
                parsed_response = response_ok && !(rx_len == 1U && rx[0] == MDB_ACK);
                if (parsed_response) {
                    mdb_cashless_handle_poll_response(device_index, rx, rx_len);
                }

                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u addr=0x%02X POLL dopo RESET",
                         (unsigned)device_index,
                         device->address);
                ret = mdb_cashless_send_simple_command(device->address,
                                                       MDB_CASHLESS_CMD_POLL,
                                                       NULL,
                                                       0,
                                                       rx,
                                                       sizeof(rx),
                                                       &rx_len,
                                                       40);
                if (ret == ESP_OK) {
                    response_ok = (rx_len > 0U);
                    parsed_response = response_ok && !(rx_len == 1U && rx[0] == MDB_ACK);
                    if (parsed_response) {
                        mdb_cashless_handle_poll_response(device_index, rx, rx_len);
                    }
                }

                s_cashless_reset_retries[device_index] = 0;
                s_cashless_setup_retries[device_index] = 0;
                device_rw->last_response_code = 0;
                device_rw->poll_state = MDB_STATE_INIT_SETUP;
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u -> %s",
                         (unsigned)device_index,
                         mdb_state_to_string(device_rw->poll_state));
            } else if (++s_cashless_reset_retries[device_index] >= MDB_CASHLESS_SETUP_MAX_RETRIES) {
                device_rw->poll_state = MDB_STATE_ERROR;
                s_cashless_error_since_tick[device_index] = xTaskGetTickCount(); /* avvia timer recovery */
                ESP_LOGE(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u reset fallito %u volte -> error (recovery in %u ms)",
                         (unsigned)device_index,
                         (unsigned)s_cashless_reset_retries[device_index],
                         (unsigned)MDB_ERROR_RECOVERY_MS);
            } else {
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u reset retry=%u/%u",
                         (unsigned)device_index,
                         (unsigned)s_cashless_reset_retries[device_index],
                         (unsigned)MDB_CASHLESS_SETUP_MAX_RETRIES);
            }
            break;

        case MDB_STATE_INIT_SETUP:
            if (!device->config_read) {
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u addr=0x%02X invio SETUP capacita VMC",
                         (unsigned)device_index,
                         device->address);
                ret = mdb_cashless_send_simple_command(device->address,
                                                       MDB_CASHLESS_CMD_SETUP,
                                                       s_pk4_setup_vmc_caps,
                                                       sizeof(s_pk4_setup_vmc_caps),
                                                       rx,
                                                       sizeof(rx),
                                                       &rx_len,
                                                       120);
                response_ok = (ret == ESP_OK && rx_len > 0U);
                if (ret == ESP_OK && rx_len >= 2U) {
                    s_cashless_setup_retries[device_index] = 0;
                    device_rw->feature_level = rx[0];
                    device_rw->cash_sale_support = (rx_len > 7U) && ((rx[7] & 0x08U) != 0U);
                    device_rw->config_read = true;
                    ESP_LOGI(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u setup#1 ok level=%u cash_sale=%d",
                             (unsigned)device_index,
                             (unsigned)device_rw->feature_level,
                             device_rw->cash_sale_support ? 1 : 0);
                } else if (++s_cashless_setup_retries[device_index] >= MDB_CASHLESS_SETUP_MAX_RETRIES) {
                    device_rw->poll_state = MDB_STATE_ERROR;
                    ESP_LOGE(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u setup#1 fallito %u volte -> error",
                             (unsigned)device_index,
                             (unsigned)s_cashless_setup_retries[device_index]);
                } else {
                    ESP_LOGW(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u setup#1 retry=%u/%u",
                             (unsigned)device_index,
                             (unsigned)s_cashless_setup_retries[device_index],
                             (unsigned)MDB_CASHLESS_SETUP_MAX_RETRIES);
                }
                break;
            }

            if (!device->vmc_setup_done) {
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u addr=0x%02X invio SETUP max/min price",
                         (unsigned)device_index,
                         device->address);
                ret = mdb_cashless_send_simple_command(device->address,
                                                       MDB_CASHLESS_CMD_SETUP,
                                                       s_pk4_setup_vmc_maxmin,
                                                       sizeof(s_pk4_setup_vmc_maxmin),
                                                       rx,
                                                       sizeof(rx),
                                                       &rx_len,
                                                       120);
                response_ok = (ret == ESP_OK && rx_len > 0U);
                if (ret == ESP_OK && rx_len == 1U && rx[0] == MDB_ACK) {
                    device_rw->vmc_setup_done = true;
                    s_cashless_setup_retries[device_index] = 0;
                    device_rw->poll_state = MDB_STATE_INIT_EXPANSION;
                    ESP_LOGI(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u setup#2 ok -> %s",
                             (unsigned)device_index,
                             mdb_state_to_string(device_rw->poll_state));
                } else if (++s_cashless_setup_retries[device_index] >= MDB_CASHLESS_SETUP_MAX_RETRIES) {
                    device_rw->poll_state = MDB_STATE_ERROR;
                    ESP_LOGE(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u setup#2 fallito %u volte -> error",
                             (unsigned)device_index,
                             (unsigned)s_cashless_setup_retries[device_index]);
                } else {
                    ESP_LOGW(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u setup#2 retry=%u/%u",
                             (unsigned)device_index,
                             (unsigned)s_cashless_setup_retries[device_index],
                             (unsigned)MDB_CASHLESS_SETUP_MAX_RETRIES);
                }
            }
            break;

        case MDB_STATE_INIT_EXPANSION:
            if (!device->expansion_read) {
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u richiedo REQUEST_ID PK4",
                         (unsigned)device_index);
                ret = mdb_cashless_send_simple_command(device->address,
                                                       MDB_CASHLESS_CMD_EXPANSION,
                                                       s_pk4_request_id_payload,
                                                       sizeof(s_pk4_request_id_payload),
                                                       rx,
                                                       sizeof(rx),
                                                       &rx_len,
                                                       120);
                if (ret == ESP_OK) {
                    response_ok = (rx_len > 0U);
                    parsed_response = response_ok;
                    mdb_cashless_handle_poll_response(device_index, rx, rx_len);
                }
                break;
            }

            device_rw->poll_state = MDB_STATE_INIT_ENABLE;
            ESP_LOGI(TAG_CASH,
                     "[C] [mdb_cashless_sm] dev=%u -> %s",
                     (unsigned)device_index,
                     mdb_state_to_string(device_rw->poll_state));
            break;

        case MDB_STATE_INIT_ENABLE:
            ESP_LOGI(TAG_CASH,
                     "[C] [mdb_cashless_sm] dev=%u invio ENABLE",
                     (unsigned)device_index);
            ret = mdb_cashless_send_simple_command(device->address,
                                                   MDB_CASHLESS_CMD_ENABLE,
                                                   s_pk4_enable_payload,
                                                   sizeof(s_pk4_enable_payload),
                                                   rx,
                                                   sizeof(rx),
                                                   &rx_len,
                                                   80);
            response_ok = (ret == ESP_OK && rx_len > 0U);
            if (ret == ESP_OK) {
                device_rw->poll_state = MDB_STATE_IDLE_POLLING;
                device_rw->enabled_status = true;
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u cashless abilitato -> %s",
                         (unsigned)device_index,
                         mdb_state_to_string(device_rw->poll_state));
            }
            break;

        case MDB_STATE_IDLE_POLLING:
            if (device->vend_status == MDB_VEND_PENDING && device->request_price_cents > 0U) {
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u invio VEND_REQUEST amount=%u item=0x%04X",
                         (unsigned)device_index,
                         (unsigned)device->request_price_cents,
                         (unsigned)device->cash_sale_item_number);
                ret = mdb_cashless_send_vend_command(device->address,
                                                     MDB_CASHLESS_VEND_REQUEST,
                                                     device->request_price_cents,
                                                     device->cash_sale_item_number,
                                                     rx,
                                                     sizeof(rx),
                                                     &rx_len,
                                                     120);
                response_ok = (ret == ESP_OK && rx_len > 0U);
                if (ret == ESP_OK) {
                    device_rw->vend_status = MDB_VEND_WORKING;
                    ESP_LOGI(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u vend in attesa esito lettore",
                             (unsigned)device_index);
                }
                break;
            }

            if (device->vend_success_requested && device->approved_price_cents > 0U) {
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u invio VEND_SUCCESS amount=%u",
                         (unsigned)device_index,
                         (unsigned)device->approved_price_cents);
                ret = mdb_cashless_send_vend_command(device->address,
                                                     MDB_CASHLESS_VEND_SUCCESS,
                                                     device->approved_price_cents,
                                                     device->cash_sale_item_number,
                                                     rx,
                                                     sizeof(rx),
                                                     &rx_len,
                                                     120);
                response_ok = (ret == ESP_OK && rx_len > 0U);
                if (ret == ESP_OK) {
                    device_rw->vend_success_requested = false;
                    device_rw->session_state = MDB_CASHLESS_SESSION_OPEN;
                    ESP_LOGI(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u VEND_SUCCESS confermato al lettore",
                             (unsigned)device_index);
                }
                break;
            }

            if (device->session_complete_requested) {
                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u invio SESSION_COMPLETE",
                         (unsigned)device_index);
                ret = mdb_cashless_send_vend_command(device->address,
                                                     MDB_CASHLESS_VEND_SESSION_COMPLETE,
                                                     0U,
                                                     0xFFFFU,
                                                     rx,
                                                     sizeof(rx),
                                                     &rx_len,
                                                     120);
                response_ok = (ret == ESP_OK && rx_len > 0U);
                if (ret == ESP_OK) {
                    device_rw->session_complete_requested = false;
                    (void)mdb_cashless_close_session_locally(device_index);
                    ESP_LOGI(TAG_CASH,
                             "[C] [mdb_cashless_sm] dev=%u richiesta chiusura sessione inviata",
                             (unsigned)device_index);
                }
                break;
            }

            if (device->revalue_status == MDB_REVALUE_REQUEST_PENDING) {
                uint8_t revalue_subcommand = (device->revalue_price_cents > 0U)
                                                 ? MDB_CASHLESS_REVALUE_REQUEST
                                                 : MDB_CASHLESS_REVALUE_LIMIT_REQUEST;

                ESP_LOGI(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u invio %s amount=%u",
                         (unsigned)device_index,
                         (revalue_subcommand == MDB_CASHLESS_REVALUE_REQUEST) ? "REVALUE_REQUEST" : "REVALUE_LIMIT",
                         (unsigned)device->revalue_price_cents);

                ret = mdb_cashless_send_revalue_command(device->address,
                                                        revalue_subcommand,
                                                        device->revalue_price_cents,
                                                        rx,
                                                        sizeof(rx),
                                                        &rx_len,
                                                        120);
                response_ok = (ret == ESP_OK && rx_len > 0U);
                if (ret == ESP_OK) {
                    parsed_response = !(rx_len == 1U && rx[0] == MDB_ACK) && response_ok;
                    if (!(rx_len == 1U && rx[0] == MDB_ACK)) {
                        mdb_cashless_handle_poll_response(device_index, rx, rx_len);
                    }
                    if (revalue_subcommand == MDB_CASHLESS_REVALUE_REQUEST &&
                        device_rw->revalue_status == MDB_REVALUE_REQUEST_PENDING) {
                        device_rw->revalue_status = MDB_REVALUE_IN_PROGRESS;
                        ESP_LOGI(TAG_CASH,
                                 "[C] [mdb_cashless_sm] dev=%u rivalutazione in progress",
                                 (unsigned)device_index);
                    }
                }
                break;
            }

            ret = mdb_cashless_send_simple_command(device->address,
                                                   MDB_CASHLESS_CMD_POLL,
                                                   NULL,
                                                   0,
                                                   rx,
                                                   sizeof(rx),
                                                   &rx_len,
                                                   50);  //TIMEOUT BREVE PER POLL
            response_ok = (ret == ESP_OK && rx_len > 0U);
            if (ret == ESP_OK) {
                parsed_response = !(rx_len == 1U && rx[0] == MDB_ACK) && response_ok;
                if (!(rx_len == 1U && rx[0] == MDB_ACK)) {
                    mdb_cashless_handle_poll_response(device_index, rx, rx_len);
                }

                if (parsed_response) {
                    s_cashless_idle_poll_timeout_count[device_index] = 0;
                    s_cashless_idle_poll_ack_only_count[device_index] = 0;
                    s_cashless_idle_poll_error_count[device_index] = 0;
                } else {
                    s_cashless_idle_poll_timeout_count[device_index] = 0;
                    s_cashless_idle_poll_ack_only_count[device_index]++;
                    s_cashless_idle_poll_error_count[device_index] = 0;
                    mdb_cashless_log_idle_poll_diagnostics(device_index, device);
                }
            } else if (ret == ESP_ERR_TIMEOUT) {
                s_cashless_idle_poll_timeout_count[device_index]++;
                s_cashless_idle_poll_ack_only_count[device_index] = 0;
                s_cashless_idle_poll_error_count[device_index] = 0;
                mdb_cashless_log_idle_poll_diagnostics(device_index, device);
            } else {
                s_cashless_idle_poll_timeout_count[device_index] = 0;
                s_cashless_idle_poll_ack_only_count[device_index] = 0;
                s_cashless_idle_poll_error_count[device_index]++;
                mdb_cashless_log_idle_poll_diagnostics(device_index, device);
            }
            break;

        case MDB_STATE_ERROR:
        {
            /* [C] Recovery automatico: se il lettore è in ERROR da più di
             * MDB_ERROR_RECOVERY_MS e non ci sono sessioni aperte, esegui
             * un reset e torna in INIT_RESET. */
            TickType_t now = xTaskGetTickCount();
            if (s_cashless_error_since_tick[device_index] == 0U) {
                s_cashless_error_since_tick[device_index] = now;
            } else if (!device->session_open &&
                       (uint32_t)pdTICKS_TO_MS(now - s_cashless_error_since_tick[device_index]) >= MDB_ERROR_RECOVERY_MS) {
                ESP_LOGW(TAG_CASH,
                         "[C] [mdb_cashless_sm] dev=%u in ERROR da >%u ms, reset di recupero automatico",
                         (unsigned)device_index,
                         (unsigned)MDB_ERROR_RECOVERY_MS);
                s_cashless_error_since_tick[device_index] = 0U;
                s_cashless_reset_retries[device_index] = 0;
                s_cashless_setup_retries[device_index] = 0;
                mdb_cashless_reset_device(device_index);
                device_rw->poll_state = MDB_STATE_INIT_RESET;
            }
            break;
        }

        default:
            ESP_LOGW(TAG_CASH,
                     "[C] [mdb_cashless_sm] dev=%u stato sconosciuto=%d, ritorno a init_reset",
                     (unsigned)device_index,
                     (int)device->poll_state);
            device_rw->poll_state = MDB_STATE_INIT_RESET;
            break;
    }

    if (parsed_response && device_rw->last_response_code == MDB_CASHLESS_RESP_JUST_RESET) {
        device_rw->poll_state = MDB_STATE_INIT_SETUP;
        ESP_LOGW(TAG_CASH,
                 "[C] [mdb_cashless_sm] dev=%u lettore ha segnalato JUST_RESET -> init_setup",
                 (unsigned)device_index);
    }

    if (parsed_response && device_rw->last_response_code == MDB_CASHLESS_RESP_OUT_OF_SEQUENCE) {
        device_rw->poll_state = MDB_STATE_INIT_RESET;
        device_rw->enabled_status = false;
        s_cashless_reset_retries[device_index] = 0;
        s_cashless_setup_retries[device_index] = 0;
        ESP_LOGW(TAG_CASH,
                 "[C] [mdb_cashless_sm] dev=%u OUT_OF_SEQUENCE -> init_reset",
                 (unsigned)device_index);
    }

    mdb_cashless_update_public_status();
}


/** @brief Avvia il motore di database.
 *  
 *  @param [in] arg Puntatore a dati di configurazione o contesto necessari per l'avvio del motore.
 *  
 *  @return Nessun valore di ritorno.
 */
void mdb_engine_run(void *arg) {
    TickType_t period_ticks = pdMS_TO_TICKS(20);
    TickType_t heartbeat_ticks = pdMS_TO_TICKS(5000);
    TickType_t last_heartbeat = xTaskGetTickCount();

    if (arg) {
        TickType_t configured_ticks = (TickType_t)(uintptr_t)arg;
        if (configured_ticks > 0) {
            period_ticks = configured_ticks;
        }
    }

    ESP_LOGI(TAG_ENGINE, "[C] [mdb_engine_run] motore di polling MDB avviato");
    while (1) {
        mdb_coin_sm();
        for (size_t device_index = 0; device_index < mdb_cashless_get_device_count(); ++device_index) {
            mdb_cashless_sm(device_index);
        }
        mdb_cashless_dispatch_pending_credit();
        mdb_cashless_dispatch_pending_vend();

        {
            TickType_t now_ticks = xTaskGetTickCount();
            if ((now_ticks - last_heartbeat) >= heartbeat_ticks) {
                const mdb_cashless_device_t *dev0 = NULL;
                last_heartbeat = now_ticks;
                if (mdb_cashless_get_device_count() > 0U) {
                    dev0 = mdb_cashless_get_device(0);
                }

                if (dev0) {
                    ESP_LOGI(TAG_ENGINE,
                             "[C] [mdb_engine_run] hb dev0 state=%s present=%d session_open=%d last_resp=0x%02X credit=%u",
                             mdb_state_to_string(dev0->poll_state),
                             dev0->present ? 1 : 0,
                             dev0->session_open ? 1 : 0,
                             dev0->last_response_code,
                             (unsigned)dev0->credit_cents);
                } else {
                    ESP_LOGI(TAG_ENGINE, "[C] [mdb_engine_run] hb nessun device cashless registrato");
                }
            }
        }

        // mdb_bill_sm(); // futuro
        vTaskDelay((period_ticks > 0) ? period_ticks : 1);
    }
}


/**
 * @brief Avvia il motore del database.
 * 
 * Questa funzione avvia il motore del database e prepara l'ambiente per l'accesso ai dati.
 * 
 * @return esp_err_t
 * - ESP_OK: Operazione riuscita.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t mdb_start_engine(void) {
    /* Il task mdb_engine è ora gestito da tasks.c tramite mdb_engine_run().
     * Questa funzione è mantenuta per retrocompatibilità ma non crea il task. */
    ESP_LOGI(TAG_ENGINE, "[C] [mdb_start_engine] task gestito da tasks.c (mdb_engine_run)");
    return ESP_OK;
}


/**
 * @brief Inizializza il database.
 *
 * Questa funzione inizializza il database e prepara tutti i componenti necessari per l'uso successivo.
 *
 * @return
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t mdb_init(void)
{
    device_config_t *d_cfg = device_config_get();
    int tout_symbols = 0;

    if (s_mdb_driver_initialized) {
        /* Evita reinizializzazioni della UART MDB quando la UI riapre la pagina Programmi. */
        s_mdb_init_failed = false;
        s_mdb_runtime_fault = false;
        return ESP_OK;
    }

    if (!d_cfg) {
        s_mdb_driver_initialized = false;
        s_mdb_init_failed = true;
        ESP_LOGE(TAG_INIT, "[C] [mdb_init] device_config non disponibile");
        return ESP_ERR_INVALID_STATE;
    }

    s_mdb_init_failed = false;
    ESP_LOGI(TAG_INIT,
             "[C] [mdb_init] uart=%d tx=%d rx=%d baud=%ld stop_bits=%d rx_buf=%d tx_buf=%d",
             MDB_UART_PORT,
             MDB_TX_GPIO,
             MDB_RX_GPIO,
             (long)d_cfg->mdb_serial.baud_rate,
             d_cfg->mdb_serial.stop_bits,
             d_cfg->mdb_serial.rx_buf_size,
             d_cfg->mdb_serial.tx_buf_size);

    uart_config_t uart_config = {
        .baud_rate = d_cfg->mdb_serial.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, // Gestito manualmente per il 9° bit
        .stop_bits = (d_cfg->mdb_serial.stop_bits == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(MDB_UART_PORT, d_cfg->mdb_serial.rx_buf_size, d_cfg->mdb_serial.tx_buf_size, 0, NULL, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        s_mdb_driver_initialized = false;
        s_mdb_init_failed = true;
        ESP_LOGE(TAG_INIT, "[C] [mdb_init] uart_driver_install fallita: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_param_config(MDB_UART_PORT, &uart_config);
    if (ret != ESP_OK) {
        s_mdb_driver_initialized = false;
        s_mdb_init_failed = true;
        ESP_LOGE(TAG_INIT, "[C] [mdb_init] uart_param_config fallita: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = uart_set_pin(MDB_UART_PORT, MDB_TX_GPIO, MDB_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        s_mdb_driver_initialized = false;
        s_mdb_init_failed = true;
        ESP_LOGE(TAG_INIT, "[C] [mdb_init] uart_set_pin fallita: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Usiamo modalità RS485 Half Duplex per controllo RTS se necessario, 
    // ma MDB è solitamente TTL. Usiamo UART_MODE_UART o RS485_APP_CTRL.
    ret = uart_set_mode(MDB_UART_PORT, UART_MODE_UART);
    if (ret != ESP_OK) {
        s_mdb_driver_initialized = false;
        s_mdb_init_failed = true;
        ESP_LOGE(TAG_INIT, "[C] [mdb_init] uart_set_mode fallita: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Imposta il timeout RX hardware per scaricare nel ring buffer le trame MDB
       corte non appena il bus torna silenzioso. */
    tout_symbols = d_cfg->mdb_serial.baud_rate / 1000;
    if (tout_symbols < 1) {
        tout_symbols = 1;
    }
    ret = uart_set_rx_timeout(MDB_UART_PORT, tout_symbols);
    if (ret != ESP_OK) {
        s_mdb_driver_initialized = false;
        s_mdb_init_failed = true;
        ESP_LOGE(TAG_INIT, "[C] [mdb_init] uart_set_rx_timeout fallita: %s", esp_err_to_name(ret));
        return ret;
    }

    s_mdb_driver_initialized = true;
    s_mdb_runtime_fault = false;
    mdb_cashless_init_state();
    mdb_cashless_update_public_status();
    ESP_LOGI(TAG_INIT,
             "[C] [mdb_init] inizializzazione MDB completata (rx_tout=%d simboli)",
             tout_symbols);

    return ESP_OK;
}


/**
 * @brief Invia un byte tramite la comunicazione MDB.
 *
 * @param [in] data Byte da inviare.
 * @param [in] mode_bit Bit di modalità da inviare.
 * @return esp_err_t Codice di errore.
 */
static esp_err_t mdb_send_byte(uint8_t data, bool mode_bit)
{
    // Simulazione 9° bit tramite parità
    bool data_parity = get_byte_parity(data);
    
    if (mode_bit) {
        // Vogliamo 9° bit = 1
        // Se data_parity è even (0), impostiamo ODD per avere parity bit = 1
        // Se data_parity è odd (1), impostiamo EVEN per avere parity bit = 1
        if (!data_parity) uart_set_parity(MDB_UART_PORT, UART_PARITY_ODD);
        else uart_set_parity(MDB_UART_PORT, UART_PARITY_EVEN);
    } else {
        // Vogliamo 9° bit = 0
        // Se data_parity è even (0), impostiamo EVEN per avere parity bit = 0
        // Se data_parity è odd (1), impostiamo ODD per avere parity bit = 0
        if (!data_parity) uart_set_parity(MDB_UART_PORT, UART_PARITY_EVEN);
        else uart_set_parity(MDB_UART_PORT, UART_PARITY_ODD);
    }
    
    uart_write_bytes(MDB_UART_PORT, &data, 1);
    
    // Attendiamo che il byte sia trasmesso prima di cambiare parità per il prossimo
    uart_wait_tx_done(MDB_UART_PORT, pdMS_TO_TICKS(100));
    
    return ESP_OK;
}

esp_err_t mdb_send_raw_byte(uint8_t data, bool mode_bit)
{
    return mdb_send_byte(data, mode_bit);
}


/**
 * @brief Invia un pacchetto tramite la libreria MDB.
 *
 * @param [in] address L'indirizzo del dispositivo a cui inviare il pacchetto.
 * @param [in] data Un puntatore al buffer contenente i dati del pacchetto.
 * @param [in] len La lunghezza del buffer dei dati.
 *
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t mdb_send_packet(uint8_t address, const uint8_t *data, size_t len)
{
    uint8_t checksum = address;

    // ESP_LOGI(TAG_IO, "[C] [mdb_send_packet] addr=0x%02X len=%u", address, (unsigned)len);

    // Svuota buffer RX prima di inviare
    uart_flush_input(MDB_UART_PORT);

    // 1. Invia Indirizzo (9° bit = 1)
    mdb_send_byte(address, true);

    // 2. Invia Dati (9° bit = 0)
    for (size_t i = 0; i < len; i++) {
        mdb_send_byte(data[i], false);
        checksum += data[i];
    }

    // 3. Invia Checksum (9° bit = 0)
    mdb_send_byte(checksum, false);

    {
        uint8_t raw[40] = {0};
        size_t raw_len = 0;

        if (len + 2U <= sizeof(raw)) {
            raw[raw_len++] = address;
            if (data && len > 0U) {
                memcpy(raw + raw_len, data, len);
                raw_len += len;
            }
            raw[raw_len++] = checksum;
            // mdb_log_packet_bytes(TAG_IO, "mdb_send_packet.tx", raw, raw_len);
        }
    }

    return ESP_OK;
}


/**
 * @brief Riceve un pacchetto da una coda di messaggi.
 *
 * Questa funzione riceve un pacchetto da una coda di messaggi e lo memorizza in un buffer fornito.
 *
 * @param [out] out_data Puntatore al buffer dove memorizzare il pacchetto ricevuto.
 * @param max_len Lunghezza massima del buffer out_data.
 * @param [out] out_len Puntatore alla variabile dove memorizzare la lunghezza effettiva del pacchetto ricevuto.
 * @param timeout_ms Timeout in millisecondi per l'attesa del pacchetto.
 *
 * @return
 * - ESP_OK: Operazione riuscita.
 * - ESP_ERR_INVALID_ARG: Argomento non valido.
 * - ESP_ERR_TIMEOUT: Timeout scaduto.
 * - ESP_FAIL: Operazione non riuscita per altri motivi.
 */
esp_err_t mdb_receive_packet(uint8_t *out_data, size_t max_len, size_t *out_len, uint32_t timeout_ms)
{
    if (!out_data || !out_len) return ESP_ERR_INVALID_ARG;

    size_t received = 0;
    uint8_t byte;
    uint8_t checksum = 0;
    TickType_t start_tick = xTaskGetTickCount();
    
    while ((xTaskGetTickCount() - start_tick) < pdMS_TO_TICKS(timeout_ms)) {
        if (uart_read_bytes(MDB_UART_PORT, &byte, 1, pdMS_TO_TICKS(timeout_ms)) > 0) {
            out_data[received++] = byte;
            
            while (received < max_len) {
                if (uart_read_bytes(MDB_UART_PORT, &byte, 1, pdMS_TO_TICKS(5)) > 0) {
                    out_data[received++] = byte;
                } else {
                    break; 
                }
            }
            break; 
        }
    }

    if (received == 0) {
        // ESP_LOGI(TAG_IO, "[C] [mdb_receive_packet] timeout dopo %u ms", (unsigned)timeout_ms);
        return ESP_ERR_TIMEOUT;
    }
    *out_len = received;

    // mdb_log_packet_bytes(TAG_IO, "mdb_receive_packet.rx", out_data, received);

    if (received == 1) {
        // ESP_LOGI(TAG_IO, "[C] [mdb_receive_packet] byte singolo=0x%02X", out_data[0]);
        return ESP_OK;
    }

    for (size_t i = 0; i < received - 1; i++) checksum += out_data[i];
    
    if (checksum != out_data[received - 1]) {
        ESP_LOGW(TAG_IO,
                 "[C] [mdb_receive_packet] checksum errata calc=0x%02X recv=0x%02X",
                 checksum,
                 out_data[received - 1]);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

#endif /* DNA_MDB == 0 */

/*
 * Mockup — nessuna UART MDB reale, nessuna periferica.
 * Attiva quando DNA_MDB == 1
 */
#if defined(DNA_MDB) && (DNA_MDB == 1)

static const char *TAG_MOCK = "MDB_MOCK";
static mdb_status_t s_mock_mdb_status = {0};
static bool s_mock_mdb_initialized = false;
static mdb_cashless_credit_callback_t s_mock_credit_callback = NULL;
static mdb_cashless_vend_callback_t s_mock_vend_callback = NULL;

const mdb_status_t *mdb_get_status(void)
{
    return &s_mock_mdb_status;
}

device_component_status_t mdb_get_component_status(void)
{
    const device_config_t *cfg = device_config_get();
    bool coin_enabled = cfg && cfg->sensors.mdb_enabled && cfg->mdb.coin_acceptor_en;
    bool cashless_enabled = cfg && cfg->sensors.mdb_enabled && cfg->mdb.cashless_en;

    if (!cfg || !cfg->sensors.mdb_enabled || (!coin_enabled && !cashless_enabled)) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (!s_mock_mdb_initialized) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    return (s_mock_mdb_status.coin.is_online || s_mock_mdb_status.cashless.is_online)
               ? DEVICE_COMPONENT_STATUS_ONLINE
               : DEVICE_COMPONENT_STATUS_OFFLINE;
}

void mdb_register_cashless_credit_callback(mdb_cashless_credit_callback_t callback)
{
    s_mock_credit_callback = callback;
}

void mdb_register_cashless_vend_callback(mdb_cashless_vend_callback_t callback)
{
    s_mock_vend_callback = callback;
}

bool mdb_cashless_request_program_vend(uint16_t amount_cents, uint16_t item_number)
{
    ESP_LOGI(TAG_MOCK,
             "[C] [MOCK] mdb_cashless_request_program_vend: amount=%u item=0x%04X",
             (unsigned)amount_cents,
             (unsigned)item_number);
    (void)s_mock_credit_callback;
    (void)s_mock_vend_callback;
    return true;
}

bool mdb_cashless_confirm_vend_success(uint16_t approved_amount_cents)
{
    ESP_LOGI(TAG_MOCK,
             "[C] [MOCK] mdb_cashless_confirm_vend_success: amount=%u",
             (unsigned)approved_amount_cents);
    return true;
}

bool mdb_cashless_complete_active_session(void)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] mdb_cashless_complete_active_session");
    return true;
}

bool mdb_cashless_request_active_revalue_limit(void)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] mdb_cashless_request_active_revalue_limit");
    return true;
}

bool mdb_cashless_request_active_revalue(uint16_t amount_cents)
{
    ESP_LOGI(TAG_MOCK,
             "[C] [MOCK] mdb_cashless_request_active_revalue: amount=%u",
             (unsigned)amount_cents);
    return true;
}


/**
 * @brief Inizializza il database.
 *
 * Questa funzione inizializza il database e prepara tutti i componenti necessari per l'uso successivo.
 *
 * @return
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t mdb_init(void)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] mdb_init: bus MDB simulato");
    s_mock_mdb_initialized = true;
    s_mock_mdb_status.coin.state = MDB_STATE_IDLE_POLLING;
    s_mock_mdb_status.coin.is_online = true;
    s_mock_mdb_status.cashless.state = MDB_STATE_IDLE_POLLING;
    s_mock_mdb_status.cashless.is_online = true;
    s_mock_mdb_status.cashless.online_devices = 1;
    s_mock_mdb_status.cashless.active_device_index = 0;
    s_mock_mdb_status.cashless.session_open = true;
    s_mock_mdb_status.cashless.revalue_limit_cents = 1000;
    return ESP_OK;
}


/**
 * @brief Avvia il motore di database.
 *
 * Questa funzione avvia il motore di database e prepara l'ambiente per l'accesso ai dati.
 *
 * @return esp_err_t
 * - ESP_OK: Operazione riuscita.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t mdb_start_engine(void)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] mdb_start_engine: polling disabilitato");
    return ESP_OK;
}


/**
 * @brief Avvia il motore del database.
 * 
 * Questa funzione avvia il motore del database e gestisce il suo ciclo di vita.
 * 
 * @param arg Puntatore a dati aggiuntivi necessari per l'avvio del motore.
 * @return Nessun valore di ritorno.
 */
void mdb_engine_run(void *arg)
{
    /* Mockup: MDB disabilitato — task in attesa indefinita */
    while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); }
}


/**
 * @brief Invia un pacchetto tramite la libreria MDB.
 *
 * @param [in] address L'indirizzo del dispositivo a cui inviare il pacchetto.
 * @param [in] data Un puntatore al buffer contenente i dati del pacchetto.
 * @param [in] len La lunghezza del buffer dei dati.
 *
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t mdb_send_packet(uint8_t address, const uint8_t *data, size_t len)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] mdb_send_packet: addr=0x%02X len=%zu ignorato", address, len);
    (void)data;
    return ESP_OK;
}

esp_err_t mdb_send_raw_byte(uint8_t data, bool mode_bit)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] mdb_send_raw_byte: data=0x%02X b9=%d ignorato", data, (int)mode_bit);
    return ESP_OK;
}


/**
 * @brief Riceve un pacchetto da una coda di messaggi.
 *
 * Questa funzione riceve un pacchetto da una coda di messaggi e lo memorizza in un buffer fornito.
 *
 * @param [out] out_data Puntatore al buffer dove memorizzare il pacchetto ricevuto.
 * @param max_len Lunghezza massima del buffer di destinazione.
 * @param [out] out_len Puntatore alla variabile dove memorizzare la lunghezza effettiva del pacchetto ricevuto.
 * @param timeout_ms Timeout in millisecondi per l'attesa del pacchetto.
 *
 * @return
 * - ESP_OK: Operazione riuscita.
 * - ESP_ERR_INVALID_ARG: Argomento non valido.
 * - ESP_ERR_TIMEOUT: Timeout scaduto.
 * - ESP_FAIL: Operazione non riuscita per altri motivi.
 */
esp_err_t mdb_receive_packet(uint8_t *out_data, size_t max_len, size_t *out_len, uint32_t timeout_ms)
{
    (void)out_data; (void)max_len; (void)timeout_ms;
    if (out_len) *out_len = 0;
    return ESP_ERR_TIMEOUT;
}

#endif /* DNA_MDB == 1 */
