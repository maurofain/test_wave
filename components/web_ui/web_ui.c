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

#include "serial_test.h"
#include "led_test.h"
#include "mdb_test.h"
#include "io_expander_test.h"
#include "pwm_test.h"

static const char *TAG = "WEB_UI";
static httpd_handle_t s_server = NULL;

// Test tasks handles
static TaskHandle_t s_led_test_handle = NULL;
static TaskHandle_t s_pwm1_test_handle = NULL;
static TaskHandle_t s_pwm2_test_handle = NULL;
static TaskHandle_t s_ioexp_test_handle = NULL;
static TaskHandle_t s_rs232_test_handle = NULL;
static TaskHandle_t s_rs485_test_handle = NULL;
static TaskHandle_t s_mdb_test_handle = NULL;

// --- LOGICA TEST HARDWARE ---

// LED TEST: Cambio colori RGB (20s) -> Running LED (20s)
static void led_test_task(void *arg) {
    ESP_LOGI(TAG, "LED Test: Avvio (40s totali)");
    // 1. RGB Cycle (20s)
    for (int i=0; i<20; i++) {
        led_fill_color(255, 0, 0); vTaskDelay(pdMS_TO_TICKS(333));
        led_fill_color(0, 255, 0); vTaskDelay(pdMS_TO_TICKS(333));
        led_fill_color(0, 0, 255); vTaskDelay(pdMS_TO_TICKS(333));
    }
    // 2. Running LED (20s)
    led_clear();
    for (int i=0; i<40; i++) {
        for (int j=0; j<8; j++) { // Assumiamo 8 LED
             led_set_pixel(j, 255, 255, 255);
             vTaskDelay(pdMS_TO_TICKS(60));
             led_set_pixel(j, 0, 0, 0);
        }
    }
    led_clear();
    ESP_LOGI(TAG, "LED Test: Completato");
    s_led_test_handle = NULL;
    vTaskDelete(NULL);
}

// PWM TEST: Duty 50->0->100 in 10s @ 100, 1k, 10k Hz
static void pwm_test_task(void *arg) {
    int channel_num = (int)arg;
    ledc_channel_t ch = (channel_num == 1) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
    int freqs[] = {100, 1000, 10000};
    ESP_LOGI(TAG, "PWM%d Test: Avvio", channel_num);
    
    for (int f=0; f<3; f++) {
        ESP_LOGI(TAG, "PWM%d - Frequenza: %d Hz", channel_num, freqs[f]);
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freqs[f]);
        // Duty cycle loop (10s total: 5s down, 5s up)
        for (int i=50; i>=0; i--) { 
            ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, (i*255)/100); 
            ledc_update_duty(LEDC_LOW_SPEED_MODE, ch); 
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
        for (int i=0; i<=100; i++) { 
            ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, (i*255)/100); 
            ledc_update_duty(LEDC_LOW_SPEED_MODE, ch); 
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }
    ESP_LOGI(TAG, "PWM%d Test: Completato", channel_num);
    if (channel_num == 1) s_pwm1_test_handle = NULL; else s_pwm2_test_handle = NULL;
    vTaskDelete(NULL);
}

