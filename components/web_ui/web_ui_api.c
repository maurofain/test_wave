#include "web_ui_internal.h"
#include "web_ui.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "device_config.h"
#include "sd_card.h"
#include "cJSON.h"
#include "tasks.h"
#include "init.h"
#include "led.h"
#include "pwm_test.h"
#include "serial_test.h"
#include "mdb_test.h"
#include "sht40.h"
#include "app_version.h"
#include "usb_cdc_scanner.h"
#include "usb/usb_host.h"
#include "driver/uart.h"
#include "aux_gpio.h"
#include "io_expander.h"
#include "bsp/esp32_p4_nano.h"
#include "led_test.h"
#include "io_expander_test.h"
#include "eeprom_test.h"
#include "mdb.h"
#include "rs232.h"
#include "rs485.h"
#include <string.h>
#include <stdlib.h>

#define TAG "WEB_UI_API"

/* --- API: debug USB --- */
esp_err_t api_debug_usb_enumerate(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /api/debug/usb/enumerate");
    uint8_t addr_list[16];
    int num_devs = 0;
    esp_err_t err = usb_host_device_addr_list_fill(sizeof(addr_list), addr_list, &num_devs);

    cJSON *root = cJSON_CreateObject();
    if (err != ESP_OK) {
        cJSON_AddStringToObject(root, "error", esp_err_to_name(err));
    } else {
        cJSON_AddNumberToObject(root, "count", num_devs);
        cJSON *addrs = cJSON_CreateArray();
        for (int i = 0; i < num_devs; ++i) {
            cJSON_AddItemToArray(addrs, cJSON_CreateNumber(addr_list[i]));
        }
        cJSON_AddItemToObject(root, "addresses", addrs);

        usb_host_client_handle_t usb_client = NULL;
        const usb_host_client_config_t client_config = {
            .is_synchronous = false,
            .max_num_event_msg = 3,
            .async = { .client_event_callback = NULL, .callback_arg = NULL }
        };
        if (usb_host_client_register(&client_config, &usb_client) == ESP_OK) {
            cJSON *devices = cJSON_CreateArray();
            for (int i = 0; i < num_devs; ++i) {
                usb_device_handle_t dev_hdl;
                if (usb_host_device_open(usb_client, addr_list[i], &dev_hdl) == ESP_OK) {
                    const usb_device_desc_t *device_desc = NULL;
                    if (usb_host_get_device_descriptor(dev_hdl, &device_desc) == ESP_OK && device_desc) {
                        cJSON *d = cJSON_CreateObject();
                        cJSON_AddNumberToObject(d, "addr", addr_list[i]);
                        cJSON_AddNumberToObject(d, "vid", device_desc->idVendor);
                        cJSON_AddNumberToObject(d, "pid", device_desc->idProduct);
                        cJSON_AddNumberToObject(d, "class", device_desc->bDeviceClass);
                        cJSON_AddItemToArray(devices, d);
                    }
                    usb_host_device_close(usb_client, dev_hdl);
                }
            }
            cJSON_AddItemToObject(root, "devices", devices);
            usb_host_client_deregister(usb_client);
        } else {
            cJSON_AddStringToObject(root, "client", "register_failed");
        }
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t api_debug_usb_restart(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/debug/usb/restart (Sperimentali)");
#ifdef CONFIG_USB_OTG_SUPPORTED
    web_ui_add_log("INFO", "USB_DBG", "[Sperimentali] Restarting USB Host via API");
    esp_err_t err = bsp_usb_host_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "bsp_usb_host_stop returned %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    err = bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "bsp_usb_host_start returned %s", esp_err_to_name(err));
        httpd_resp_sendstr(req, "restart_failed");
    } else {
        httpd_resp_sendstr(req, "ok");
    }
#else
    httpd_resp_sendstr(req, "usb_not_supported");
#endif
    return ESP_OK;
}

/* --- API: config management --- */
esp_err_t api_config_get(httpd_req_t *req)
{
    ESP_LOGD(TAG, "[C] GET /api/config");
    device_config_t *cfg = device_config_get();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_name", cfg->device_name);
        cJSON *eth = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth, "enabled", cfg->eth.enabled);
    cJSON_AddBoolToObject(eth, "dhcp_enabled", cfg->eth.dhcp_enabled);
    cJSON_AddStringToObject(eth, "ip", cfg->eth.ip);
    cJSON_AddStringToObject(eth, "subnet", cfg->eth.subnet);
    cJSON_AddStringToObject(eth, "gateway", cfg->eth.gateway);
    cJSON_AddItemToObject(root, "eth", eth);
    
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi, "sta_enabled", cfg->wifi.sta_enabled);
    cJSON_AddBoolToObject(wifi, "dhcp_enabled", cfg->wifi.dhcp_enabled);
    cJSON_AddStringToObject(wifi, "ssid", cfg->wifi.ssid);
    cJSON_AddStringToObject(wifi, "password", cfg->wifi.password);
    cJSON_AddStringToObject(wifi, "ip", cfg->wifi.ip);
    cJSON_AddStringToObject(wifi, "subnet", cfg->wifi.subnet);
    cJSON_AddStringToObject(wifi, "gateway", cfg->wifi.gateway);
    cJSON_AddItemToObject(root, "wifi", wifi);
    
    cJSON_AddBoolToObject(root, "ntp_enabled", cfg->ntp_enabled);
    
    // NTP configuration
    cJSON *ntp = cJSON_CreateObject();
    cJSON_AddStringToObject(ntp, "server1", cfg->ntp.server1);
    cJSON_AddStringToObject(ntp, "server2", cfg->ntp.server2);
    cJSON_AddNumberToObject(ntp, "timezone_offset", cfg->ntp.timezone_offset);
    cJSON_AddItemToObject(root, "ntp", ntp);

    // Server/Cloud configuration
    cJSON *server = cJSON_CreateObject();
    cJSON_AddBoolToObject(server, "enabled", cfg->server.enabled);
    cJSON_AddStringToObject(server, "url", cfg->server.url);
    cJSON_AddItemToObject(root, "server", server);
    
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors, "io_expander_enabled", cfg->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors, "temperature_enabled", cfg->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors, "led_enabled", cfg->sensors.led_enabled);
    cJSON_AddNumberToObject(sensors, "led_count", cfg->sensors.led_count);
    cJSON_AddBoolToObject(sensors, "rs232_enabled", cfg->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors, "rs485_enabled", cfg->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors, "mdb_enabled", cfg->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors, "cctalk_enabled", cfg->sensors.cctalk_enabled);
    cJSON_AddBoolToObject(sensors, "sd_card_enabled", cfg->sensors.sd_card_enabled);
    cJSON_AddBoolToObject(sensors, "pwm1_enabled", cfg->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors, "pwm2_enabled", cfg->sensors.pwm2_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors);

    cJSON *display = cJSON_CreateObject();
    cJSON_AddBoolToObject(display, "enabled", cfg->display.enabled);
    cJSON_AddNumberToObject(display, "lcd_brightness", cfg->display.lcd_brightness);
    cJSON_AddItemToObject(root, "display", display);

    // RS232
    cJSON *rs232 = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs232, "baud", cfg->rs232.baud_rate);
    cJSON_AddNumberToObject(rs232, "data_bits", cfg->rs232.data_bits);
    cJSON_AddNumberToObject(rs232, "parity", cfg->rs232.parity);
    cJSON_AddNumberToObject(rs232, "stop_bits", cfg->rs232.stop_bits);
    cJSON_AddNumberToObject(rs232, "rx_buf", cfg->rs232.rx_buf_size);
    cJSON_AddNumberToObject(rs232, "tx_buf", cfg->rs232.tx_buf_size);
    cJSON_AddItemToObject(root, "rs232", rs232);

    // RS485
    cJSON *rs485 = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs485, "baud", cfg->rs485.baud_rate);
    cJSON_AddNumberToObject(rs485, "data_bits", cfg->rs485.data_bits);
    cJSON_AddNumberToObject(rs485, "parity", cfg->rs485.parity);
    cJSON_AddNumberToObject(rs485, "stop_bits", cfg->rs485.stop_bits);
    cJSON_AddNumberToObject(rs485, "rx_buf", cfg->rs485.rx_buf_size);
    cJSON_AddNumberToObject(rs485, "tx_buf", cfg->rs485.tx_buf_size);
    cJSON_AddItemToObject(root, "rs485", rs485);

    // MDB Serial
    cJSON *mdb_s = cJSON_CreateObject();
    cJSON_AddNumberToObject(mdb_s, "baud", cfg->mdb_serial.baud_rate);
    cJSON_AddNumberToObject(mdb_s, "data_bits", cfg->mdb_serial.data_bits);
    cJSON_AddNumberToObject(mdb_s, "parity", cfg->mdb_serial.parity);
    cJSON_AddNumberToObject(mdb_s, "stop_bits", cfg->mdb_serial.stop_bits);
    cJSON_AddNumberToObject(mdb_s, "rx_buf", cfg->mdb_serial.rx_buf_size);
    cJSON_AddNumberToObject(mdb_s, "tx_buf", cfg->mdb_serial.tx_buf_size);
    cJSON_AddItemToObject(root, "mdb_serial", mdb_s);

    // GPIOs
    cJSON *gpios = cJSON_CreateObject();
    cJSON *g33 = cJSON_CreateObject();
    cJSON_AddNumberToObject(g33, "mode", cfg->gpios.gpio33.mode);
    cJSON_AddBoolToObject(g33, "state", cfg->gpios.gpio33.initial_state);
    cJSON_AddItemToObject(gpios, "gpio33", g33);
    cJSON_AddItemToObject(root, "gpios", gpios);

    // Remote logging configuration
    cJSON *remote_log = cJSON_CreateObject();
    cJSON_AddNumberToObject(remote_log, "server_port", cfg->remote_log.server_port);
    cJSON_AddBoolToObject(remote_log, "use_broadcast", cfg->remote_log.use_broadcast);
    cJSON_AddItemToObject(root, "remote_log", remote_log);

    // Scanner configuration
    cJSON *scanner = cJSON_CreateObject();
    cJSON_AddBoolToObject(scanner, "enabled", cfg->scanner.enabled);
    cJSON_AddNumberToObject(scanner, "vid", cfg->scanner.vid);
    cJSON_AddNumberToObject(scanner, "pid", cfg->scanner.pid);
    cJSON_AddNumberToObject(scanner, "dual_pid", cfg->scanner.dual_pid);
    cJSON_AddItemToObject(root, "scanner", scanner);
    
    char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t api_config_backup(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/config/backup");
    if (!sd_card_is_mounted()) {
        const char *resp_str = "{\"error\":\"Scheda SD non montata\"}";
        httpd_resp_set_status(req, "500");
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }
    device_config_t *cfg = device_config_get();
    cJSON *root = cJSON_CreateObject();
    cJSON *eth = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth, "enabled", cfg->eth.enabled);
    cJSON_AddBoolToObject(eth, "dhcp_enabled", cfg->eth.dhcp_enabled);
    cJSON_AddStringToObject(eth, "ip", cfg->eth.ip);
    cJSON_AddStringToObject(eth, "subnet", cfg->eth.subnet);
    cJSON_AddStringToObject(eth, "gateway", cfg->eth.gateway);
    cJSON_AddItemToObject(root, "eth", eth);
    
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi, "sta_enabled", cfg->wifi.sta_enabled);
    cJSON_AddBoolToObject(wifi, "dhcp_enabled", cfg->wifi.dhcp_enabled);
    cJSON_AddStringToObject(wifi, "ssid", cfg->wifi.ssid);
    cJSON_AddStringToObject(wifi, "password", cfg->wifi.password);
    cJSON_AddStringToObject(wifi, "ip", cfg->wifi.ip);
    cJSON_AddStringToObject(wifi, "subnet", cfg->wifi.subnet);
    cJSON_AddStringToObject(wifi, "gateway", cfg->wifi.gateway);
    cJSON_AddItemToObject(root, "wifi", wifi);
    
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors, "io_expander_enabled", cfg->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors, "temperature_enabled", cfg->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors, "led_enabled", cfg->sensors.led_enabled);
    cJSON_AddNumberToObject(sensors, "led_count", cfg->sensors.led_count);
    cJSON_AddBoolToObject(sensors, "rs232_enabled", cfg->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors, "rs485_enabled", cfg->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors, "mdb_enabled", cfg->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors, "cctalk_enabled", cfg->sensors.cctalk_enabled);
    cJSON_AddBoolToObject(sensors, "sd_card_enabled", cfg->sensors.sd_card_enabled);
    cJSON_AddBoolToObject(sensors, "pwm1_enabled", cfg->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors, "pwm2_enabled", cfg->sensors.pwm2_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors);

    cJSON *display = cJSON_CreateObject();
    cJSON_AddBoolToObject(display, "enabled", cfg->display.enabled);
    cJSON_AddNumberToObject(display, "lcd_brightness", cfg->display.lcd_brightness);
    cJSON_AddItemToObject(root, "display", display);

    // RS232
    cJSON *rs232 = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs232, "baud", cfg->rs232.baud_rate);
    cJSON_AddNumberToObject(rs232, "data_bits", cfg->rs232.data_bits);
    cJSON_AddNumberToObject(rs232, "parity", cfg->rs232.parity);
    cJSON_AddNumberToObject(rs232, "stop_bits", cfg->rs232.stop_bits);
    cJSON_AddNumberToObject(rs232, "rx_buf", cfg->rs232.rx_buf_size);
    cJSON_AddNumberToObject(rs232, "tx_buf", cfg->rs232.tx_buf_size);
    cJSON_AddItemToObject(root, "rs232", rs232);

    // RS485
    cJSON *rs485 = cJSON_CreateObject();
    cJSON_AddNumberToObject(rs485, "baud", cfg->rs485.baud_rate);
    cJSON_AddNumberToObject(rs485, "data_bits", cfg->rs485.data_bits);
    cJSON_AddNumberToObject(rs485, "parity", cfg->rs485.parity);
    cJSON_AddNumberToObject(rs485, "stop_bits", cfg->rs485.stop_bits);
    cJSON_AddNumberToObject(rs485, "rx_buf", cfg->rs485.rx_buf_size);
    cJSON_AddNumberToObject(rs485, "tx_buf", cfg->rs485.tx_buf_size);
    cJSON_AddItemToObject(root, "rs485", rs485);

    // MDB Serial
    cJSON *mdb_s = cJSON_CreateObject();
    cJSON_AddNumberToObject(mdb_s, "baud", cfg->mdb_serial.baud_rate);
    cJSON_AddNumberToObject(mdb_s, "data_bits", cfg->mdb_serial.data_bits);
    cJSON_AddNumberToObject(mdb_s, "parity", cfg->mdb_serial.parity);
    cJSON_AddNumberToObject(mdb_s, "stop_bits", cfg->mdb_serial.stop_bits);
    cJSON_AddNumberToObject(mdb_s, "rx_buf", cfg->mdb_serial.rx_buf_size);
    cJSON_AddNumberToObject(mdb_s, "tx_buf", cfg->mdb_serial.tx_buf_size);
    cJSON_AddItemToObject(root, "mdb_serial", mdb_s);

    char *json = cJSON_Print(root);
    esp_err_t err = sd_card_write_file("/sdcard/config.jsn", json);
    
    char response[128];
    if (err == ESP_OK) {
        snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Backup salvato in /sdcard/config.jsn\"}");
    } else {
        snprintf(response, sizeof(response), "{\"error\":\"Errore scrittura file\"}");
    }
    
    free(json);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

