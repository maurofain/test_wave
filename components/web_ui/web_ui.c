#include "web_ui.h"
#include "esp_log.h"
#include "esp_check.h"
#include "device_config.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "init.h"
#include "led.h"
#include "mdb.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "app_version.h"

#include "serial_test.h"
#include "led_test.h"
#include "mdb_test.h"
#include "io_expander.h"
#include "io_expander_test.h"
#include "eeprom_test.h"
#include "pwm.h"
#include "pwm_test.h"
#include "sd_card.h"

static const char *TAG = "WEB_UI";
static httpd_handle_t s_server = NULL;

// Elementi HTML comuni
static const char *HTML_NAV = "<nav><a href='/'>🏠 Home</a><a href='/config'>⚙️ Config</a><a href='/stats'>📈 Statistiche</a><a href='/tasks'>📋 Task</a><a href='/test'>🔧 Test</a><a href='/ota'>🔄 OTA</a></nav>";

static const char *HTML_STYLE_NAV = 
    "nav{background:#000;padding:10px;display:flex;justify-content:center;gap:10px;box-shadow:0 2px 5px rgba(0,0,0,0.1)}"
    "nav a{color:white;text-decoration:none;padding:8px 15px;border-radius:4px;background:#2c3e50;font-weight:bold;font-size:14px;transition:.2s}"
    "nav a:hover{background:#3498db}";

static esp_err_t send_head(httpd_req_t *req, const char *title, const char *extra_style, bool show_nav) {
    char *buf = malloc(4096);
    if (!buf) return ESP_ERR_NO_MEM;
    snprintf(buf, 4096, 
        "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>%s</title><style>"
        "body{font-family:Arial;background:#f5f5f5;color:#333;margin:0}header{background:#000;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".container{max-width:1000px;margin:20px auto;padding:0 20px}"
        "%s %s"
        "</style></head><body>"
        "<header>"
        "<div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>%s [%s]</h1></div>"
        "<div style='text-align:right;font-size:12px;opacity:0.8;'>v%s (%s)</div>"
        "</header>"
        "%s", title, show_nav?HTML_STYLE_NAV:"", extra_style?extra_style:"", title, device_config_get_running_app_name(), APP_VERSION, APP_DATE, show_nav?HTML_NAV:"");
    httpd_resp_sendstr_chunk(req, buf);
    free(buf);
    return ESP_OK;
}

// Handle dei task di test per Serial Blink Test
static TaskHandle_t s_rs232_test_handle = NULL;
static TaskHandle_t s_rs485_test_handle = NULL;

// TEST UART: 0x55, 0xAA, 0x01, 0x07 (periodico)
static void uart_test_task(void *arg) {
    uart_port_t port = (uart_port_t)arg;
    uint8_t seq[] = {0x55, 0xAA, 0x01, 0x07};
    ESP_LOGI(TAG, "Test Porta UART %d: Avvio", port);
    while(1) {
        for (int i=0; i<4; i++) {
            ESP_LOGD(TAG, "UART %d invio 0x%02X", port, seq[i]);
            for (int j=0; j<50; j++) { // Circa 1s (50 * 20ms)
                uart_write_bytes(port, &seq[i], 1);
                vTaskDelay(pdMS_TO_TICKS(20)); 
            }
        }
    }
}

// --- GESTORI (HANDLERS) ---

// Utilità: converte info IP in stringa
static void ip_to_str(esp_netif_t *netif, char *out, size_t len)
{
    if (!netif || !out) return;
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
        ip4addr_ntoa_r((const ip4_addr_t *)&info.ip, out, len);
    }
}

// Utilità: esegue l'aggiornamento OTA
static esp_err_t perform_ota(const char *url)
{
    if (!url || strlen(url) == 0) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "Avvio OTA da %s", url);
    esp_http_client_config_t client_cfg = {.url = url, .timeout_ms = 15000, .cert_pem = NULL, .skip_cert_common_name_check = true};
    esp_https_ota_config_t ota_cfg = {.http_config = &client_cfg};
    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA riuscito. Riavvio in corso...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    return ret;
}

static esp_err_t reboot_factory_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in Factory Mode...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_factory();
    return ESP_OK;
}

static esp_err_t reboot_app_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in Production Mode...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_app();
    return ESP_OK;
}

// Handler della Homepage
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *extra_style = 
        ".card{background:white;padding:25px;margin:20px 0;border-radius:12px;box-shadow:0 4px 15px rgba(0,0,0,0.1);transition:.3s}"
        ".card:hover{transform:translateY(-5px)}h2{color:#2c3e50;border-bottom:3px solid #3498db;padding-bottom:10px;margin-top:0}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;gap:20px}@media(max-width:600px){.grid{grid-template-columns:1fr}}"
        ".btn-link{display:flex;align-items:center;padding:20px;background:#3498db;color:white;text-decoration:none;border-radius:8px;font-weight:bold;font-size:18px;transition:.3s;gap:15px}"
        ".btn-link:hover{background:#2980b9;box-shadow:0 4px 8px rgba(0,0,0,0.2)}"
        ".btn-config{background:#27ae60}.btn-config:hover{background:#219150}"
        ".btn-test{background:#e67e22}.btn-test:hover{background:#d35400}"
        ".btn-ota{background:#e74c3c}.btn-ota:hover{background:#c0392b}.icon{font-size:30px}"
        ".btn-reboot{display:inline-block;padding:10px 20px;background:#2c3e50;color:white;text-decoration:none;border-radius:5px;margin-top:10px;font-weight:bold}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Factory Console", extra_style, false);

    const char *body = 
        "<div class='container'><div class='grid'>"
        "<a href='/config' class='btn-link btn-config'><span class='icon'>⚙️</span><span>Configurazione</span></a>"
        "<a href='/stats' class='btn-link'><span class='icon'>📈</span><span>Statistiche</span></a>"
        "<a href='/test' class='btn-link btn-test'><span class='icon'>🔧</span><span>Test Hardware</span></a>"
        "<a href='/tasks' class='btn-link'><span class='icon'>📋</span><span>Editor CSV</span></a>"
        "<a href='/ota' class='btn-link btn-ota'><span class='icon'>🔄</span><span>Update OTA</span></a>"
        "</div>"
        "<div class='card'><h2>ℹ️ Informazioni</h2>"
        "<p>Questa interfaccia permette di configurare, testare e aggiornare la scheda <b>ESP32-P4</b>.</p>"
        "<p>Utilizzare il menu <b>Configurazione</b> per abilitare i driver hardware necessari prima di eseguire i test.</p>"
        "<div style='margin-top:20px; border-top:1px solid #eee; padding-top:15px;'>"
        "<strong>Azioni Rapide:</strong><br>"
        "<a href='/reboot/factory' class='btn-reboot' style='background:#c0392b;'>Reboot in Factory</a> "
        "<a href='/reboot/app' class='btn-reboot' style='background:#27ae60;'>Reboot in App</a>"
        "</div>"
        "</div>"
        "<div class='card'><h2>🔗 API Endpoints</h2>"
        "<div style='overflow-x:auto;'>"
        "<table style='width:100%;border-collapse:collapse;'>"
        "<tr style='background:#ecf0f1;'><th style='text-align:left;padding:8px;border:1px solid #ddd;'>Metodo</th><th style='text-align:left;padding:8px;border:1px solid #ddd;'>Endpoint</th><th style='text-align:left;padding:8px;border:1px solid #ddd;'>Descrizione</th></tr>"
        "<tr><td style='padding:8px;border:1px solid #ddd;'>GET</td><td style='padding:8px;border:1px solid #ddd;'>/status</td><td style='padding:8px;border:1px solid #ddd;'>Info stato sistema (JSON)</td></tr>"
        "<tr><td style='padding:8px;border:1px solid #ddd;'>GET</td><td style='padding:8px;border:1px solid #ddd;'>/api/config</td><td style='padding:8px;border:1px solid #ddd;'>Configurazione attuale</td></tr>"
        "<tr><td style='padding:8px;border:1px solid #ddd;'>POST</td><td style='padding:8px;border:1px solid #ddd;'>/api/config/save</td><td style='padding:8px;border:1px solid #ddd;'>Salva configurazione</td></tr>"
        "<tr><td style='padding:8px;border:1px solid #ddd;'>POST</td><td style='padding:8px;border:1px solid #ddd;'>/api/config/reset</td><td style='padding:8px;border:1px solid #ddd;'>Reset alle impostazioni di fabbrica</td></tr>"
        "<tr><td style='padding:8px;border:1px solid #ddd;'>GET</td><td style='padding:8px;border:1px solid #ddd;'>/api/tasks</td><td style='padding:8px;border:1px solid #ddd;'>Lista dei task pianificati</td></tr>"
        "<tr><td style='padding:8px;border:1px solid #ddd;'>POST</td><td style='padding:8px;border:1px solid #ddd;'>/api/tasks/save</td><td style='padding:8px;border:1px solid #ddd;'>Salva lista task</td></tr>"
        "<tr><td style='padding:8px;border:1px solid #ddd;'>POST</td><td style='padding:8px;border:1px solid #ddd;'>/api/test/*</td><td style='padding:8px;border:1px solid #ddd;'>Endpoint per i test hardware</td></tr>"
        "<tr><td style='padding:8px;border:1px solid #ddd;'>POST</td><td style='padding:8px;border:1px solid #ddd;'>/ota/upload</td><td style='padding:8px;border:1px solid #ddd;'>Upload firmware (multipart)</td></tr>"
        "</table></div></div>"
        "</div></body></html>";
    
    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Handler del Logo