// IO EXPANDER TEST: ON/OFF all ports at 1Hz
static void ioexp_test_task(void *arg) {
    ESP_LOGI(TAG, "I/O Expander Test: Avvio");
    while(1) {
        uint8_t data_on[] = {0x02, 0xFF, 0xFF}; // Register 0x02 (output port 0), 2 bytes
        i2c_master_write_to_device(CONFIG_APP_I2C_PORT, 0x20, data_on, 3, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(500));
        uint8_t data_off[] = {0x02, 0x00, 0x00};
        i2c_master_write_to_device(CONFIG_APP_I2C_PORT, 0x20, data_off, 3, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// UART TEST: 0x55, 0xAA, 0x01, 0x07 (1s each)
static void uart_test_task(void *arg) {
    uart_port_t port = (uart_port_t)arg;
    uint8_t seq[] = {0x55, 0xAA, 0x01, 0x07};
    ESP_LOGI(TAG, "UART Port %d Test: Avvio", port);
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

// MDB TEST: Poll gettoniera (0x08)
static void mdb_test_task(void *arg) {
    ESP_LOGI(TAG, "MDB Test: Avvio (Polling 0x08)");
    uint8_t poll_cmd = 0x08; // VMC 0x08 = Coin Acceptor Poll
    uint8_t rx_buf[36];
    size_t rx_len;

    while(1) {
        ESP_LOGD(TAG, "MDB Sending POLL 0x08...");
        mdb_send_packet(poll_cmd, NULL, 0);
        
        esp_err_t ret = mdb_receive_packet(rx_buf, sizeof(rx_buf), &rx_len, 20);
        if (ret == ESP_OK) {
            if (rx_len == 1 && rx_buf[0] == 0x00) {
                ESP_LOGI(TAG, "MDB Recv: ACK (0x00)");
            } else {
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, rx_buf, rx_len, ESP_LOG_INFO);
            }
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "MDB Timeout (Nessuna risposta)");
        } else {
            ESP_LOGW(TAG, "MDB Recv Error: %s", esp_err_to_name(ret));
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// --- HANDLERS ---

// Utility: IP info to string
static void ip_to_str(esp_netif_t *netif, char *out, size_t len)
{
    if (!netif || !out) return;
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
        ip4addr_ntoa_r((const ip4_addr_t *)&info.ip, out, len);
    }
}

// Utility: Perform OTA
static esp_err_t perform_ota(const char *url)
{
    if (!url || strlen(url) == 0) return ESP_ERR_INVALID_ARG;
    ESP_LOGI(TAG, "Starting OTA from %s", url);
    esp_http_client_config_t client_cfg = {.url = url, .timeout_ms = 15000, .cert_pem = NULL, .skip_cert_common_name_check = true};
    esp_https_ota_config_t ota_cfg = {.http_config = &client_cfg};
    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA successful. Rebooting...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    return ret;
}

// Handler Homepage
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Factory Server</title><style>"
        "body{font-family:Arial;background:#f5f5f5;margin:0}header{background:#2c3e50;color:white;padding:15px 30px;display:flex;align-items:center;justify-content:center;gap:20px}"
        ".container{max-width:900px;margin:30px auto;padding:0 20px}.card{background:white;padding:25px;margin:20px 0;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,0.1)}"
        "h1{margin:0;font-size:24px}h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px;margin-top:0}"
        ".endpoint{display:flex;align-items:center;padding:12px;margin:8px 0;background:#ecf0f1;border-radius:4px}"
        ".method{font-weight:bold;color:white;padding:4px 12px;border-radius:4px;margin-right:15px;min-width:60px;text-align:center}"
        ".get{background:#27ae60}.post{background:#e67e22}"
        ".uri{font-family:monospace;color:#2c3e50;flex:1}.desc{color:#7f8c8d;margin-left:10px}"
        "a{color:#3498db;text-decoration:none}a:hover{text-decoration:underline}"
        "</style></head><body><header><img src='/logo.jpg' alt='Logo' style='max-height:50px;'><div><h1>🏭 ESP32-P4 Factory Server</h1><p style='margin:5px 0 0 0;opacity:0.8'>Recovery, Test & Flash</p></div></header>"
        "<div class='container'>"
        "<div class='card'><h2>📡 API Endpoints</h2>"
        "<div class='endpoint'><span class='method get'>GET</span><span class='uri'>/</span><span class='desc'>Homepage</span></div>"
        "<div class='endpoint'><span class='method get'>GET</span><span class='uri'>/status</span><span class='desc'>Stato JSON</span></div>"
        "<div class='endpoint'><span class='method get'>GET</span><span class='uri'>/config</span><span class='desc'>Configurazione WEB</span></div>"
        "<div class='endpoint'><span class='method get'>GET</span><span class='uri'>/stats</span><span class='desc'>Statistiche</span></div>"
        "<div class='endpoint'><span class='method get'>GET</span><span class='uri'>/test</span><span class='desc'>Test Hardware</span></div>"
        "<div class='endpoint'><span class='method get'>GET</span><span class='uri'>/tasks</span><span class='desc'>Editor CSV</span></div>"
        "<div class='endpoint'><span class='method post'>POST</span><span class='uri'>/ota/upload</span><span class='desc'>Upload Firmware</span></div>"
        "</div>"
        "<div class='card'><h2>🔗 Collegamenti Rapidi</h2>"
        "<p><a href='/config'>⚙️ Configurazione</a></p>"
        "<p><a href='/stats'>📈 Statistiche</a></p>"
        "<p><a href='/tasks'>📋 Editor Tasks</a></p>"
        "<p><a href='/test'>🔧 Test Hardware</a></p>"
        "<p><a href='/ota'>🔄 Aggiornamento Firmware</a></p>"
        "</div></div></body></html>";
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, strlen(html));
}

// Handler Logo
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

// Handler Status JSON
static esp_err_t status_get_handler(httpd_req_t *req)
{
    esp_netif_t *ap, *sta, *eth;
    init_get_netifs(&ap, &sta, &eth);
    char ap_ip[16]="0.0.0.0", sta_ip[16]="0.0.0.0", eth_ip[16]="0.0.0.0";
    ip_to_str(ap, ap_ip, 16); ip_to_str(sta, sta_ip, 16); ip_to_str(eth, eth_ip, 16);
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    const mdb_status_t *mdb = mdb_get_status();

    char resp[1024];
    snprintf(resp, sizeof(resp), 
             "{\"partition_running\":\"%s\",\"partition_boot\":\"%s\",\"ip_ap\":\"%s\",\"ip_sta\":\"%s\",\"ip_eth\":\"%s\","
             "\"mdb\":{\"coin_online\":%s,\"coin_state\":%d,\"credit\":%lu}}",
             running?running->label:"?", boot?boot->label:"?", ap_ip, sta_ip, eth_ip,
             mdb->coin.is_online?"true":"false", mdb->coin.state, mdb->coin.credit_cents);
             
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, strlen(resp));
}

// Handler OTA Page
static esp_err_t ota_get_handler(httpd_req_t *req)
{
    const char *html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>OTA</title><style>"
        "body{font-family:Arial;background:#f5f5f5;margin:0}header{background:#2c3e50;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".card{background:white;padding:25px;margin:20px auto;max-width:600px;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,0.1)}"
        "input,button{width:100%;padding:10px;margin:10px 0;border-radius:4px;border:1px solid #ddd;box-sizing:border-box}"
        "button{background:#e67e22;color:white;font-weight:bold;cursor:pointer}button:hover{background:#d35400}"
        "</style></head><body><header><div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>🔄 OTA Update</h1></div><a href='/' style='color:white;text-decoration:none;font-weight:bold;background:#34495e;padding:8px 15px;border-radius:4px;'>🏠 Home</a></header><div class='card'>"
        "<form id='f' enctype='multipart/form-data'><input type='file' id='i' accept='.bin' required><button type='submit'>⬆️ Upload</button></form>"
        "<div id='s'></div></div><script>"
        "document.getElementById('f').onsubmit=async function(e){e.preventDefault();"
        "const fd=new FormData();fd.append('f',document.getElementById('i').files[0]);"
        "document.getElementById('s').innerText='Upload in corso...';"
        "try{const r=await fetch('/ota/upload',{method:'POST',body:fd});"
        "if(r.ok) document.getElementById('s').innerText='✅ Successo! Riavvio...';"
        "else document.getElementById('s').innerText='❌ Errore';}catch(e){document.getElementById('s').innerText='❌ Error: '+e;}};"
        "</script></body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, strlen(html));
}

