#include "modbus_relay.h"

#include "device_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

#include "mbcontroller.h"

/* MB_FUNC_* constants from mb_proto.h (private header — defined locally) */
#ifndef MB_FUNC_READ_COILS
#define MB_FUNC_READ_COILS           1
#define MB_FUNC_READ_DISCRETE_INPUTS 2
#define MB_FUNC_WRITE_SINGLE_COIL    5
#define MB_FUNC_WRITE_MULTIPLE_COILS 15
#endif

#ifndef DNA_RS485
#define DNA_RS485 0
#endif

#define TAG "MODBUS_RELAY"

#define MODBUS_RELAY_MIN_SLAVE_ID 1U
#define MODBUS_RELAY_MAX_SLAVE_ID 255U
#define MODBUS_RELAY_DEFAULT_POINTS 8U
#define MODBUS_RELAY_MAX_RETRIES 5U

esp_err_t rs485_deinit(void);

typedef struct {
    bool initialized;
    bool running;
    void *handler;
    modbus_relay_status_t status;
#if defined(DNA_RS485) && (DNA_RS485 == 1)
    uint8_t mock_relay_bits[MODBUS_RELAY_MAX_BYTES];
    uint8_t mock_input_bits[MODBUS_RELAY_MAX_BYTES];
#endif
} modbus_relay_ctx_t;

static modbus_relay_ctx_t s_ctx = {0};
static SemaphoreHandle_t s_lock = NULL;

enum {
    MODBUS_RELAY_CID_COILS = 0,
    MODBUS_RELAY_CID_DISCRETE = 1,
    MODBUS_RELAY_CID_COUNT
};

static size_t bits_to_bytes(uint16_t count)
{
    return (size_t)((count + 7U) / 8U);
}

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static uint8_t clamp_slave_id(uint8_t slave_id)
{
    if (slave_id < MODBUS_RELAY_MIN_SLAVE_ID || slave_id > MODBUS_RELAY_MAX_SLAVE_ID) {
        return MODBUS_RELAY_MIN_SLAVE_ID;
    }
    return slave_id;
}

static uint16_t sanitize_count(uint16_t count)
{
    if (count == 0U) {
        return MODBUS_RELAY_DEFAULT_POINTS;
    }
    if (count > MODBUS_RELAY_MAX_POINTS) {
        return MODBUS_RELAY_MAX_POINTS;
    }
    return count;
}

static uint8_t sanitize_retries(uint8_t retries)
{
    if (retries > MODBUS_RELAY_MAX_RETRIES) {
        return MODBUS_RELAY_MAX_RETRIES;
    }
    return retries;
}

static void status_set_comm(uint8_t slave_id,
                            uint16_t relay_start,
                            uint16_t relay_count,
                            uint16_t input_start,
                            uint16_t input_count)
{
    s_ctx.status.slave_id = clamp_slave_id(slave_id);
    s_ctx.status.relay_start = relay_start;
    s_ctx.status.relay_count = sanitize_count(relay_count);
    s_ctx.status.input_start = input_start;
    s_ctx.status.input_count = sanitize_count(input_count);
}