static esp_err_t logo_get_handler(httpd_req_t *req)
{
    FILE *f = fopen("/spiffs/logo.jpg", "r");
    if (!f) { httpd_resp_send_404(req); return ESP_FAIL; }
    fseek(f, 0, SEEK_END); size_t size = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(size);
    if (!buf) { fclose(f); httpd_resp_send_500(req); return ESP_FAIL; }
    fread(buf, 1, size, f); fclose(f);
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, buf, size); free(buf);
    return ESP_OK;
}

// Handler per lo stato JSON
static esp_err_t status_get_handler(httpd_req_t *req)
{
    esp_netif_t *ap, *sta, *eth;
    init_get_netifs(&ap, &sta, &eth);
    char ap_ip[16]="0.0.0.0", sta_ip[16]="0.0.0.0", eth_ip[16]="0.0.0.0";
    ip_to_str(ap, ap_ip, 16); ip_to_str(sta, sta_ip, 16); ip_to_str(eth, eth_ip, 16);
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    const mdb_status_t *mdb = mdb_get_status();

    char resp[1280];
    snprintf(resp, sizeof(resp), 
             "{\"partition_running\":\"%s\",\"partition_boot\":\"%s\",\"ip_ap\":\"%s\",\"ip_sta\":\"%s\",\"ip_eth\":\"%s\","
             "\"mdb\":{\"coin_online\":%s,\"coin_state\":%d,\"credit\":%lu},"
             "\"sd\":{\"mounted\":%s,\"total_kb\":%llu,\"used_kb\":%llu}}",
             running?running->label:"?", boot?boot->label:"?", ap_ip, sta_ip, eth_ip,
             mdb->coin.is_online?"true":"false", mdb->coin.state, mdb->coin.credit_cents,
             sd_card_is_mounted()?"true":"false", sd_card_get_total_size(), sd_card_get_used_size());
             
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, strlen(resp));
}

// Handler per la pagina OTA
static esp_err_t ota_get_handler(httpd_req_t *req)
{
    const char *extra_style = 
        ".card{background:white;padding:25px;margin:20px auto;max-width:600px;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,0.1)}"
        "input,button{width:100%;padding:10px;margin:10px 0;border-radius:4px;border:1px solid #ddd;box-sizing:border-box}"
        "button{background:#e67e22;color:white;font-weight:bold;cursor:pointer}button:hover{background:#d35400}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Aggiornamento OTA", extra_style, true);

    const char *body = 
        "<div class='container'><div class='card'>"
        "<form id='f' enctype='multipart/form-data'><input type='file' id='i' accept='.bin' required><button type='submit'>⬆️ Carica Firmware</button></form>"
        "<div id='s'></div></div></div><script>"
        "document.getElementById('f').onsubmit=async function(e){e.preventDefault();"
        "const fd=new FormData();fd.append('f',document.getElementById('i').files[0]);"
        "document.getElementById('s').innerText='Upload in corso...';"
        "try{const r=await fetch('/ota/upload',{method:'POST',body:fd});"
        "if(r.ok) document.getElementById('s').innerText='✅ Successo! Riavvio...';"
        "else document.getElementById('s').innerText='❌ Errore';}catch(e){document.getElementById('s').innerText='❌ Errore: '+e;}};"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Handler per l'upload OTA (POST)
static esp_err_t ota_upload_handler(httpd_req_t *req)
{
    if (req->content_len <= 0) return ESP_FAIL;
    const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
    if (!p) return ESP_FAIL;
    esp_ota_handle_t h; esp_ota_begin(p, OTA_SIZE_UNKNOWN, &h);
    char b[1024]; int rem = req->content_len;
    while (rem > 0) {
        int n = httpd_req_recv(req, b, MIN(rem, 1024));
        if (n <= 0) { esp_ota_abort(h); return ESP_FAIL; }
        esp_ota_write(h, b, n); rem -= n;
    }
    esp_ota_end(h); esp_ota_set_boot_partition(p);
    httpd_resp_send(req, "OTA completato con successo, riavvio in corso...", -1);
    vTaskDelay(pdMS_TO_TICKS(1000)); esp_restart();
    return ESP_OK;
}

// Handler per l'URL OTA (POST)
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    char q[256], u[200] = {0};
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) httpd_query_key_value(q, "url", u, sizeof(u));
    if (strlen(u) == 0) return ESP_FAIL;
    perform_ota(u);
    return ESP_OK;
}

// Handler per errore 404
static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t error)
{
    httpd_resp_set_status(req, "404 Non Trovato");
    return httpd_resp_send(req, "404 Non Trovato", -1);
}