// Handler OTA Upload (POST)
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
    httpd_resp_send(req, "OTA Success, rebooting...", -1);
    vTaskDelay(pdMS_TO_TICKS(1000)); esp_restart();
    return ESP_OK;
}

// Handler OTA URL (POST)
static esp_err_t ota_post_handler(httpd_req_t *req)
{
    char q[256], u[200] = {0};
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) httpd_query_key_value(q, "url", u, sizeof(u));
    if (strlen(u) == 0) return ESP_FAIL;
    perform_ota(u);
    return ESP_OK;
}

// Handler 404
static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t error)
{
    httpd_resp_set_status(req, "404 Not Found");
    return httpd_resp_send(req, "404 Not Found", -1);
}

// Handler per pagina configurazione
static esp_err_t config_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /config");
    
    const char *html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Config</title><style>"
        "body{font-family:Arial;background:#f5f5f5;margin:0}header{background:#2c3e50;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".container{max-width:1000px;margin:20px auto;padding:0 20px}.section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px}.form-group{margin:15px 0}"
        "label{display:block;margin:5px 0;font-weight:bold}input{padding:8px;border:1px solid #ddd;border-radius:4px;width:100%;margin:5px 0}"
        "button{padding:10px 20px;background:#27ae60;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:bold;margin:5px}"
        "button:hover{background:#229954}nav{display:flex;gap:10px;margin:20px 0}nav a{padding:10px 20px;background:#3498db;color:white;text-decoration:none;border-radius:4px}"
        ".indent{margin-left:30px}.checkbox-group{display:flex;align-items:center;gap:10px}"
        "</style></head><body><header><div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>⚙️ Configurazione Device</h1></div><a href='/' style='color:white;text-decoration:none;font-weight:bold;background:#34495e;padding:8px 15px;border-radius:4px;'>🏠 Home</a></header>"
        "<div class='container'>"
        "<nav><a href='/'>Home</a><a href='/config'>Configurazione</a><a href='/stats'>Statistiche</a><a href='/ota'>OTA</a></nav>"
        "<div id='alert'></div>"
        "<form id='configForm'>"
        "<div class='section'><h2>🌐 Ethernet</h2>"
        "<div class='form-group'><div class='checkbox-group'><input type='checkbox' id='eth_en' name='eth_en'><label for='eth_en'>Abilitato</label></div></div>"
        "<div class='indent'><div class='form-group'><div class='checkbox-group'><input type='checkbox' id='eth_dhcp' name='eth_dhcp' checked><label for='eth_dhcp'>DHCP</label></div></div>"
        "<div class='form-group'><label>IP: <input type='text' id='eth_ip' name='eth_ip' placeholder='192.168.1.100'></label></div>"
        "<div class='form-group'><label>Subnet: <input type='text' id='eth_subnet' name='eth_subnet' placeholder='255.255.255.0'></label></div>"
        "<div class='form-group'><label>Gateway: <input type='text' id='eth_gateway' name='eth_gateway' placeholder='192.168.1.1'></label></div></div>"
        "</div>"
        "<div class='section'><h2>📡 WiFi STA</h2>"
        "<div class='form-group'><div class='checkbox-group'><input type='checkbox' id='wifi_en' name='wifi_en'><label for='wifi_en'>Abilitato</label></div></div>"
        "<div class='indent'><div class='form-group'><label>SSID: <input type='text' id='wifi_ssid' name='wifi_ssid'></label></div>"
        "<div class='form-group'><label>Password: <input type='text' id='wifi_pwd' name='wifi_pwd'></label></div>"
        "<div class='form-group'><div class='checkbox-group'><input type='checkbox' id='wifi_dhcp' name='wifi_dhcp' checked><label for='wifi_dhcp'>DHCP</label></div></div></div>"
        "</div>"
        "<div class='section'><h2>🔌 Sensori</h2>"
        "<div class='form-group'><input type='checkbox' id='io_exp' name='io_exp' checked><label for='io_exp'>I/O Expander</label></div>"
        "<div class='form-group'><input type='checkbox' id='temp' name='temp' checked><label for='temp'>Temperatura</label></div>"
        "<div class='form-group'><input type='checkbox' id='led' name='led' checked><label for='led'>LED</label></div>"
        "<div class='form-group'><input type='checkbox' id='rs232' name='rs232' checked><label for='rs232'>RS232</label></div>"
        "<div class='form-group'><input type='checkbox' id='rs485' name='rs485' checked><label for='rs485'>RS485</label></div>"
        "<div class='form-group'><input type='checkbox' id='mdb' name='mdb' checked><label for='mdb'>MDB</label></div>"
        "<div class='form-group'><input type='checkbox' id='pwm1' name='pwm1' checked><label for='pwm1'>PWM1</label></div>"
        "<div class='form-group'><input type='checkbox' id='pwm2' name='pwm2' checked><label for='pwm2'>PWM2</label></div>"
        "</div>"
        "<div class='section'><button type='submit'>💾 Salva</button><button type='button' onclick='resetConfig()'>🔄 Reset</button></div>"
        "</form></div>"
        "<script>"
        "async function resetConfig(){if(confirm(\"Resettare ai valori di fabbrica?\")){await fetch(\"/api/config/reset\",{method:\"POST\"});location.reload();}}"
        "window.addEventListener('load',loadConfig);"
        "async function loadConfig(){"
        "try{const r=await fetch('/api/config');if(!r.ok)throw new Error('HTTP '+r.status);"
        "const c=await r.json();"
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
        "document.getElementById('pwm1').checked=c.sensors.pwm1_enabled;"
        "document.getElementById('pwm2').checked=c.sensors.pwm2_enabled;"
        "}"
        "document.getElementById('configForm').onsubmit=async function(e){e.preventDefault();"
        "const cfg={eth:{enabled:document.getElementById('eth_en').checked,dhcp_enabled:document.getElementById('eth_dhcp').checked,ip:document.getElementById('eth_ip').value,subnet:document.getElementById('eth_subnet').value,gateway:document.getElementById('eth_gateway').value},wifi:{sta_enabled:document.getElementById('wifi_en').checked,dhcp_enabled:document.getElementById('wifi_dhcp').checked,ssid:document.getElementById('wifi_ssid').value,password:document.getElementById('wifi_pwd').value,ip:'',subnet:'',gateway:''},sensors:{io_expander_enabled:document.getElementById('io_exp').checked,temperature_enabled:document.getElementById('temp').checked,led_enabled:document.getElementById('led').checked,rs232_enabled:document.getElementById('rs232').checked,rs485_enabled:document.getElementById('rs485').checked,mdb_enabled:document.getElementById('mdb').checked,pwm1_enabled:document.getElementById('pwm1').checked,pwm2_enabled:document.getElementById('pwm2').checked}};"
        "const r=await fetch('/api/config/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(cfg)});"
        "if(r.ok) alert('Configurazione salvata!'); else alert('Errore!');"
        "}"
        "</script></body></html>";
    
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, strlen(html));
}