esp_err_t api_config_save(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] Ricevuta richiesta salvataggio configurazione");
    char buf[4096] = {0};
    httpd_req_recv(req, buf, sizeof(buf)-1);
    ESP_LOGI(TAG, "[C] JSON configurazione ricevuto: %s", buf);
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        const char *resp_str = "{\"error\":\"Invalid JSON\"}";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }
    device_config_t *cfg = device_config_get();
    cJSON *name_obj = cJSON_GetObjectItem(root, "device_name");
    if (name_obj && name_obj->valuestring) strncpy(cfg->device_name, name_obj->valuestring, sizeof(cfg->device_name)-1);
    
    cJSON *eth_obj = cJSON_GetObjectItem(root, "eth");
    if (eth_obj) {
        cfg->eth.enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "enabled"));
        cfg->eth.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "dhcp_enabled"));
        cJSON *ip = cJSON_GetObjectItem(eth_obj, "ip");
        if (ip && ip->valuestring) strncpy(cfg->eth.ip, ip->valuestring, sizeof(cfg->eth.ip)-1);
        cJSON *subnet = cJSON_GetObjectItem(eth_obj, "subnet");
        if (subnet && subnet->valuestring) strncpy(cfg->eth.subnet, subnet->valuestring, sizeof(cfg->eth.subnet)-1);
        cJSON *gateway = cJSON_GetObjectItem(eth_obj, "gateway");
        if (gateway && gateway->valuestring) strncpy(cfg->eth.gateway, gateway->valuestring, sizeof(cfg->eth.gateway)-1);
    }
    
    cJSON *wifi_obj = cJSON_GetObjectItem(root, "wifi");
    if (wifi_obj) {
        cfg->wifi.sta_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "sta_enabled"));
        cfg->wifi.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "dhcp_enabled"));
        cJSON *ssid = cJSON_GetObjectItem(wifi_obj, "ssid");
        if (ssid && ssid->valuestring) strncpy(cfg->wifi.ssid, ssid->valuestring, sizeof(cfg->wifi.ssid)-1);
        cJSON *password = cJSON_GetObjectItem(wifi_obj, "password");
        if (password && password->valuestring) strncpy(cfg->wifi.password, password->valuestring, sizeof(cfg->wifi.password)-1);
    }
    
    // NTP configuration
    cfg->ntp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(root, "ntp_enabled"));
    cJSON *ntp_obj = cJSON_GetObjectItem(root, "ntp");
    if (ntp_obj) {
        cJSON *server1 = cJSON_GetObjectItem(ntp_obj, "server1");
        if (server1 && server1->valuestring) strncpy(cfg->ntp.server1, server1->valuestring, sizeof(cfg->ntp.server1)-1);
        cJSON *server2 = cJSON_GetObjectItem(ntp_obj, "server2");
        if (server2 && server2->valuestring) strncpy(cfg->ntp.server2, server2->valuestring, sizeof(cfg->ntp.server2)-1);
        cJSON *tz_offset = cJSON_GetObjectItem(ntp_obj, "timezone_offset");
        if (tz_offset && cJSON_IsNumber(tz_offset)) cfg->ntp.timezone_offset = tz_offset->valueint;
    }

    // Server/Cloud configuration
    cJSON *server_obj = cJSON_GetObjectItem(root, "server");
    if (server_obj) {
        cfg->server.enabled = cJSON_IsTrue(cJSON_GetObjectItem(server_obj, "enabled"));
        cJSON *url = cJSON_GetObjectItem(server_obj, "url");
        if (url && url->valuestring) strncpy(cfg->server.url, url->valuestring, sizeof(cfg->server.url)-1);
    }
    
    cJSON *sensors_obj = cJSON_GetObjectItem(root, "sensors");
    if (sensors_obj) {
        cfg->sensors.io_expander_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "io_expander_enabled"));
        cfg->sensors.temperature_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "temperature_enabled"));
        
        bool old_led_enabled = cfg->sensors.led_enabled;
        uint32_t old_led_count = cfg->sensors.led_count;
        
        cfg->sensors.led_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "led_enabled"));
        cJSON *lc = cJSON_GetObjectItem(sensors_obj, "led_count");
        if (lc) cfg->sensors.led_count = (uint32_t)lc->valueint;
        
        cfg->sensors.rs232_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs232_enabled"));
        cfg->sensors.rs485_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs485_enabled"));
        cfg->sensors.mdb_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "mdb_enabled"));
        cfg->sensors.cctalk_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "cctalk_enabled"));
        cfg->sensors.pwm1_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm1_enabled"));
        cfg->sensors.pwm2_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm2_enabled"));
        cfg->sensors.sd_card_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "sd_card_enabled"));

        if (cfg->sensors.led_enabled && (cfg->sensors.led_count != old_led_count || !old_led_enabled)) {
            ESP_LOGI(TAG, "[C] Cambio configurazione LED rilevato: re-inizializzo driver");
            led_init();
        }
    }

    cJSON *display_obj = cJSON_GetObjectItem(root, "display");
    if (display_obj) {
        cJSON *enabled = cJSON_GetObjectItem(display_obj, "enabled");
        if (enabled) {
#if FORCE_VIDEO_DISABLED
            cfg->display.enabled = false;
            ESP_LOGW(TAG, "[C] FORCE_VIDEO_DISABLED attivo: richiesta display.enabled ignorata");
#else
            cfg->display.enabled = cJSON_IsTrue(enabled);
            ESP_LOGI(TAG, "[C] Display enabled set to %d", cfg->display.enabled);
#endif
        }
        cJSON *lcd_b = cJSON_GetObjectItem(display_obj, "lcd_brightness");
        if (lcd_b) {
            int bright = lcd_b->valueint;
            if (bright < 0) bright = 0;
            if (bright > 100) bright = 100;
            cfg->display.lcd_brightness = (uint8_t)bright;
        }
        if (cfg->display.enabled) {
            bsp_display_brightness_set(cfg->display.lcd_brightness);
        }
    }

    cJSON_Delete(root);
    device_config_save(cfg);
    const char *ok = "{\"status\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ok, strlen(ok));
    return ESP_OK;
}

