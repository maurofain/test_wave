#include "web_ui.h"
#include "esp_log.h"
#include "esp_check.h"
#include "device_config.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_partition.h"
#include "bsp/display.h"
#include "bsp/esp32_p4_nano.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <stdint.h>
#include "usb_cdc_scanner.h"
#include "usb/usb_host.h"
#include "driver/uart.h"
#include "init.h"
#include "led.h"
#include "mdb_test.h"
#include <stdlib.h>
#include "app_version.h"
#include "serial_test.h"
#include "mdb.h"
#include "led_test.h"
#include "pwm_test.h"
#include "io_expander.h"
#include "io_expander_test.h"
#include "rs232.h"
#include "rs485.h"
#include "tasks.h"
#include "sd_card.h"
#include "aux_gpio.h"
#include "eeprom_test.h"
#include "sht40.h"
#include "http_services.h"
#include "web_ui_programs.h"

#if DNA_SYS_MONITOR == 0
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/task.h"
#endif

#define TAG "WEB_UI"
#define MAX_STORED_LOGS 100

/*
 * Forzatura temporanea: disabilita SEMPRE la parte video.
 *
 * 1 = headless forzato (ignora richieste web per abilitare display/LVGL)
 * 0 = comportamento normale da configurazione utente
 */
#define FORCE_VIDEO_DISABLED 0

/* Log store e handler spostati in components/web_ui/web_ui_logs.c */



/* Handle del server spostato in web_ui_server.c */

// Elementi HTML comuni
#include "web_ui_internal.h" /* spostato in web_ui_common.c */
// Handle dei task di test per Serial Blink Test
TaskHandle_t s_rs232_test_handle = NULL;
TaskHandle_t s_rs485_test_handle = NULL;

static bool web_ui_extract_lang_from_filename(const char *name, char *out_lang, size_t out_len)
{
    if (!name || !out_lang || out_len < 3) {
        return false;
    }

    const char *prefix = "i18n_";
    const char *suffix = ".json";
    const size_t prefix_len = 5;
    const size_t suffix_len = 5;
    const size_t name_len = strlen(name);
    if (name_len <= (prefix_len + suffix_len)) {
        return false;
    }
    if (strncmp(name, prefix, prefix_len) != 0) {
        return false;
    }
    if (strcmp(name + name_len - suffix_len, suffix) != 0) {
        return false;
    }

    const size_t lang_len = name_len - prefix_len - suffix_len;
    if (lang_len < 2 || lang_len >= out_len || lang_len > 7) {
        return false;
    }

    for (size_t i = 0; i < lang_len; ++i) {
        char c = name[prefix_len + i];
        bool valid = ((c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') ||
                      c == '_' || c == '-');
        if (!valid) {
            return false;
        }
        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        out_lang[i] = c;
    }
    out_lang[lang_len] = '\0';
    return true;
}

static void web_ui_lang_label_from_code(const char *lang, char *label, size_t label_len)
{
    if (!label || label_len == 0) {
        return;
    }
    if (!lang || !lang[0]) {
        snprintf(label, label_len, "Unknown");
        return;
    }

    if (strcmp(lang, "it") == 0) {
        snprintf(label, label_len, "Italiano (IT)");
    } else if (strcmp(lang, "en") == 0) {
        snprintf(label, label_len, "English (EN)");
    } else {
        char upper[16] = {0};
        size_t n = strlen(lang);
        if (n >= sizeof(upper)) {
            n = sizeof(upper) - 1;
        }
        for (size_t i = 0; i < n; ++i) {
            char c = lang[i];
            if (c >= 'a' && c <= 'z') {
                c = (char)(c - 'a' + 'A');
            }
            upper[i] = c;
        }
        upper[n] = '\0';
        snprintf(label, label_len, "%s", upper);
    }
}

// TEST UART: 0x55, 0xAA, 0x01, 0x07 (periodico)
void uart_test_task(void *arg) {
    uart_port_t port = (uart_port_t)(intptr_t)arg;
    const char* seq_hex[] = {"\\0x55", "\\0xAA", "\\0x01", "\\0x07"};
    uint8_t rx_buf[128];
    size_t rx_len;
    ESP_LOGI(TAG, "Test Porta UART %d: Avvio (TX + RX Monitor HEX)", port);
    
    serial_test_init();
    
    while(1) {
        for (int i=0; i<4; i++) {
            // Invio tramite componente serial_test per avere visibilità anche del TX nel monitor
            serial_test_send_uart(port, seq_hex[i]);
            
            // Monitoraggio ricezione per circa 1 secondo tra un invio e l'altro
            for (int j=0; j<10; j++) { 
                // serial_test_read_uart ha un timeout interno di 50ms e logga automaticamente in HEX (prefix RX<)
                if (serial_test_read_uart(port, rx_buf, sizeof(rx_buf), &rx_len) == ESP_OK) {
                    ESP_LOGD(TAG, "UART %d ricevuti %d bytes", port, (int)rx_len);
                }
                vTaskDelay(pdMS_TO_TICKS(50)); 
            }
        }
    }
}

// --- GESTORI (HANDLERS) ---

/* Helpers `ip_to_str` and `perform_ota` are implemented in `web_ui_common.c` (shared helpers). */

esp_err_t reboot_factory_handler(httpd_req_t *req)
{
    if (!web_ui_has_valid_password(req)) {
        return web_ui_send_password_required(req, "Reboot Factory", "/reboot/factory");
    }

    httpd_resp_sendstr(req, "<html><body><h1>Reboot in Factory Mode...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_factory();
    return ESP_OK;
}

esp_err_t reboot_app_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in Production Mode...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_app();
    return ESP_OK;
}

esp_err_t reboot_app_last_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in App Last...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_app_last();
    return ESP_OK;
}

esp_err_t reboot_ota0_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in OTA0...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_ota0();
    return ESP_OK;
}

esp_err_t reboot_ota1_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in OTA1...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_ota1();
    return ESP_OK;
}


// Handler per la pagina OTA
esp_err_t ota_get_handler(httpd_req_t *req)
{
    const char *extra_style = 
        ".card{background:white;padding:25px;margin:20px auto;max-width:600px;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,0.1)}"
        "input,button{width:100%;padding:10px;margin:10px 0;border-radius:4px;border:1px solid #ddd;box-sizing:border-box}"
        "button{background:#e67e22;color:white;font-weight:bold;cursor:pointer}button:hover{background:#d35400}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Aggiornamento OTA", extra_style, true);

    const char *body = 
        "<div class='container'><div class='card'>"
        "<form id='f'><input type='file' id='i' accept='.bin' required><button type='submit'>⬆️ Carica Firmware</button></form>"
        "<div style='margin-top:10px'><progress id='p' value='0' max='100' style='width:100%;height:20px;'></progress><div id='pct' style='margin-top:6px;font-weight:bold;'>0%</div></div>"
        "<div id='s' style='margin-top:10px'></div></div></div><script>"
        "document.getElementById('f').onsubmit=function(e){e.preventDefault();"
        "const file=document.getElementById('i').files[0];if(!file){return;}"
        "const p=document.getElementById('p');const pct=document.getElementById('pct');const s=document.getElementById('s');"
        "p.value=0;pct.innerText='0%';s.innerText='Upload in corso...';"
        "const x=new XMLHttpRequest();x.open('POST','/ota/upload',true);"
        "x.setRequestHeader('Content-Type','application/octet-stream');"
        "x.upload.onprogress=function(ev){if(ev.lengthComputable){const v=Math.min(100,Math.round((ev.loaded*100)/ev.total));p.value=v;pct.innerText=v+'%';}};"
        "x.onload=function(){if(x.status>=200&&x.status<300){p.value=100;pct.innerText='100%';s.innerText='✅ Successo! Riavvio...';}else{s.innerText='❌ Errore ('+x.status+')';}};"
        "x.onerror=function(){s.innerText='❌ Errore di rete';};"
        "x.send(file);"
        "};"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Handler per l'upload OTA (POST)
esp_err_t ota_upload_handler(httpd_req_t *req)
{
    char content_type[64] = {0};
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) == ESP_OK) {
        if (strstr(content_type, "multipart/form-data") != NULL) {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_send(req, "Inviare binario raw (application/octet-stream), non multipart/form-data", -1);
            return ESP_FAIL;
        }
    }

    if (req->content_len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Payload OTA vuoto", -1);
        return ESP_FAIL;
    }

    const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
    if (!p) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    esp_ota_handle_t h;
    esp_err_t err = esp_ota_begin(p, OTA_SIZE_UNKNOWN, &h);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Errore avvio OTA", -1);
        return ESP_FAIL;
    }

    char b[1024]; int rem = req->content_len;
    while (rem > 0) {
        int n = httpd_req_recv(req, b, MIN(rem, 1024));
        if (n <= 0) {
            esp_ota_abort(h);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Errore ricezione dati OTA", -1);
            return ESP_FAIL;
        }

        err = esp_ota_write(h, b, n);
        if (err != ESP_OK) {
            esp_ota_abort(h);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "Errore scrittura OTA", -1);
            return ESP_FAIL;
        }
        rem -= n;
    }

    err = esp_ota_end(h);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Immagine OTA non valida", -1);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(p);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Errore impostazione boot partition", -1);
        return ESP_FAIL;
    }

    httpd_resp_send(req, "OTA completato con successo, riavvio in corso...", -1);
    vTaskDelay(pdMS_TO_TICKS(1000)); esp_restart();
    return ESP_OK;
}

// Handler per l'URL OTA (POST)
esp_err_t ota_post_handler(httpd_req_t *req)
{
    char q[256], u[200] = {0};
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) httpd_query_key_value(q, "url", u, sizeof(u));
    if (strlen(u) == 0) return ESP_FAIL;
    perform_ota(u);
    return ESP_OK;
}

// Handler per errore 404
esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t error)
{
    httpd_resp_set_status(req, "404 Non Trovato");
    return httpd_resp_send(req, "404 Non Trovato", -1);
}