// Handler API GET /api/config
static esp_err_t api_config_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /api/config");
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
    cJSON_AddBoolToObject(sensors, "rs232_enabled", cfg->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors, "rs485_enabled", cfg->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors, "mdb_enabled", cfg->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors, "pwm1_enabled", cfg->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors, "pwm2_enabled", cfg->sensors.pwm2_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors);
    
    char *json = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler API POST /api/config/save
static esp_err_t api_config_save(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST /api/config/save");
    
    char buf[2048] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf)-1);
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        const char *resp_str = "{\"error\":\"Invalid JSON\"}";
    httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }
    
    device_config_t *cfg = device_config_get();
    
    cJSON *eth_obj = cJSON_GetObjectItem(root, "eth");
    if (eth_obj) {
        cfg->eth.enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "enabled"));
        cfg->eth.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "dhcp_enabled"));
        cJSON *ip = cJSON_GetObjectItem(eth_obj, "ip");
        if (ip && ip->valuestring) strncpy(cfg->eth.ip, ip->valuestring, sizeof(cfg->eth.ip)-1);
    }
    
    cJSON *wifi_obj = cJSON_GetObjectItem(root, "wifi");
    if (wifi_obj) {
        cfg->wifi.sta_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "sta_enabled"));
        cJSON *ssid = cJSON_GetObjectItem(wifi_obj, "ssid");
        if (ssid && ssid->valuestring) strncpy(cfg->wifi.ssid, ssid->valuestring, sizeof(cfg->wifi.ssid)-1);
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
    }
    
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
    
    const char *html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Statistiche</title><style>"
        "body{font-family:Arial;background:#f5f5f5;margin:0}header{background:#2c3e50;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".container{max-width:1000px;margin:20px auto;padding:0 20px}.section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px}.stat-row{display:flex;justify-content:space-between;padding:10px;border-bottom:1px solid #ecf0f1}"
        ".stat-label{font-weight:bold;color:#34495e}.stat-value{color:#27ae60;font-family:monospace}"
        "nav{display:flex;gap:10px;margin:20px 0}nav a{padding:10px 20px;background:#3498db;color:white;text-decoration:none;border-radius:4px}"
        ".badge{padding:3px 8px;border-radius:3px;font-size:12px;font-weight:bold}.badge-on{background:#27ae60;color:white}.badge-off{background:#95a5a6;color:white}"
        "</style></head><body><header><div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>📊 Statistiche Device</h1></div><a href='/' style='color:white;text-decoration:none;font-weight:bold;background:#34495e;padding:8px 15px;border-radius:4px;'>🏠 Home</a></header>"
        "<div class='container'>"
        "<nav><a href='/'>Home</a><a href='/config'>Configurazione</a><a href='/stats'>Statistiche</a><a href='/ota'>OTA</a></nav>"
        "<div class='section'><h2>🌐 Rete</h2><div id='network'>Caricamento...</div></div>"
        "<div class='section'><h2>💾 Partizioni</h2><div id='partitions'>Caricamento...</div></div>"
        "<div class='section'><h2>🔌 Sensori Abilitati</h2><div id='sensors'>Caricamento...</div></div>"
        "<div class='section'><h2>🎰 MDB Status</h2><div id='mdb_info'>Caricamento...</div></div>"
        "<div class='section'><h2>⏱️ Sistema</h2><div id='system'>Caricamento...</div></div>"
        "</div>"
        "<script>"
        "async function loadStats(){"
        "try{"
        "const r=await fetch('/status');if(!r.ok)throw new Error('Status Error');const status=await r.json();"
        "const rc=await fetch('/api/config');if(!rc.ok)throw new Error('Config Error');const config=await rc.json();"
        "document.getElementById('network').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>IP Ethernet:</span><span class='stat-value'>${status.ip_eth||'Non connesso'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>IP WiFi AP:</span><span class='stat-value'>${status.ip_ap||'Non attivo'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>IP WiFi STA:</span><span class='stat-value'>${status.ip_sta||'Non connesso'}</span></div>`;"
        "document.getElementById('partitions').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Partizione Running:</span><span class='stat-value'>${status.partition_running||'?'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Partizione Boot:</span><span class='stat-value'>${status.partition_boot||'?'}</span></div>`;"
        "const s=config.sensors;"
        "document.getElementById('sensors').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>I/O Expander:</span><span class='badge ${s.io_expander_enabled?'badge-on':'badge-off'}'>${s.io_expander_enabled?'ON':'OFF'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Temperatura:</span><span class='badge ${s.temperature_enabled?'badge-on':'badge-off'}'>${s.temperature_enabled?'ON':'OFF'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>LED:</span><span class='badge ${s.led_enabled?'badge-on':'badge-off'}'>${s.led_enabled?'ON':'OFF'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>RS232:</span><span class='badge ${s.rs232_enabled?'badge-on':'badge-off'}'>${s.rs232_enabled?'ON':'OFF'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>RS485:</span><span class='badge ${s.rs485_enabled?'badge-on':'badge-off'}'>${s.rs485_enabled?'ON':'OFF'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>MDB:</span><span class='badge ${s.mdb_enabled?'badge-on':'badge-off'}'>${s.mdb_enabled?'ON':'OFF'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>PWM1:</span><span class='badge ${s.pwm1_enabled?'badge-on':'badge-off'}'>${s.pwm1_enabled?'ON':'OFF'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>PWM2:</span><span class='badge ${s.pwm2_enabled?'badge-on':'badge-off'}'>${s.pwm2_enabled?'ON':'OFF'}</span></div>`;"
        "const m=status.mdb;"
        "document.getElementById('mdb_info').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Gettoniera:</span><span class='badge ${m.coin_online?'badge-on':'badge-off'}'>${m.coin_online?'ONLINE':'OFFLINE'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Credito:</span><span class='stat-value'>€ ${(m.credit/100).toFixed(2)}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Stato SM:</span><span class='stat-value'>${m.coin_state}</span></div>`;"
        "const uptime=Math.floor(Date.now()/1000);"
        "document.getElementById('system').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Free Heap:</span><span class='stat-value'>Calcolo...</span></div>"
        "<div class='stat-row'><span class='stat-label'>Uptime (browser):</span><span class='stat-value'>${Math.floor(uptime/3600)}h ${Math.floor((uptime%3600)/60)}m</span></div>`;"
        "}catch(e){console.error(e);}"
        "}"
        "window.addEventListener('load',loadStats);"
        "setInterval(loadStats,5000);"
        "</script></body></html>";
    
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, strlen(html));
}

