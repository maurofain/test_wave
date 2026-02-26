#include "web_ui.h"
#include "web_ui_internal.h"
#include "esp_log.h"
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
#include "eeprom_test.h"
#include "mdb_test.h"
#include "mdb.h"
#include "serial_test.h"
#include "sd_card.h"
#include "aux_gpio.h"
#include "sht40.h"

/* needed for publishing FSM actions when scanner commands are triggered */
#include "fsm.h"

#define TAG "WEB_UI_TEST_API"

esp_err_t api_test_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST %s", req->uri);
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
        ESP_LOGI(TAG, "[C] POST /api/test/scanner_on");
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
        ESP_LOGI(TAG, "[C] POST /api/test/scanner_off");
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
        if (!s_rs485_test_handle) {
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
        if (sd_card_is_mounted()) {
            sd_card_unmount();
        }
        esp_err_t err = sd_card_mount();
        if (err == ESP_OK) {
            snprintf(response, sizeof(response), "{\"message\":\"SD Inizializzata con successo\"}");
        } else {
            snprintf(response, sizeof(response), "{\"error\":\"Inizializzazione fallita: %s\"}", esp_err_to_name(err));
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
            } else {
                int port = (port_raw && strcmp(port_raw, "rs485") == 0) ? CONFIG_APP_RS485_UART_PORT : CONFIG_APP_RS232_UART_PORT;
                if (data_str) {
                    if (port_raw && strcmp(port_raw, "rs485") == 0) rs485_init(); else rs232_init();
                    serial_test_send_uart(port, data_str);
                    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Inviato su %s\"}", port_raw);
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
                int port = (strcmp(port_obj->valuestring, "rs485") == 0) ? CONFIG_APP_RS485_UART_PORT :
                           (strcmp(port_obj->valuestring, "mdb") == 0) ? CONFIG_APP_MDB_UART_PORT : CONFIG_APP_RS232_UART_PORT;
                serial_test_clear_monitor(port);
            }
            cJSON_Delete(root);
        }
        snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
    }

    else {
        snprintf(response, sizeof(response), "{\"error\":\"Test sconosciuto: %s\"}", test_name);
        httpd_resp_set_status(req, "404");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}