static esp_err_t ensure_lock(void)
{
    if (s_lock) {
        return ESP_OK;
    }
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t take_lock(void)
{
    esp_err_t err = ensure_lock();
    if (err != ESP_OK) {
        return err;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void give_lock(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}

#if DNA_RS485 == 0

static uart_parity_t map_parity(int parity)
{
    if (parity == 1) {
        return UART_PARITY_ODD;
    }
    if (parity == 2) {
        return UART_PARITY_EVEN;
    }
    return UART_PARITY_DISABLE;
}

static uart_word_length_t map_data_bits(int data_bits)
{
    return (data_bits == 7) ? UART_DATA_7_BITS : UART_DATA_8_BITS;
}

static uart_stop_bits_t map_stop_bits(int stop_bits)
{
    return (stop_bits == 2) ? UART_STOP_BITS_2 : UART_STOP_BITS_1;
}

// Aggiungo monitor UART a basso livello per debug
static void uart_monitor_task(void *arg)
{
    uart_port_t uart_num = (uart_port_t)CONFIG_APP_RS485_UART_PORT;
    static uint8_t data[256];  // Static per evitare stack overflow
    
    ESP_LOGI(TAG, "[C] Monitor UART attivo su porta %u", uart_num);
    
    while (1) {
        int len = uart_read_bytes(uart_num, data, sizeof(data), pdMS_TO_TICKS(100));
        if (len > 0) {
            ESP_LOGI(TAG, "[C] UART RX %d bytes:", len);
            
            // Log esadecimale semplificato per evitare stack overflow
            if (len <= 32) {
                char hex_str[128] = {0};
                for (int i = 0; i < len; i++) {
                    int pos = i * 3;
                    if (pos < sizeof(hex_str) - 4) {
                        snprintf(hex_str + pos, sizeof(hex_str) - pos, "%02X ", data[i]);
                    }
                }
                ESP_LOGI(TAG, "[C] RX: %s", hex_str);
            } else {
                // Per pacchetti lunghi, log solo i primi 16 byte
                char hex_str[64] = {0};
                for (int i = 0; i < 16; i++) {
                    int pos = i * 3;
                    if (pos < sizeof(hex_str) - 4) {
                        snprintf(hex_str + pos, sizeof(hex_str) - pos, "%02X ", data[i]);
                    }
                }
                ESP_LOGI(TAG, "[C] RX (first 16): %s... (+%d bytes)", hex_str, len - 16);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));  // Aumento delay per ridurre CPU usage
    }
    
    vTaskDelete(NULL);
}

static esp_err_t start_locked(void)
{
    if (s_ctx.running && s_ctx.handler != NULL) {
        return ESP_OK; // Già in esecuzione con handler valido
    }

    device_config_t *cfg = device_config_get();
    if (!cfg) {
        return ESP_ERR_INVALID_STATE;
    }

    mb_communication_info_t comm = {
        .ser_opts = {
            .mode = MB_RTU,
            .uid = clamp_slave_id(cfg->modbus.slave_id),
            .port = (uart_port_t)CONFIG_APP_RS485_UART_PORT,
            .response_tout_ms = (cfg->modbus.timeout_ms > 0U) ? (uint32_t)cfg->modbus.timeout_ms : 200U,
            .baudrate = (cfg->rs485.baud_rate > 0) ? (uint32_t)cfg->rs485.baud_rate : 9600U,
            .data_bits = map_data_bits(cfg->rs485.data_bits),
            .stop_bits = map_stop_bits(cfg->rs485.stop_bits),
            .parity = map_parity(cfg->rs485.parity),
        }
    };

    esp_err_t release_err = rs485_deinit();
    if (release_err != ESP_OK) {
        ESP_LOGW(TAG, "[C] rs485_deinit prima di Modbus: %s", esp_err_to_name(release_err));
    }

    // Configura i pin UART per Modbus (DE su RTS)
    esp_err_t pin_err = uart_set_pin(CONFIG_APP_RS485_UART_PORT, 
                                   CONFIG_APP_RS485_TX_GPIO,
                                   CONFIG_APP_RS485_RX_GPIO, 
                                   CONFIG_APP_RS485_DE_GPIO,
                                   UART_PIN_NO_CHANGE);
    if (pin_err != ESP_OK) {
        ESP_LOGE(TAG, "[C] uart_set_pin per Modbus fallita: %s", esp_err_to_name(pin_err));
        return pin_err;
    }

    void *handler = NULL;
    esp_err_t err = mbc_master_create_serial(&comm, &handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] mbc_master_create_serial fallita: %s", esp_err_to_name(err));
        s_ctx.status.last_error = (int32_t)err;
        return err;
    }

    mb_parameter_descriptor_t descriptors[MODBUS_RELAY_CID_COUNT] = {
        {
            .cid = MODBUS_RELAY_CID_COILS,
            .param_key = "relay_coils",
            .param_units = "bits",
            .mb_slave_addr = comm.ser_opts.uid,
            .mb_param_type = MB_PARAM_COIL,
            .mb_reg_start = cfg->modbus.relay_start,
            .mb_size = sanitize_count(cfg->modbus.relay_count),
            .param_offset = 0,
            .param_type = PARAM_TYPE_U16,
            .param_size = PARAM_SIZE_U16,
            .param_opts = {
                .opt1 = 0,
                .opt2 = 0,
                .opt3 = 0,
            },
            .access = PAR_PERMS_READ_WRITE,
        },
        {
            .cid = MODBUS_RELAY_CID_DISCRETE,
            .param_key = "relay_inputs",
            .param_units = "bits",
            .mb_slave_addr = comm.ser_opts.uid,
            .mb_param_type = MB_PARAM_DISCRETE,
            .mb_reg_start = cfg->modbus.input_start,
            .mb_size = sanitize_count(cfg->modbus.input_count),
            .param_offset = 0,
            .param_type = PARAM_TYPE_U16,
            .param_size = PARAM_SIZE_U16,
            .param_opts = {
                .opt1 = 0,
                .opt2 = 0,
                .opt3 = 0,
            },
            .access = PAR_PERMS_READ,
        },
    };

    err = mbc_master_set_descriptor(handler, descriptors, MODBUS_RELAY_CID_COUNT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] mbc_master_set_descriptor fallita: %s", esp_err_to_name(err));
        (void)mbc_master_delete(handler);
        s_ctx.status.last_error = (int32_t)err;
        return err;
    }

    err = mbc_master_start(handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] mbc_master_start fallita: %s", esp_err_to_name(err));
        (void)mbc_master_delete(handler);
        s_ctx.status.last_error = (int32_t)err;
        return err;
    }

    err = uart_set_mode(CONFIG_APP_RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] uart_set_mode RS485 half-duplex fallita: %s", esp_err_to_name(err));
        (void)mbc_master_delete(handler);
        s_ctx.status.last_error = (int32_t)err;
        return err;
    }

    // Attendi stabilizzazione del controller Modbus
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "[C] Monitor UART disabilitato per non interferire con Modbus controller");

    s_ctx.handler = handler;
    s_ctx.initialized = true;
    s_ctx.running = true;
    s_ctx.status.initialized = true;
    s_ctx.status.running = true;
    s_ctx.status.last_error = ESP_OK;
    s_ctx.status.last_update_ms = now_ms();

    status_set_comm(cfg->modbus.slave_id,
                    cfg->modbus.relay_start,
                    cfg->modbus.relay_count,
                    cfg->modbus.input_start,
                    cfg->modbus.input_count);

    ESP_LOGI(TAG,
             "[C] Modbus master avviato su UART%d (slave=%u, baud=%lu)",
             CONFIG_APP_RS485_UART_PORT,
             (unsigned)comm.ser_opts.uid,
             (unsigned long)comm.ser_opts.baudrate);

    return ESP_OK;
}