// Handler per pagina configurazione
esp_err_t config_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /config");
    const bool config_read_only = !web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS);
    
    const char *extra_style = 
        ".section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px}.form-group{margin:15px 0}"
        "label{display:block;margin:5px 0;font-weight:bold;color:#34495e}input[type=text],input[type=password]{padding:8px;border:1px solid #ddd;border-radius:4px;width:100%;margin:5px 0;box-sizing:border-box;color:#333}"
        "button{padding:10px 20px;background:#27ae60;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:bold;margin:5px}"
        "button:hover{background:#229954}.indent{margin-left:30px;padding-left:15px;border-left:2px solid #ecf0f1}"
        ".sw-row{display:flex;align-items:center;gap:15px;padding:12px 0;border-bottom:1px solid #f9f9f9}"
        ".sw-row:last-child{border-bottom:none}"
        ".switch{position:relative;display:inline-block;width:46px;height:24px}"
        ".switch input{opacity:0;width:0;height:0}"
        ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;transition:.3s;border-radius:24px}"
        ".slider:before{position:absolute;content:\"\";height:18px;width:18px;left:3px;bottom:3px;background-color:white;transition:.3s;border-radius:50%}"
        "input:checked + .slider{background-color:#27ae60}"
        "input:checked + .slider:before{transform:translateX(22px)}"
        "/* Collapsible sections (config page) */"
        ".section.collapsed > :not(h2){display:none !important;}"
        ".section h2{cursor:pointer; position:relative;}"
        ".section h2 .section-toggle-icon{position:absolute; right:12px; top:6px; font-size:18px; color:#95a5a6;}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Configurazione Device", extra_style, true);

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_sendstr_chunk(req, "<script>window.__showFactoryPasswordSection=true;</script>");
    } else {
        httpd_resp_sendstr_chunk(req, "<script>window.__showFactoryPasswordSection=false;</script>");
    }

    /* DO_NOT_MODIFY_START: /config page - autogenerated UI markup */
    const char *body = 
        "<div class='container'>"
        "<div id='alert'></div>"
        "<form id='configForm'>"
        
        "<div class='section collapsed' style='background:#f1f4f6; border-left:5px solid #3498db;'>"
        "<h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'>🆔 Identità Dispositivo<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='form-group'><label>Nome Dispositivo</label>"
        "<input type='text' id='dev_name' name='dev_name' placeholder='es: TestWave-01' style='font-size:1.1em; font-weight:bold;'></div>"
        "<div class='form-group'><label>Lingua UI</label>"
        "<select id='ui_language' name='ui_language' style='width:100%; padding:8px; border:1px solid #ddd; border-radius:4px;'><option value='it'>Italiano (IT)</option></select></div>"
        "</div>"

        "<div class='section collapsed'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'>🌐 Ethernet<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='eth_en' name='eth_en'><span class='slider'></span></label><span>Abilitato</span></div>"
        "<div class='indent'>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='eth_dhcp' name='eth_dhcp'><span class='slider'></span></label><span>DHCP</span></div>"
        "<div class='form-group'><label>Indirizzo IP</label><input type='text' id='eth_ip' name='eth_ip' placeholder='192.168.1.100'></div>"
        "<div class='form-group'><label>Subnet Mask</label><input type='text' id='eth_subnet' name='eth_subnet' placeholder='255.255.255.0'></div>"
        "<div class='form-group'><label>Gateway</label><input type='text' id='eth_gateway' name='eth_gateway' placeholder='192.168.1.1'></div>"
        "</div></div>"

        "<div class='section collapsed'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'>📡 WiFi STA<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='wifi_en' name='wifi_en'><span class='slider'></span></label><span>WiFi Abilitato</span></div>"
        "<div class='indent'>"
        "<div class='form-group'><label>SSID</label><input type='text' id='wifi_ssid' name='wifi_ssid'></div>"
        "<div class='form-group'><label>Password</label><input type='password' id='wifi_pwd' name='wifi_pwd'></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='wifi_dhcp' name='wifi_dhcp'><span class='slider'></span></label><span>DHCP</span></div>"
        "</div></div>"

        "<div class='section collapsed'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'> NTP<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='ntp_en' name='ntp_en'><span class='slider'></span></label><span>NTP Abilitato</span><button type='button' onclick='syncNTP()' style='margin-left:10px; background:#f39c12; color:white; border:none; padding:8px 15px; border-radius:4px; cursor:pointer;'>Aggiorna</button><span id='current_time' style='margin-left:15px; font-weight:bold; color:#27ae60;'></span></div>"
        "<div class='indent'>"
        "<div class='form-group'><label>Server NTP 1</label><input type='text' id='ntp_server1' name='ntp_server1' placeholder='time.google.com'></div>"
        "<div class='form-group'><label>Server NTP 2</label><input type='text' id='ntp_server2' name='ntp_server2' placeholder='pool.ntp.org'></div>"
        "<div class='form-group'><label>Offset Fuso Orario (ore)</label><input type='number' id='ntp_timezone_offset' name='ntp_timezone_offset' min='-12' max='12' placeholder='1'></div>"
        "</div></div>"


        "<div class='section collapsed'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'>🔁 Server Remoto<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='server_en' name='server_en'><span class='slider'></span></label><span>Server abilitato</span></div>"
        "<div class='form-group indent'><label>Base server URL</label><input type='text' id='server_url' name='server_url' placeholder='http://195.231.69.227:5556/'></div>"
        "<div class='form-group indent'><label>Server serial</label><input type='text' id='server_serial' name='server_serial' placeholder='AD-34-DFG-333'></div>"
        "<div class='form-group indent'><label>Server password (MD5)</label><input type='text' id='server_password' name='server_password' placeholder='c1ef6429c5e0f753ff24a114de6ee7d4'></div>"
        "</div>"

        "<div class='section collapsed'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'>📊 Logging Remoto<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='remote_log_broadcast' name='remote_log_broadcast'><span class='slider'></span></label><span>Usa broadcast UDP</span></div>"
        "<div class='form-group indent'><label>Porta UDP</label><input type='number' id='remote_log_port' name='remote_log_port' min='1024' max='65535' placeholder='9514' style='width:120px; padding:6px; border:1px solid #ddd; border-radius:4px;'></div>"
        "</div>"

        "<div class='section collapsed'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'>🔌 Periferiche Hardware<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='io_exp' name='io_exp'><span class='slider'></span></label><span>I/O Expander</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='temp' name='temp'><span class='slider'></span></label><span>Sensore Temperatura</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='led' name='led'><span class='slider'></span></label><span>LED Strip (WS2812)</span></div>"
        "<div class='form-group indent'><label>Numero di LED</label><input type='number' id='led_count' name='led_count' min='1' max='512' style='width:120px; padding:6px; border:1px solid #ddd; border-radius:4px;'></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='rs232' name='rs232'><span class='slider'></span></label><span>UART RS232</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='rs485' name='rs485'><span class='slider'></span></label><span>UART RS485</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='mdb' name='mdb'><span class='slider'></span></label><span>MDB Engine</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='sd_card' name='sd_card'><span class='slider'></span></label><span>Scheda SD</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='pwm1' name='pwm1'><span class='slider'></span></label><span>PWM Canale 1</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='pwm2' name='pwm2'><span class='slider'></span></label><span>PWM Canale 2</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='scanner_en' name='scanner_en'><span class='slider'></span></label><span>Scanner USB (CDC)</span></div>"
        "<div class='form-group indent'><label>VID</label><input type='text' id='scanner_vid' name='scanner_vid' placeholder='0x1EAB' style='width:120px'></div>"
        "<div class='form-group indent'><label>PID</label><input type='text' id='scanner_pid' name='scanner_pid' placeholder='0x0006' style='width:120px'></div>"
        "<div class='form-group indent'><label>Dual PID (opzionale)</label><input type='text' id='scanner_dual_pid' name='scanner_dual_pid' placeholder='0x0000' style='width:120px'></div>"
        "</div>"

        "<div class='section collapsed'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'>📺 Display<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='display_en' name='display_en'><span class='slider'></span></label><span>Display abilitato</span></div>"
        "<div class='form-group'><label>Luminosità LCD (<span id='bright_val'>--</span>%)</label>"
        "<input type='range' id='lcd_bright' name='lcd_bright' min='0' max='100' style='width:100%' oninput='onDisplayBrightInput(this.value)'></div>"
        "</div>"

        "<div class='section collapsed'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\");if(i){i.innerText=s.classList.contains(\"collapsed\")?\"▸\":\"▾\";}' tabindex='0'>🔘 GPIO Ausiliario (GPIO33)<span class='section-toggle-icon'>▸</span></h2>"
        "<div class='indent'>"
        "<b>GPIO 33</b>"
        "<div class='form-group'><label>Modalità</label>"
        "<select id='g33_mode' style='width:100%; padding:8px;'><option value='0'>Input (Float)</option><option value='1'>Input (Pull-up)</option><option value='2'>Input (Pull-down)</option><option value='3'>Output</option></select></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='g33_state'><span class='slider'></span></label><span>Stato Iniziale (Solo Out)</span></div>"
        "</div></div>"

        "<div class='section'><h2>(TAG) Porte Seriali</h2>"
        "<details><summary><b>RS232 Configuration</b></summary>"
        "<div class='form-group'><label>Baudrate</label><input type='text' id='rs232_baud'></div>"
        "<div class='form-group'><label>Data Bits</label><input type='text' id='rs232_bits'></div>"
        "<div class='form-group'><label>Parità (0:None, 1:Odd, 2:Even)</label><input type='text' id='rs232_par'></div>"
        "<div class='form-group'><label>Stop Bits (1, 2)</label><input type='text' id='rs232_stop'></div>"
        "<div class='form-group'><label>Buffer RX</label><input type='text' id='rs232_rx'></div>"
        "<div class='form-group'><label>Buffer TX</label><input type='text' id='rs232_tx'></div>"
        "</details>"
        "<details style='margin-top:10px'><summary><b>RS485 Configuration</b></summary>"
        "<div class='form-group'><label>Baudrate</label><input type='text' id='rs485_baud'></div>"
        "<div class='form-group'><label>Data Bits</label><input type='text' id='rs485_bits'></div>"
        "<div class='form-group'><label>Parità (0:None, 1:Odd, 2:Even)</label><input type='text' id='rs485_par'></div>"
        "<div class='form-group'><label>Stop Bits (1, 2)</label><input type='text' id='rs485_stop'></div>"
        "<div class='form-group'><label>Buffer RX</label><input type='text' id='rs485_rx'></div>"
        "<div class='form-group'><label>Buffer TX</label><input type='text' id='rs485_tx'></div>"
        "</details>"
        "<details style='margin-top:10px'><summary><b>MDB Configuration</b></summary>"
        "<div class='form-group'><label>Baudrate</label><input type='text' id='mdb_baud'></div>"
        "<div class='form-group'><label>Buffer RX</label><input type='text' id='mdb_rx'></div>"
        "<div class='form-group'><label>Buffer TX</label><input type='text' id='mdb_tx'></div>"
        "</details>"
        "</div>"

        "<div class='section' style='display:flex; justify-content:center; gap:20px;'>"
        "<button type='submit' style='flex:1; max-width:200px;'>💾 Salva Configurazione</button>"
        "<button type='button' onclick='backupConfig()' style='background:#9b59b6; flex:1; max-width:200px;'>📥 Backup Config</button>"
        "<button type='button' onclick='loadConfig()' style='background:#3498db; flex:1; max-width:200px;'>🔄 Aggiorna Dati</button>"
        "<button type='button' onclick='resetConfig()' style='background:#7f8c8d; flex:1; max-width:200px;'>⚠️ Reset Fabbrica</button>"
        "</div>"
        "</form></div>"
        "<script>"
        "/* DO_NOT_MODIFY_END: /config page - autogenerated UI markup */"
        "async function backupConfig(){"
        "try{const r=await fetch('/api/config/backup',{method:'POST'});"
        "const res=await r.json();"
        "if(r.ok) alert('✅ '+res.message); else alert('❌ '+res.error);"
        "}catch(e){alert('❌ Errore: '+e);}}"
        "async function resetConfig(){if(confirm(\"Resettare ai valori di fabbrica?\")){await fetch(\"/api/config/reset\",{method:\"POST\"});location.reload();}}"
        "async function syncNTP(){"
        "try{const r=await fetch('/api/ntp/sync',{method:'POST'});"
        "const res=await r.json();"
        "if(r.ok) alert('✅ '+res.message); else alert('❌ '+res.message);"
        "}catch(e){alert('❌ Errore: '+e);}}"
        "function updateCurrentTime(){const now=new Date();document.getElementById('current_time').innerText=now.toLocaleString('it-IT');}"
        "document.getElementById('ntp_en').addEventListener('change',function(){if(this.checked){syncNTP();}});"
        "document.getElementById('remote_log_broadcast').addEventListener('change',function(){"
        "const ipField=document.getElementById('remote_log_ip');"
        "if(!ipField) return;"
        "ipField.disabled=this.checked;"
        "if(this.checked){ipField.value='255.255.255.255';}else{ipField.value='';}"
        "});"
        "setInterval(updateCurrentTime,1000);"
        "function ensureFactorySections(){"
        "if(!window.__showFactoryPasswordSection) return;"
        "if(document.getElementById('boot_pwd_current')) return;"
        "const form=document.getElementById('configForm');"
        "const nameSection=form?form.querySelector('.section'):null;"
        "if(!nameSection) return;"
        "const pwd=document.createElement('div');"
        "pwd.className='section';"
        "pwd.innerHTML=\"<h2>🔐 Password boot/emulatore</h2><p style='margin:8px 0 12px 0;color:#566573;'>Modifica password richiesta per /emulator e reboot FACTORY.</p><div class='form-group'><label>Password attuale</label><input type='password' id='boot_pwd_current' placeholder='password attuale'></div><div class='form-group'><label>Nuova password</label><input type='password' id='boot_pwd_new' placeholder='min 4 caratteri'></div><div class='form-group'><label>Conferma nuova password</label><input type='password' id='boot_pwd_confirm' placeholder='ripeti password'></div><button type='button' onclick='saveBootPassword()' style='background:#8e44ad;'>💾 Salva password boot</button>\";"
        "nameSection.insertAdjacentElement('afterend',pwd);"
        "const prg=document.createElement('div');"
        "prg.className='section';"
        "prg.style.cssText='background:#eef6ff;border-left:5px solid #1f6feb;';"
        "prg.innerHTML=\"<h2 style='margin-top:0;'>📊 Tabella Programmi</h2><p style='margin:10px 0 14px 0;'>Gestione prezzi, durata e relay per programma.</p><a href='/config/programs' style='display:inline-block;padding:10px 14px;background:#1f6feb;color:#fff;text-decoration:none;border-radius:6px;font-weight:bold;'>Apri editor programmi</a>\";"
        "pwd.insertAdjacentElement('afterend',prg);"
        "}"
        "ensureFactorySections();"
        "window.__configOriginalLang='it';"
        "window.addEventListener('load',loadConfig);"
        "function indexI18nRecords(records){"
        "const byScoped={};const byKey={};"
        "if(!Array.isArray(records)) return {byScoped,byKey};"
        "for(const item of records){"
        "if(!item||typeof item!=='object') continue;"
        "const scope=(item.scope!=null)?String(item.scope):'';"
        "const key=(item.key!=null)?String(item.key):'';"
        "const text=(item.text!=null)?String(item.text):'';"
        "if(!scope||!key) continue;"
        "const sk=scope+'.'+key;"
        "byScoped[sk]=text;"
        "if(byKey[key]===undefined) byKey[key]=text;"
        "}"
        "return {byScoped,byKey};"
        "}"
        "function buildI18nTableFromRecords(targetRecords, baseRecords){"
        "const table={};"
        "const t=indexI18nRecords(targetRecords);"
        "const b=indexI18nRecords(baseRecords);"
        "for(const sk in t.byScoped){"
        "const targetText=t.byScoped[sk];"
        "table[sk]=targetText;"
        "const key=sk.includes('.')?sk.substring(sk.indexOf('.')+1):sk;"
        "if(table[key]===undefined){const tk=t.byKey[key];table[key]=(tk!==undefined?tk:targetText);}"
        "const baseText=b.byScoped[sk];"
        "if(baseText&&baseText!==targetText){table[baseText]=targetText;const bt=baseText.trim();if(bt&&bt!==baseText)table[bt]=targetText;}"
        "}"
        "return table;"
        "}"
        "async function reloadLanguageTables(lang){"
        "const target=(lang?String(lang):'it').toLowerCase();"
        "const r=await fetch('/api/ui/texts?lang='+encodeURIComponent(target));"
        "if(!r.ok) throw new Error('HTTP '+r.status);"
        "const data=await r.json();"
        "let baseRecords=[];"
        "try{const rb=await fetch('/api/ui/texts?lang=it');if(rb.ok){const db=await rb.json();if(Array.isArray(db.records)) baseRecords=db.records;}}catch(_e){}"
        "const table=buildI18nTableFromRecords(data.records,baseRecords);"
        "if(window.uiI18n){window.uiI18n.language=target;window.uiI18n.table=table;if(typeof window.uiI18n.apply==='function'){window.uiI18n.apply(document.body);}}"
        "}"
        "async function loadUiLanguages(selectedLang){"
        "const uiLangEl=document.getElementById('ui_language');"
        "if(!uiLangEl) return;"
        "const target=(selectedLang?String(selectedLang):'it').toLowerCase();"
        "let list=[];"
        "try{const r=await fetch('/api/ui/languages');if(r.ok){const data=await r.json();if(Array.isArray(data.languages))list=data.languages;}}catch(e){console.warn('ui languages fetch failed',e);}"
        "if(!Array.isArray(list)||!list.length){list=[{code:'it',label:'Italiano (IT)'}];}"
        "uiLangEl.innerHTML='';"
        "const seen={};"
        "for(const item of list){if(!item||!item.code)continue;const code=String(item.code).toLowerCase();if(seen[code])continue;seen[code]=true;const opt=document.createElement('option');opt.value=code;opt.textContent=item.label?String(item.label):code.toUpperCase();uiLangEl.appendChild(opt);}"
        "if(!seen.it){const opt=document.createElement('option');opt.value='it';opt.textContent='Italiano (IT)';uiLangEl.appendChild(opt);}"
        "uiLangEl.value=target;"
        "if(!uiLangEl.value)uiLangEl.value='it';"
        "}"
        "async function loadConfig(){"
        "ensureFactorySections();"
        "try{const r=await fetch('/api/config');if(!r.ok)throw new Error('HTTP '+r.status);"
        "const c=await r.json();"
        "document.getElementById('dev_name').value=c.device_name || '';"
        "const uiLangEl=document.getElementById('ui_language');"
        "if(uiLangEl){const lang=(c.ui&&c.ui.language)?String(c.ui.language).toLowerCase():'it';await loadUiLanguages(lang);window.__configOriginalLang=lang;}"
        "document.getElementById('eth_en').checked=c.eth.enabled;"
        "document.getElementById('eth_dhcp').checked=c.eth.dhcp_enabled;"
        "document.getElementById('eth_ip').value=c.eth.ip;"
        "document.getElementById('eth_subnet').value=c.eth.subnet;"
        "document.getElementById('eth_gateway').value=c.eth.gateway;"
        "document.getElementById('wifi_en').checked=c.wifi.sta_enabled;"
        "document.getElementById('wifi_dhcp').checked=c.wifi.dhcp_enabled;"
        "document.getElementById('wifi_ssid').value=c.wifi.ssid;"
        "document.getElementById('wifi_pwd').value=c.wifi.password;"
        "document.getElementById('ntp_en').checked=c.ntp_enabled;"
        "document.getElementById('ntp_server1').value=c.ntp.server1;"
        "document.getElementById('ntp_server2').value=c.ntp.server2;"
        "document.getElementById('ntp_timezone_offset').value=c.ntp.timezone_offset;"
        "document.getElementById('server_en').checked = (c.server && c.server.enabled) ? true : false;"
        "document.getElementById('server_url').value = (c.server && c.server.url) ? c.server.url : 'http://195.231.69.227:5556/';"
        "document.getElementById('server_serial').value = (c.server && c.server.serial) ? c.server.serial : 'AD-34-DFG-333';"
        "document.getElementById('server_password').value = (c.server && c.server.password) ? c.server.password : 'c1ef6429c5e0f753ff24a114de6ee7d4';"
        "document.getElementById('remote_log_port').value=c.remote_log.server_port;"
        "document.getElementById('remote_log_broadcast').checked=c.remote_log.use_broadcast;"
        "document.getElementById('io_exp').checked=c.sensors.io_expander_enabled;"
        "document.getElementById('temp').checked=c.sensors.temperature_enabled;"
        "document.getElementById('led').checked=c.sensors.led_enabled;"
        "document.getElementById('led_count').value=c.sensors.led_count || 16;"
        "document.getElementById('rs232').checked=c.sensors.rs232_enabled;"
        "document.getElementById('rs485').checked=c.sensors.rs485_enabled;"
        "document.getElementById('mdb').checked=c.sensors.mdb_enabled;"
        "document.getElementById('sd_card').checked=c.sensors.sd_card_enabled;"
        "document.getElementById('pwm1').checked=c.sensors.pwm1_enabled;"
        "document.getElementById('pwm2').checked=c.sensors.pwm2_enabled;"
        "if (c.scanner) {"
        "    document.getElementById('scanner_en').checked = c.scanner.enabled;"
        "    document.getElementById('scanner_vid').value = c.scanner.vid ? ('0x' + c.scanner.vid.toString(16).padStart(4, '0').toUpperCase()) : '0x0000';"
        "    document.getElementById('scanner_pid').value = c.scanner.pid ? ('0x' + c.scanner.pid.toString(16).padStart(4, '0').toUpperCase()) : '0x0000';"
        "    document.getElementById('scanner_dual_pid').value = c.scanner.dual_pid ? ('0x' + c.scanner.dual_pid.toString(16).padStart(4, '0').toUpperCase()) : '0x0000';"
        "}"
        "if (typeof c.display.enabled === 'undefined') { try { const s = await (await fetch('/status')).json(); document.getElementById('display_en').checked = s.config && s.config.display ? !!s.config.display.enabled : true; } catch(e) { document.getElementById('display_en').checked = true; } } else { document.getElementById('display_en').checked = c.display.enabled; }" 
        "document.getElementById('lcd_bright').value=c.display.lcd_brightness;"
        "document.getElementById('bright_val').innerText=c.display.lcd_brightness;"
        "// ensure slider visual triggers the live update icon/text\n"
        "onDisplayBrightInput(c.display.lcd_brightness);"
        "document.getElementById('lcd_bright').disabled = !document.getElementById('display_en').checked;"
        "document.getElementById('display_en').addEventListener('change', function(){ document.getElementById('lcd_bright').disabled = !this.checked; });"
        "document.getElementById('rs232_baud').value=c.rs232.baud;"        "document.getElementById('rs232_bits').value=c.rs232.data_bits;"
        "document.getElementById('rs232_par').value=c.rs232.parity;"
        "document.getElementById('rs232_stop').value=c.rs232.stop_bits;"
        "document.getElementById('rs232_rx').value=c.rs232.rx_buf;"
        "document.getElementById('rs232_tx').value=c.rs232.tx_buf;"
        "document.getElementById('rs485_baud').value=c.rs485.baud;"
        "document.getElementById('rs485_bits').value=c.rs485.data_bits;"
        "document.getElementById('rs485_par').value=c.rs485.parity;"
        "document.getElementById('rs485_stop').value=c.rs485.stop_bits;"
        "document.getElementById('rs485_rx').value=c.rs485.rx_buf;"
        "document.getElementById('rs485_tx').value=c.rs485.tx_buf;"
        "document.getElementById('mdb_baud').value=c.mdb_serial.baud;"
        "document.getElementById('mdb_rx').value=c.mdb_serial.rx_buf;"
        "document.getElementById('mdb_tx').value=c.mdb_serial.tx_buf;"
        "document.getElementById('g33_mode').value=c.gpios.gpio33.mode;"
        "document.getElementById('g33_state').checked=c.gpios.gpio33.state;"
        "if(c.remote_log.use_broadcast){const ipEl=document.getElementById('remote_log_ip'); if(ipEl){ipEl.disabled=true; ipEl.value='255.255.255.255';}}"
        "updateCurrentTime();"
        "}catch(e){console.error(e);ensureFactorySections();}"
        "}"
        "async function saveBootPassword(){"
        "const current=document.getElementById('boot_pwd_current');"
        "const next=document.getElementById('boot_pwd_new');"
        "const confirm=document.getElementById('boot_pwd_confirm');"
        "if(!current||!next||!confirm)return;"
        "if(next.value.length<4){alert('Password troppo corta (min 4)');return;}"
        "if(next.value!==confirm.value){alert('Conferma password non valida');return;}"
        "try{const r=await fetch('/api/security/password',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({current_password:current.value,new_password:next.value})});const t=await r.text();if(r.ok){alert('✅ Password aggiornata');current.value='';next.value='';confirm.value='';}else{alert('❌ '+(t||('HTTP '+r.status)));}}catch(e){alert('❌ Errore: '+e);}"
        "}"
        "document.getElementById('configForm').onsubmit=async function(e){e.preventDefault();"
        "const prevLang=(window.__configOriginalLang?String(window.__configOriginalLang):'it').toLowerCase();"
        "const newLang=(document.getElementById('ui_language').value?String(document.getElementById('ui_language').value):'it').toLowerCase();"
        "const langChanged=(prevLang!==newLang);"
        "if(langChanged&&!confirm('Confermi il cambio lingua? Verranno ricaricate le tabelle di traduzione.')) return;"
        "const cfg={"
        "device_name:document.getElementById('dev_name').value,"
        "ui:{language:document.getElementById('ui_language').value},"
        "eth:{enabled:document.getElementById('eth_en').checked,dhcp_enabled:document.getElementById('eth_dhcp').checked,ip:document.getElementById('eth_ip').value,subnet:document.getElementById('eth_subnet').value,gateway:document.getElementById('eth_gateway').value},"
        "wifi:{sta_enabled:document.getElementById('wifi_en').checked,dhcp_enabled:document.getElementById('wifi_dhcp').checked,ssid:document.getElementById('wifi_ssid').value,password:document.getElementById('wifi_pwd').value,ip:'',subnet:'',gateway:''},"
        "ntp_enabled:document.getElementById('ntp_en').checked,"
        "ntp:{server1:document.getElementById('ntp_server1').value,server2:document.getElementById('ntp_server2').value,timezone_offset:parseInt(document.getElementById('ntp_timezone_offset').value)},"
        "server:{enabled:document.getElementById('server_en').checked,url:document.getElementById('server_url').value,serial:document.getElementById('server_serial').value,password:document.getElementById('server_password').value},"
        "remote_log:{server_port:parseInt(document.getElementById('remote_log_port').value),use_broadcast:document.getElementById('remote_log_broadcast').checked},"
        "sensors:{io_expander_enabled:document.getElementById('io_exp').checked,temperature_enabled:document.getElementById('temp').checked,led_enabled:document.getElementById('led').checked,led_count:parseInt(document.getElementById('led_count').value),rs232_enabled:document.getElementById('rs232').checked,rs485_enabled:document.getElementById('rs485').checked,mdb_enabled:document.getElementById('mdb').checked,sd_card_enabled:document.getElementById('sd_card').checked,pwm1_enabled:document.getElementById('pwm1').checked,pwm2_enabled:document.getElementById('pwm2').checked},"