esp_err_t api_config_reset(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/config/reset");
    device_config_reset_defaults();
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

esp_err_t api_ntp_sync(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/ntp/sync");
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = init_sync_ntp();
    if (ret == ESP_OK) {
        const char *ok_resp = "{\"status\":\"ok\",\"message\":\"NTP synchronization completed successfully\"}";
        httpd_resp_send(req, ok_resp, strlen(ok_resp));
    } else {
        const char *err_resp = "{\"status\":\"error\",\"message\":\"NTP synchronization failed\"}";
        httpd_resp_send(req, err_resp, strlen(err_resp));
    }
    return ESP_OK;
}

/* --- Tasks API --- */
esp_err_t api_tasks_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /api/tasks (Lettura da SPIFFS)");
    
    FILE *f = fopen("/spiffs/tasks.csv", "r");
    if (!f) {
        ESP_LOGE(TAG, "[C] Impossibile aprire tasks.csv");
        httpd_resp_set_status(req, "500");
        const char *resp_str = "{\"error\":\"File non trovato\"}";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_FAIL;
    }
    
    char line[256];
    char response[4096] = "[";
    bool first = true;
    bool skip_header = true;
    
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (skip_header) { skip_header = false; continue; }
        char name[64], state[16];
        int priority, core, period_ms, stack_words;
        if (sscanf(line, "%63[^,],%15[^,],%d,%d,%d,%d", name, state, &priority, &core, &period_ms, &stack_words) == 6) {
            char task_json[256];
            snprintf(task_json, sizeof(task_json),
                     "%s{\"name\":\"%s\",\"state\":\"%s\",\"priority\":%d,\"core\":%d,\"period_ms\":%d,\"stack_words\":%d}",
                     first ? "" : ",", name, state, priority, core, period_ms, stack_words);
            strcat(response, task_json);
            first = false;
        }
    }
    
    strcat(response, "]");
    fclose(f);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