static esp_err_t send_request_with_retry_locked(mb_param_request_t *request, void *data_ptr)
{
    if (!request || !data_ptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // Log dettagliato sullo stato
    ESP_LOGD(TAG, "[C] Modbus state check: handler=%p, running=%s", 
             s_ctx.handler, s_ctx.running ? "true" : "false");

    // Se handler invalido, prova a reinizializzare
    if (!s_ctx.handler || !s_ctx.running) {
        ESP_LOGW(TAG, "[C] Modbus handler invalido, tentativo reinizializzazione...");
        esp_err_t reinit_err = start_locked();
        if (reinit_err != ESP_OK) {
            ESP_LOGE(TAG, "[C] Reinizializzazione Modbus fallita: %s", esp_err_to_name(reinit_err));
            return ESP_ERR_INVALID_STATE;
        }
        ESP_LOGI(TAG, "[C] Modbus reinizializzato con successo");
    }

    device_config_t *cfg = device_config_get();
    uint8_t retries = 0;
    if (cfg) {
        retries = sanitize_retries(cfg->modbus.retries);
    }

    esp_err_t last_err = ESP_FAIL;
    for (uint8_t attempt = 0; attempt <= retries; ++attempt) {
        // Log pacchetto in uscita
        ESP_LOGD(TAG, "[C] Modbus TX attempt %u/%u: slave=%u cmd=0x%02X start=%u size=%u", 
                 attempt + 1, retries + 1, request->slave_addr, request->command, 
                 request->reg_start, request->reg_size);
        
        // Verifica handler prima di ogni tentativo
        if (!s_ctx.handler || !s_ctx.running) {
            ESP_LOGE(TAG, "[C] Handler diventato invalido durante tentativi");
            return ESP_ERR_INVALID_STATE;
        }
        
        last_err = mbc_master_send_request(s_ctx.handler, request, data_ptr);
        
        if (last_err == ESP_OK) {
            // Log pacchetto ricevuto (se presente)
            if (data_ptr && request->command == MB_FUNC_READ_COILS) {
                uint8_t *bytes = (uint8_t*)data_ptr;
                size_t byte_count = bits_to_bytes(request->reg_size);
                ESP_LOGD(TAG, "[C] Modbus RX OK: %zu bytes", byte_count);
                
                // Hexdump manuale
                char hex_str[64] = {0};
                for (size_t i = 0; i < byte_count && i < 16; i++) {
                    snprintf(hex_str + (i * 3), sizeof(hex_str) - (i * 3), "%02X ", bytes[i]);
                }
                ESP_LOGD(TAG, "[C] RX Data: %s", hex_str);
            } else if (data_ptr) {
                ESP_LOGD(TAG, "[C] Modbus RX OK");
                uint8_t *bytes = (uint8_t*)data_ptr;
                char hex_str[32] = {0};
                for (int i = 0; i < 4 && i < 8; i++) {
                    snprintf(hex_str + (i * 3), sizeof(hex_str) - (i * 3), "%02X ", bytes[i]);
                }
                ESP_LOGD(TAG, "[C] RX Data: %s", hex_str);
            }
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "[C] Modbus TX attempt %u failed: %s (handler=%p)", 
                     attempt + 1, esp_err_to_name(last_err), s_ctx.handler);
            
            // Se INVALID_STATE, forza deallocazione e reinizializza per il prossimo tentativo
            if (last_err == ESP_ERR_INVALID_STATE && attempt < retries) {
                ESP_LOGW(TAG, "[C] INVALID_STATE rilevato, dealloco e reinizializzo per tentativo %u", attempt + 2);
                
                // Forza deallocazione dell'handler esistente
                if (s_ctx.handler) {
                    ESP_LOGD(TAG, "[C] Deallocazione handler esistente: %p", s_ctx.handler);
                    esp_err_t destroy_err = mbc_master_delete(s_ctx.handler);
                    if (destroy_err != ESP_OK) {
                        ESP_LOGW(TAG, "[C] mbc_master_delete fallita: %s", esp_err_to_name(destroy_err));
                    }
                    s_ctx.handler = NULL;
                }
                
                // Resetta stato
                s_ctx.running = false;
                s_ctx.initialized = false;
                s_ctx.status.running = false;
                s_ctx.status.initialized = false;
                
                // Reinizializza
                esp_err_t reinit_err = start_locked();
                if (reinit_err != ESP_OK) {
                    ESP_LOGE(TAG, "[C] Reinizializzazione fallita durante retry: %s", esp_err_to_name(reinit_err));
                    break;
                }
                ESP_LOGI(TAG, "[C] Reinizializzazione completata, nuovo handler: %p", s_ctx.handler);
            }
        }
        
        if (attempt < retries) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    ESP_LOGE(TAG, "[C] Modbus failed after %u attempts, last error: %s", 
             retries + 1, esp_err_to_name(last_err));
    return last_err;
}

#else

static esp_err_t start_locked(void)
{
    if (s_ctx.running) {
        return ESP_OK;
    }

    device_config_t *cfg = device_config_get();
    if (cfg) {
        status_set_comm(cfg->modbus.slave_id,
                        cfg->modbus.relay_start,
                        cfg->modbus.relay_count,
                        cfg->modbus.input_start,
                        cfg->modbus.input_count);
    }

    memset(s_ctx.mock_relay_bits, 0, sizeof(s_ctx.mock_relay_bits));
    memset(s_ctx.mock_input_bits, 0, sizeof(s_ctx.mock_input_bits));

    s_ctx.initialized = true;
    s_ctx.running = true;
    s_ctx.status.initialized = true;
    s_ctx.status.running = true;
    s_ctx.status.last_error = ESP_OK;
    s_ctx.status.last_update_ms = now_ms();

    ESP_LOGI(TAG, "[C] [MOCK] Modbus relay avviato (DNA_RS485=1)");
    return ESP_OK;
}

#endif

esp_err_t modbus_relay_init(void)
{
    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    err = start_locked();

    give_lock();
    return err;
}

esp_err_t modbus_relay_deinit(void)
{
    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

#if DNA_RS485 == 0
    if (s_ctx.running || s_ctx.initialized) {
        esp_err_t destroy_err = mbc_master_delete(s_ctx.handler);
        if (destroy_err != ESP_OK && destroy_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "[C] mbc_master_delete: %s", esp_err_to_name(destroy_err));
        }
    }
#endif

    s_ctx.handler = NULL;
    s_ctx.initialized = false;
    s_ctx.running = false;
    s_ctx.status.initialized = false;
    s_ctx.status.running = false;
    s_ctx.status.last_error = ESP_OK;
    s_ctx.status.last_update_ms = now_ms();

    give_lock();
    return ESP_OK;
}