"scanner:{enabled:document.getElementById('scanner_en').checked,vid:document.getElementById('scanner_vid').value,pid:document.getElementById('scanner_pid').value,dual_pid:document.getElementById('scanner_dual_pid').value},"
        "display:{enabled:document.getElementById('display_en').checked,lcd_brightness:parseInt(document.getElementById('lcd_bright').value)},"
        "rs232:{baud:parseInt(document.getElementById('rs232_baud').value),data_bits:parseInt(document.getElementById('rs232_bits').value),parity:parseInt(document.getElementById('rs232_par').value),stop_bits:parseInt(document.getElementById('rs232_stop').value),rx_buf:parseInt(document.getElementById('rs232_rx').value),tx_buf:parseInt(document.getElementById('rs232_tx').value)},"
        "rs485:{baud:parseInt(document.getElementById('rs485_baud').value),data_bits:parseInt(document.getElementById('rs485_bits').value),parity:parseInt(document.getElementById('rs485_par').value),stop_bits:parseInt(document.getElementById('rs485_stop').value),rx_buf:parseInt(document.getElementById('rs485_rx').value),tx_buf:parseInt(document.getElementById('rs485_tx').value)},"
        "mdb_serial:{baud:parseInt(document.getElementById('mdb_baud').value),data_bits:8,parity:0,stop_bits:1,rx_buf:parseInt(document.getElementById('mdb_rx').value),tx_buf:parseInt(document.getElementById('mdb_tx').value)},"
        "gpios:{"
        "gpio33:{mode:parseInt(document.getElementById('g33_mode').value),state:document.getElementById('g33_state').checked}}"
        "};"
        "const r=await fetch('/api/config/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(cfg)});"
        "if(r.ok){"
        "try{if(langChanged){await reloadLanguageTables(newLang);window.__configOriginalLang=newLang;}}catch(err){console.warn('reloadLanguageTables failed',err);}"
        "if(langChanged){alert('✅ Configurazione salvata. Lingua aggiornata!');const u=new URL(window.location.href);u.pathname='/config';u.searchParams.set('lang',newLang);u.searchParams.set('_ts',String(Date.now()));window.location.replace(u.toString());return;}"
        "alert('✅ Configurazione salvata!');"
        "} else alert('❌ Errore durante il salvataggio!');"
        "}"

        "// Rendiamo collassabili tutte le sezioni: init robusta (immediata + fallback su load)\n"
        "function initCollapsibleSections(){\n"
        "  const sections = Array.from(document.querySelectorAll('.section')).filter(s => s.querySelector('h2'));\n"
        "  sections.forEach((s, idx) => {\n"
        "    if (idx === 0) s.classList.remove('collapsed'); else s.classList.add('collapsed');\n"
        "    const h = s.querySelector('h2'); if(!h) return;\n"
        "    if (h.dataset.collapsibleReady === '1') return;\n"
        "    h.dataset.collapsibleReady = '1';\n"
        "    h.setAttribute('role', 'button');\n"
        "    h.setAttribute('tabindex', '0');\n"
        "    if (h.hasAttribute('onclick')) { h.removeAttribute('onclick'); }\n"
        "    if (!h.querySelector('.section-toggle-icon')) {\n"
        "      const ic = document.createElement('span'); ic.className = 'section-toggle-icon'; ic.innerText = s.classList.contains('collapsed') ? '▸' : '▾'; h.appendChild(ic);\n"
        "    } else {\n"
        "      const ic = h.querySelector('.section-toggle-icon'); ic.innerText = s.classList.contains('collapsed') ? '▸' : '▾';\n"
        "    }\n"
        "    const toggle = () => {\n"
        "      s.classList.toggle('collapsed');\n"
        "      const ic2 = h.querySelector('.section-toggle-icon');\n"
        "      if (ic2) ic2.innerText = s.classList.contains('collapsed') ? '▸' : '▾';\n"
        "    };\n"
        "    h.addEventListener('click', function(ev){ ev.preventDefault(); toggle(); });\n"
        "    h.addEventListener('keydown', function(ev){ if(ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); toggle(); } });\n"
        "  });\n"
        "}\n"
        "initCollapsibleSections();\n"
        "window.addEventListener('load', initCollapsibleSections);\n"
        "// Sperimentali: append USB Debug buttons near scanner fields (se presenti)\n"
        "try{(function(){const pidEl=document.getElementById('scanner_pid');"
        " if(pidEl){const btnEnum=document.createElement('button');"
        " btnEnum.textContent='USB:Enumerate (Sperimentali)';"
        " btnEnum.type='button';"
        " btnEnum.onclick=async function(){const r=await fetch('/api/debug/usb/enumerate'); const j=await r.json(); alert(JSON.stringify(j,null,2));};"
        " pidEl.parentNode.insertBefore(btnEnum,pidEl.nextSibling);"
        " const btnRestart=document.createElement('button');"
        " btnRestart.textContent='USB:Restart Host (Sperimentali)';"
        " btnRestart.type='button';"
        " btnRestart.onclick=async function(){const r=await fetch('/api/debug/usb/restart',{method:'POST'}); const txt=await r.text(); alert('Restart: '+txt);};"
        " pidEl.parentNode.insertBefore(btnRestart,btnEnum.nextSibling); }"
        " })();}catch(e){}"
        "</script></body></html>";
    
    httpd_resp_sendstr_chunk(req, body);
    if (config_read_only) {
        httpd_resp_sendstr_chunk(req,
            "<script>(function(){"
            "var box=document.createElement('div');"
            "box.style='margin:16px 20px;padding:10px 12px;background:#fff3cd;color:#856404;border:1px solid #ffeeba;border-radius:6px;font-weight:bold;';"
            "box.innerText='Modalità APP: configurazione in sola lettura';"
            "var c=document.querySelector('.container');if(c){c.insertBefore(box,c.firstChild);}"
            "var form=document.getElementById('configForm');"
            "if(form){form.addEventListener('submit',function(e){e.preventDefault();alert('Modalità APP: modifica configurazione non consentita.');});"
            "form.querySelectorAll('input,select,textarea,button').forEach(function(el){el.disabled=true;});}"
            "})();</script>");
    }
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Handler API GET /api/debug/usb/enumerate
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