// Handler per pagina configurazione
static esp_err_t config_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /config");
    
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
        "input:checked + .slider:before{transform:translateX(22px)}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Configurazione Device", extra_style, true);

    const char *body = 
        "<div class='container'>"
        "<div id='alert'></div>"
        "<form id='configForm'>"
        
        "<div class='section' style='background:#f1f4f6; border-left:5px solid #3498db;'>"
        "<h2>🆔 Identità Dispositivo</h2>"
        "<div class='form-group'><label>Nome Dispositivo</label>"
        "<input type='text' id='dev_name' name='dev_name' placeholder='es: TestWave-01' style='font-size:1.1em; font-weight:bold;'></div>"
        "</div>"

        "<div class='section'><h2>🌐 Ethernet</h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='eth_en' name='eth_en'><span class='slider'></span></label><span>Abilitato</span></div>"
        "<div class='indent'>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='eth_dhcp' name='eth_dhcp'><span class='slider'></span></label><span>DHCP</span></div>"
        "<div class='form-group'><label>Indirizzo IP</label><input type='text' id='eth_ip' name='eth_ip' placeholder='192.168.1.100'></div>"
        "<div class='form-group'><label>Subnet Mask</label><input type='text' id='eth_subnet' name='eth_subnet' placeholder='255.255.255.0'></div>"
        "<div class='form-group'><label>Gateway</label><input type='text' id='eth_gateway' name='eth_gateway' placeholder='192.168.1.1'></div>"
        "</div></div>"

        "<div class='section'><h2>📡 WiFi STA</h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='wifi_en' name='wifi_en'><span class='slider'></span></label><span>WiFi Abilitato</span></div>"
        "<div class='indent'>"
        "<div class='form-group'><label>SSID</label><input type='text' id='wifi_ssid' name='wifi_ssid'></div>"
        "<div class='form-group'><label>Password</label><input type='password' id='wifi_pwd' name='wifi_pwd'></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='wifi_dhcp' name='wifi_dhcp'><span class='slider'></span></label><span>DHCP</span></div>"
        "</div></div>"

        "<div class='section'><h2>🔌 Periferiche Hardware</h2>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='io_exp' name='io_exp'><span class='slider'></span></label><span>I/O Expander</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='temp' name='temp'><span class='slider'></span></label><span>Sensore Temperatura</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='led' name='led'><span class='slider'></span></label><span>LED Strip (WS2812)</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='rs232' name='rs232'><span class='slider'></span></label><span>UART RS232</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='rs485' name='rs485'><span class='slider'></span></label><span>UART RS485</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='mdb' name='mdb'><span class='slider'></span></label><span>MDB Engine</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='sd_card' name='sd_card'><span class='slider'></span></label><span>Scheda SD</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='pwm1' name='pwm1'><span class='slider'></span></label><span>PWM Canale 1</span></div>"
        "<div class='sw-row'><label class='switch'><input type='checkbox' id='pwm2' name='pwm2'><span class='slider'></span></label><span>PWM Canale 2</span></div>"
        "</div>"

        "<div class='section'><h2>📺 Display</h2>"
        "<div class='form-group'><label>Luminosità LCD (<span id='bright_val'>--</span>%)</label>"
        "<input type='range' id='lcd_bright' name='lcd_bright' min='0' max='100' style='width:100%' oninput='document.getElementById(\"bright_val\").innerText=this.value'></div>"
        "</div>"

        "<div class='section'><h2>📟 Porte Seriali</h2>"
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
        "async function backupConfig(){"
        "try{const r=await fetch('/api/config/backup',{method:'POST'});"
        "const res=await r.json();"
        "if(r.ok) alert('✅ '+res.message); else alert('❌ '+res.error);"
        "}catch(e){alert('❌ Errore: '+e);}}"
        "async function resetConfig(){if(confirm(\"Resettare ai valori di fabbrica?\")){await fetch(\"/api/config/reset\",{method:\"POST\"});location.reload();}}"
        "window.addEventListener('load',loadConfig);"
        "async function loadConfig(){"
        "try{const r=await fetch('/api/config');if(!r.ok)throw new Error('HTTP '+r.status);"
        "const c=await r.json();"
        "document.getElementById('dev_name').value=c.device_name || '';"
        "document.getElementById('eth_en').checked=c.eth.enabled;"
        "document.getElementById('eth_dhcp').checked=c.eth.dhcp_enabled;"
        "document.getElementById('eth_ip').value=c.eth.ip;"
        "document.getElementById('eth_subnet').value=c.eth.subnet;"
        "document.getElementById('eth_gateway').value=c.eth.gateway;"
        "document.getElementById('wifi_en').checked=c.wifi.sta_enabled;"
        "document.getElementById('wifi_dhcp').checked=c.wifi.dhcp_enabled;"
        "document.getElementById('wifi_ssid').value=c.wifi.ssid;"
        "document.getElementById('wifi_pwd').value=c.wifi.password;"
        "document.getElementById('io_exp').checked=c.sensors.io_expander_enabled;"
        "document.getElementById('temp').checked=c.sensors.temperature_enabled;"
        "document.getElementById('led').checked=c.sensors.led_enabled;"
        "document.getElementById('rs232').checked=c.sensors.rs232_enabled;"
        "document.getElementById('rs485').checked=c.sensors.rs485_enabled;"
        "document.getElementById('mdb').checked=c.sensors.mdb_enabled;"
        "document.getElementById('sd_card').checked=c.sensors.sd_card_enabled;"
        "document.getElementById('pwm1').checked=c.sensors.pwm1_enabled;"
        "document.getElementById('pwm2').checked=c.sensors.pwm2_enabled;"
        "document.getElementById('lcd_bright').value=c.display.lcd_brightness;"
        "document.getElementById('bright_val').innerText=c.display.lcd_brightness;"
        "document.getElementById('rs232_baud').value=c.rs232.baud;"
        "document.getElementById('rs232_bits').value=c.rs232.data_bits;"
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
        "}catch(e){console.error(e);}"
        "}"
        "document.getElementById('configForm').onsubmit=async function(e){e.preventDefault();"
        "const cfg={"
        "device_name:document.getElementById('dev_name').value,"
        "eth:{enabled:document.getElementById('eth_en').checked,dhcp_enabled:document.getElementById('eth_dhcp').checked,ip:document.getElementById('eth_ip').value,subnet:document.getElementById('eth_subnet').value,gateway:document.getElementById('eth_gateway').value},"
        "wifi:{sta_enabled:document.getElementById('wifi_en').checked,dhcp_enabled:document.getElementById('wifi_dhcp').checked,ssid:document.getElementById('wifi_ssid').value,password:document.getElementById('wifi_pwd').value,ip:'',subnet:'',gateway:''},"
        "sensors:{io_expander_enabled:document.getElementById('io_exp').checked,temperature_enabled:document.getElementById('temp').checked,led_enabled:document.getElementById('led').checked,rs232_enabled:document.getElementById('rs232').checked,rs485_enabled:document.getElementById('rs485').checked,mdb_enabled:document.getElementById('mdb').checked,sd_card_enabled:document.getElementById('sd_card').checked,pwm1_enabled:document.getElementById('pwm1').checked,pwm2_enabled:document.getElementById('pwm2').checked},"
        "display:{lcd_brightness:parseInt(document.getElementById('lcd_bright').value)},"
        "rs232:{baud:parseInt(document.getElementById('rs232_baud').value),data_bits:parseInt(document.getElementById('rs232_bits').value),parity:parseInt(document.getElementById('rs232_par').value),stop_bits:parseInt(document.getElementById('rs232_stop').value),rx_buf:parseInt(document.getElementById('rs232_rx').value),tx_buf:parseInt(document.getElementById('rs232_tx').value)},"
        "rs485:{baud:parseInt(document.getElementById('rs485_baud').value),data_bits:parseInt(document.getElementById('rs485_bits').value),parity:parseInt(document.getElementById('rs485_par').value),stop_bits:parseInt(document.getElementById('rs485_stop').value),rx_buf:parseInt(document.getElementById('rs485_rx').value),tx_buf:parseInt(document.getElementById('rs485_tx').value)},"
        "mdb_serial:{baud:parseInt(document.getElementById('mdb_baud').value),data_bits:8,parity:0,stop_bits:1,rx_buf:parseInt(document.getElementById('mdb_rx').value),tx_buf:parseInt(document.getElementById('mdb_tx').value)}"
        "};"
        "const r=await fetch('/api/config/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(cfg)});"
        "if(r.ok) alert('✅ Configurazione salvata!'); else alert('❌ Errore durante il salvataggio!');"
        "}"
        "</script></body></html>";
    
    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Handler API GET /api/config
static esp_err_t api_config_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /api/config (Forzo ricaricamento NVS)");
    device_config_t *cfg = device_config_get();
    device_config_load(cfg); // Forza rilettura da NVS
    
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
    
    char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler API POST /api/config/backup