bool modbus_relay_is_running(void)
{
    return s_ctx.running;
}

esp_err_t modbus_relay_read_coils(uint8_t slave_id,
                                  uint16_t start,
                                  uint16_t count,
                                  uint8_t *out_bits,
                                  size_t out_bits_size)
{
    if (!out_bits) {
        return ESP_ERR_INVALID_ARG;
    }

    count = sanitize_count(count);
    size_t required_bytes = bits_to_bytes(count);
    if (out_bits_size < required_bytes) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(out_bits, 0, out_bits_size);

    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    err = start_locked();
    if (err != ESP_OK) {
        give_lock();
        return err;
    }

#if DNA_RS485 == 0
    mb_param_request_t req = {
        .slave_addr = clamp_slave_id(slave_id),
        .command = MB_FUNC_READ_COILS,
        .reg_start = start,
        .reg_size = count,
    };
    err = send_request_with_retry_locked(&req, out_bits);
#else
    memcpy(out_bits, s_ctx.mock_relay_bits, required_bytes);
    err = ESP_OK;
#endif

    s_ctx.status.last_error = (int32_t)err;
    if (err == ESP_OK) {
        s_ctx.status.last_update_ms = now_ms();
    }

    give_lock();
    return err;
}

esp_err_t modbus_relay_read_discrete_inputs(uint8_t slave_id,
                                            uint16_t start,
                                            uint16_t count,
                                            uint8_t *out_bits,
                                            size_t out_bits_size)
{
    if (!out_bits) {
        return ESP_ERR_INVALID_ARG;
    }

    count = sanitize_count(count);
    size_t required_bytes = bits_to_bytes(count);
    if (out_bits_size < required_bytes) {
        return ESP_ERR_INVALID_SIZE;
    }

    memset(out_bits, 0, out_bits_size);

    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    err = start_locked();
    if (err != ESP_OK) {
        give_lock();
        return err;
    }

#if DNA_RS485 == 0
    mb_param_request_t req = {
        .slave_addr = clamp_slave_id(slave_id),
        .command = MB_FUNC_READ_DISCRETE_INPUTS,
        .reg_start = start,
        .reg_size = count,
    };
    err = send_request_with_retry_locked(&req, out_bits);
#else
    memcpy(out_bits, s_ctx.mock_input_bits, required_bytes);
    err = ESP_OK;
#endif

    s_ctx.status.last_error = (int32_t)err;
    if (err == ESP_OK) {
        s_ctx.status.last_update_ms = now_ms();
    }

    give_lock();
    return err;
}

