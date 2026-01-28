#include "mdb.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "device_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "MDB";

#define MDB_UART_PORT   CONFIG_APP_MDB_UART_PORT
#define MDB_TX_GPIO     CONFIG_APP_MDB_TX_GPIO
#define MDB_RX_GPIO     CONFIG_APP_MDB_RX_GPIO

static mdb_status_t s_mdb_status = {0};

const mdb_status_t* mdb_get_status(void) {
    return &s_mdb_status;
}

// Helper per calcolare la parità di un byte (ritorna true se dispari)
static bool get_byte_parity(uint8_t data) {
    return __builtin_parity(data);
}

// Helper: Invia ACK
static void mdb_send_ack(void) {
    uint8_t ack = MDB_ACK;
    // ACK è un data byte, quindi 9° bit = 0
    // Per avere 9° bit = 0 con dato 0x00 (parità pari): usiamo EVEN
    uart_set_parity(MDB_UART_PORT, UART_PARITY_EVEN);
    uart_write_bytes(MDB_UART_PORT, &ack, 1);
    uart_wait_tx_done(MDB_UART_PORT, pdMS_TO_TICKS(10));
}

// Logic for Coin Acceptor State Machine
static void mdb_coin_sm(void) {
    uint8_t rx[36];
    size_t rx_len;
    esp_err_t ret;

    switch (s_mdb_status.coin.state) {
        case MDB_STATE_INACTIVE:
            if (device_config_get()->mdb.coin_acceptor_en) s_mdb_status.coin.state = MDB_STATE_INIT_RESET;
            break;

        case MDB_STATE_INIT_RESET:
            ESP_LOGI(TAG, "MDB Coin: Sending RESET...");
            mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_CMD_RESET, NULL, 0);
            vTaskDelay(pdMS_TO_TICKS(500)); // Aspetta reboot periferica
            s_mdb_status.coin.state = MDB_STATE_INIT_SETUP;
            break;

        case MDB_STATE_INIT_SETUP:
            ESP_LOGI(TAG, "MDB Coin: Sending SETUP...");
            mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_CMD_SETUP, NULL, 0);
            if (mdb_receive_packet(rx, sizeof(rx), &rx_len, 100) == ESP_OK) {
                if (rx_len >= 23) {
                    s_mdb_status.coin.feature_level = rx[0];
                    s_mdb_status.coin.currency_code = (rx[1] << 8) | rx[2];
                    s_mdb_status.coin.scaling_factor = rx[3];
                    s_mdb_status.coin.decimal_places = rx[4];
                    s_mdb_status.coin.coin_routing = (rx[5] << 8) | rx[6];
                    for (int i = 0; i < 16; i++) {
                        s_mdb_status.coin.coin_values[i] = rx[7 + i];
                    }
                    
                    ESP_LOGI(TAG, "MDB Coin Setup: Level %d, Currency %04X, Scaling %d, Decimal %d", 
                             s_mdb_status.coin.feature_level, s_mdb_status.coin.currency_code,
                             s_mdb_status.coin.scaling_factor, s_mdb_status.coin.decimal_places);
                    
                    mdb_send_ack();
                    s_mdb_status.coin.state = MDB_STATE_INIT_ENABLE; // Passa alla fase di abilitazione
                    s_mdb_status.coin.is_online = true;
                } else {
                    ESP_LOGW(TAG, "MDB Coin: Setup response too short (%d)", rx_len);
                }
            }
            break;

        case MDB_STATE_INIT_ENABLE:
            ESP_LOGI(TAG, "MDB Coin: Enabling all coin types...");
            {
                uint8_t enable_data[4] = {0xFF, 0xFF, 0xFF, 0xFF}; // Abilita tutto (Accept & Dispense)
                mdb_send_packet(MDB_ADDR_COIN_CHANGER | MDB_COIN_CMD_COIN_TYPE, enable_data, 4);
            }
            if (mdb_receive_packet(rx, sizeof(rx), &rx_len, 50) == ESP_OK && rx[0] == MDB_ACK) {
                ESP_LOGI(TAG, "MDB Coin: Enabled!");
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
                    ESP_LOGI(TAG, "MDB Coin Event: Received %d bytes", (int)rx_len);
                    mdb_send_ack();
                    
                    // Parsing eventi (solo i più importanti)
                    for (int i=0; i < rx_len-1; i++) { // L'ultimo è il checksum
                        uint8_t b = rx[i];
                        if ((b & 0x80) == 0) { // Moneta accettata
                            uint8_t routing = (b >> 4) & 0x03; // 00: Cash box, 01: Tubes, 02: Not Used, 03: Reject
                            uint8_t type = b & 0x0F;
                            uint16_t real_val = s_mdb_status.coin.coin_values[type] * s_mdb_status.coin.scaling_factor;
                            s_mdb_status.coin.credit_cents += real_val;
                            ESP_LOGI(TAG, "MDB Coin: Moneta tipo %d accreditata (+%d units), Routing: %d", type, real_val, routing);
                        } else if (b == 0x0B) { // Changer Reset
                            ESP_LOGW(TAG, "MDB Coin: Reset rilevato, ri-inizializzazione...");
                            s_mdb_status.coin.state = MDB_STATE_INIT_SETUP;
                        }
                    }
                }
                s_mdb_status.coin.is_online = true;
            } else {
                s_mdb_status.coin.is_online = false;
                ESP_LOGD(TAG, "MDB Coin: No response");
            }
            break;
            
        default: break;
    }
}

static void mdb_engine_task(void *arg) {
    ESP_LOGI(TAG, "MDB Polling Engine avviato");
    while (1) {
        mdb_coin_sm();
        // mdb_bill_sm(); // futuro
        vTaskDelay(pdMS_TO_TICKS(500)); // Ciclo di polling
    }
}

esp_err_t mdb_start_engine(void) {
    xTaskCreate(mdb_engine_task, "mdb_engine", 4096, NULL, 5, NULL);
    return ESP_OK;
}

esp_err_t mdb_init(void)
{
    ESP_LOGI(TAG, "Inizializzazione MDB su UART %d (TX:%d RX:%d)", MDB_UART_PORT, MDB_TX_GPIO, MDB_RX_GPIO);

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, // Gestito manualmente per il 9° bit
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(MDB_UART_PORT, 1024, 1024, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(MDB_UART_PORT, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(MDB_UART_PORT, MDB_TX_GPIO, MDB_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Usiamo modalità RS485 Half Duplex per controllo RTS se necessario, 
    // ma MDB è solitamente TTL. Usiamo UART_MODE_UART o RS485_APP_CTRL.
    ESP_ERROR_CHECK(uart_set_mode(MDB_UART_PORT, UART_MODE_UART));

    return ESP_OK;
}

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

esp_err_t mdb_send_packet(uint8_t address, const uint8_t *data, size_t len)
{
    uint8_t checksum = address;
    
    ESP_LOGD(TAG, "Sending MDB packet: Addr 0x%02X, Len %zu", address, len);

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

    return ESP_OK;
}

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

    if (received == 0) return ESP_ERR_TIMEOUT;
    *out_len = received;

    if (received == 1) return ESP_OK;

    for (size_t i = 0; i < received - 1; i++) checksum += out_data[i];
    
    if (checksum != out_data[received - 1]) {
        ESP_LOGW(TAG, "MDB Checksum Error: Calc 0x%02X != Recv 0x%02X", checksum, out_data[received-1]);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}