// Sperimentali: POST /api/debug/usb/restart -> forces bsp_usb_host_stop + bsp_usb_host_start
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

// Handler API GET /api/config
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
    cJSON_AddStringToObject(server, "serial", cfg->server.serial);
    cJSON_AddStringToObject(server, "password", cfg->server.password);
    cJSON_AddItemToObject(root, "server", server);
    
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors, "io_expander_enabled", cfg->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors, "temperature_enabled", cfg->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors, "led_enabled", cfg->sensors.led_enabled);
    cJSON_AddNumberToObject(sensors, "led_count", cfg->sensors.led_count);
    cJSON_AddBoolToObject(sensors, "rs232_enabled", cfg->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors, "rs485_enabled", cfg->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors, "mdb_enabled", cfg->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors, "sd_card_enabled", cfg->sensors.sd_card_enabled);
    cJSON_AddBoolToObject(sensors, "pwm1_enabled", cfg->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors, "pwm2_enabled", cfg->sensors.pwm2_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors);

    cJSON *display = cJSON_CreateObject();
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

    // UI multilingua
    cJSON *ui = cJSON_CreateObject();
    cJSON_AddStringToObject(ui, "language", cfg->ui.language);
    cJSON_AddStringToObject(ui, "storage", "spiffs");
    cJSON_AddItemToObject(root, "ui", ui);
    cJSON_AddStringToObject(root, "ui_language", cfg->ui.language);
    
    char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler API GET /api/ui/texts
