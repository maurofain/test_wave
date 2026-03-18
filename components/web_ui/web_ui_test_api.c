#include "web_ui.h"
#include "web_ui_internal.h"
#include "init.h"
#include "tasks.h"
#include "esp_log.h"
#include "led.h"

/**
 * @file web_ui_test_api.c
 * @brief Implementazione delle API per eseguire test hardware via HTTP
 *
 * Contiene il dispatcher principale per gli endpoint sotto il prefisso
 * `/api/test/` e le chiamate ai vari
 * sottosistemi di test (LED, seriali, scanner, EEPROM, MDB, ecc.).
 */
#include "cJSON.h"
#include "device_config.h"
#include "bsp/display.h"
#include <string.h>
#include <stdlib.h>

#include "usb_cdc_scanner.h"
#include "led_test.h"
#include "pwm_test.h"
#include "io_expander.h"
#include "io_expander_test.h"
#include "rs232.h"
#include "rs485.h"
#include "cctalk.h"
#include "eeprom_test.h"
#include "mdb_test.h"
#include "mdb.h"
#include "serial_test.h"
#include "sd_card.h"
#include "aux_gpio.h"
#include "sht40.h"
#include "modbus_relay.h"
#include "digital_io.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ctype.h>

/* needed for publishing FSM actions when scanner commands are triggered */
#include "fsm.h"

#define TAG "WEB_UI_TEST_API"
#define CCTALK_WEB_CMD_TIMEOUT_MS (700U)

static volatile bool s_sd_init_in_progress = false;
static TaskHandle_t s_cctalk_test_handle = NULL;
static bool s_cctalk_test_owned = false;

static uint16_t clamp_u16_value(int value, uint16_t min, uint16_t max)
{
    if (value < (int)min) {
        return min;
    }
    if (value > (int)max) {
        return max;
    }
    return (uint16_t)value;
}

static uint8_t clamp_u8_value(int value, uint8_t min, uint8_t max)
{
    if (value < (int)min) {
        return min;
    }
    if (value > (int)max) {
        return max;
    }
    return (uint8_t)value;
}


/**
 * @brief Restituisce l'indirizzo CCtalk configurato (default 2).
 *
 * @return uint8_t Indirizzo gettoniera valido (1..255).
 */
static uint8_t get_cctalk_address_configured(void)
{
    uint8_t addr = CCTALK_DEFAULT_DEVICE_ADDR;
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->cctalk.address >= 1U) {
        addr = cfg->cctalk.address;
    }
    return clamp_u8_value(addr, 1U, 255U);
}


/**
 * @brief Salva in configurazione l'indirizzo CCtalk.
 *
 * @param [in] addr Nuovo indirizzo gettoniera.
 * @return esp_err_t ESP_OK su successo.
 */
static esp_err_t set_cctalk_address_configured(uint8_t addr)
{
    device_config_t *cfg = device_config_get();
    if (!cfg) {
        return ESP_ERR_INVALID_STATE;
    }

    cfg->cctalk.address = clamp_u8_value(addr, 1U, 255U);
    cfg->updated = true;
    return device_config_save(cfg);
}


/**
 * @brief Estrae l'indirizzo CCtalk dal payload JSON della richiesta.
 *
 * Formato atteso: {"addr":<1..255>}
 *
 * @param [in] req Richiesta HTTP con payload JSON.
 * @param [out] out_addr Indirizzo estratto.
 * @return esp_err_t ESP_OK su successo.
 */