static esp_err_t api_config_backup(httpd_req_t *req)
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

    char *json = cJSON_Print(root);
    esp_err_t err = sd_card_write_file("/sdcard/config_backup.json", json);
    
    char response[128];
    if (err == ESP_OK) {
        snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Backup salvato in /sdcard/config_backup.json\"}");
    } else {
        snprintf(response, sizeof(response), "{\"error\":\"Errore scrittura file\"}");
    }
    
    free(json);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

// Handler API POST /api/config/save
static esp_err_t api_config_save(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/config/save");
    
    char buf[2048] = {0};
    httpd_req_recv(req, buf, sizeof(buf)-1);
    
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
    
    cJSON *sensors_obj = cJSON_GetObjectItem(root, "sensors");
    if (sensors_obj) {
        cfg->sensors.io_expander_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "io_expander_enabled"));
        cfg->sensors.temperature_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "temperature_enabled"));
        cfg->sensors.led_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "led_enabled"));
        cfg->sensors.rs232_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs232_enabled"));
        cfg->sensors.rs485_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs485_enabled"));
        cfg->sensors.mdb_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "mdb_enabled"));
        cfg->sensors.pwm1_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm1_enabled"));
        cfg->sensors.pwm2_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm2_enabled"));
        cfg->sensors.sd_card_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "sd_card_enabled"));
    }

    cJSON *display_obj = cJSON_GetObjectItem(root, "display");
    if (display_obj) {
        cJSON *bright = cJSON_GetObjectItem(display_obj, "lcd_brightness");
        if (bright) {
            cfg->display.lcd_brightness = (uint8_t)bright->valueint;
            // Applica immediatamente la luminosità se il PWM è abilitato
            if (cfg->sensors.pwm1_enabled) {
                pwm_set_duty(0, cfg->display.lcd_brightness);
            }
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
    
    cfg->updated = true;
    device_config_save(cfg);
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

// Handler pagina statistiche
static esp_err_t stats_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /stats");
    const char *extra_style = 
        ".section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px}.stat-row{display:flex;justify-content:space-between;padding:12px;border-bottom:1px solid #ecf0f1}"
        ".stat-row:last-child{border-bottom:none}.stat-label{font-weight:bold;color:#34495e}.stat-value{color:#27ae60;font-family:monospace;font-weight:bold}"
        ".badge{padding:4px 10px;border-radius:20px;font-size:11px;font-weight:bold;text-transform:uppercase}"
        ".badge-on{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
        ".badge-off{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Statistiche Device", extra_style, true);

    const char *body = 
        "<div class='container'>"
        "<div class='section'><h2>🌐 Rete</h2><div id='network'>Caricamento...</div></div>"
        "<div class='section'><h2>💾 Firmware</h2><div id='partitions'>Caricamento...</div></div>"
        "<div class='section'><h2>� SD Card</h2><div id='sd_card'>Caricamento...</div></div>"
        "<div class='section'><h2>�🔌 Stato Driver</h2><div id='sensors'>Caricamento...</div></div>"
        "<div class='section'><h2>🎰 MDB Status</h2><div id='mdb_info'>Caricamento...</div></div>"
        "</div>"
        "<script>"
        "async function loadStats(){"
        "try{"
        "const r=await fetch('/status');if(!r.ok)throw new Error('Status Error');const status=await r.json();"
        "const rc=await fetch('/api/config');if(!rc.ok)throw new Error('Config Error');const config=await rc.json();"
        "document.getElementById('network').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Indirizzo IP Ethernet</span><span class='stat-value'>${status.ip_eth||'---'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Indirizzo IP WiFi STA</span><span class='stat-value'>${status.ip_sta||'---'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Indirizzo IP WiFi AP</span><span class='stat-value'>${status.ip_ap||'---'}</span></div>`;"
        "document.getElementById('partitions').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Partizione Corrente</span><span class='stat-value'>${status.partition_running||'?'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Partizione al Boot</span><span class='stat-value'>${status.partition_boot||'?'}</span></div>`;"
        "const sd=status.sd;"
        "document.getElementById('sd_card').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Stato</span><span class='badge ${sd.mounted==='true'?'badge-on':'badge-off'}'>${sd.mounted==='true'?'Montata':'Non Trovata'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Spazio Totale</span><span class='stat-value'>${sd.mounted==='true'?(sd.total_kb/1024).toFixed(1)+' MB':'---'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Spazio Usato</span><span class='stat-value'>${sd.mounted==='true'?(sd.used_kb/1024).toFixed(1)+' MB ('+((sd.used_kb/sd.total_kb)*100).toFixed(1)+'%)':'---'}</span></div>`;"
        "const s=config.sensors;"
        "document.getElementById('sensors').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>I/O Expander</span><span class='badge ${s.io_expander_enabled?'badge-on':'badge-off'}'>${s.io_expander_enabled?'Attivo':'Disabilitato'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Sensore Temperatura</span><span class='badge ${s.temperature_enabled?'badge-on':'badge-off'}'>${s.temperature_enabled?'Attivo':'Disabilitato'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>LED WS2812</span><span class='badge ${s.led_enabled?'badge-on':'badge-off'}'>${s.led_enabled?'Attivo':'Disabilitato'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>UART RS232</span><span class='badge ${s.rs232_enabled?'badge-on':'badge-off'}'>${s.rs232_enabled?'Attivo':'Disabilitato'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>UART RS485</span><span class='badge ${s.rs485_enabled?'badge-on':'badge-off'}'>${s.rs485_enabled?'Attivo':'Disabilitato'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>MDB Engine</span><span class='badge ${s.mdb_enabled?'badge-on':'badge-off'}'>${s.mdb_enabled?'Attivo':'Disabilitato'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>SD Card</span><span class='badge ${s.sd_card_enabled?'badge-on':'badge-off'}'>${s.sd_card_enabled?'Attivo':'Disabilitato'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>PWM Channel 1/2</span><span class='badge ${(s.pwm1_enabled||s.pwm2_enabled)?'badge-on':'badge-off'}'>${(s.pwm1_enabled||s.pwm2_enabled)?'Attivi':'Disabilitati'}</span></div>`;"
        "const m=status.mdb;"
        "document.getElementById('mdb_info').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Gettoniera</span><span class='badge ${m.coin_online?'badge-on':'badge-off'}'>${m.coin_online?'ONLINE':'OFFLINE'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Credito Accumulato</span><span class='stat-value'>€ ${(m.credit/100).toFixed(2)}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Stato Logico (SM)</span><span class='stat-value'>${m.coin_state}</span></div>`;"
        "}catch(e){console.error(e);}"
        "}"
        "window.addEventListener('load',loadStats);"
        "setInterval(loadStats,5000);"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Handler pagina tasks
static esp_err_t tasks_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /tasks");
    
    const char *extra_style = 
        ".section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px}"
        "table{width:100%;border-collapse:collapse;margin:20px 0}th,td{padding:10px;border:1px solid #ddd;text-align:left;color:#333}"
        "th{background:#34495e;color:white}input,select{width:100%;padding:5px;border:1px solid #ddd;border-radius:3px;box-sizing:border-box;color:#333}"
        "button{padding:10px 20px;background:#27ae60;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:bold;margin:5px}"
        "button:hover{background:#229954}.btn-add{background:#3498db}.btn-add:hover{background:#2980b9}"
        "#status{margin:15px 0;padding:10px;border-radius:4px}.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
        ".error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Editor Tasks", extra_style, true);

    const char *body = 
        "<div class='container'>"
        "<div class='section'><h2>📋 Configurazione Task</h2>"
        "<div id='status'></div>"
        "<table id='tasksTable'><thead><tr>"
        "<th>Nome</th><th>Stato</th><th>Priorità</th><th>Core</th><th>Periodo (ms)</th><th>Stack Words</th>"
        "</tr></thead><tbody id='tasksBody'>Caricamento...</tbody></table>"
        "<div style='display:flex; gap:10px;'>"
        "<button type='button' class='btn-add' onclick='addRow()'>➕ Aggiungi Task</button>"
        "<button type='button' onclick='saveTasks()'>💾 Salva</button>"
        "<button type='button' onclick='loadTasks()' style='background:#3498db'>🔄 Aggiorna Dati</button>"
        "</div>"
        "</div></div>"
        "<script>"
        "let tasks=[];"
        "async function loadTasks(){"
        "try{const r=await fetch('/api/tasks');if(!r.ok)throw new Error('HTTP '+r.status);"
        "tasks=await r.json();renderTable();}catch(e){showStatus('Errore caricamento tasks: '+e.message,'error');}}"
        "function renderTable(){"
        "const tbody=document.getElementById('tasksBody');tbody.innerHTML='';"
        "tasks.forEach((task,idx)=>{"
        "const row=tbody.insertRow();"
        "row.innerHTML=`<td><input type='text' value='${task.name}' onchange='tasks[${idx}].name=this.value'></td>"
        "<td><select onchange='tasks[${idx}].state=this.value'><option value='idle' ${task.state==='idle'?'selected':''}>idle</option>"
        "<option value='run' ${task.state==='run'?'selected':''}>run</option></select></td>"
        "<td><input type='number' value='${task.priority}' min='0' max='25' onchange='tasks[${idx}].priority=parseInt(this.value)'></td>"
        "<td><select onchange='tasks[${idx}].core=parseInt(this.value)'><option value='0' ${task.core==0?'selected':''}>0</option>"
        "<option value='1' ${task.core==1?'selected':''}>1</option></select></td>"
        "<td><input type='number' value='${task.period_ms}' min='1' onchange='tasks[${idx}].period_ms=parseInt(this.value)'></td>"
        "<td><input type='number' value='${task.stack_words}' min='512' step='512' onchange='tasks[${idx}].stack_words=parseInt(this.value)'></td>`;});"
        "}"
        "function addRow(){tasks.push({name:'new_task',state:'idle',priority:4,core:0,period_ms:100,stack_words:2048});renderTable();}"
        "async function saveTasks(){"
        "try{const r=await fetch('/api/tasks/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(tasks)});"
        "if(r.ok){showStatus('✅ Tasks salvate con successo!','success');}else{showStatus('❌ Errore durante il salvataggio','error');}}catch(e){showStatus('❌ Errore: '+e,'error');}}"
        "function showStatus(msg,type){const s=document.getElementById('status');s.textContent=msg;s.className=type;setTimeout(()=>s.className='',3000);}"
        "window.addEventListener('load',loadTasks);"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Handler API GET /api/tasks