esp_err_t api_ui_texts_get(httpd_req_t *req)
{
    device_config_t *cfg = device_config_get();
    char language[8] = {0};
    strncpy(language, cfg->ui.language, sizeof(language) - 1);

    char query[128] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char lang_q[8] = {0};
        if (httpd_query_key_value(query, "lang", lang_q, sizeof(lang_q)) == ESP_OK && strlen(lang_q) == 2) {
            strncpy(language, lang_q, sizeof(language) - 1);
        }
    }

    char *records_json = device_config_get_ui_texts_records_json(language);
    if (!records_json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"i18n_load_failed\"}", -1);
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "language", language);
    cJSON_AddStringToObject(root, "storage", "spiffs");

    char path[64] = {0};
    snprintf(path, sizeof(path), "/spiffs/i18n_%s.json", language);
    cJSON_AddStringToObject(root, "file", path);

    cJSON *records = cJSON_Parse(records_json);
    free(records_json);
    if (records && cJSON_IsArray(records)) {
        cJSON_AddItemToObject(root, "records", records);
    } else {
        if (records) {
            cJSON_Delete(records);
        }
        cJSON_AddItemToObject(root, "records", cJSON_CreateArray());
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"json_build_failed\"}", -1);
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

// Handler API GET /api/ui/languages
esp_err_t api_ui_languages_get(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *languages = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "languages", languages);

    bool has_it = false;
    DIR *dir = opendir("/spiffs");
    if (dir) {
        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL) {
            char lang[8] = {0};
            if (!web_ui_extract_lang_from_filename(entry->d_name, lang, sizeof(lang))) {
                continue;
            }

            bool duplicate = false;
            int count = cJSON_GetArraySize(languages);
            for (int i = 0; i < count; ++i) {
                cJSON *it = cJSON_GetArrayItem(languages, i);
                cJSON *code = cJSON_GetObjectItem(it, "code");
                if (cJSON_IsString(code) && code->valuestring && strcmp(code->valuestring, lang) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                continue;
            }

            char label[32] = {0};
            web_ui_lang_label_from_code(lang, label, sizeof(label));

            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "code", lang);
            cJSON_AddStringToObject(item, "label", label);

            char file_path[64] = {0};
            snprintf(file_path, sizeof(file_path), "/spiffs/i18n_%s.json", lang);
            cJSON_AddStringToObject(item, "file", file_path);
            cJSON_AddItemToArray(languages, item);

            if (strcmp(lang, "it") == 0) {
                has_it = true;
            }
        }
        closedir(dir);
    } else {
        ESP_LOGW(TAG, "Impossibile aprire /spiffs per scansione lingue");
    }

    if (!has_it) {
        cJSON *it_item = cJSON_CreateObject();
        cJSON_AddStringToObject(it_item, "code", "it");
        cJSON_AddStringToObject(it_item, "label", "Italiano (IT)");
        cJSON_AddStringToObject(it_item, "file", "/spiffs/i18n_it.json");
        cJSON_AddItemToArray(languages, it_item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"json_build_failed\"}", -1);
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

// Handler API POST /api/config/backup
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
    
    cJSON *sensors = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors, "io_expander_enabled", cfg->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors, "temperature_enabled", cfg->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors, "led_enabled", cfg->sensors.led_enabled);
    cJSON_AddNumberToObject(sensors, "led_count", cfg->sensors.led_count);
    cJSON_AddBoolToObject(sensors, "rs232_enabled", cfg->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors, "rs485_enabled", cfg->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors, "mdb_enabled", cfg->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors, "sd_card_enabled", cfg->sensors.sd_card_enabled);
    cJSON_AddBoolToObject(sensors, "pwm1_enabled", cfg->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors, "pwm2_enabled", cfg->sensors.pwm2_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors);

    cJSON *display = cJSON_CreateObject();
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

    // UI multilingua
    cJSON *ui = cJSON_CreateObject();
    cJSON_AddStringToObject(ui, "language", cfg->ui.language);
    cJSON_AddStringToObject(ui, "storage", "spiffs");
    cJSON_AddItemToObject(root, "ui", ui);
    cJSON_AddStringToObject(root, "ui_language", cfg->ui.language);

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

// Handler API POST /api/config/save
esp_err_t api_config_save(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Configurazione in sola lettura in modalità APP", -1);
    }

    ESP_LOGI(TAG, "[C] Ricevuta richiesta salvataggio configurazione");
    
    char buf[4096] = {0};
    httpd_req_recv(req, buf, sizeof(buf)-1);
    
    // Log del JSON ricevuto per debugging
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
        cJSON *serial = cJSON_GetObjectItem(server_obj, "serial");
        if (serial && serial->valuestring) strncpy(cfg->server.serial, serial->valuestring, sizeof(cfg->server.serial)-1);
        cJSON *password = cJSON_GetObjectItem(server_obj, "password");
        if (password && password->valuestring) strncpy(cfg->server.password, password->valuestring, sizeof(cfg->server.password)-1);
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
        cfg->sensors.pwm1_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm1_enabled"));
        cfg->sensors.pwm2_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm2_enabled"));
        cfg->sensors.sd_card_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "sd_card_enabled"));

        // Se i LED sono abilitati e il numero è cambiato, re-inizializziamo la stripe
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
            /* In build headless forzata ignoriamo il valore ricevuto dalla pagina /config */
            cfg->display.enabled = false;
            ESP_LOGW(TAG, "[C] FORCE_VIDEO_DISABLED attivo: richiesta display.enabled ignorata");
#else
            cfg->display.enabled = cJSON_IsTrue(enabled);
            ESP_LOGI(TAG, "[C] Display enabled set to %d", cfg->display.enabled);