esp_err_t api_tasks_save(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/tasks/save");
    char buf[4096] = {0};
    httpd_req_recv(req, buf, sizeof(buf)-1);
    FILE *f = fopen("/spiffs/tasks.csv", "w");
    if (f) { fputs(buf, f); fclose(f); }
    const char *ok = "{\"status\":\"ok\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ok, strlen(ok));
    return ESP_OK;
}

esp_err_t api_tasks_apply(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/tasks/apply");
    tasks_load_config("/spiffs/tasks.csv");
    tasks_apply_n_run();
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\",\"message\":\"Task applicati con successo\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

esp_err_t api_test_endpoints_handler(httpd_req_t *req)
{
    static const char *json =
        "{"
        "\"note\":\"Supportati sia token legacy (/api/test/led_start) sia endpoint REST (/api/test/led/start)\","
        "\"legacy_base\":\"/api/test/<token>\","
        "\"rest_base\":\"/api/test/<group>/<action>\","
        "\"tokens\":["
        "\"led_start\",\"led_stop\",\"led_set\",\"led_bright\","
        "\"pwm1_start\",\"pwm1_stop\",\"pwm2_start\",\"pwm2_stop\",\"pwm_set\","
        "\"ioexp_start\",\"ioexp_stop\",\"io_get\",\"io_set\","
        "\"rs232_start\",\"rs232_stop\",\"rs485_start\",\"rs485_stop\",\"cctalk_start\",\"cctalk_stop\","
        "\"mdb_start\",\"mdb_stop\","
        "\"serial_send\",\"serial_monitor\",\"serial_clear\","
        "\"gpio_get\",\"gpio_set\","
        "\"sht_read\","
        "\"sd_init\",\"sd_list\",\"sd_format\","
        "\"scanner_setup\",\"scanner_state\",\"scanner_on\",\"scanner_off\","
        "\"display_bright\",\"eeprom\""
        "]"
        "}";

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

/* --- Generic /api/test handler (routing) --- */
esp_err_t api_test_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST %s", req->uri);
    const char *raw_test_name = req->uri + strlen("/api/test/");
    char normalized_test_name[96] = {0};
    size_t write_idx = 0;

    while (*raw_test_name == '/') raw_test_name++;
    for (size_t i = 0; raw_test_name[i] != '\0' && raw_test_name[i] != '?' && write_idx < sizeof(normalized_test_name) - 1; ++i) {
        char current = raw_test_name[i];
        normalized_test_name[write_idx++] = (current == '/' || current == '-') ? '_' : current;
    }
    normalized_test_name[write_idx] = '\0';

    const char *test_name = normalized_test_name;
    char response[256] = {0};

    // --- TEST LED ---
    if (strcmp(test_name, "led_start") == 0) {
        if (led_test_start() == ESP_OK) {
            snprintf(response, sizeof(response), "{\"message\":\"Test LED avviato\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "led_stop") == 0) {
        led_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test LED fermato\"}");
    }

    // --- COMANDI SCANNER QR (protocollo framed) ---
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
            ESP_LOGI(TAG, "Scanner OFF command dispatched");
            snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Scanner OFF inviato\"}");
        } else {
            ESP_LOGW(TAG, "Scanner OFF command failed: %s", esp_err_to_name(err));
            snprintf(response, sizeof(response), "{\"status\":\"error\",\"error\":\"Comando Scanner OFF fallito\",\"err\":\"%s\"}", esp_err_to_name(err));
        }
    }

    // --- CONTROLLO AGGIORNAMENTO LUMINOSITÀ ---
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

    // --- CONTROLLO LUMINOSITÀ DISPLAY (live via I2C) ---
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
                    if (cfg) cfg->display.lcd_brightness = (uint8_t)bright; // update runtime value
                    snprintf(response, sizeof(response), "{\"status\":\"ok\"}");
                }
            }
            if (root) {
                cJSON_Delete(root);
            }
        }
    }

    // --- CONTROLLO MANUALE LED ---
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

    // --- TEST PWM1 ---
    else if (strcmp(test_name, "pwm1_start") == 0) {
        pwm_test_start(1);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM1 avviato\"}");
    } else if (strcmp(test_name, "pwm1_stop") == 0) {
        pwm_test_stop(1);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM1 fermato\"}");
    }

    // --- TEST PWM2 ---
    else if (strcmp(test_name, "pwm2_start") == 0) {
        pwm_test_start(2);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM2 avviato\"}");
    } else if (strcmp(test_name, "pwm2_stop") == 0) {
        pwm_test_stop(2);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM2 fermato\"}");
    }

    // --- CONTROLLO MANUALE PWM ---
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

    // --- TEST I/O EXPANDER ---
    else if (strcmp(test_name, "ioexp_start") == 0) {
        io_expander_test_start();
        snprintf(response, sizeof(response), "{\"message\":\"Test I/O Expander avviato (1Hz)\"}");
    } else if (strcmp(test_name, "ioexp_stop") == 0) {
        io_expander_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test I/O Expander fermato\"}");
    }

    // --- CONTROLLO MANUALE I/O EXPANDER ---
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

    // --- (continues identical to original) ---
    else if (strcmp(test_name, "io_get") == 0) {
        uint8_t in = io_get();
        snprintf(response, sizeof(response), "{\"input\":%d,\"output\":%d}", in, io_output_state);
    }

    else if (strcmp(test_name, "rs232_start") == 0) {
        if (!s_rs232_test_handle) {
            rs232_init();
            xTaskCreate(uart_test_task, "rs232_test", 2048, (void*)CONFIG_APP_RS232_UART_PORT, 5, &s_rs232_test_handle);
            snprintf(response, sizeof(response), "{\"message\":\"Test RS232 avviato (caratteri 55,AA,01,07)\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "rs232_stop") == 0) {
        if (s_rs232_test_handle) { vTaskDelete(s_rs232_test_handle); s_rs232_test_handle = NULL; }
        snprintf(response, sizeof(response), "{\"message\":\"Test RS232 fermato\"}");
    }

    else if (strcmp(test_name, "cctalk_start") == 0) {
        if (!s_rs232_test_handle) {
            rs232_init();
            xTaskCreate(uart_test_task, "cctalk_test", 2048, (void*)CONFIG_APP_RS232_UART_PORT, 5, &s_rs232_test_handle);
            snprintf(response, sizeof(response), "{\"message\":\"Test CCtalk avviato\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "cctalk_stop") == 0) {
        if (s_rs232_test_handle) { vTaskDelete(s_rs232_test_handle); s_rs232_test_handle = NULL; }
        snprintf(response, sizeof(response), "{\"message\":\"Test CCtalk fermato\"}");
    }

    else if (strcmp(test_name, "rs485_start") == 0) {
        if (!s_rs485_test_handle) {
            rs485_init();
            xTaskCreate(uart_test_task, "rs485_test", 2048, (void*)CONFIG_APP_RS485_UART_PORT, 5, &s_rs485_test_handle);
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