static esp_err_t api_tasks_get(httpd_req_t *req)
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
        // Rimuovi newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Salta header
        if (skip_header) {
            skip_header = false;
            continue;
        }
        
        // Parse CSV: name,state,priority,core,period_ms,stack_words
        char name[64], state[16];
        int priority, core, period_ms, stack_words;
        
        if (sscanf(line, "%63[^,],%15[^,],%d,%d,%d,%d", 
                   name, state, &priority, &core, &period_ms, &stack_words) == 6) {
            
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

// Handler API POST /api/tasks/save
static esp_err_t api_tasks_save(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/tasks/save");
    
    char buf[4096] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf)-1);
    if (len <= 0) {
        const char *resp_str = "{\"error\":\"Nessun dato ricevuto\"}";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }
    
    cJSON *root = cJSON_Parse(buf);
    if (!root || !cJSON_IsArray(root)) {
        const char *resp_str = "{\"error\":\"JSON non valido\"}";
    httpd_resp_send(req, resp_str, strlen(resp_str));
        if (root) cJSON_Delete(root);
        return ESP_OK;
    }
    
    // Scrivi nel file
    FILE *f = fopen("/spiffs/tasks.csv", "w");
    if (!f) {
        ESP_LOGE(TAG, "[C] Impossibile aprire tasks.csv per scrittura");
        cJSON_Delete(root);
        httpd_resp_set_status(req, "500");
        const char *resp_str = "{\"error\":\"Impossibile scrivere il file\"}";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_FAIL;
    }
    
    // Scrivi header
    fprintf(f, "name,state,priority,core,period_ms,stack_words\n");
    
    // Scrivi ogni task
    int count = cJSON_GetArraySize(root);
    for (int i = 0; i < count; i++) {
        cJSON *task = cJSON_GetArrayItem(root, i);
        if (!task) continue;
        
        cJSON *name = cJSON_GetObjectItem(task, "name");
        cJSON *state = cJSON_GetObjectItem(task, "state");
        cJSON *priority = cJSON_GetObjectItem(task, "priority");
        cJSON *core = cJSON_GetObjectItem(task, "core");
        cJSON *period_ms = cJSON_GetObjectItem(task, "period_ms");
        cJSON *stack_words = cJSON_GetObjectItem(task, "stack_words");
        
        if (name && state && priority && core && period_ms && stack_words) {
            fprintf(f, "%s,%s,%d,%d,%d,%d\n",
                    name->valuestring,
                    state->valuestring,
                    priority->valueint,
                    core->valueint,
                    period_ms->valueint,
                    stack_words->valueint);
        }
    }
    
    fclose(f);
    cJSON_Delete(root);
    
    ESP_LOGI(TAG, "[C] Tasks salvate: %d righe", count);
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