// Handler pagina tasks
static esp_err_t tasks_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /tasks");
    
    const char *html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Tasks Editor</title><style>"
        "body{font-family:Arial;background:#f5f5f5;margin:0}header{background:#2c3e50;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".container{max-width:1200px;margin:20px auto;padding:0 20px}.section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px}nav{display:flex;gap:10px;margin:20px 0}nav a{padding:10px 20px;background:#3498db;color:white;text-decoration:none;border-radius:4px}"
        "table{width:100%;border-collapse:collapse;margin:20px 0}th,td{padding:10px;border:1px solid #ddd;text-align:left}"
        "th{background:#34495e;color:white}input,select{width:100%;padding:5px;border:1px solid #ddd;border-radius:3px;box-sizing:border-box}"
        "button{padding:10px 20px;background:#27ae60;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:bold;margin:5px}"
        "button:hover{background:#229954}.btn-add{background:#3498db}.btn-add:hover{background:#2980b9}"
        "#status{margin:15px 0;padding:10px;border-radius:4px}.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
        ".error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}.warning{background:#fff3cd;color:#856404;border:1px solid #ffeeba}"
        "</style></head><body><header><div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>⚙️ Editor Tasks</h1></div><a href='/' style='color:white;text-decoration:none;font-weight:bold;background:#34495e;padding:8px 15px;border-radius:4px;'>🏠 Home</a></header>"
        "<div class='container'>"
        "<nav><a href='/'>Home</a><a href='/config'>Configurazione</a><a href='/stats'>Statistiche</a><a href='/tasks'>Tasks</a><a href='/ota'>OTA</a></nav>"
        "<div class='section'><h2>📋 Tasks Configuration</h2>"
        "<div id='status'></div>"
        "<table id='tasksTable'><thead><tr>"
        "<th>Name</th><th>State</th><th>Priority</th><th>Core</th><th>Period (ms)</th><th>Stack Words</th>"
        "</tr></thead><tbody id='tasksBody'>Caricamento...</tbody></table>"
        "<button type='button' class='btn-add' onclick='addRow()'>➕ Aggiungi Task</button>"
        "<button type='button' onclick='saveTasks()'>💾 Salva</button>"
        "<button type='button' onclick='location.reload()'>🔄 Ricarica</button>"
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
    
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, strlen(html));
}