#endif
        }
        cJSON *bright = cJSON_GetObjectItem(display_obj, "lcd_brightness");
        if (bright) {
            cfg->display.lcd_brightness = (uint8_t)bright->valueint;
            // Applica immediatamente la luminosità del display SOLO se display abilitato
            if (cfg->display.enabled) {
                bsp_display_brightness_set(cfg->display.lcd_brightness);
                /* Registra nel log SOLO il valore persistito (evita i log dei valori live inviati durante lo slider) */
                char __msg_bright[64];
                snprintf(__msg_bright, sizeof(__msg_bright), "Luminosità persistita: %u%%", cfg->display.lcd_brightness);
                web_ui_add_log("INFO", "DISPLAY", __msg_bright);
            } else {
                web_ui_add_log("WARN", "DISPLAY", "Display disabilitato: ignorata richiesta luminosità");
            }
        }
    }

    cJSON *gpios_obj = cJSON_GetObjectItem(root, "gpios");
    if (gpios_obj) {
        cJSON *g33 = cJSON_GetObjectItem(gpios_obj, "gpio33");
        if (g33) {
            cfg->gpios.gpio33.mode = (device_gpio_cfg_mode_t)cJSON_GetNumberValue(cJSON_GetObjectItem(g33, "mode"));
            cfg->gpios.gpio33.initial_state = cJSON_IsTrue(cJSON_GetObjectItem(g33, "state"));
        }
    }

    // RS232 cfg
    cJSON *rs232_obj = cJSON_GetObjectItem(root, "rs232");
    if (rs232_obj) {
        cfg->rs232.baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "baud"));
        cfg->rs232.data_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "data_bits"));
        cfg->rs232.parity = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "parity"));
        cfg->rs232.stop_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "stop_bits"));
        cfg->rs232.rx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "rx_buf"));
        cfg->rs232.tx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(rs232_obj, "tx_buf"));
    }
    // RS485 cfg
    cJSON *rs485_obj = cJSON_GetObjectItem(root, "rs485");
    if (rs485_obj) {
        cfg->rs485.baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "baud"));
        cfg->rs485.data_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "data_bits"));
        cfg->rs485.parity = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "parity"));
        cfg->rs485.stop_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "stop_bits"));
        cfg->rs485.rx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "rx_buf"));
        cfg->rs485.tx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(rs485_obj, "tx_buf"));
    }
    // MDB cfg
    cJSON *mdb_s_obj = cJSON_GetObjectItem(root, "mdb_serial");
    if (mdb_s_obj) {
        cfg->mdb_serial.baud_rate = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "baud"));
        cfg->mdb_serial.data_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "data_bits"));
        cfg->mdb_serial.parity = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "parity"));
        cfg->mdb_serial.stop_bits = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "stop_bits"));
        cfg->mdb_serial.rx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "rx_buf"));
        cfg->mdb_serial.tx_buf_size = cJSON_GetNumberValue(cJSON_GetObjectItem(mdb_s_obj, "tx_buf"));
    }

    // Remote logging configuration
    cJSON *remote_log_obj = cJSON_GetObjectItem(root, "remote_log");
    if (remote_log_obj) {
        cJSON *server_port = cJSON_GetObjectItem(remote_log_obj, "server_port");
        if (server_port) {
            cfg->remote_log.server_port = (uint16_t)server_port->valueint;
        }
        cfg->remote_log.use_broadcast = cJSON_IsTrue(cJSON_GetObjectItem(remote_log_obj, "use_broadcast"));
        ESP_LOGI(TAG, "[C] Remote logging config: port=%d, broadcast=%d",
                cfg->remote_log.server_port, cfg->remote_log.use_broadcast);
    }

    // Scanner configuration (API save)
    cJSON *scanner_obj = cJSON_GetObjectItem(root, "scanner");
    if (scanner_obj) {
        cfg->scanner.enabled = cJSON_IsTrue(cJSON_GetObjectItem(scanner_obj, "enabled"));
        cJSON *sv = cJSON_GetObjectItem(scanner_obj, "vid");
        if (sv) {
            if (cJSON_IsNumber(sv)) cfg->scanner.vid = (uint16_t)sv->valueint;
            else if (cJSON_IsString(sv) && sv->valuestring) cfg->scanner.vid = (uint16_t)strtoul(sv->valuestring, NULL, 0);
        }
        cJSON *sp = cJSON_GetObjectItem(scanner_obj, "pid");
        if (sp) {
            if (cJSON_IsNumber(sp)) cfg->scanner.pid = (uint16_t)sp->valueint;
            else if (cJSON_IsString(sp) && sp->valuestring) cfg->scanner.pid = (uint16_t)strtoul(sp->valuestring, NULL, 0);
        }
        cJSON *sdual = cJSON_GetObjectItem(scanner_obj, "dual_pid");
        if (sdual) {
            if (cJSON_IsNumber(sdual)) cfg->scanner.dual_pid = (uint16_t)sdual->valueint;
            else if (cJSON_IsString(sdual) && sdual->valuestring) cfg->scanner.dual_pid = (uint16_t)strtoul(sdual->valuestring, NULL, 0);
        }
    }

    // UI multilingua
    cJSON *ui_obj = cJSON_GetObjectItem(root, "ui");
    if (ui_obj) {
        cJSON *language = cJSON_GetObjectItem(ui_obj, "language");
        if (language && cJSON_IsString(language) && language->valuestring) {
            strncpy(cfg->ui.language, language->valuestring, sizeof(cfg->ui.language) - 1);
        }

        cJSON *records = cJSON_GetObjectItem(ui_obj, "records");
        if (!records) {
            records = cJSON_GetObjectItem(ui_obj, "texts");
        }
        if (records && cJSON_IsArray(records)) {
            char *records_json = cJSON_PrintUnformatted(records);
            if (records_json) {
                device_config_set_ui_texts_records_json(cfg->ui.language, records_json);
                web_ui_i18n_cache_invalidate();
                free(records_json);
            }
        }
    }

    cJSON *ui_lang_flat = cJSON_GetObjectItem(root, "ui_language");
    if (ui_lang_flat && cJSON_IsString(ui_lang_flat) && ui_lang_flat->valuestring) {
        strncpy(cfg->ui.language, ui_lang_flat->valuestring, sizeof(cfg->ui.language) - 1);
    }
    
    cfg->updated = true;
    device_config_save(cfg);
    // Applica stati task che dipendono dalla configurazione (es. display on/off)
    tasks_apply_n_run();
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}




// Handler API GET /api/tasks
esp_err_t api_tasks_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /api/tasks (Lettura da SPIFFS)");

    FILE *f = fopen("/spiffs/tasks.json", "r");
    if (!f) {
        ESP_LOGE(TAG, "[C] Impossibile aprire tasks.json");
        httpd_resp_set_status(req, "500");
        return httpd_resp_send(req, "{\"error\":\"File non trovato\"}", -1);
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 32768) {
        fclose(f);
        httpd_resp_set_status(req, "500");
        return httpd_resp_send(req, "{\"error\":\"Dimensione file non valida\"}", -1);
    }

    char *buf = malloc((size_t)file_size + 1);
    if (!buf) {
        fclose(f);
        httpd_resp_set_status(req, "500");
        return httpd_resp_send(req, "{\"error\":\"Out of memory\"}", -1);
    }
    fread(buf, 1, (size_t)file_size, f);
    fclose(f);
    buf[file_size] = '\0';

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, buf, (ssize_t)file_size);
    free(buf);
    return ret;
}

// Handler API POST /api/tasks/save
esp_err_t api_tasks_save(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/tasks/save");

    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 16384) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"Content-Length non valido\"}", -1);
    }

    char *buf = calloc(1, total_len + 1);
    if (!buf) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"Out of memory\"}", -1);
    }

    int received = 0;
    while (received < total_len) {
        int r = httpd_req_recv(req, buf + received, total_len - received);
        if (r <= 0) {
            free(buf);
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "{\"error\":\"Ricezione interrotta\"}", -1);
        }
        received += r;
    }
    buf[received] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsArray(root)) {
        ESP_LOGE(TAG, "[C] tasks/save: JSON non valido");
        if (root) cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "{\"error\":\"JSON non valido\"}", -1);
    }
    
    // Serializza su file JSON con chiavi compatte (n/s/p/c/m/w/k + enum numerici)
    int count = cJSON_GetArraySize(root);
    cJSON *compact = cJSON_CreateArray();
    cJSON *item;
    cJSON_ArrayForEach(item, root) {
        cJSON *jn = cJSON_GetObjectItem(item, "name");
        cJSON *js = cJSON_GetObjectItem(item, "state");
        cJSON *jp = cJSON_GetObjectItem(item, "priority");
        cJSON *jc = cJSON_GetObjectItem(item, "core");
        cJSON *jm = cJSON_GetObjectItem(item, "period_ms");
        cJSON *jw = cJSON_GetObjectItem(item, "stack_words");
        cJSON *jk = cJSON_GetObjectItem(item, "stack_caps");
        if (!jn) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "n", jn->valuestring ? jn->valuestring : "");
        /* state: JS invia già int (0/1/2), ma accettiamo anche stringa per compatibilità */
        int sv = 0;
        if (js && cJSON_IsNumber(js)) { sv = js->valueint; }
        else if (js && cJSON_IsString(js)) {
            if (strcmp(js->valuestring, "run") == 0) sv = 1;
            else if (strcmp(js->valuestring, "pause") == 0) sv = 2;
        }
        cJSON_AddNumberToObject(o, "s", sv);
        cJSON_AddNumberToObject(o, "p", jp ? jp->valueint : 4);
        cJSON_AddNumberToObject(o, "c", jc ? jc->valueint : 0);
        cJSON_AddNumberToObject(o, "m", jm ? jm->valueint : 0);
        cJSON_AddNumberToObject(o, "w", jw ? jw->valueint : 2048);
        int kv = 0;
        if (jk && cJSON_IsNumber(jk)) { kv = jk->valueint; }
        else if (jk && cJSON_IsString(jk) && strcmp(jk->valuestring, "internal") == 0) { kv = 1; }
        cJSON_AddNumberToObject(o, "k", kv);
        cJSON_AddItemToArray(compact, o);
    }
    cJSON_Delete(root);
    char *json_out = cJSON_PrintUnformatted(compact);
    cJSON_Delete(compact);
    if (!json_out) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"JSON encode failed\"}", -1);
    }

    FILE *f = fopen("/spiffs/tasks.json", "w");
    if (!f) {
        ESP_LOGE(TAG, "[C] Impossibile aprire tasks.json per scrittura");
        free(json_out);
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "{\"error\":\"Impossibile scrivere il file\"}", -1);
    }
    fputs(json_out, f);
    fclose(f);
    free(json_out);

    ESP_LOGI(TAG, "[C] Tasks salvate: %d voci", count);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
}