// Handler pagina test
static esp_err_t test_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /test");
    
    const char *extra_style = 
        ".test-item{display:flex;align-items:center;justify-content:space-between;padding:15px;margin:10px 0;background:#ecf0f1;border-radius:6px}"
        ".test-label{font-weight:bold;color:#2c3e50;flex:1}.test-controls{display:flex;gap:10px;align-items:center}"
        "button{padding:8px 16px;background:#e67e22;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:bold}"
        "button:hover{background:#d35400}.btn-stop{background:#e74c3c}.btn-stop:hover{background:#c0392b}"
        "input[type=text],input[type=number]{padding:6px;border:1px solid #bdc3c7;border-radius:4px;width:120px;color:#333}"
        ".status-box{padding:10px;margin:10px 0;border-radius:4px;font-family:monospace;font-size:13px;background:#003366;color:#ffffff;min-height:60px;overflow-y:auto;max-height:250px;border:1px solid #3498db;box-sizing:border-box}"
        ".result{margin:5px 0;padding:5px;border-left:3px solid #3498db}"
        ".refresh-btn{background:#3498db;margin-bottom:10px}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Test Hardware", extra_style, true);

    const char *body = 
        "<div class='container'>"
        "<div style='text-align:right;'><button class='refresh-btn' onclick='location.reload()'>🔄 Aggiorna Pagina</button></div>"
        
        "<div class='section'><h2>💡 Striscia LED (WS2812)</h2>"
        "<div class='test-item'><span class='test-label'>Pattern RGB Completo (40s)</span>"
        "<div class='test-controls'><button onclick=\"runTest('led_start')\">▶️ Avvia</button><button class='btn-stop' onclick=\"runTest('led_stop')\">⏹️ Ferma</button></div></div>"
        
        "<h3>Controllo Manuale</h3>"
        "<div class='test-item'><span>Seleziona Colore:</span>"
        "<div class='test-controls'>"
        "  <input type='color' id='led_color' value='#ff0000' style='width:60px; height:35px; border:none; padding:0; cursor:pointer'>"
        "  <button onclick=\"setLED()\" style='background:#2ecc71'>🚀 Applica</button>"
        "</div></div>"
        
        "<div id='led_status' class='status-box'>Pronto per test LED</div></div>"

        "<div class='section'><h2>🔌 I/O Expander</h2>"
        "<div class='test-item'><span class='test-label'>Blink Tutte le Porte (1Hz)</span>"
        "<div class='test-controls'><button onclick=\"runTest('ioexp_start')\">▶️ Avvia</button><button class='btn-stop' onclick=\"runTest('ioexp_stop')\">⏹️ Ferma</button></div></div>"
        
        "<h3>Controllo Manuale</h3>"
        "<div id='io_manual_control' style='display:grid; grid-template-columns: 1fr 1fr; gap: 20px;'>"
        "  <div><h4>Uscite (Chip 0x43)</h4><div id='outputs_grid' style='display:flex; flex-wrap:wrap; gap:5px;'></div></div>"
        "  <div><h4>Ingressi (Chip 0x44)</h4><div id='inputs_grid' style='display:flex; flex-wrap:wrap; gap:5px;'></div></div>"
        "</div>"
        
        "<div id='ioexp_status' class='status-box'>Pronto per test I/O Expander</div></div>"
        
        "<div class='section'><h2>⚡ PWM</h2>"
        "<div class='test-item'><span class='test-label'>PWM1 Duty Cycle Sweep</span>"
        "<div class='test-controls'><button onclick=\"runTest('pwm1_start')\">▶️ Avvia</button><button class='btn-stop' onclick=\"runTest('pwm1_stop')\">⏹️ Ferma</button></div></div>"
        "<div class='test-item'><span class='test-label'>PWM2 Duty Cycle Sweep</span>"
        "<div class='test-controls'><button onclick=\"runTest('pwm2_start')\">▶️ Avvia</button><button class='btn-stop' onclick=\"runTest('pwm2_stop')\">⏹️ Ferma</button></div></div>"
        
        "<h3>Controllo Manuale PWM</h3>"
        "<div class='test-item'>"
        "  <span>Canale:</span>"
        "  <select id='pwm_ch' style='padding:6px; border-radius:4px;'><option value='1'>OUT1 (GPIO47)</option><option value='2'>OUT2 (GPIO48)</option></select>"
        "  <span>Freq (Hz):</span><input type='number' id='pwm_freq' value='1000' min='100' max='20000' style='width:80px'>"
        "  <span>Duty (%):</span><input type='number' id='pwm_duty' value='50' min='0' max='100' style='width:60px'>"
        "  <button onclick=\"setPWM()\" style='background:#2980b9'>🚀 Applica</button>"
        "</div>"
        
        "<div id='pwm_status' class='status-box'>Pronto per test PWM</div></div>"
        
        "<div class='section'><h2>📡 Seriale RS232</h2>"
        "<div class='test-item'><span class='test-label'>Test Loopback: 0x55, 0xAA, 0x01, 0x07</span>"
        "<div class='test-controls'><button onclick=\"runTest('rs232_start')\">▶️ Avvia</button><button class='btn-stop' onclick=\"runTest('rs232_stop')\">⏹️ Ferma</button></div></div>"
        "<div class='test-item'><span>Invia Stringa (es: \\0x55\\0xAA\\r\\n)</span>"
        "<div class='test-controls'><input type='text' id='rs232_input' placeholder='\\0x55 Test...'><button onclick=\"sendSerial('rs232')\">🚀 Invia</button></div></div>"
        "<div id='rs232_status' class='status-box'>Monitor:</div></div>"
        
        "<div class='section'><h2>📡 Seriale RS485</h2>"
        "<div class='test-item'><span class='test-label'>Test Loopback: 0x55, 0xAA, 0x01, 0x07</span>"
        "<div class='test-controls'><button onclick=\"runTest('rs485_start')\">▶️ Avvia</button><button class='btn-stop' onclick=\"runTest('rs485_stop')\">⏹️ Ferma</button></div></div>"
        "<div class='test-item'><span>Invia Stringa</span>"
        "<div class='test-controls'><input type='text' id='rs485_input' placeholder='Richiesta...'><button onclick=\"sendSerial('rs485')\">🚀 Invia</button></div></div>"
        "<div id='rs485_status' class='status-box'>Monitor:</div></div>"
        
        "<div class='section'><h2>💾 EEPROM 24LC16 <span id='eeprom_header_info' style='font-size:14px; font-weight:normal; margin-left:15px; color:#666;'></span></h2>"
        "<div class='test-item'><span>Indirizzo (0-2047)</span>"
        "<div class='test-controls'><input type='number' id='eeprom_addr' value='0' min='0' max='2047' style='width:80px'></div></div>"
        "<div class='test-item'><span>Dato Byte (0-255)</span>"
        "<div class='test-controls'><input type='number' id='eeprom_val' value='123' min='0' max='255' style='width:80px'>"
        "<button onclick=\"testEEPROM('write')\">✍️ Scrivi</button>"
        "<button onclick=\"testEEPROM('read')\" style='background:#3498db'>📖 Leggi</button>"
        "<button onclick=\"testEEPROM('read_json')\" style='background:#9b59b6'>📄 Leggi JSON</button>"
        "</div></div>"
        "<div id='eeprom_status' class='status-box'>Pronto per test EEPROM</div></div>"

        "<div class='section'><h2>🎰 MDB (Multi-Drop Bus)</h2>"
        "<div class='test-item'><span class='test-label'>Test Loopback/Echo</span>"
        "<div class='test-controls'><button onclick=\"runTest('mdb_start')\">▶️ Avvia</button><button class='btn-stop' onclick=\"runTest('mdb_stop')\">⏹️ Ferma</button></div></div>"
        "<div class='test-item'><span>Invia Stringa (Hex, es: 08 00)</span>"
        "<div class='test-controls'><input type='text' id='mdb_input' placeholder='08 00...'><button onclick=\"sendSerial('mdb')\">🚀 Invia</button></div></div>"
        "<div id='mdb_status' class='status-box'>Pronto per test MDB</div></div>"

        "<div class='section'><h2>💾 Scheda MicroSD</h2>"
        "<div class='test-item'><span class='test-label'>Stato Montaggio:</span>"
        "<span id='sd_mounted_status' style='margin-right:10px'>--</span>"
        "<button onclick='refreshSDStatus()' style='background:#3498db; padding:4px 8px; font-size:12px;'>🔄 Aggiorna</button></div>"
        "<div class='test-item'><span>Elenco File (Root)</span>"
        "<div class='test-controls'><button onclick=\"runTest('sd_list')\">📂 Elenca</button></div></div>"
        "<div class='test-item'><span>Backup JSON Config</span>"
        "<div class='test-controls'><button onclick=\"runConfigBackup()\" style='background:#9b59b6'>📥 Backup</button></div></div>"
        "<div class='test-item'><span style='color:#c0392b; font-weight:bold;'>⚠️ Formattazione FAT32</span>"
        "<div class='test-controls'><button onclick=\"if(confirm('Cancellare TUTTI i dati?'))runTest('sd_format')\" style='background:#c0392b'>🧨 Formatta</button></div></div>"
        "<div id='sd_status' class='status-box'>Pronto per test SD</div></div>"
        "</div>"
        "<script>"
        "async function runTest(test,params={}){"
        "const url='/api/test/'+test;const statusId=test.split('_')[0]+'_status';"
        "const statusBox=document.getElementById(statusId);if(!statusBox)return;"
        "statusBox.innerHTML+='<div class=\"result\">➡️ Esecuzione: '+test+'...</div>';"
        "try{const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(params)});"
        "const result=await r.json().catch(()=>({error:'Risposta non valida dal server'}));"
        "if(r.ok){statusBox.innerHTML+='<div class=\"result\">✅ '+(result.message||'OK')+'</div>';}"
        "else{statusBox.innerHTML+='<div class=\"result\">❌ Errore: '+(result.error||r.status)+'</div>';}}"
        "catch(e){statusBox.innerHTML+='<div class=\"result\">❌ Errore: '+e+'</div>';}"
        "statusBox.scrollTop=statusBox.scrollHeight;}"
        
        "async function sendSerial(port){"
        "const data=document.getElementById(port+'_input').value;"
        "const r=await fetch('/api/test/serial_send',{method:'POST',body:JSON.stringify({port,data})});"
        "const res=await r.json();"
        "if(!r.ok)alert(res.error);"
        "}"
        
        "async function updateMonitors(){"
        "const r=await fetch('/api/test/serial_monitor',{method:'POST'});"
        "const res=await r.json();"
        "if(res.log){"
        "document.getElementById('rs232_status').innerText = res.log;"
        "document.getElementById('rs485_status').innerText = res.log;"
        "document.getElementById('mdb_status').innerText = res.log;"
        "}"
        
        "const rio=await fetch('/api/test/io_get',{method:'POST'});"
        "const io=await rio.json();"
        "if(io){"
        "  const out_grid = document.getElementById('outputs_grid');"
        "  const in_grid = document.getElementById('inputs_grid');"
        "  if(out_grid && out_grid.children.length === 0){"
        "    for(let i=0;i<8;i++){"
        "      let b = document.createElement('button');"
        "      b.innerText = 'OUT'+i;"
        "      b.id = 'out_btn_'+i;"
        "      b.onclick = () => toggleIO(i);"
        "      out_grid.appendChild(b);"
        "      let s = document.createElement('span');"
        "      s.id = 'in_val_'+i;"
        "      s.style = 'padding:8px; border:1px solid #ccc; border-radius:4px; min-width:40px; text-align:center; background:#eee; color:#333;';"
        "      s.innerText = 'IN'+i;"
        "      in_grid.appendChild(s);"
        "    }"
        "  }"
        "  for(let i=0;i<8;i++){"
        "    const b = document.getElementById('out_btn_'+i);"
        "    if(b){"
        "      const is_on = (io.output >> i) & 1;"
        "      b.style.background = is_on ? '#2ecc71' : '#95a5a6';"
        "    }"
        "    const s = document.getElementById('in_val_'+i);"
        "    if(s){"
        "      const is_on = (io.input >> i) & 1;"
        "      s.style.background = is_on ? '#2ecc71' : '#34495e';"
        "      s.style.color = is_on ? '#fff' : '#bdc3c7';"
        "    }"
        "  }"
        "}"
        "}"
        
        "async function toggleIO(pin){"
        "const rio=await fetch('/api/test/io_get',{method:'POST'});"
        "const io=await rio.json();"
        "const cur = (io.output >> pin) & 1;"
        "await fetch('/api/test/io_set',{method:'POST',body:JSON.stringify({pin:pin, val:cur?0:1})});"
        "}"
        
        "async function setLED(){"
        "const color=document.getElementById('led_color').value;"
        "const r=parseInt(color.substr(1,2),16);"
        "const g=parseInt(color.substr(3,2),16);"
        "const b=parseInt(color.substr(5,2),16);"
        "const res=await fetch('/api/test/led_set',{method:'POST',body:JSON.stringify({r,g,b})});"
        "if(res.ok) document.getElementById('led_status').innerHTML+='<div class=\"result\">✅ Colore impostato: '+color+'</div>';"
        "}"
        
        "async function setPWM(){"
        "const ch=parseInt(document.getElementById('pwm_ch').value);"
        "const freq=parseInt(document.getElementById('pwm_freq').value);"
        "const duty=parseInt(document.getElementById('pwm_duty').value);"
        "const r=await fetch('/api/test/pwm_set',{method:'POST',body:JSON.stringify({ch,freq,duty})});"
        "const res=await r.json();"
        "if(res.status==='ok') document.getElementById('pwm_status').innerHTML+='<div class=\"result\">✅ PWM'+ch+' impostato: '+freq+'Hz, '+duty+'%</div>';"
        "}"
        
        "async function testEEPROM(op){"
        "const addr=parseInt(document.getElementById('eeprom_addr').value);"
        "const val=parseInt(document.getElementById('eeprom_val').value);"
        "const statusBox=document.getElementById('eeprom_status');"
        "let body={op:op, addr:addr};"
        "if(op==='write') body.data=[val]; else body.len=1;"
        "try{"
        "const r=await fetch('/api/test/eeprom',{method:'POST',body:JSON.stringify(body)});"
        "const res=await r.json();"
        "if(res.status==='ok'){"
        "  if(op==='read') statusBox.innerHTML='<div class=\"result\">📖 Letto da '+addr+': <b>'+res.data[0]+'</b> (0x'+res.data[0].toString(16).toUpperCase()+')</div>';"
        "  else if(op==='read_json') statusBox.innerHTML='<div class=\"result\">📄 Config JSON:<br><pre style=\"background:#002244;color:#00ff00;padding:10px;border-radius:4px;overflow-x:auto;margin:5px 0;font-size:11px\">'+JSON.stringify(JSON.parse(res.json),null,2)+'</pre></div>';"
        "  else statusBox.innerHTML='<div class=\"result\">✅ Scritto '+val+' a '+addr+'</div>';"
        "  refreshEEPROMStatus();"
        "}else statusBox.innerHTML='<div class=\"result\">❌ Errore operazione</div>';"
        "}catch(e){statusBox.innerHTML='<div class=\"result\">❌ Errore: '+e+'</div>';}"
        "}"
        
        "async function runConfigBackup(){"
        "  const statusBox=document.getElementById('sd_status');"
        "  statusBox.innerHTML+='<div class=\"result\">➡️ Esecuzione Backup Config...</div>';"
        "  try {"
        "    const r=await fetch('/api/config/backup',{method:'POST'});"
        "    const res=await r.json();"
        "    if(r.ok) statusBox.innerHTML+='<div class=\"result\">✅ '+res.message+'</div>';"
        "    else statusBox.innerHTML+='<div class=\"result\">❌ '+res.error+'</div>';"
        "  } catch(e) { statusBox.innerHTML+='<div class=\"result\">❌ Errore: '+e+'</div>'; }"
        "  statusBox.scrollTop=statusBox.scrollHeight;"
        "}"
        
        "async function refreshEEPROMStatus(){"
        "try{"
        "  const r=await fetch('/api/test/eeprom',{method:'POST',body:JSON.stringify({op:'status'})});"
        "  const res=await r.json();"
        "  if(res.status==='ok') document.getElementById('eeprom_header_info').innerText='CRC: 0x'+res.crc.toString(16).toUpperCase()+' | Updated: '+(res.updated?'SÌ':'NO');"
        "}catch(e){}"
        "}"
        
        "async function refreshSDStatus(){"
        "  try {"
        "    const r=await fetch('/status');"
        "    const res=await r.json();"
        "    const el=document.getElementById('sd_mounted_status');"
        "    if(el) el.innerText = res.sd.mounted ? '✅ MONTATA ('+(res.sd.total_kb/1024).toFixed(1)+' MB)' : '❌ NON MONTATA';"
        "  } catch(e){}"
        "}"
        
        "refreshEEPROMStatus();"
        "refreshSDStatus();"
        "setInterval(updateMonitors, 1000);"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