// Handler API GET /api/tasks
static esp_err_t api_tasks_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /api/tasks");
    
    FILE *f = fopen("/spiffs/tasks.csv", "r");
    if (!f) {
        ESP_LOGE(TAG, "[C] Impossibile aprire tasks.csv");
        httpd_resp_set_status(req, "500");
        const char *resp_str = "{\"error\":\"File not found\"}";
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
        const char *resp_str = "{\"error\":\"No data\"}";
        httpd_resp_send(req, resp_str, strlen(resp_str));
        return ESP_OK;
    }
    
    cJSON *root = cJSON_Parse(buf);
    if (!root || !cJSON_IsArray(root)) {
        const char *resp_str = "{\"error\":\"Invalid JSON\"}";
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
        const char *resp_str = "{\"error\":\"Cannot write file\"}";
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
    
    const char *html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Test Hardware</title><style>"
        "body{font-family:Arial;background:#f5f5f5;margin:0}header{background:#2c3e50;color:white;padding:10px 20px;display:flex;align-items:center;justify-content:space-between}"
        ".container{max-width:1000px;margin:20px auto;padding:0 20px}.section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "h2{color:#2c3e50;border-bottom:2px solid #e67e22;padding-bottom:10px}nav{display:flex;gap:10px;margin:20px 0}nav a{padding:10px 20px;background:#3498db;color:white;text-decoration:none;border-radius:4px}"
        ".test-item{display:flex;align-items:center;justify-content:space-between;padding:15px;margin:10px 0;background:#ecf0f1;border-radius:6px}"
        ".test-label{font-weight:bold;color:#2c3e50;flex:1}.test-controls{display:flex;gap:10px;align-items:center}"
        "button{padding:8px 16px;background:#e67e22;color:white;border:none;border-radius:4px;cursor:pointer;font-weight:bold}"
        "button:hover{background:#d35400}.btn-stop{background:#e74c3c}.btn-stop:hover{background:#c0392b}"
        "input[type=text],input[type=number]{padding:6px;border:1px solid #bdc3c7;border-radius:4px;width:120px}"
        ".status-box{padding:10px;margin:10px 0;border-radius:4px;font-family:monospace;font-size:13px;background:#34495e;color:#ecf0f1;min-height:60px;overflow-y:auto;max-height:200px}"
        ".result{margin:5px 0;padding:5px;border-left:3px solid #3498db}"
        "</style></head><body><header><div style='display:flex;align-items:center;'><img src='/logo.jpg' alt='Logo' style='max-height:40px;margin-right:15px;'><h1 style='margin:0;font-size:22px;'>🔧 Test Hardware</h1></div><a href='/' style='color:white;text-decoration:none;font-weight:bold;background:#34495e;padding:8px 15px;border-radius:4px;'>🏠 Home</a></header>"
        "<div class='container'>"
        "<nav><a href='/'>Home</a><a href='/config'>Configurazione</a><a href='/stats'>Statistiche</a><a href='/tasks'>Tasks</a><a href='/test'>Test</a><a href='/ota'>OTA</a></nav>"
        
        "<div class='section'><h2>💡 LED Stripe (WS2812)</h2>"
        "<div class='test-item'><span class='test-label'>Full RGB Pattern (40s)</span>"
        "<div class='test-controls'><button onclick=\"runTest('led_start')\">▶️ Start</button><button class='btn-stop' onclick=\"runTest('led_stop')\">⏹️ Stop</button></div></div>"
        "<div id='led_status' class='status-box'>Pronto per test LED</div></div>"

        "<div class='section'><h2>🔌 I/O Expander</h2>"
        "<div class='test-item'><span class='test-label'>Blink All Ports (1Hz)</span>"
        "<div class='test-controls'><button onclick=\"runTest('ioexp_start')\">▶️ Start</button><button class='btn-stop' onclick=\"runTest('ioexp_stop')\">⏹️ Stop</button></div></div>"
        "<div id='ioexp_status' class='status-box'>Pronto per test I/O Expander</div></div>"
        
        "<div class='section'><h2>⚡ PWM</h2>"
        "<div class='test-item'><span class='test-label'>PWM1 Duty Cycle Sweep</span>"
        "<div class='test-controls'><button onclick=\"runTest('pwm1_start')\">▶️ Start</button><button class='btn-stop' onclick=\"runTest('pwm1_stop')\">⏹️ Stop</button></div></div>"
        "<div class='test-item'><span class='test-label'>PWM2 Duty Cycle Sweep</span>"
        "<div class='test-controls'><button onclick=\"runTest('pwm2_start')\">▶️ Start</button><button class='btn-stop' onclick=\"runTest('pwm2_stop')\">⏹️ Stop</button></div></div>"
        "<div id='pwm_status' class='status-box'>Pronto per test PWM</div></div>"
        
        "<div class='section'><h2>📡 Seriale RS232</h2>"
        "<div class='test-item'><span>Invia Stringa (es: \\0x55\\0xAA\\r\\n)</span>"
        "<div class='test-controls'><input type='text' id='rs232_input' placeholder='\\0x55 Test...'><button onclick=\"sendSerial('rs232')\">🚀 Invia</button></div></div>"
        "<div id='rs232_status' class='status-box'>Monitor:</div></div>"
        
        "<div class='section'><h2>📡 Seriale RS485</h2>"
        "<div class='test-item'><span>Invia Stringa</span>"
        "<div class='test-controls'><input type='text' id='rs485_input' placeholder='Richiesta...'><button onclick=\"sendSerial('rs485')\">🚀 Invia</button></div></div>"
        "<div id='rs485_status' class='status-box'>Monitor:</div></div>"
        
        "<div class='section'><h2>🎰 MDB (Multi-Drop Bus)</h2>"
        "<div class='test-item'><span class='test-label'>Test Loopback/Echo</span>"
        "<div class='test-controls'><button onclick=\"runTest('mdb_start')\">▶️ Start</button><button class='btn-stop' onclick=\"runTest('mdb_stop')\">⏹️ Stop</button></div></div>"
        "<div id='mdb_status' class='status-box'>Pronto per test MDB</div></div>"
        
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
        "const logLines = res.log.split(' ');" // Semplificato
        "document.getElementById('rs232_status').innerText = res.log;"
        "document.getElementById('rs485_status').innerText = res.log;"
        "}"
        "}"
        "setInterval(updateMonitors, 1000);"
        "</script></body></html>";
    
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, html, strlen(html));
}