static esp_err_t parse_cctalk_addr_from_request(httpd_req_t *req, uint8_t *out_addr)
{
    if (!req || !out_addr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (req->content_len <= 0) {
        return ESP_ERR_NOT_FOUND;
    }

    char buf[128] = {0};
    if (req->content_len >= (int)sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }

    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *addr_obj = cJSON_GetObjectItem(root, "addr");
    if (!addr_obj || !cJSON_IsNumber(addr_obj)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    *out_addr = clamp_u8_value(addr_obj->valueint, 1U, 255U);
    cJSON_Delete(root);
    return ESP_OK;
}


/**
 * @brief Analizza un payload in formato esadecimale.
 *
 * Questa funzione prende in input una stringa in formato esadecimale e la converte in un array di byte.
 *
 * @param [in] input Puntatore alla stringa in formato esadecimale da analizzare.
 * @param [out] out Puntatore all'array di byte dove verrà memorizzato il risultato.
 * @param [in] out_max Dimensione massima dell'array di output.
 * @param [out] out_len Puntatore alla variabile dove verrà memorizzata la lunghezza dell'array di output.
 * @return esp_err_t Codice di errore che indica il successo o la causa dell'errore.
 */
static esp_err_t parse_hex_payload(const char *input, uint8_t *out, size_t out_max, size_t *out_len)
{
    if (!input || !out || !out_len) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = 0;
    const char *cursor = input;

    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (isspace((unsigned char)*cursor) || *cursor == ',' || *cursor == ';')) {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        char token[16] = {0};
        size_t token_len = 0;
        while (*cursor != '\0' &&
               !isspace((unsigned char)*cursor) &&
               *cursor != ',' &&
               *cursor != ';') {
            if (token_len < sizeof(token) - 1) {
                token[token_len++] = *cursor;
            }
            cursor++;
        }
        token[token_len] = '\0';

        const char *hex_ptr = token;
        if (hex_ptr[0] == '\\') {
            hex_ptr++;
        }
        if (hex_ptr[0] == '0' && (hex_ptr[1] == 'x' || hex_ptr[1] == 'X')) {
            hex_ptr += 2;
        }
        if (*hex_ptr == '\0') {
            return ESP_ERR_INVALID_ARG;
        }

        char *endptr = NULL;
        long value = strtol(hex_ptr, &endptr, 16);
        if (*endptr != '\0' || value < 0 || value > 255) {
            return ESP_ERR_INVALID_ARG;
        }
        if (len >= out_max) {
            return ESP_ERR_INVALID_SIZE;
        }

        out[len++] = (uint8_t)value;
    }

    if (len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_len = len;
    return ESP_OK;
}


/**
 * @brief Inizializza la task del worker.
 *
 * Questa funzione inizializza la task del worker, preparandola per l'esecuzione.
 *
 * @param pv Puntatore a dati di input necessari per l'inizializzazione.
 * @return Nessun valore di ritorno.
 */
static void sd_init_worker_task(void *pv)
{
    (void)pv;
    ESP_LOGI(TAG, "[C] SD init worker avviato");

    if (sd_card_is_mounted()) {
        (void)sd_card_unmount();
    }

    esp_err_t err = sd_card_mount();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[C] SD init worker completato con successo");
    } else {
        ESP_LOGW(TAG, "[C] SD init worker fallito: %s", esp_err_to_name(err));
    }

    s_sd_init_in_progress = false;
    vTaskDelete(NULL);
}

/**
 * @brief Dispatcher API per i comandi di test hardware sotto il prefisso `/api/test/`.
 *
 * Analizza il suffisso URI, invoca il modulo test corrispondente (LED, seriali,
 * EEPROM, MDB, SD, scanner, GPIO, sensori) e restituisce una risposta JSON.
 *
 * @param req Richiesta HTTP POST.
 * @return ESP_OK dopo l'invio della risposta HTTP.
 */
esp_err_t api_test_handler(httpd_req_t *req)
{
    extern bool enable_api_log;
    if (enable_api_log) ESP_LOGI(TAG, "[C] POST %s", req->uri);
    const char *test_name = req->uri + strlen("/api/test/");
    char response[256] = {0};

    if (strcmp(test_name, "led_start") == 0) {
        if (led_test_start() == ESP_OK) {
            snprintf(response, sizeof(response), "{\"message\":\"Test LED avviato\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "led_stop") == 0) {
        led_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test LED fermato\"}");
    }

    else if (strcmp(test_name, "scanner_setup") == 0) {
        esp_err_t err = usb_cdc_scanner_send_setup_command();
        if (err == ESP_OK) {
            snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Scanner setup inviato\"}");
        } else {
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Setup scanner fallito\",\"err\":\"%s\"}", esp_err_to_name(err));
        }
    }
    else if (strcmp(test_name, "scanner_state") == 0) {
        esp_err_t err = usb_cdc_scanner_send_state_command();
        if (err == ESP_OK) {
            snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Richiesta stato scanner inviata\"}");
        } else {
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Richiesta stato scanner fallita\",\"err\":\"%s\"}", esp_err_to_name(err));
        }
    }
    else if (strcmp(test_name, "scanner_on") == 0) {
        if (enable_api_log) ESP_LOGI(TAG, "[C] POST /api/test/scanner_on");
        web_ui_add_log("INFO", "SCANNER", "API scanner_on called");
        esp_err_t err = usb_cdc_scanner_send_on_command();
        if (err == ESP_OK) {
            /* publish corresponding FSM action so other agents can observe it */
            fsm_input_event_t ev = {
                .from = AGN_ID_WEB_UI,
                .to = {AGN_ID_FSM},
                .action = ACTION_ID_USB_CDC_SCANNER_ON,
                .type = FSM_INPUT_EVENT_NONE,
                .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
                .value_i32 = 0,
                .value_u32 = 0,
                .aux_u32 = 0,
                .text = {0},
            };
            (void)fsm_event_publish(&ev, pdMS_TO_TICKS(20));

            ESP_LOGI(TAG, "Scanner ON command dispatched");
            snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Scanner ON inviato\"}");
        } else {
            ESP_LOGW(TAG, "Scanner ON command failed: %s", esp_err_to_name(err));
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Comando Scanner ON fallito\",\"err\":\"%s\"}", esp_err_to_name(err));
        }
    }
    else if (strcmp(test_name, "scanner_off") == 0) {
        if (enable_api_log) ESP_LOGI(TAG, "[C] POST /api/test/scanner_off");
        web_ui_add_log("INFO", "SCANNER", "API scanner_off called");
        esp_err_t err = usb_cdc_scanner_send_off_command();
        if (err == ESP_OK) {
            /* also post an FSM action event */
            fsm_input_event_t ev = {
                .from = AGN_ID_WEB_UI,
                .to = {AGN_ID_FSM},
                .action = ACTION_ID_USB_CDC_SCANNER_OFF,
                .type = FSM_INPUT_EVENT_NONE,
                .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
                .value_i32 = 0,
                .value_u32 = 0,
                .aux_u32 = 0,
                .text = {0},
            };
            (void)fsm_event_publish(&ev, pdMS_TO_TICKS(20));

            ESP_LOGI(TAG, "Scanner OFF command dispatched");
            snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Scanner OFF inviato\"}");
        } else {
            ESP_LOGW(TAG, "Scanner OFF command failed: %s", esp_err_to_name(err));
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Comando Scanner OFF fallito\",\"err\":\"%s\"}", esp_err_to_name(err));
        }
    }

    else if (strcmp(test_name, "led_bright") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *br_obj = cJSON_GetObjectItem(root, "bright");
            if (br_obj) {
                led_test_set_brightness(br_obj->valueint);
                snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
            }
            cJSON_Delete(root);
        }
    }

    else if (strcmp(test_name, "display_bright") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *br_obj = cJSON_GetObjectItem(root, "bright");
            if (br_obj) {
                device_config_t *cfg = device_config_get();
                if (!cfg || !cfg->display.enabled) {
                    snprintf(response, sizeof(response), "{\"status\":\"ignored\",\"message\":\"Display disabilitato\"}");
                    cJSON_Delete(root);
                    root = NULL;
                } else {
                    int bright = br_obj->valueint;
                    if (bright < 0) bright = 0;
                    if (bright > 100) bright = 100;
                    bsp_display_brightness_set(bright);
                    if (cfg) cfg->display.lcd_brightness = (uint8_t)bright;
                    snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
                }
            }
            if (root) {
                cJSON_Delete(root);
            }
        }
    }

    else if (strcmp(test_name, "led_set") == 0) {
        char buf[128] = {0};
        int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
        if (ret > 0) {
            cJSON *root = cJSON_Parse(buf);
            if (root) {
                cJSON *r_obj = cJSON_GetObjectItem(root, "r");
                cJSON *g_obj = cJSON_GetObjectItem(root, "g");
                cJSON *b_obj = cJSON_GetObjectItem(root, "b");
                cJSON *br_obj = cJSON_GetObjectItem(root, "bright");
                if (r_obj && g_obj && b_obj && br_obj) {
                    led_test_set_color(r_obj->valueint, g_obj->valueint, b_obj->valueint, br_obj->valueint);
                    snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
                }
                cJSON_Delete(root);
            }
        }
    }

    else if (strcmp(test_name, "pwm1_start") == 0) {
        pwm_test_start(1);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM1 avviato\"}");
    } else if (strcmp(test_name, "pwm1_stop") == 0) {
        pwm_test_stop(1);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM1 fermato\"}");
    }

    else if (strcmp(test_name, "pwm2_start") == 0) {
        pwm_test_start(2);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM2 avviato\"}");
    } else if (strcmp(test_name, "pwm2_stop") == 0) {
        pwm_test_stop(2);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM2 fermato\"}");
    }

    else if (strcmp(test_name, "pwm_set") == 0) {
        char buf[128] = {0};
        int ret = httpd_req_recv(req, buf, sizeof(buf)-1);
        if (ret > 0) {
            cJSON *root = cJSON_Parse(buf);
            if (root) {
                cJSON *ch_obj = cJSON_GetObjectItem(root, "ch");
                cJSON *f_obj = cJSON_GetObjectItem(root, "freq");
                cJSON *d_obj = cJSON_GetObjectItem(root, "duty");
                if (ch_obj && f_obj && d_obj) {
                    pwm_test_set_param(ch_obj->valueint, f_obj->valueint, d_obj->valueint);
                    snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
                }
                cJSON_Delete(root);
            }
        }
    }

    else if (strcmp(test_name, "ioexp_start") == 0) {
        io_expander_test_start();
        snprintf(response, sizeof(response), "{\"message\":\"Test I/O Expander avviato (1Hz)\"}");
    } else if (strcmp(test_name, "ioexp_stop") == 0) {
        io_expander_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test I/O Expander fermato\"}");
    }

    else if (strcmp(test_name, "io_set") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *pin_obj = cJSON_GetObjectItem(root, "pin");
            cJSON *val_obj = cJSON_GetObjectItem(root, "val");
            if (pin_obj && val_obj) {
                io_set_pin(pin_obj->valueint, val_obj->valueint);
                snprintf(response, sizeof(response), "{\"status\":\"ok\",\"output\":%d}", io_output_state);
            }
            cJSON_Delete(root);
        }
    }
    else if (strcmp(test_name, "io_get") == 0) {
        uint8_t in = io_get();
        snprintf(response, sizeof(response), "{\"input\":%d,\"output\":%d}", in, io_output_state);
    }

    else if (strcmp(test_name, "dio_set") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *output_obj = cJSON_GetObjectItem(root, "output");
            cJSON *val_obj = cJSON_GetObjectItem(root, "val");
            if (output_obj && val_obj && cJSON_IsNumber(output_obj) && cJSON_IsNumber(val_obj)) {
                esp_err_t err = tasks_digital_io_set_output_via_agent((uint8_t)output_obj->valueint,
                                                                       val_obj->valueint != 0,
                                                                       pdMS_TO_TICKS(250));
                if (err == ESP_OK) {
                    snprintf(response, sizeof(response),
                             "{\"status\":\"ok\",\"output\":%d,\"val\":%d}",
                             output_obj->valueint,
                             val_obj->valueint ? 1 : 0);
                } else {
                    snprintf(response, sizeof(response),
                             "{\"status\":\"error\",\"error\":\"%s\"}",
                             esp_err_to_name(err));
                }
            }
            cJSON_Delete(root);
        }
    }

    else if (strcmp(test_name, "dio_get") == 0) {
        digital_io_snapshot_t snapshot = {0};
        esp_err_t err = tasks_digital_io_get_snapshot_via_agent(&snapshot,
                                                                 pdMS_TO_TICKS(250));
        if (err == ESP_OK) {
            snprintf(response,
                     sizeof(response),
                     "{\"status\":\"ok\",\"outputs_mask\":%u,\"inputs_mask\":%u}",
                     (unsigned)snapshot.outputs_mask,
                     (unsigned)snapshot.inputs_mask);
        } else {
            snprintf(response,
                     sizeof(response),
                     "{\"status\":\"error\",\"outputs_mask\":%u,\"inputs_mask\":%u,\"error\":\"%s\"}",
                     (unsigned)snapshot.outputs_mask,
                     (unsigned)snapshot.inputs_mask,
                     esp_err_to_name(err));
        }
    }

    else if (strcmp(test_name, "rs232_start") == 0) {
        if (!s_rs232_test_handle) {
            rs232_init();
            xTaskCreate(uart_test_task, "rs232_test",
            /* original 2048 words (≈8 KB) proved insufficient: formatting/logging
             * in uart_test_task can consume a few kilobytes.  Allocate double to
             * give headroom and avoid stack‑protection panics. */
            4096,
            (void*)CONFIG_APP_RS232_UART_PORT, 5, &s_rs232_test_handle);
            snprintf(response, sizeof(response), "{\"message\":\"Test RS232 avviato (caratteri 55,AA,01,07)\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "rs232_stop") == 0) {
        if (s_rs232_test_handle) { vTaskDelete(s_rs232_test_handle); s_rs232_test_handle = NULL; }
        snprintf(response, sizeof(response), "{\"message\":\"Test RS232 fermato\"}");
    }

    else if (strcmp(test_name, "rs485_start") == 0) {
        if (modbus_relay_is_running()) {
            snprintf(response, sizeof(response), "{\"error\":\"RS485 occupata da Modbus\"}");
        } else if (!s_rs485_test_handle) {
            rs485_init();
            xTaskCreate(uart_test_task, "rs485_test",
            /* same reasoning as rs232_test above */
            4096,
            (void*)CONFIG_APP_RS485_UART_PORT, 5, &s_rs485_test_handle);
            snprintf(response, sizeof(response), "{\"message\":\"Test RS485 avviato (caratteri 55,AA,01,07)\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "rs485_stop") == 0) {
        if (s_rs485_test_handle) { vTaskDelete(s_rs485_test_handle); s_rs485_test_handle = NULL; }
        snprintf(response, sizeof(response), "{\"message\":\"Test RS485 fermato\"}");
    }

    else if (strcmp(test_name, "modbus_status") == 0 || strcmp(test_name, "modbus/status") == 0) {
        modbus_relay_status_t st = {0};
        esp_err_t err = modbus_relay_get_status(&st);
        if (err != ESP_OK) {
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Stato Modbus non disponibile\",\"err\":\"%s\"}", esp_err_to_name(err));
        } else {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "status", "ok");
            cJSON_AddBoolToObject(root, "running", st.running);
            cJSON_AddBoolToObject(root, "initialized", st.initialized);
            cJSON_AddNumberToObject(root, "slave_id", st.slave_id);
            cJSON_AddNumberToObject(root, "relay_start", st.relay_start);
            cJSON_AddNumberToObject(root, "relay_count", st.relay_count);
            cJSON_AddNumberToObject(root, "input_start", st.input_start);
            cJSON_AddNumberToObject(root, "input_count", st.input_count);
            cJSON_AddNumberToObject(root, "poll_ok", st.poll_ok_count);
            cJSON_AddNumberToObject(root, "poll_err", st.poll_err_count);
            cJSON_AddNumberToObject(root, "last_error", st.last_error);
            cJSON_AddNumberToObject(root, "last_update_ms", st.last_update_ms);

            cJSON *coils = cJSON_CreateArray();
            for (uint16_t i = 0; i < st.relay_count && i < MODBUS_RELAY_MAX_POINTS; ++i) {
                int bit = (st.relay_bits[i / 8U] >> (i % 8U)) & 0x01;
                cJSON_AddItemToArray(coils, cJSON_CreateNumber(bit));
            }
            cJSON_AddItemToObject(root, "coils", coils);

            cJSON *inputs = cJSON_CreateArray();
            for (uint16_t i = 0; i < st.input_count && i < MODBUS_RELAY_MAX_POINTS; ++i) {
                int bit = (st.input_bits[i / 8U] >> (i % 8U)) & 0x01;
                cJSON_AddItemToArray(inputs, cJSON_CreateNumber(bit));
            }
            cJSON_AddItemToObject(root, "inputs", inputs);

            char *json_out = cJSON_PrintUnformatted(root);
            if (!json_out) {
                cJSON_Delete(root);
                snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"JSON build failed\"}");
            } else {
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, json_out, strlen(json_out));
                free(json_out);
                cJSON_Delete(root);
                return ESP_OK;
            }
        }
    }

    else if (strcmp(test_name, "modbus_read_di") == 0 || strcmp(test_name, "modbus/read/di") == 0 ||
             strcmp(test_name, "modbus_read_coils") == 0 || strcmp(test_name, "modbus/read/coils") == 0) {
        if (s_rs485_test_handle) {
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Test RS485 attivo: fermarlo prima\"}");
        } else {
            const bool read_coils = (strcmp(test_name, "modbus_read_coils") == 0 || strcmp(test_name, "modbus/read/coils") == 0);
            device_config_t *cfg = device_config_get();
            uint8_t slave_id = (cfg ? cfg->modbus.slave_id : 1);
            uint16_t start = (cfg ? cfg->modbus.input_start : 0);
            uint16_t count = (cfg ? cfg->modbus.input_count : 8);

            if (read_coils) {
                start = (cfg ? cfg->modbus.relay_start : 0);
                count = (cfg ? cfg->modbus.relay_count : 8);
            }

            if (count < 1U) count = 1U;
            if (count > MODBUS_RELAY_MAX_POINTS) count = MODBUS_RELAY_MAX_POINTS;

            cJSON *root = NULL;
            char buf[256] = {0};
            if (req->content_len > 0) {
                if (req->content_len >= (int)sizeof(buf)) {
                    snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Payload troppo grande\"}");
                    goto modbus_read_end;
                }
                int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
                if (ret <= 0) {
                    snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Lettura payload fallita\"}");
                    goto modbus_read_end;
                }
                root = cJSON_Parse(buf);
                if (!root) {
                    snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"JSON non valido\"}");
                    goto modbus_read_end;
                }

                cJSON *slave_obj = cJSON_GetObjectItem(root, "slave_id");
                if (slave_obj && cJSON_IsNumber(slave_obj)) {
                    slave_id = clamp_u8_value(slave_obj->valueint, 1, 255);
                }

                cJSON *start_obj = cJSON_GetObjectItem(root, "start");
                if (start_obj && cJSON_IsNumber(start_obj)) {
                    start = clamp_u16_value(start_obj->valueint, 0, 65535);
                }

                cJSON *count_obj = cJSON_GetObjectItem(root, "count");
                if (count_obj && cJSON_IsNumber(count_obj)) {
                    count = clamp_u16_value(count_obj->valueint, 1, MODBUS_RELAY_MAX_POINTS);
                }
            }

            {
                uint8_t bits[MODBUS_RELAY_MAX_BYTES] = {0};
                esp_err_t init_err = modbus_relay_init();
                if (init_err != ESP_OK) {
                    snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Init Modbus fallita\",\"err\":\"%s\"}", esp_err_to_name(init_err));
                } else {
                    esp_err_t read_err;
                    if (read_coils) {
                        read_err = modbus_relay_read_coils(slave_id, start, count, bits, sizeof(bits));
                    } else {
                        read_err = modbus_relay_read_discrete_inputs(slave_id, start, count, bits, sizeof(bits));
                    }

                    if (read_err != ESP_OK) {
                        snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Lettura Modbus fallita\",\"err\":\"%s\"}", esp_err_to_name(read_err));
                    } else {
                        cJSON *out = cJSON_CreateObject();
                        cJSON_AddStringToObject(out, "status", "ok");
                        cJSON_AddStringToObject(out,
                                                "type",
                                                read_coils ? "coils" : "discrete_inputs");
                        cJSON_AddNumberToObject(out, "slave_id", slave_id);
                        cJSON_AddNumberToObject(out, "start", start);
                        cJSON_AddNumberToObject(out, "count", count);

                        cJSON *values = cJSON_CreateArray();
                        for (uint16_t i = 0; i < count; ++i) {
                            int bit = (bits[i / 8U] >> (i % 8U)) & 0x01;
                            cJSON_AddItemToArray(values, cJSON_CreateNumber(bit));
                        }
                        cJSON_AddItemToObject(out, "values", values);

                        char *json_out = cJSON_PrintUnformatted(out);
                        if (!json_out) {
                            cJSON_Delete(out);
                            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"JSON build failed\"}");
                        } else {
                            httpd_resp_set_type(req, "application/json");
                            httpd_resp_send(req, json_out, strlen(json_out));
                            free(json_out);
                            cJSON_Delete(out);
                            if (root) cJSON_Delete(root);
                            return ESP_OK;
                        }
                    }
                }
            }