esp_err_t modbus_relay_write_single_coil(uint8_t slave_id,
                                         uint16_t coil_addr,
                                         bool on)
{
    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    err = start_locked();
    if (err != ESP_OK) {
        give_lock();
        return err;
    }

#if DNA_RS485 == 0
    uint16_t coil_value = on ? 0xFF00U : 0x0000U;
    mb_param_request_t req = {
        .slave_addr = clamp_slave_id(slave_id),
        .command = MB_FUNC_WRITE_SINGLE_COIL,
        .reg_start = coil_addr,
        .reg_size = 1,
    };
    err = send_request_with_retry_locked(&req, &coil_value);
#else
    size_t bit_index = coil_addr % MODBUS_RELAY_MAX_POINTS;
    uint8_t *byte_ptr = &s_ctx.mock_relay_bits[bit_index / 8U];
    uint8_t mask = (uint8_t)(1U << (bit_index % 8U));
    if (on) {
        *byte_ptr |= mask;
    } else {
        *byte_ptr &= (uint8_t)(~mask);
    }
    err = ESP_OK;
#endif

    s_ctx.status.last_error = (int32_t)err;
    if (err == ESP_OK) {
        s_ctx.status.last_update_ms = now_ms();
    }

    give_lock();
    return err;
}

esp_err_t modbus_relay_write_multiple_coils(uint8_t slave_id,
                                            uint16_t start,
                                            const uint8_t *packed_bits,
                                            uint16_t count)
{
    if (!packed_bits) {
        return ESP_ERR_INVALID_ARG;
    }

    count = sanitize_count(count);

    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    err = start_locked();
    if (err != ESP_OK) {
        give_lock();
        return err;
    }

#if DNA_RS485 == 0
    mb_param_request_t req = {
        .slave_addr = clamp_slave_id(slave_id),
        .command = MB_FUNC_WRITE_MULTIPLE_COILS,
        .reg_start = start,
        .reg_size = count,
    };
    err = send_request_with_retry_locked(&req, (void *)packed_bits);
#else
    size_t copy_bytes = bits_to_bytes(count);
    if (copy_bytes > sizeof(s_ctx.mock_relay_bits)) {
        copy_bytes = sizeof(s_ctx.mock_relay_bits);
    }
    memcpy(s_ctx.mock_relay_bits, packed_bits, copy_bytes);
    err = ESP_OK;
#endif

    s_ctx.status.last_error = (int32_t)err;
    if (err == ESP_OK) {
        s_ctx.status.last_update_ms = now_ms();
    }

    give_lock();
    return err;
}