// Handler API POST /api/test/*
static esp_err_t api_test_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST %s", req->uri);
    const char *test_name = req->uri + strlen("/api/test/");
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
                if (r_obj && g_obj && b_obj) {
                    led_test_set_color(r_obj->valueint, g_obj->valueint, b_obj->valueint);
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
    else if (strcmp(test_name, "io_get") == 0) {
        uint8_t in = io_get();
        snprintf(response, sizeof(response), "{\"input\":%d,\"output\":%d}", in, io_output_state);
    }

    // --- TEST RS232 ---
    else if (strcmp(test_name, "rs232_start") == 0) {
        if (!s_rs232_test_handle) {
            xTaskCreate(uart_test_task, "rs232_test", 2048, (void*)CONFIG_APP_RS232_UART_PORT, 5, &s_rs232_test_handle);
            snprintf(response, sizeof(response), "{\"message\":\"Test RS232 avviato (caratteri 55,AA,01,07)\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "rs232_stop") == 0) {
        if (s_rs232_test_handle) { vTaskDelete(s_rs232_test_handle); s_rs232_test_handle = NULL; }
        snprintf(response, sizeof(response), "{\"message\":\"Test RS232 fermato\"}");
    }

    // --- TEST RS485 ---
    else if (strcmp(test_name, "rs485_start") == 0) {
        if (!s_rs485_test_handle) {
            xTaskCreate(uart_test_task, "rs485_test", 2048, (void*)CONFIG_APP_RS485_UART_PORT, 5, &s_rs485_test_handle);
            snprintf(response, sizeof(response), "{\"message\":\"Test RS485 avviato (caratteri 55,AA,01,07)\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "rs485_stop") == 0) {
        if (s_rs485_test_handle) { vTaskDelete(s_rs485_test_handle); s_rs485_test_handle = NULL; }
        snprintf(response, sizeof(response), "{\"message\":\"Test RS485 fermato\"}");
    }

    // --- TEST EEPROM ---
    else if (strcmp(test_name, "eeprom") == 0) {
        return eeprom_test_handler(req);
    }

    // --- TEST MDB ---
    if (strcmp(test_name, "mdb_start") == 0) {
        if (mdb_test_start() == ESP_OK) {
            snprintf(response, sizeof(response), "{\"message\":\"Test MDB avviato\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "mdb_stop") == 0) {
        mdb_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test MDB fermato\"}");
    }
    
    // --- TEST SD CARD ---
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
                // Inviamo direttamente il buffer come parte di un JSON (attenzione all'escaping se necessario)
                // Ma poiché sd_card_list_dir produce testo con newline, lo mettiamo in un campo message
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
    
    // --- TEST INVIO SERIALE (RS232/RS485/MDB) ---
    else if (strcmp(test_name, "serial_send") == 0) {
        char buf[512] = {0};
        httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            const char *port_raw = cJSON_GetStringValue(cJSON_GetObjectItem(root, "port"));
            const char *data_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "data"));
            
            if (port_raw && strcmp(port_raw, "mdb") == 0) {
                // Logica di invio MDB
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
                    serial_test_send_uart(port, data_str);
                    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Inviato su %s\"}", port_raw);
                }
            }
            cJSON_Delete(root);
        } else snprintf(response, sizeof(response), "{\"error\":\"JSON non valido\"}");
    }
    
    // --- MONITOR SERIALE ---
    else if (strcmp(test_name, "serial_monitor") == 0) {
        snprintf(response, sizeof(response), "{\"log\":\"%s\"}", serial_test_get_monitor());
    }

    else {
        snprintf(response, sizeof(response), "{\"error\":\"Test sconosciuto: %s\"}", test_name);
        httpd_resp_set_status(req, "404");
    }
    
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, strlen(response));
}