// Handler API POST /api/tasks/apply
esp_err_t api_tasks_apply(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/tasks/apply");
    
    // Ricarica la configurazione dal file CSV salvato su SPIFFS
    tasks_load_config("/spiffs/tasks.json");
    
    // Applica i cambiamenti ai task FreeRTOS
    tasks_apply_n_run();
    
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\",\"message\":\"Task applicati con successo\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

// Handler pagina test legacy rimosso (spostato in web_ui_test_pages.c)


// Handler API POST /api/config/reset
esp_err_t api_config_reset(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Configurazione in sola lettura in modalità APP", -1);
    }

    ESP_LOGI(TAG, "[C] POST /api/config/reset");
    device_config_reset_defaults();
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

// Handler API POST /api/ntp/sync
esp_err_t api_ntp_sync(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Configurazione in sola lettura in modalità APP", -1);
    }

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

esp_err_t api_debug_crash(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Endpoint disponibile solo in Factory", -1);
    }

    ESP_LOGW(TAG, "[C] POST /api/debug/crash (factory)");
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\",\"message\":\"Crash intenzionale in corso\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));

    vTaskDelay(pdMS_TO_TICKS(120));
    volatile uint32_t *bad_ptr = (volatile uint32_t *)0x0;
    *bad_ptr = 0xC0DEFACE;
    return ESP_OK;
}

esp_err_t api_debug_restore(httpd_req_t *req)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running || running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Restore disponibile solo in APP", -1);
    }

    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                              ESP_PARTITION_SUBTYPE_APP_FACTORY,
                                                              NULL);
    if (!factory) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Partizione Factory non trovata", -1);
    }

    esp_partition_subtype_t target_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    if (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
        target_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
    } else if (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) {
        target_subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
    }

    const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                             target_subtype,
                                                             NULL);
    if (!target) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Partizione OTA target non trovata", -1);
    }

    if (target->size < factory->size) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Dimensione OTA target insufficiente", -1);
    }

    ESP_LOGW(TAG, "[C] POST /api/debug/restore running=%s target=%s source=%s", running->label, target->label, factory->label);

    esp_err_t err = esp_partition_erase_range(target, 0, target->size);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Erase OTA target fallita", -1);
    }

    uint8_t *buf = malloc(4096);
    if (!buf) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "Memoria insufficiente per restore", -1);
    }

    size_t copied = 0;
    while (copied < factory->size) {
        size_t chunk = factory->size - copied;
        if (chunk > 4096) {
            chunk = 4096;
        }

        err = esp_partition_read(factory, copied, buf, chunk);
        if (err != ESP_OK) {
            free(buf);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_send(req, "Lettura Factory fallita", -1);
        }

        err = esp_partition_write(target, copied, buf, chunk);
        if (err != ESP_OK) {
            free(buf);
            httpd_resp_set_status(req, "500 Internal Server Error");
            return httpd_resp_send(req, "Scrittura OTA target fallita", -1);
        }

        copied += chunk;
    }

    free(buf);

    char resp[160];
    snprintf(resp,
             sizeof(resp),
             "Restore completato: Factory (%s) copiata in %s (%u bytes)",
             factory->label,
             target->label,
             (unsigned)factory->size);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, resp, -1);
}