modbus_read_end:
            if (root) {
                cJSON_Delete(root);
            }
        }
    }

    else if (strcmp(test_name, "modbus_write_coil") == 0 || strcmp(test_name, "modbus/write/coil") == 0) {
        if (s_rs485_test_handle) {
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Test RS485 attivo: fermarlo prima\"}");
        } else {
            char buf[256] = {0};
            int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
            if (ret <= 0) {
                snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Payload mancante\"}");
            } else {
                cJSON *root = cJSON_Parse(buf);
                if (!root) {
                    snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"JSON non valido\"}");
                } else {
                    device_config_t *cfg = device_config_get();
                    uint8_t slave_id = (cfg ? cfg->modbus.slave_id : 1);
                    uint16_t coil = (cfg ? cfg->modbus.relay_start : 0);
                    bool state = false;

                    cJSON *slave_obj = cJSON_GetObjectItem(root, "slave_id");
                    if (slave_obj && cJSON_IsNumber(slave_obj)) {
                        slave_id = clamp_u8_value(slave_obj->valueint, 1, 255);
                    }

                    cJSON *coil_obj = cJSON_GetObjectItem(root, "coil");
                    if (coil_obj && cJSON_IsNumber(coil_obj)) {
                        coil = clamp_u16_value(coil_obj->valueint, 0, 65535);
                    }

                    cJSON *state_obj = cJSON_GetObjectItem(root, "state");
                    if (!state_obj) {
                        state_obj = cJSON_GetObjectItem(root, "value");
                    }
                    if (state_obj) {
                        if (cJSON_IsBool(state_obj)) {
                            state = cJSON_IsTrue(state_obj);
                        } else if (cJSON_IsNumber(state_obj)) {
                            state = (state_obj->valueint != 0);
                        }
                    }

                    esp_err_t init_err = modbus_relay_init();
                    if (init_err != ESP_OK) {
                        snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Init Modbus fallita\",\"err\":\"%s\"}", esp_err_to_name(init_err));
                    } else {
                        esp_err_t wr_err = modbus_relay_write_single_coil(slave_id, coil, state);
                        if (wr_err != ESP_OK) {
                            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Scrittura coil fallita\",\"err\":\"%s\"}", esp_err_to_name(wr_err));
                        } else {
                            snprintf(response,
                                     sizeof(response),
                                     "{\"status\":\"ok\",\"message\":\"Coil aggiornata\",\"slave_id\":%u,\"coil\":%u,\"state\":%d}",
                                     (unsigned)slave_id,
                                     (unsigned)coil,
                                     state ? 1 : 0);
                        }
                    }
                    cJSON_Delete(root);
                }
            }
        }
    }

    else if (strcmp(test_name, "modbus_write_coils") == 0 || strcmp(test_name, "modbus/write/coils") == 0) {
        if (s_rs485_test_handle) {
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Test RS485 attivo: fermarlo prima\"}");
        } else {
            char buf[512] = {0};
            int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
            if (ret <= 0) {
                snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Payload mancante\"}");
            } else {
                cJSON *root = cJSON_Parse(buf);
                if (!root) {
                    snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"JSON non valido\"}");
                } else {
                    device_config_t *cfg = device_config_get();
                    uint8_t slave_id = (cfg ? cfg->modbus.slave_id : 1);
                    uint16_t start = (cfg ? cfg->modbus.relay_start : 0);

                    cJSON *slave_obj = cJSON_GetObjectItem(root, "slave_id");
                    if (slave_obj && cJSON_IsNumber(slave_obj)) {
                        slave_id = clamp_u8_value(slave_obj->valueint, 1, 255);
                    }

                    cJSON *start_obj = cJSON_GetObjectItem(root, "start");
                    if (start_obj && cJSON_IsNumber(start_obj)) {
                        start = clamp_u16_value(start_obj->valueint, 0, 65535);
                    }

                    cJSON *states = cJSON_GetObjectItem(root, "states");
                    if (!states) {
                        states = cJSON_GetObjectItem(root, "coils");
                    }
                    if (!states || !cJSON_IsArray(states)) {
                        snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Campo 'states/coils' non valido\"}");
                    } else {
                        uint8_t packed[MODBUS_RELAY_MAX_BYTES] = {0};
                        uint16_t count = 0;
                        cJSON *item = NULL;
                        cJSON_ArrayForEach(item, states) {
                            if (count >= MODBUS_RELAY_MAX_POINTS) {
                                break;
                            }

                            bool on = false;
                            if (cJSON_IsBool(item)) {
                                on = cJSON_IsTrue(item);
                            } else if (cJSON_IsNumber(item)) {
                                on = (item->valueint != 0);
                            }

                            if (on) {
                                packed[count / 8U] |= (uint8_t)(1U << (count % 8U));
                            }
                            count++;
                        }

                        if (count == 0U) {
                            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Array 'states' vuoto\"}");
                        } else {
                            esp_err_t init_err = modbus_relay_init();
                            if (init_err != ESP_OK) {
                                snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Init Modbus fallita\",\"err\":\"%s\"}", esp_err_to_name(init_err));
                            } else {
                                esp_err_t wr_err = modbus_relay_write_multiple_coils(slave_id, start, packed, count);
                                if (wr_err != ESP_OK) {
                                    snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Scrittura multipla coil fallita\",\"err\":\"%s\"}", esp_err_to_name(wr_err));
                                } else {
                                    snprintf(response,
                                             sizeof(response),
                                             "{\"status\":\"ok\",\"message\":\"Coils aggiornate\",\"slave_id\":%u,\"start\":%u,\"count\":%u}",
                                             (unsigned)slave_id,
                                             (unsigned)start,
                                             (unsigned)count);
                                }
                            }
                        }
                    }
                    cJSON_Delete(root);
                }
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* modbus/scan — discover which slave IDs respond on the RS485 bus      */
    /* POST body (optional): {"id_min":1,"id_max":16}                       */
    /* ------------------------------------------------------------------ */
    else if (strcmp(test_name, "modbus/scan") == 0) {
        ESP_LOGI(TAG, "[C] POST /api/test/modbus/scan");
        uint8_t id_min = 1;
        uint8_t id_max = 16;

        char body[128] = {0};
        int body_len = httpd_req_recv(req, body, sizeof(body) - 1);
        if (body_len > 0) {
            body[body_len] = '\0';
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *jmin = cJSON_GetObjectItem(root, "id_min");
                cJSON *jmax = cJSON_GetObjectItem(root, "id_max");
                if (cJSON_IsNumber(jmin) && jmin->valueint >= 1) {
                    id_min = (uint8_t)jmin->valueint;
                }
                if (cJSON_IsNumber(jmax) && jmax->valueint <= 247) {
                    id_max = (uint8_t)jmax->valueint;
                }
                cJSON_Delete(root);
            }
        }
        if (id_max < id_min) { id_max = id_min; }

        ESP_LOGI(TAG, "[C] Scansione Modbus slave ID da %u a %u", (unsigned)id_min, (unsigned)id_max);
        esp_err_t init_err = modbus_relay_init();
        if (init_err != ESP_OK) {
            snprintf(response, sizeof(response),
                     "{\"status\":\"error\",\"error\":\"Init Modbus fallita\",\"err\":\"%s\"}",
                     esp_err_to_name(init_err));
        } else {
            device_config_t *cfg = device_config_get();
            uint16_t relay_count = cfg ? cfg->modbus.relay_count : 8;
            uint16_t input_count = cfg ? cfg->modbus.input_count : 8;
            uint16_t relay_start = cfg ? cfg->modbus.relay_start : 0;
            uint16_t input_start = cfg ? cfg->modbus.input_start : 0;

            /* Build JSON response manually (may have many slaves) */
            const size_t scan_cap = 2048;
            char *scan_out = malloc(scan_cap);
            if (!scan_out) {
                snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"OOM\"}");
            } else {
                size_t pos = 0;
                pos += snprintf(scan_out + pos, scan_cap - pos,
                                "{\"status\":\"ok\",\"id_min\":%u,\"id_max\":%u,\"found\":[",
                                (unsigned)id_min, (unsigned)id_max);

                bool first = true;

                // Scansione range richiesto (no broadcast: letture su ID 0 non valide Modbus)
                for (uint8_t sid = id_min; sid <= id_max; ++sid) {
                    ESP_LOGI(TAG, "[C] Probe slave ID %u...", (unsigned)sid);
                    uint8_t probe[1] = {0};
                    esp_err_t probe_err = modbus_relay_read_coils(sid, relay_start, 1, probe, sizeof(probe));
                    if (probe_err == ESP_OK) {
                        ESP_LOGI(TAG, "[C] Slave ID %u risponde OK", (unsigned)sid);
                        pos += snprintf(scan_out + pos, scan_cap - pos,
                                        "%s{\"slave_id\":%u,\"relay_count\":%u,\"input_count\":%u,"
                                        "\"relay_start\":%u,\"input_start\":%u}",
                                        first ? "" : ",",
                                        (unsigned)sid,
                                        (unsigned)relay_count,
                                        (unsigned)input_count,
                                        (unsigned)relay_start,
                                        (unsigned)input_start);
                        first = false;
                    }
                    if (pos >= scan_cap - 64) break; /* safety */
                }

                snprintf(scan_out + pos, scan_cap - pos, "]}");
                httpd_resp_set_type(req, "application/json");
                esp_err_t ret = httpd_resp_send(req, scan_out, strlen(scan_out));
                free(scan_out);
                return ret;
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* modbus/read_inputs — read discrete inputs                            */
    /* POST body (optional): {"slave_id":1,"input_start":0,"input_count":8} */
    /* ------------------------------------------------------------------ */
    else if (strcmp(test_name, "modbus/read_inputs") == 0) {
        ESP_LOGI(TAG, "[C] POST /api/test/modbus/read_inputs");

        device_config_t *cfg = device_config_get();
        uint8_t slave_id = cfg ? clamp_u8_value(cfg->modbus.slave_id, 1, 247) : 1;
        uint16_t input_start = cfg ? cfg->modbus.input_start : 0;
        uint16_t input_count = cfg ? cfg->modbus.input_count : 8;
        if (input_count > MODBUS_RELAY_MAX_POINTS) input_count = MODBUS_RELAY_MAX_POINTS;

        char body[128] = {0};
        int body_len = httpd_req_recv(req, body, sizeof(body) - 1);
        if (body_len > 0) {
            body[body_len] = '\0';
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *jsid = cJSON_GetObjectItem(root, "slave_id");
                cJSON *jstart = cJSON_GetObjectItem(root, "input_start");
                cJSON *jcount = cJSON_GetObjectItem(root, "input_count");

                if (cJSON_IsNumber(jsid) && jsid->valueint >= 1) {
                    slave_id = clamp_u8_value(jsid->valueint, 1, 247);
                }
                if (cJSON_IsNumber(jstart) && jstart->valueint >= 0) {
                    input_start = clamp_u16_value(jstart->valueint, 0, 65535);
                }
                if (cJSON_IsNumber(jcount) && jcount->valueint >= 1) {
                    input_count = clamp_u16_value(jcount->valueint, 1, MODBUS_RELAY_MAX_POINTS);
                }
                cJSON_Delete(root);
            }
        }

        esp_err_t init_err = modbus_relay_init();
        if (init_err != ESP_OK) {
            snprintf(response, sizeof(response),
                     "{\"status\":\"error\",\"error\":\"Init Modbus fallita\",\"err\":\"%s\"}",
                     esp_err_to_name(init_err));
        } else {
            uint8_t input_bits[MODBUS_RELAY_MAX_BYTES] = {0};
            bool used_coil_fallback = false;

            ESP_LOGI(TAG, "[C] Reading %u discrete inputs from slave %u (start=%u)", 
                     input_count, slave_id, input_start);
            
            esp_err_t err_di = modbus_relay_read_discrete_inputs(slave_id, input_start, input_count,
                                                                  input_bits, sizeof(input_bits));

            if (err_di == ESP_ERR_INVALID_RESPONSE) {
                ESP_LOGW(TAG, "[C] Read DI fallita con invalid response, provo fallback read_coils");
                esp_err_t fb_err = modbus_relay_read_coils(slave_id,
                                                           input_start,
                                                           input_count,
                                                           input_bits,
                                                           sizeof(input_bits));
                if (fb_err == ESP_OK) {
                    err_di = ESP_OK;
                    used_coil_fallback = true;
                } else {
                    ESP_LOGW(TAG, "[C] Fallback read_coils fallito: %s", esp_err_to_name(fb_err));
                }
            }

            if (err_di == ESP_OK) {
                // Converti bit array a string di 0/1 per facile visualizzazione
                char input_str[MODBUS_RELAY_MAX_POINTS + 1] = {0};
                for (uint16_t i = 0; i < input_count; i++) {
                    uint8_t byte_idx = i / 8;
                    uint8_t bit_idx = i % 8;
                    input_str[i] = (input_bits[byte_idx] & (1 << bit_idx)) ? '1' : '0';
                }
                input_str[input_count] = '\0';

                // Crea anche hexdump per debug
                char hex_str[64] = {0};
                size_t byte_count = (input_count + 7) / 8;
                for (size_t i = 0; i < byte_count && i < 8; i++) {
                    snprintf(hex_str + (i * 3), sizeof(hex_str) - (i * 3), "%02X ", input_bits[i]);
                }

                snprintf(response, sizeof(response),
                         "{\"status\":\"ok\",\"slave_id\":%u,\"input_count\":%u,\"input_start\":%u,\"source\":\"%s\","
                         "\"inputs\":\"%s\",\"hex\":\"%s\"}",
                         slave_id,
                         input_count,
                         input_start,
                         used_coil_fallback ? "coils_fallback" : "discrete_inputs",
                         input_str,
                         hex_str);
                
                ESP_LOGI(TAG, "[C] Inputs read OK (%s): %s (%s)",
                         used_coil_fallback ? "coils_fallback" : "discrete_inputs",
                         input_str,
                         hex_str);
            } else {
                snprintf(response, sizeof(response),
                         "{\"status\":\"error\",\"error\":\"Read discrete inputs fallita\",\"err\":\"%s\"}",
                         esp_err_to_name(err_di));
                ESP_LOGE(TAG, "[C] Read discrete inputs failed: %s", esp_err_to_name(err_di));
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /* modbus/poll — read coils + discrete inputs for a given slave_id      */
    /* POST body: {"slave_id":1}  (optional, uses config default)           */
    /* ------------------------------------------------------------------ */
    else if (strcmp(test_name, "modbus/poll") == 0) {
        char body[128] = {0};
        int body_len = httpd_req_recv(req, body, sizeof(body) - 1);
        device_config_t *cfg = device_config_get();

        uint8_t slave_id   = cfg ? cfg->modbus.slave_id   : 1;
        uint16_t relay_start = cfg ? cfg->modbus.relay_start : 0;
        uint16_t relay_count = cfg ? cfg->modbus.relay_count : 8;
        uint16_t input_start = cfg ? cfg->modbus.input_start : 0;
        uint16_t input_count = cfg ? cfg->modbus.input_count : 8;
        if (relay_count > MODBUS_RELAY_MAX_POINTS) relay_count = MODBUS_RELAY_MAX_POINTS;
        if (input_count > MODBUS_RELAY_MAX_POINTS) input_count = MODBUS_RELAY_MAX_POINTS;

        if (body_len > 0) {
            body[body_len] = '\0';
            cJSON *root = cJSON_Parse(body);
            if (root) {
                cJSON *jsid = cJSON_GetObjectItem(root, "slave_id");
                if (cJSON_IsNumber(jsid) && jsid->valueint >= 1) {
                    slave_id = (uint8_t)jsid->valueint;
                }
                cJSON_Delete(root);
            }
        }

        esp_err_t init_err = modbus_relay_init();
        if (init_err != ESP_OK) {
            snprintf(response, sizeof(response),
                     "{\"status\":\"error\",\"error\":\"Init Modbus fallita\",\"err\":\"%s\"}",
                     esp_err_to_name(init_err));
        } else {
            uint8_t relay_bits[MODBUS_RELAY_MAX_BYTES] = {0};
            uint8_t input_bits[MODBUS_RELAY_MAX_BYTES] = {0};

            esp_err_t err_co = modbus_relay_read_coils(slave_id, relay_start, relay_count,
                                                       relay_bits, sizeof(relay_bits));
            esp_err_t err_di = modbus_relay_read_discrete_inputs(slave_id, input_start, input_count,
                                                                  input_bits, sizeof(input_bits));

            const size_t poll_cap = 1024;
            char *poll_out = malloc(poll_cap);
            if (!poll_out) {
                snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"OOM\"}");
            } else {
                size_t pos = 0;
                pos += snprintf(poll_out + pos, poll_cap - pos,
                                "{\"status\":\"%s\",\"slave_id\":%u,",
                                (err_co == ESP_OK || err_di == ESP_OK) ? "ok" : "error",
                                (unsigned)slave_id);

                /* relay array */
                pos += snprintf(poll_out + pos, poll_cap - pos, "\"relays\":[");
                for (uint16_t i = 0; i < relay_count; ++i) {
                    uint8_t bit = (relay_bits[i / 8] >> (i % 8)) & 1;
                    pos += snprintf(poll_out + pos, poll_cap - pos,
                                    "%s%u", i ? "," : "", (unsigned)bit);
                }

                /* inputs array */
                pos += snprintf(poll_out + pos, poll_cap - pos, "],\"inputs\":[");
                for (uint16_t i = 0; i < input_count; ++i) {
                    uint8_t bit = (input_bits[i / 8] >> (i % 8)) & 1;
                    pos += snprintf(poll_out + pos, poll_cap - pos,
                                    "%s%u", i ? "," : "", (unsigned)bit);
                }

                pos += snprintf(poll_out + pos, poll_cap - pos,
                                "],\"err_co\":\"%s\",\"err_di\":\"%s\"}",
                                esp_err_to_name(err_co), esp_err_to_name(err_di));

                httpd_resp_set_type(req, "application/json");
                esp_err_t ret = httpd_resp_send(req, poll_out, strlen(poll_out));
                free(poll_out);
                return ret;
            }
        }
    }

    else if (strcmp(test_name, "cctalk_addr_get") == 0) {
        uint8_t cctalk_addr = get_cctalk_address_configured();
        snprintf(response,
                 sizeof(response),
                 "{\"status\":\"ok\",\"addr\":%u}",
                 (unsigned)cctalk_addr);
    }
    else if (strcmp(test_name, "cctalk_addr_set") == 0) {
        uint8_t cctalk_addr = 0;
        esp_err_t parse_err = parse_cctalk_addr_from_request(req, &cctalk_addr);
        if (parse_err != ESP_OK) {
            snprintf(response,
                     sizeof(response),
                     "{\"status\":\"error\",\"error\":\"ADDR non valido\",\"err\":\"%s\"}",
                     esp_err_to_name(parse_err));
        } else {
            esp_err_t save_err = set_cctalk_address_configured(cctalk_addr);
            if (save_err != ESP_OK) {
                snprintf(response,
                         sizeof(response),
                         "{\"status\":\"error\",\"error\":\"Salvataggio ADDR fallito\",\"err\":\"%s\"}",
                         esp_err_to_name(save_err));
            } else {
                serial_test_push_monitor_action("CCTALK", "ADDR aggiornato");
                snprintf(response,
                         sizeof(response),
                         "{\"status\":\"ok\",\"message\":\"ADDR CCtalk salvato\",\"addr\":%u}",
                         (unsigned)cctalk_addr);
            }
        }
    }
    else if (strcmp(test_name, "cctalk_start") == 0) {
        uint8_t cctalk_addr = get_cctalk_address_configured();
        serial_test_clear_cctalk_monitor();
        serial_test_push_monitor_action("CCTALK", "START richiesta");
        esp_err_t rs232_err = rs232_init();
        if (rs232_err != ESP_OK) {
            serial_test_push_monitor_action("CCTALK", "START errore RS232");
            snprintf(response, sizeof(response), "{\"error\":\"Init RS232 fallita: %s\"}", esp_err_to_name(rs232_err));
        } else {
            esp_err_t cctalk_err = cctalk_driver_init();
            if (cctalk_err != ESP_OK) {
                serial_test_push_monitor_action("CCTALK", "START errore init CCtalk");
                snprintf(response, sizeof(response), "{\"error\":\"Init CCtalk fallita: %s\"}", esp_err_to_name(cctalk_err));
            } else {
                bool monitor_ready = false;
                bool own_task = false;

                if (s_cctalk_test_handle) {
                    monitor_ready = true;
                    own_task = s_cctalk_test_owned;
                } else {
                    TaskHandle_t existing = xTaskGetHandle("cctalk_task");
                    if (existing) {
                        s_cctalk_test_handle = existing;
                        s_cctalk_test_owned = false;
                        monitor_ready = true;
                        own_task = false;
                        serial_test_push_monitor_action("CCTALK", "START monitor agganciato");
                    } else {
                        BaseType_t ok = xTaskCreate(cctalk_task_run, "cctalk_test", 4096, NULL, 5, &s_cctalk_test_handle);
                        if (ok == pdPASS) {
                            s_cctalk_test_owned = true;
                            monitor_ready = true;
                            own_task = true;
                            serial_test_push_monitor_action("CCTALK", "START task locale creato");
                        } else {
                            s_cctalk_test_handle = NULL;
                            s_cctalk_test_owned = false;
                            serial_test_push_monitor_action("CCTALK", "START errore task");
                            snprintf(response, sizeof(response), "{\"error\":\"Impossibile avviare task CCtalk\"}");
                        }
                    }
                }

                if (monitor_ready) {
                    /* Publish start request to CCTALK agent instead of direct call */
                    fsm_input_event_t ev = {
                        .from = AGN_ID_WEB_UI,
                        .to = {AGN_ID_CCTALK},
                        .action = ACTION_ID_CCTALK_START,
                        .type = FSM_INPUT_EVENT_NONE,
                        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
                        .value_i32 = (int32_t)cctalk_addr,
                        .value_u32 = 0,
                        .aux_u32 = 0,
                        .text = {0},
                    };
                    if (!fsm_event_publish(&ev, pdMS_TO_TICKS(50))) {
                        serial_test_push_monitor_action("CCTALK", "START publish fallito");
                        snprintf(response, sizeof(response), "{\"error\":\"Coda eventi piena, publish fallito\"}");
                    } else {
                        char addr_msg[48] = {0};
                        snprintf(addr_msg, sizeof(addr_msg), "ADDR 0x%02X", (unsigned)cctalk_addr);
                        serial_test_push_monitor_action("CCTALK", addr_msg);
                        serial_test_push_monitor_action("CCTALK", "START event published");
                        if (own_task) {
                            snprintf(response, sizeof(response), "{\"message\":\"Test CCtalk avviato (task locale)\"}");
                        } else {
                            snprintf(response, sizeof(response), "{\"message\":\"Test CCtalk avviato (task scheduler)\"}");
                        }
                    }
                }
            }
        }
    } else if (strcmp(test_name, "cctalk_stop") == 0) {
        serial_test_push_monitor_action("CCTALK", "STOP richiesta");

        /* Publish stop request to CCTALK agent instead of direct call */
        fsm_input_event_t ev = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_CCTALK},
            .action = ACTION_ID_CCTALK_STOP,
            .type = FSM_INPUT_EVENT_NONE,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = 0,
            .value_u32 = 0,
            .aux_u32 = 0,
            .text = {0},
        };

        if (s_cctalk_test_handle && s_cctalk_test_owned) {
            vTaskDelete(s_cctalk_test_handle);
            s_cctalk_test_handle = NULL;
            s_cctalk_test_owned = false;
            serial_test_push_monitor_action("CCTALK", "STOP task locale fermato");
        } else if (s_cctalk_test_handle && !s_cctalk_test_owned) {
            s_cctalk_test_handle = NULL;
            s_cctalk_test_owned = false;
            serial_test_push_monitor_action("CCTALK", "STOP monitor sganciato");
        }

        if (!fsm_event_publish(&ev, pdMS_TO_TICKS(50))) {
            serial_test_push_monitor_action("CCTALK", "STOP publish fallito");
            snprintf(response, sizeof(response), "{\"error\":\"Coda eventi piena, publish fallito\"}");
        } else {
            serial_test_push_monitor_action("CCTALK", "STOP event published");
            snprintf(response, sizeof(response), "{\"message\":\"Gettoniera CCtalk stop event published\"}");
        }
    } else if (strcmp(test_name, "cctalk_retention_on") == 0 ||
               strcmp(test_name, "cctalk_retention_off") == 0) {
        bool retention_on = (strcmp(test_name, "cctalk_retention_on") == 0);
        bool accept_enabled = !retention_on;
        uint8_t cctalk_addr = get_cctalk_address_configured();
        serial_test_push_monitor_action("CCTALK", accept_enabled ? "Master Inhibit=1 (abilitato) richiesta" : "Master Inhibit=0 (inibito) richiesta");

        esp_err_t rs232_err = rs232_init();
        if (rs232_err != ESP_OK) {
            snprintf(response, sizeof(response),
                     "{\"error\":\"Init RS232 fallita: %s\"}",
                     esp_err_to_name(rs232_err));
        } else {
            esp_err_t cctalk_err = cctalk_driver_init();
            if (cctalk_err != ESP_OK) {
                snprintf(response, sizeof(response),
                         "{\"error\":\"Init CCtalk fallita: %s\"}",
                         esp_err_to_name(cctalk_err));
            } else {
                bool inhibit_std_ok = cctalk_modify_master_inhibit_std(cctalk_addr,
                                                                       accept_enabled,
                                                                       CCTALK_WEB_CMD_TIMEOUT_MS);
                bool mask_ok = true;
                if (!retention_on) {
                    mask_ok = cctalk_modify_inhibit_status(cctalk_addr,
                                                           0xFFU,
                                                           0xFFU,
                                                           CCTALK_WEB_CMD_TIMEOUT_MS);
                }

                if (!inhibit_std_ok || !mask_ok) {
                    serial_test_push_monitor_action("CCTALK", accept_enabled ? "Master Inhibit=1 (abilitato) FAIL" : "Master Inhibit=0 (inibito) FAIL");
                    snprintf(response, sizeof(response),
                             "{\"error\":\"Comando ritenzione/abilitazione formati fallito\"}");
                } else {
                    if (!retention_on) {
                        serial_test_push_monitor_action("CCTALK", "FORMATI abilitati mask 00 00");
                    }
                    uint8_t mask_low = 0;
                    uint8_t mask_high = 0;
                    bool status_ok = cctalk_request_inhibit_status(cctalk_addr,
                                                                   &mask_low,
                                                                   &mask_high,
                                                                   CCTALK_WEB_CMD_TIMEOUT_MS);

                    serial_test_push_monitor_action("CCTALK", accept_enabled ? "Master Inhibit=1 (abilitato) ok" : "Master Inhibit=0 (inibito) ok");
                    if (status_ok) {
                        snprintf(response, sizeof(response),
                                 "{\"message\":\"Ritenzione %s\",\"mask_low\":%u,\"mask_high\":%u}",
                                 retention_on ? "attivata" : "disattivata",
                                 (unsigned)mask_low,
                                 (unsigned)mask_high);
                    } else {
                        snprintf(response, sizeof(response),
                                 "{\"message\":\"Ritenzione %s\"}",
                                 retention_on ? "attivata" : "disattivata");
                    }
                }
            }
        }
    } else if (strcmp(test_name, "cctalk_retention_ch1_2") == 0) {
        const uint8_t mask_low = 0x03U;
        const uint8_t mask_high = 0x00U;
        uint8_t cctalk_addr = get_cctalk_address_configured();
        serial_test_push_monitor_action("CCTALK", "ABILITA CH1-CH2 richiesta");

        /* Publish mask update event to CCTALK agent instead of direct HW calls */
        fsm_input_event_t ev = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_CCTALK},
            .action = ACTION_ID_CCTALK_MASK,
            .type = FSM_INPUT_EVENT_NONE,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = (int32_t)cctalk_addr,
            .value_u32 = ((uint32_t)mask_high << 8) | (uint32_t)mask_low,
            .aux_u32 = 0,
            .text = {0},
        };

        if (!fsm_event_publish(&ev, pdMS_TO_TICKS(50))) {
            serial_test_push_monitor_action("CCTALK", "ABILITA CH1-CH2 publish fallito");
            snprintf(response, sizeof(response), "{\"error\":\"Coda eventi piena, publish fallito\"}");
        } else {
            serial_test_push_monitor_action("CCTALK", "ABILITA CH1-CH2 event published");
            snprintf(response, sizeof(response), "{\"message\":\"Abilitazione CH1-CH2 richiesta pubblicata\"}");
        }
    }

    else if (strcmp(test_name, "eeprom") == 0) {
        return eeprom_test_handler(req);
    }

    else if (strcmp(test_name, "mdb_start") == 0) {
        if (mdb_test_start() == ESP_OK) {
            snprintf(response, sizeof(response), "{\"message\":\"Test MDB avviato\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "mdb_stop") == 0) {
        mdb_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test MDB fermato\"}");
    }

    else if (strcmp(test_name, "sht_read") == 0) {
        float t, h;
        esp_err_t err = sht40_read(&t, &h);
        if (err == ESP_OK) {
            snprintf(response, sizeof(response), "{\"status\":\"ok\",\"temperature\":%.1f,\"humidity\":%.1f,\"message\":\"Lettura SHT40 OK\"}", t, h);
        } else {
            snprintf(response, sizeof(response), "{\"error\":\"Lettura fallita: %s\"}", esp_err_to_name(err));
        }
    }

    else if (strcmp(test_name, "sd_init") == 0) {
        if (s_sd_init_in_progress) {
            snprintf(response, sizeof(response), "{\"message\":\"Inizializzazione SD già in corso\"}");
        } else {
            s_sd_init_in_progress = true;
            BaseType_t ok = xTaskCreate(sd_init_worker_task, "sd_init_worker", 4096, NULL, 5, NULL);
            if (ok == pdPASS) {
                snprintf(response, sizeof(response), "{\"message\":\"Inizializzazione SD avviata\"}");
            } else {
                s_sd_init_in_progress = false;
                snprintf(response, sizeof(response), "{\"error\":\"Impossibile avviare init SD\"}");
            }
        }
    }
    else if (strcmp(test_name, "sd_format") == 0) {
        esp_err_t err = sd_card_format();
        if (err == ESP_OK) {
            snprintf(response, sizeof(response), "{\"message\":\"Formattazione completata con successo\"}");
        } else {
            snprintf(response, sizeof(response), "{\"error\":\"Formattazione fallita: %s\"}", esp_err_to_name(err));
        }
    }
    else if (strcmp(test_name, "sd_list") == 0) {
        char *list_buf = malloc(2048);
        if (list_buf) {
            esp_err_t err = sd_card_list_dir("/sdcard", list_buf, 2048);
            if (err == ESP_OK) {
                httpd_resp_set_type(req, "application/json");
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "message", list_buf);
                char *json_str = cJSON_PrintUnformatted(root);
                httpd_resp_sendstr(req, json_str);
                free(json_str);
                cJSON_Delete(root);
                free(list_buf);
                return ESP_OK;
            } else {
                snprintf(response, sizeof(response), "{\"error\":\"Errore lettura directory: %s\"}", esp_err_to_name(err));
                free(list_buf);
            }
        } else {
            snprintf(response, sizeof(response), "{\"error\":\"Memoria insufficiente\"}");
        }
    }

    else if (strcmp(test_name, "serial_send") == 0) {
        char buf[512] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            const char *port_raw = cJSON_GetStringValue(cJSON_GetObjectItem(root, "port"));
            const char *data_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "data"));

            if (port_raw && strcmp(port_raw, "mdb") == 0) {
                if (data_str) {
                    uint8_t mdb_packet[32];
                    int mdb_len = 0;
                    char *p = (char*)data_str;
                    while(*p && mdb_len < 32) {
                        if(*p == ' ') { p++; continue; }
                        mdb_packet[mdb_len++] = (uint8_t)strtol(p, &p, 16);
                    }
                    if (mdb_len > 0) {
                        mdb_send_packet(mdb_packet[0], mdb_packet + 1, mdb_len - 1);
                        snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Pacchetto MDB inviato (Addr: 0x%02X)\"}", mdb_packet[0]);
                    }
                }
            } else if (port_raw && strcmp(port_raw, "cctalk") == 0) {
                if (data_str) {
                    esp_err_t rs232_err = rs232_init();
                    esp_err_t cctalk_err = cctalk_driver_init();
                    if (rs232_err != ESP_OK || cctalk_err != ESP_OK) {
                        snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Init CCtalk fallita\",\"rs232\":\"%s\",\"cctalk\":\"%s\"}",
                                 esp_err_to_name(rs232_err), esp_err_to_name(cctalk_err));
                    } else {
                        esp_err_t send_err = serial_test_send_hex_uart(CONFIG_APP_RS232_UART_PORT, data_str);
                        if (send_err == ESP_OK) {
                            uint8_t tx_buf[64] = {0};
                            size_t tx_len = 0;
                            if (parse_hex_payload(data_str, tx_buf, sizeof(tx_buf), &tx_len) == ESP_OK) {
                                serial_test_push_monitor_entry("CCTALK_TX", tx_buf, tx_len);
                            }
                            snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Pacchetto CCtalk inviato\"}");
                        } else {
                            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Formato pacchetto non valido\",\"err\":\"%s\"}",
                                     esp_err_to_name(send_err));
                        }
                    }
                }
            } else {
                int port = (port_raw && strcmp(port_raw, "rs485") == 0) ? CONFIG_APP_RS485_UART_PORT : CONFIG_APP_RS232_UART_PORT;
                if (data_str) {
                    if (port_raw && strcmp(port_raw, "rs485") == 0 && modbus_relay_is_running()) {
                        snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"RS485 occupata da Modbus\"}");
                    } else {
                        if (port_raw && strcmp(port_raw, "rs485") == 0) rs485_init(); else rs232_init();
                        serial_test_send_uart(port, data_str);
                        snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Inviato su %s\"}", port_raw);
                    }
                }
            }
            cJSON_Delete(root);
        } else snprintf(response, sizeof(response), "{\"error\":\"JSON non valido\"}");
    }

    else if (strcmp(test_name, "gpio_get") == 0) {
        char json_buf[128];
        if (aux_gpio_get_all_json(json_buf, sizeof(json_buf)) == ESP_OK) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, json_buf);
        } else {
            httpd_resp_send_500(req);
        }
        return ESP_OK;
    }
    else if (strcmp(test_name, "gpio_set") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            int pin = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(root, "pin"));
            int val = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(root, "val"));
            aux_gpio_set_level(pin, val);
            snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
            cJSON_Delete(root);
        }
    }

    else if (strcmp(test_name, "serial_monitor") == 0) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "rs232", serial_test_get_monitor(CONFIG_APP_RS232_UART_PORT));
        cJSON_AddStringToObject(root, "rs485", serial_test_get_monitor(CONFIG_APP_RS485_UART_PORT));
        cJSON_AddStringToObject(root, "mdb", serial_test_get_monitor(CONFIG_APP_MDB_UART_PORT));
        cJSON_AddStringToObject(root, "cctalk", serial_test_get_cctalk_monitor());
        char *json_out = cJSON_PrintUnformatted(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_out, strlen(json_out));
        free(json_out);
        cJSON_Delete(root);
        return ESP_OK;
    }

    else if (strcmp(test_name, "serial_clear") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *port_obj = cJSON_GetObjectItem(root, "port");
            if (port_obj) {
                if (strcmp(port_obj->valuestring, "cctalk") == 0) {
                    serial_test_clear_cctalk_monitor();
                } else {
                    int port = (strcmp(port_obj->valuestring, "rs485") == 0) ? CONFIG_APP_RS485_UART_PORT :
                               (strcmp(port_obj->valuestring, "mdb") == 0) ? CONFIG_APP_MDB_UART_PORT : CONFIG_APP_RS232_UART_PORT;
                    serial_test_clear_monitor(port);
                }
            }
            cJSON_Delete(root);
        }
        snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
    }

    /* LED Bar test endpoints */
    else if (strcmp(test_name, "led_bar_init") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *total_leds_obj = cJSON_GetObjectItem(root, "total_leds");
            if (total_leds_obj && cJSON_IsNumber(total_leds_obj)) {
                uint32_t total_leds = (uint32_t)total_leds_obj->valuedouble;
                esp_err_t err = led_bar_init(total_leds);
                if (err == ESP_OK) {
                    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"total_leds\":%lu}", (unsigned long)total_leds);
                } else {
                    snprintf(response, sizeof(response), "{\"error\":\"LED bar init failed: %s\"}", esp_err_to_name(err));
                    httpd_resp_set_status(req, "500");
                }
            } else {
                snprintf(response, sizeof(response), "{\"error\":\"Missing or invalid total_leds\"}");
                httpd_resp_set_status(req, "400");
            }
            cJSON_Delete(root);
        } else {
            snprintf(response, sizeof(response), "{\"error\":\"Invalid JSON\"}");
            httpd_resp_set_status(req, "400");
        }
    }

    else if (strcmp(test_name, "led_bar_set_state") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *state_obj = cJSON_GetObjectItem(root, "state");
            if (state_obj && cJSON_IsNumber(state_obj)) {
                int state = state_obj->valueint;
                esp_err_t err = led_bar_set_state((led_bar_state_t)state);
                if (err == ESP_OK) {
                    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"state\":%d}", state);
                } else {
                    snprintf(response, sizeof(response), "{\"error\":\"LED bar set_state failed: %s\"}", esp_err_to_name(err));
                    httpd_resp_set_status(req, "500");
                }
            } else {
                snprintf(response, sizeof(response), "{\"error\":\"Missing or invalid state\"}");
                httpd_resp_set_status(req, "400");
            }
            cJSON_Delete(root);
        } else {
            snprintf(response, sizeof(response), "{\"error\":\"Invalid JSON\"}");
            httpd_resp_set_status(req, "400");
        }
    }

    else if (strcmp(test_name, "led_bar_set_progress") == 0) {
        char buf[128] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            cJSON *progress_obj = cJSON_GetObjectItem(root, "progress_percent");
            if (progress_obj && cJSON_IsNumber(progress_obj)) {
                uint8_t progress = (uint8_t)progress_obj->valuedouble;
                esp_err_t err = led_bar_set_progress(progress);
                if (err == ESP_OK) {
                    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"progress_percent\":%u}", progress);
                } else {
                    snprintf(response, sizeof(response), "{\"error\":\"LED bar set_progress failed: %s\"}", esp_err_to_name(err));
                    httpd_resp_set_status(req, "500");
                }
            } else {
                snprintf(response, sizeof(response), "{\"error\":\"Missing or invalid progress_percent\"}");
                httpd_resp_set_status(req, "400");
            }
            cJSON_Delete(root);
        } else {
            snprintf(response, sizeof(response), "{\"error\":\"Invalid JSON\"}");
            httpd_resp_set_status(req, "400");
        }
    }

    else if (strcmp(test_name, "led_bar_clear") == 0) {
        esp_err_t err = led_bar_clear();
        if (err == ESP_OK) {
            snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
        } else {
            snprintf(response, sizeof(response), "{\"error\":\"LED bar clear failed: %s\"}", esp_err_to_name(err));
            httpd_resp_set_status(req, "500");
        }
    }

    else {
        snprintf(response, sizeof(response), "{\"error\":\"Test sconosciuto: %s\"}", test_name);
        httpd_resp_set_status(req, "404");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}