// Handler API POST /api/config/reset
static esp_err_t api_config_reset(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/config/reset");
    device_config_reset_defaults();
    httpd_resp_set_type(req, "application/json");
    const char *ok_resp = "{\"status\":\"ok\"}";
    httpd_resp_send(req, ok_resp, strlen(ok_resp));
    return ESP_OK;
}

esp_err_t web_ui_init(void)
{
    if (s_server != NULL) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_APP_HTTP_PORT;
    config.max_uri_handlers = 20;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "[C] Avvio server HTTP sulla porta %d", config.server_port);
    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) return ret;

    // Registrazione URI di sistema (ex-init.c)
    httpd_uri_t uri_root = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
    httpd_register_uri_handler(s_server, &uri_root);
    
    httpd_uri_t uri_logo = {.uri = "/logo.jpg", .method = HTTP_GET, .handler = logo_get_handler};
    httpd_register_uri_handler(s_server, &uri_logo);
    
    httpd_uri_t uri_status = {.uri = "/status", .method = HTTP_GET, .handler = status_get_handler};
    httpd_register_uri_handler(s_server, &uri_status);

    httpd_uri_t uri_ota_get = {.uri = "/ota", .method = HTTP_GET, .handler = ota_get_handler};
    httpd_register_uri_handler(s_server, &uri_ota_get);
    
    httpd_uri_t uri_ota_post = {.uri = "/ota", .method = HTTP_POST, .handler = ota_post_handler};
    httpd_register_uri_handler(s_server, &uri_ota_post);

    httpd_uri_t uri_ota_upload = {.uri = "/ota/upload", .method = HTTP_POST, .handler = ota_upload_handler};
    httpd_register_uri_handler(s_server, &uri_ota_upload);

    // Registrazione URI Web UI (Pagine e API)
    httpd_uri_t uri_config = {.uri = "/config", .method = HTTP_GET, .handler = config_page_handler};
    httpd_register_uri_handler(s_server, &uri_config);
    
    httpd_uri_t uri_stats = {.uri = "/stats", .method = HTTP_GET, .handler = stats_page_handler};
    httpd_register_uri_handler(s_server, &uri_stats);
    
    httpd_uri_t uri_tasks = {.uri = "/tasks", .method = HTTP_GET, .handler = tasks_page_handler};
    httpd_register_uri_handler(s_server, &uri_tasks);
    
    httpd_uri_t uri_test = {.uri = "/test", .method = HTTP_GET, .handler = test_page_handler};
    httpd_register_uri_handler(s_server, &uri_test);

    // API
    httpd_uri_t uri_api_get = {.uri = "/api/config", .method = HTTP_GET, .handler = api_config_get};
    httpd_register_uri_handler(s_server, &uri_api_get);
    
    httpd_uri_t uri_api_save = {.uri = "/api/config/save", .method = HTTP_POST, .handler = api_config_save};
    httpd_register_uri_handler(s_server, &uri_api_save);

    httpd_uri_t uri_api_backup = {.uri = "/api/config/backup", .method = HTTP_POST, .handler = api_config_backup};
    httpd_register_uri_handler(s_server, &uri_api_backup);
    
    httpd_uri_t uri_api_reset = {.uri = "/api/config/reset", .method = HTTP_POST, .handler = api_config_reset};
    httpd_register_uri_handler(s_server, &uri_api_reset);

    httpd_uri_t uri_api_tasks = {.uri = "/api/tasks", .method = HTTP_GET, .handler = api_tasks_get};
    httpd_register_uri_handler(s_server, &uri_api_tasks);
    
    httpd_uri_t uri_api_tasks_save = {.uri = "/api/tasks/save", .method = HTTP_POST, .handler = api_tasks_save};
    httpd_register_uri_handler(s_server, &uri_api_tasks_save);
    
    httpd_uri_t uri_api_test = {.uri = "/api/test/*", .method = HTTP_POST, .handler = api_test_handler};
    httpd_register_uri_handler(s_server, &uri_api_test);

    // Reboot Handlers
    httpd_uri_t uri_reboot_factory = {.uri = "/reboot/factory", .method = HTTP_GET, .handler = reboot_factory_handler};
    httpd_register_uri_handler(s_server, &uri_reboot_factory);

    httpd_uri_t uri_reboot_app = {.uri = "/reboot/app", .method = HTTP_GET, .handler = reboot_app_handler};
    httpd_register_uri_handler(s_server, &uri_reboot_app);

    httpd_register_err_handler(s_server, HTTPD_404_NOT_FOUND, not_found_handler);

    return ESP_OK;
}

void web_ui_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
}