#if DNA_SYS_MONITOR == 0
/* GET /api/sysinfo — heap DRAM/SPIRAM + CPU usage per core */
static esp_err_t sysinfo_get_handler(httpd_req_t *req)
{
    size_t dram_free  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t dram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t spi_free   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t spi_total  = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    long long uptime_s = (long long)(esp_timer_get_time() / 1000000LL);

    int core0_pct = -1, core1_pct = -1;
    bool cpu_available = false;

#ifdef CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    cpu_available = true;
    static uint32_t s_idle0 = 0, s_idle1 = 0, s_total = 0;
    UBaseType_t ntasks = uxTaskGetNumberOfTasks();
    TaskStatus_t *tbuf = heap_caps_malloc(ntasks * sizeof(TaskStatus_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (tbuf) {
        uint32_t total_time = 0;
        UBaseType_t got = uxTaskGetSystemState(tbuf, ntasks, &total_time);
        uint32_t idle0 = 0, idle1 = 0;
        for (UBaseType_t i = 0; i < got; i++) {
            if (strcmp(tbuf[i].pcTaskName, "IDLE0") == 0) idle0 = tbuf[i].ulRunTimeCounter;
            if (strcmp(tbuf[i].pcTaskName, "IDLE1") == 0) idle1 = tbuf[i].ulRunTimeCounter;
        }
        heap_caps_free(tbuf);
        if (s_total != 0 && total_time > s_total) {
            uint32_t dt = total_time - s_total;
            uint32_t d0 = (idle0 >= s_idle0) ? (idle0 - s_idle0) : 0;
            uint32_t d1 = (idle1 >= s_idle1) ? (idle1 - s_idle1) : 0;
            /* total_time è un contatore wall-clock (esp_timer µs): ogni core
             * accumula indipendentemente fino a dt µs — formula semplice. */
            core0_pct = (int)(100 - (int64_t)d0 * 100 / dt);
            core1_pct = (int)(100 - (int64_t)d1 * 100 / dt);
            if (core0_pct < 0)   core0_pct = 0;
            if (core1_pct < 0)   core1_pct = 0;
            if (core0_pct > 100) core0_pct = 100;
            if (core1_pct > 100) core1_pct = 100;
        }
        s_idle0 = idle0;  s_idle1 = idle1;  s_total = total_time;
    }
#endif /* CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS */

    char resp[256];
    snprintf(resp, sizeof(resp),
        "{\"heap\":{\"dram_free\":%zu,\"dram_total\":%zu"
        ",\"spiram_free\":%zu,\"spiram_total\":%zu}"
        ",\"cpu\":{\"available\":%s,\"core0_pct\":%d,\"core1_pct\":%d}"
        ",\"uptime_s\":%lld}",
        dram_free, dram_total, spi_free, spi_total,
        cpu_available ? "true" : "false",
        core0_pct, core1_pct, uptime_s);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, -1);
}
#endif /* DNA_SYS_MONITOR == 0 */


esp_err_t web_ui_register_handlers(httpd_handle_t server)
{
    // Registrazione URI di sistema (ex-init.c)
    httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
    httpd_register_uri_handler(server, &uri_root);
    
    httpd_uri_t uri_logo = {.uri = "/logo.jpg", .method = HTTP_GET, .handler = logo_get_handler};
    httpd_register_uri_handler(server, &uri_logo);
    
    httpd_uri_t uri_status = {.uri = "/status", .method = HTTP_GET, .handler = status_get_handler};
    httpd_register_uri_handler(server, &uri_status);

#if DNA_SYS_MONITOR == 0
    httpd_uri_t uri_sysinfo = {.uri = "/api/sysinfo", .method = HTTP_GET, .handler = sysinfo_get_handler};
    httpd_register_uri_handler(server, &uri_sysinfo);
#endif

    httpd_uri_t uri_ota_get = {.uri = "/ota", .method = HTTP_GET, .handler = ota_get_handler};
    httpd_register_uri_handler(server, &uri_ota_get);
    
    httpd_uri_t uri_ota_post = {.uri = "/ota", .method = HTTP_POST, .handler = ota_post_handler};
    httpd_register_uri_handler(server, &uri_ota_post);

    httpd_uri_t uri_ota_upload = {.uri = "/ota/upload", .method = HTTP_POST, .handler = ota_upload_handler};
    httpd_register_uri_handler(server, &uri_ota_upload);

    // Registrazione URI Web UI (Pagine e API)
    httpd_uri_t uri_config = {.uri = "/config", .method = HTTP_GET, .handler = config_page_handler};
    httpd_register_uri_handler(server, &uri_config);
    
    httpd_uri_t uri_stats = {.uri = "/stats", .method = HTTP_GET, .handler = stats_page_handler};
    httpd_register_uri_handler(server, &uri_stats);

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_uri_t uri_programs = {.uri = "/config/programs", .method = HTTP_GET, .handler = programs_page_handler};
        httpd_register_uri_handler(server, &uri_programs);
    }
    
    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TASKS)) {
        httpd_uri_t uri_tasks = {.uri = "/tasks", .method = HTTP_GET, .handler = tasks_page_handler};
        httpd_register_uri_handler(server, &uri_tasks);
    }

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TEST)) {
        httpd_uri_t uri_test = {.uri = "/test", .method = HTTP_GET, .handler = test_page_handler};
        httpd_register_uri_handler(server, &uri_test);
    }

    httpd_uri_t uri_files = {.uri = "/files", .method = HTTP_GET, .handler = files_page_handler};
    httpd_register_uri_handler(server, &uri_files);

    httpd_uri_t uri_httpservices = {.uri = "/httpservices", .method = HTTP_GET, .handler = httpservices_page_handler};
    httpd_register_uri_handler(server, &uri_httpservices);
    
    httpd_uri_t uri_logs = {.uri = "/logs", .method = HTTP_GET, .handler = logs_page_handler};
    httpd_register_uri_handler(server, &uri_logs);

    httpd_uri_t uri_api_index = {.uri = "/api", .method = HTTP_GET, .handler = api_index_page_handler};
    httpd_register_uri_handler(server, &uri_api_index);
    ESP_LOGI(TAG, "Registered GET /api (index)");

    httpd_uri_t uri_api_index_slash = {.uri = "/api/", .method = HTTP_GET, .handler = api_index_page_handler};
    httpd_register_uri_handler(server, &uri_api_index_slash);
    ESP_LOGI(TAG, "Registered GET /api/ (index)");

    // API
    httpd_uri_t uri_api_get = {.uri = "/api/config", .method = HTTP_GET, .handler = api_config_get};
    httpd_register_uri_handler(server, &uri_api_get);

    httpd_uri_t uri_api_ui_texts = {.uri = "/api/ui/texts", .method = HTTP_GET, .handler = api_ui_texts_get};
    httpd_register_uri_handler(server, &uri_api_ui_texts);

    httpd_uri_t uri_api_ui_languages = {.uri = "/api/ui/languages", .method = HTTP_GET, .handler = api_ui_languages_get};
    httpd_register_uri_handler(server, &uri_api_ui_languages);
    
    httpd_uri_t uri_api_save = {.uri = "/api/config/save", .method = HTTP_POST, .handler = api_config_save};
    httpd_register_uri_handler(server, &uri_api_save);

    httpd_uri_t uri_api_backup = {.uri = "/api/config/backup", .method = HTTP_POST, .handler = api_config_backup};
    httpd_register_uri_handler(server, &uri_api_backup);
    
    httpd_uri_t uri_api_reset = {.uri = "/api/config/reset", .method = HTTP_POST, .handler = api_config_reset};
    httpd_register_uri_handler(server, &uri_api_reset);

    httpd_uri_t uri_api_ntp_sync = {.uri = "/api/ntp/sync", .method = HTTP_POST, .handler = api_ntp_sync};
    httpd_register_uri_handler(server, &uri_api_ntp_sync);

    httpd_uri_t uri_api_files_list = {.uri = "/api/files/list", .method = HTTP_GET, .handler = api_files_list_get};
    httpd_register_uri_handler(server, &uri_api_files_list);

    httpd_uri_t uri_api_files_upload = {.uri = "/api/files/upload", .method = HTTP_POST, .handler = api_files_upload_post};
    httpd_register_uri_handler(server, &uri_api_files_upload);

    httpd_uri_t uri_api_files_delete = {.uri = "/api/files/delete", .method = HTTP_POST, .handler = api_files_delete_post};
    httpd_register_uri_handler(server, &uri_api_files_delete);

    httpd_uri_t uri_api_files_download = {.uri = "/api/files/download", .method = HTTP_GET, .handler = api_files_download_get};
    httpd_register_uri_handler(server, &uri_api_files_download);

    httpd_uri_t uri_api_remote_files_list = {.uri = "/api/remote/files/list", .method = HTTP_GET, .handler = api_files_list_get};
    httpd_register_uri_handler(server, &uri_api_remote_files_list);

    httpd_uri_t uri_api_remote_files_upload = {.uri = "/api/remote/files/upload", .method = HTTP_POST, .handler = api_files_upload_post};
    httpd_register_uri_handler(server, &uri_api_remote_files_upload);

    httpd_uri_t uri_api_remote_files_delete = {.uri = "/api/remote/files/delete", .method = HTTP_POST, .handler = api_files_delete_post};
    httpd_register_uri_handler(server, &uri_api_remote_files_delete);

    httpd_uri_t uri_api_remote_files_download = {.uri = "/api/remote/files/download", .method = HTTP_GET, .handler = api_files_download_get};
    httpd_register_uri_handler(server, &uri_api_remote_files_download);

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TASKS)) {
        httpd_uri_t uri_api_tasks = {.uri = "/api/tasks", .method = HTTP_GET, .handler = api_tasks_get};
        httpd_register_uri_handler(server, &uri_api_tasks);

        httpd_uri_t uri_api_tasks_save = {.uri = "/api/tasks/save", .method = HTTP_POST, .handler = api_tasks_save};
        httpd_register_uri_handler(server, &uri_api_tasks_save);

        httpd_uri_t uri_api_tasks_apply = {.uri = "/api/tasks/apply", .method = HTTP_POST, .handler = api_tasks_apply};
        httpd_register_uri_handler(server, &uri_api_tasks_apply);
    }

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_TEST)) {
        httpd_uri_t uri_api_test = {.uri = "/api/test/*", .method = HTTP_POST, .handler = api_test_handler};
        httpd_register_uri_handler(server, &uri_api_test);
    }

    {
        httpd_uri_t uri_api_programs_get = {.uri = "/api/programs", .method = HTTP_GET, .handler = api_programs_get};
        httpd_register_uri_handler(server, &uri_api_programs_get);
    }

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_uri_t uri_api_programs_save = {.uri = "/api/programs/save", .method = HTTP_POST, .handler = api_programs_save};
        httpd_register_uri_handler(server, &uri_api_programs_save);

        httpd_uri_t uri_api_security_password = {.uri = "/api/security/password", .method = HTTP_POST, .handler = api_security_password};
        httpd_register_uri_handler(server, &uri_api_security_password);
    }

    httpd_uri_t uri_api_emulator_relay = {.uri = "/api/emulator/relay", .method = HTTP_POST, .handler = api_emulator_relay_control};
    httpd_register_uri_handler(server, &uri_api_emulator_relay);

    httpd_uri_t uri_api_emulator_coin = {.uri = "/api/emulator/coin", .method = HTTP_POST, .handler = api_emulator_coin_event};
    httpd_register_uri_handler(server, &uri_api_emulator_coin);

    httpd_uri_t uri_api_emulator_program_start = {.uri = "/api/emulator/program/start", .method = HTTP_POST, .handler = api_emulator_program_start};
    httpd_register_uri_handler(server, &uri_api_emulator_program_start);

    httpd_uri_t uri_api_emulator_program_stop = {.uri = "/api/emulator/program/stop", .method = HTTP_POST, .handler = api_emulator_program_stop};
    httpd_register_uri_handler(server, &uri_api_emulator_program_stop);

    httpd_uri_t uri_api_emulator_program_pause_toggle = {.uri = "/api/emulator/program/pause_toggle", .method = HTTP_POST, .handler = api_emulator_program_pause_toggle};
    httpd_register_uri_handler(server, &uri_api_emulator_program_pause_toggle);

    httpd_uri_t uri_api_emulator_fsm_messages = {.uri = "/api/emulator/fsm/messages", .method = HTTP_GET, .handler = api_emulator_fsm_messages_get};
    httpd_register_uri_handler(server, &uri_api_emulator_fsm_messages);
    
    httpd_uri_t uri_api_logs_get = {.uri = "/api/logs", .method = HTTP_GET, .handler = api_logs_get};
    httpd_register_uri_handler(server, &uri_api_logs_get);
    ESP_LOGI(TAG, "Registered GET /api/logs handler");

    httpd_uri_t uri_api_logs_receive = {.uri = "/api/logs/receive", .method = HTTP_POST, .handler = api_logs_receive};
    httpd_register_uri_handler(server, &uri_api_logs_receive);
    ESP_LOGI(TAG, "Registered POST /api/logs/receive handler");

    // API: lista livelli correnti per tag (usata dalla UI per costruire i combobox)
    httpd_uri_t uri_api_logs_levels = {.uri = "/api/logs/levels", .method = HTTP_GET, .handler = api_logs_levels_get};
    httpd_register_uri_handler(server, &uri_api_logs_levels);
    ESP_LOGI(TAG, "Registered GET /api/logs/levels handler");

    // API: imposta livello di log per un singolo tag
    httpd_uri_t uri_api_logs_set_level = {.uri = "/api/logs/level", .method = HTTP_POST, .handler = api_logs_set_level};
    httpd_register_uri_handler(server, &uri_api_logs_set_level);
    ESP_LOGI(TAG, "Registered POST /api/logs/level handler");

    httpd_uri_t uri_api_logs_options = {.uri = "/api/logs/*", .method = HTTP_OPTIONS, .handler = api_logs_options};
    httpd_register_uri_handler(server, &uri_api_logs_options);
    ESP_LOGI(TAG, "Registered OPTIONS /api/logs/* handler");

    // Register debug USB enumeration endpoint
    httpd_uri_t uri_api_debug_usb = {.uri = "/api/debug/usb/enumerate", .method = HTTP_GET, .handler = api_debug_usb_enumerate};
    httpd_register_uri_handler(server, &uri_api_debug_usb);
    ESP_LOGI(TAG, "Registered GET /api/debug/usb/enumerate handler");

    // Sperimentali: register POST /api/debug/usb/restart
    httpd_uri_t uri_api_debug_usb_restart = {.uri = "/api/debug/usb/restart", .method = HTTP_POST, .handler = api_debug_usb_restart};
    httpd_register_uri_handler(server, &uri_api_debug_usb_restart);
    ESP_LOGI(TAG, "Registered POST /api/debug/usb/restart handler (Sperimentali)");

    httpd_uri_t uri_api_debug_crash = {.uri = "/api/debug/crash", .method = HTTP_POST, .handler = api_debug_crash};
    httpd_register_uri_handler(server, &uri_api_debug_crash);
    ESP_LOGI(TAG, "Registered POST /api/debug/crash handler");

    httpd_uri_t uri_api_debug_restore = {.uri = "/api/debug/restore", .method = HTTP_POST, .handler = api_debug_restore};
    httpd_register_uri_handler(server, &uri_api_debug_restore);
    ESP_LOGI(TAG, "Registered POST /api/debug/restore handler");

    // Register API handlers from http_services component
    http_services_register_handlers(server);

    if (web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_EMULATOR)) {
        httpd_uri_t uri_emulator = {.uri = "/emulator", .method = HTTP_GET, .handler = emulator_page_handler_local};
        httpd_register_uri_handler(server, &uri_emulator);
    }

    // Reboot Handlers
    httpd_uri_t uri_reboot_factory = {.uri = "/reboot/factory", .method = HTTP_GET, .handler = reboot_factory_handler};
    httpd_register_uri_handler(server, &uri_reboot_factory);

    httpd_uri_t uri_reboot_app = {.uri = "/reboot/app", .method = HTTP_GET, .handler = reboot_app_handler};
    httpd_register_uri_handler(server, &uri_reboot_app);

    httpd_uri_t uri_reboot_app_last = {.uri = "/reboot/app_last", .method = HTTP_GET, .handler = reboot_app_last_handler};
    httpd_register_uri_handler(server, &uri_reboot_app_last);

    httpd_uri_t uri_reboot_ota0 = {.uri = "/reboot/ota0", .method = HTTP_GET, .handler = reboot_ota0_handler};
    httpd_register_uri_handler(server, &uri_reboot_ota0);

    httpd_uri_t uri_reboot_ota1 = {.uri = "/reboot/ota1", .method = HTTP_GET, .handler = reboot_ota1_handler};
    httpd_register_uri_handler(server, &uri_reboot_ota1);

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found_handler);

    return ESP_OK;
}