esp_err_t modbus_relay_poll_once(void)
{
    device_config_t *cfg = device_config_get();
    if (!cfg) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t slave_id = clamp_slave_id(cfg->modbus.slave_id);
    uint16_t relay_count = sanitize_count(cfg->modbus.relay_count);
    uint16_t input_count = sanitize_count(cfg->modbus.input_count);

    uint8_t relay_bits[MODBUS_RELAY_MAX_BYTES] = {0};
    uint8_t input_bits[MODBUS_RELAY_MAX_BYTES] = {0};

    esp_err_t err_di = modbus_relay_read_discrete_inputs(slave_id,
                                                         cfg->modbus.input_start,
                                                         input_count,
                                                         input_bits,
                                                         sizeof(input_bits));
    esp_err_t err_co = modbus_relay_read_coils(slave_id,
                                               cfg->modbus.relay_start,
                                               relay_count,
                                               relay_bits,
                                               sizeof(relay_bits));

#if DNA_RS485 == 1
    if (err_di == ESP_OK) {
        input_bits[0] ^= 0x01;
        memcpy(s_ctx.mock_input_bits, input_bits, sizeof(s_ctx.mock_input_bits));
    }
#endif

    esp_err_t lock_err = take_lock();
    if (lock_err == ESP_OK) {
        status_set_comm(slave_id,
                        cfg->modbus.relay_start,
                        relay_count,
                        cfg->modbus.input_start,
                        input_count);

        size_t relay_bytes = bits_to_bytes(relay_count);
        size_t input_bytes = bits_to_bytes(input_count);
        if (relay_bytes > sizeof(s_ctx.status.relay_bits)) {
            relay_bytes = sizeof(s_ctx.status.relay_bits);
        }
        if (input_bytes > sizeof(s_ctx.status.input_bits)) {
            input_bytes = sizeof(s_ctx.status.input_bits);
        }

        memset(s_ctx.status.relay_bits, 0, sizeof(s_ctx.status.relay_bits));
        memset(s_ctx.status.input_bits, 0, sizeof(s_ctx.status.input_bits));

        if (err_co == ESP_OK) {
            memcpy(s_ctx.status.relay_bits, relay_bits, relay_bytes);
        }
        if (err_di == ESP_OK) {
            memcpy(s_ctx.status.input_bits, input_bits, input_bytes);
        }

        if (err_co == ESP_OK && err_di == ESP_OK) {
            s_ctx.status.poll_ok_count++;
            s_ctx.status.last_error = ESP_OK;
            s_ctx.status.last_update_ms = now_ms();
        } else {
            s_ctx.status.poll_err_count++;
            s_ctx.status.last_error = (int32_t)((err_di != ESP_OK) ? err_di : err_co);
        }

        give_lock();
    }

    if (err_di != ESP_OK) {
        return err_di;
    }
    return err_co;
}

esp_err_t modbus_relay_get_status(modbus_relay_status_t *out_status)
{
    if (!out_status) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    *out_status = s_ctx.status;

    give_lock();
    return ESP_OK;
}