// Handler API POST /api/test/*
static esp_err_t api_test_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] POST %s", req->uri);
    const char *test_name = req->uri + strlen("/api/test/");
    char response[256] = {0};

    // --- LED TEST ---
    if (strcmp(test_name, "led_start") == 0) {
        if (led_test_start() == ESP_OK) {
            snprintf(response, sizeof(response), "{\"message\":\"Test LED avviato\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "led_stop") == 0) {
        led_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test LED fermato\"}");
    }

    // --- PWM1 TEST ---
    else if (strcmp(test_name, "pwm1_start") == 0) {
        pwm_test_start(1);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM1 avviato\"}");
    } else if (strcmp(test_name, "pwm1_stop") == 0) {
        pwm_test_stop(1);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM1 fermato\"}");
    }

    // --- PWM2 TEST ---
    else if (strcmp(test_name, "pwm2_start") == 0) {
        pwm_test_start(2);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM2 avviato\"}");
    } else if (strcmp(test_name, "pwm2_stop") == 0) {
        pwm_test_stop(2);
        snprintf(response, sizeof(response), "{\"message\":\"Test PWM2 fermato\"}");
    }

    // --- I/O EXPANDER TEST ---
    else if (strcmp(test_name, "ioexp_start") == 0) {
        io_expander_test_start();
        snprintf(response, sizeof(response), "{\"message\":\"Test I/O Expander avviato (1Hz)\"}");
    } else if (strcmp(test_name, "ioexp_stop") == 0) {
        io_expander_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test I/O Expander fermato\"}");
    }

    // --- RS232 TEST ---
    else if (strcmp(test_name, "rs232_start") == 0) {
        if (!s_rs232_test_handle) {
            xTaskCreate(uart_test_task, "rs232_test", 2048, (void*)CONFIG_APP_RS232_UART_PORT, 5, &s_rs232_test_handle);
            snprintf(response, sizeof(response), "{\"message\":\"Test RS232 avviato (caratteri 55,AA,01,07)\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "rs232_stop") == 0) {
        if (s_rs232_test_handle) { vTaskDelete(s_rs232_test_handle); s_rs232_test_handle = NULL; }
        snprintf(response, sizeof(response), "{\"message\":\"Test RS232 fermato\"}");
    }

    // --- RS485 TEST ---
    else if (strcmp(test_name, "rs485_start") == 0) {
        if (!s_rs485_test_handle) {
            xTaskCreate(uart_test_task, "rs485_test", 2048, (void*)CONFIG_APP_RS485_UART_PORT, 5, &s_rs485_test_handle);
            snprintf(response, sizeof(response), "{\"message\":\"Test RS485 avviato (caratteri 55,AA,01,07)\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "rs485_stop") == 0) {
        if (s_rs485_test_handle) { vTaskDelete(s_rs485_test_handle); s_rs485_test_handle = NULL; }
        snprintf(response, sizeof(response), "{\"message\":\"Test RS485 fermato\"}");
    }
    
    // --- MDB TEST ---
    if (strcmp(test_name, "mdb_start") == 0) {
        if (mdb_test_start() == ESP_OK) {
            snprintf(response, sizeof(response), "{\"message\":\"Test MDB avviato\"}");
        } else snprintf(response, sizeof(response), "{\"error\":\"Già in esecuzione\"}");
    } else if (strcmp(test_name, "mdb_stop") == 0) {
        mdb_test_stop();
        snprintf(response, sizeof(response), "{\"message\":\"Test MDB fermato\"}");
    }
    
    // --- SERIAL SEND TEST (RS232/RS485) ---
    else if (strcmp(test_name, "serial_send") == 0) {
        char buf[512] = {0};
        int len_req = httpd_req_recv(req, buf, sizeof(buf)-1);
        cJSON *root = cJSON_Parse(buf);
        if (root) {
            const char *port_raw = cJSON_GetStringValue(cJSON_GetObjectItem(root, "port"));
            const char *data_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "data"));
            int port = (port_raw && strcmp(port_raw, "rs485") == 0) ? CONFIG_APP_RS485_UART_PORT : CONFIG_APP_RS232_UART_PORT;
            if (data_str) {
                serial_test_send_uart(port, data_str);
                snprintf(response, sizeof(response), "{\"status\":\"ok\",\"message\":\"Inviato su %s\"}", port_raw);
            }
            cJSON_Delete(root);
        } else snprintf(response, sizeof(response), "{\"error\":\"Invalid JSON\"}");
    }
    
    // --- SERIAL MONITOR ---
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
    
    httpd_uri_t uri_api_reset = {.uri = "/api/config/reset", .method = HTTP_POST, .handler = api_config_reset};
    httpd_register_uri_handler(s_server, &uri_api_reset);

    httpd_uri_t uri_api_tasks = {.uri = "/api/tasks", .method = HTTP_GET, .handler = api_tasks_get};
    httpd_register_uri_handler(s_server, &uri_api_tasks);
    
    httpd_uri_t uri_api_tasks_save = {.uri = "/api/tasks/save", .method = HTTP_POST, .handler = api_tasks_save};
    httpd_register_uri_handler(s_server, &uri_api_tasks_save);
    
    httpd_uri_t uri_api_test = {.uri = "/api/test/*", .method = HTTP_POST, .handler = api_test_handler};
    httpd_register_uri_handler(s_server, &uri_api_test);

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
