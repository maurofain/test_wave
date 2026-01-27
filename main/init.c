#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_eth_mac_esp.h"
#include "esp_spiffs.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/ledc.h"
#include "driver/rmt_tx.h"
#include "led_strip.h"
#include "lwip/ip4_addr.h"
#include "init.h"
#include "sdkconfig.h"

static const char *TAG = "INIT";

static esp_netif_t *s_netif_ap;
static esp_netif_t *s_netif_sta;
static esp_netif_t *s_netif_eth;
static httpd_handle_t s_httpd;
static led_strip_handle_t s_led_strip;

// -----------------------------------------------------------------------------
// Inizializzazione hardware
// -----------------------------------------------------------------------------

static esp_err_t init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_APP_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_APP_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_APP_I2C_CLOCK_HZ,
    };

    ESP_RETURN_ON_ERROR(i2c_param_config(CONFIG_APP_I2C_PORT, &conf), TAG, "I2C param config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(CONFIG_APP_I2C_PORT, conf.mode, 0, 0, 0), TAG, "I2C install failed");
    ESP_LOGI(TAG, "[F] I2C inizializzato su porta %d (SDA=%d, SCL=%d)", CONFIG_APP_I2C_PORT, CONFIG_APP_I2C_SDA_GPIO, CONFIG_APP_I2C_SCL_GPIO);
    return ESP_OK;
}

static esp_err_t init_boot_button(void)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << CONFIG_APP_BOOT_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "Boot button config failed");
    ESP_LOGI(TAG, "[F] Pulsante BOOT configurato su GPIO %d", CONFIG_APP_BOOT_BUTTON_GPIO);
    return ESP_OK;
}

static esp_err_t init_ws2812(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_APP_WS2812_GPIO,
        .max_leds = CONFIG_APP_WS2812_LEDS,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config_strip = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };

    ESP_RETURN_ON_ERROR(led_strip_new_rmt_device(&strip_config, &rmt_config_strip, &s_led_strip), TAG, "LED strip create failed");
    ESP_LOGI(TAG, "[F] Striscia WS2812 inizializzata su GPIO %d (%d LED)", CONFIG_APP_WS2812_GPIO, CONFIG_APP_WS2812_LEDS);
    return ESP_OK;
}

static esp_err_t init_uart_rs232(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    const uart_port_t port = CONFIG_APP_RS232_UART_PORT;
    ESP_RETURN_ON_ERROR(uart_driver_install(port, 2048, 0, 0, NULL, 0), TAG, "RS232 driver install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(port, &cfg), TAG, "RS232 param config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(port, CONFIG_APP_RS232_TX_GPIO, CONFIG_APP_RS232_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "RS232 pin config failed");
    ESP_LOGI(TAG, "[F] UART RS232 inizializzata su porta %d (TX=%d, RX=%d)", port, CONFIG_APP_RS232_TX_GPIO, CONFIG_APP_RS232_RX_GPIO);
    return ESP_OK;
}

static esp_err_t init_uart_rs485(void)
{
#if SOC_UART_NUM >= 3
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    const uart_port_t port = CONFIG_APP_RS485_UART_PORT;
    ESP_RETURN_ON_ERROR(uart_driver_install(port, 2048, 0, 0, NULL, 0), TAG, "RS485 driver install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(port, &cfg), TAG, "RS485 param config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(port, CONFIG_APP_RS485_TX_GPIO, CONFIG_APP_RS485_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "RS485 pin config failed");
    ESP_RETURN_ON_ERROR(uart_set_mode(port, UART_MODE_RS485_HALF_DUPLEX), TAG, "RS485 mode set failed");

    gpio_set_direction(CONFIG_APP_RS485_DE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_APP_RS485_DE_GPIO, 0);

    ESP_LOGI(TAG, "[F] UART RS485 inizializzata su porta %d (TX=%d, RX=%d, DE=%d)", port, CONFIG_APP_RS485_TX_GPIO, CONFIG_APP_RS485_RX_GPIO, CONFIG_APP_RS485_DE_GPIO);
    return ESP_OK;
#else
    ESP_LOGW(TAG, "[F] RS485 non inizializzato: target ha solo %d UART", SOC_UART_NUM);
    return ESP_OK;
#endif
}

static esp_err_t init_uart_mdb(void)
{
#if SOC_UART_NUM >= 4
    uart_config_t cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    const uart_port_t port = CONFIG_APP_MDB_UART_PORT;
    ESP_RETURN_ON_ERROR(uart_driver_install(port, 2048, 0, 0, NULL, 0), TAG, "MDB driver install failed");
    ESP_RETURN_ON_ERROR(uart_param_config(port, &cfg), TAG, "MDB param config failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(port, CONFIG_APP_MDB_TX_GPIO, CONFIG_APP_MDB_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "MDB pin config failed");
    ESP_LOGI(TAG, "[F] UART MDB inizializzata su porta %d (TX=%d, RX=%d)", port, CONFIG_APP_MDB_TX_GPIO, CONFIG_APP_MDB_RX_GPIO);
    return ESP_OK;
#else
    ESP_LOGW(TAG, "[F] MDB non inizializzato: target ha solo %d UART", SOC_UART_NUM);
    return ESP_OK;
#endif
}

static esp_err_t init_pwm(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,  // Ridotto da 12-bit per compatibilità
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,  // Ridotto da 20kHz a 5kHz
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "LEDC timer config failed");

    ledc_channel_config_t ch0 = {
        .gpio_num = CONFIG_APP_PWM_OUT1_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config_t ch1 = {
        .gpio_num = CONFIG_APP_PWM_OUT2_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };

    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch0), TAG, "PWM channel 0 config failed");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch1), TAG, "PWM channel 1 config failed");
    ESP_LOGI(TAG, "[F] Uscite PWM configurate su GPIO %d e GPIO %d", CONFIG_APP_PWM_OUT1_GPIO, CONFIG_APP_PWM_OUT2_GPIO);
    return ESP_OK;
}

static void log_sht40_stub(void)
{
    ESP_LOGI(TAG, "[F] Sensore SHT40 stub: aggiungere driver sensore ESP-IDF quando disponibile (I2C %d)", CONFIG_APP_I2C_PORT);
}

// -----------------------------------------------------------------------------
// Rete + HTTP + OTA (factory)
// -----------------------------------------------------------------------------

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "NVS init failed");
    return ESP_OK;
}

static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[F] Montaggio SPIFFS fallito: %s", esp_err_to_name(ret));
        return ret;
    }
    size_t total = 0, used = 0;
    if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK) {
        ESP_LOGI(TAG, "[F] SPIFFS montato: totale=%u, usato=%u", (unsigned)total, (unsigned)used);
    }
    return ESP_OK;
}

static void log_partitions(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    ESP_LOGI(TAG, "[F] Partizione in esecuzione: %s (tipo %d, sottotipo %d)", running ? running->label : "?", running ? running->type : -1, running ? running->subtype : -1);
    ESP_LOGI(TAG, "[F] Partizione boot      : %s (tipo %d, sottotipo %d)", boot ? boot->label : "?", boot ? boot->type : -1, boot ? boot->subtype : -1);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    switch (event_id) {
    case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG, "[F] Access Point Wi-Fi avviato (SSID: %s)", CONFIG_APP_WIFI_AP_SSID);
        break;
    case WIFI_EVENT_AP_STACONNECTED: {
        wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "[F] Client connesso ad AP: AID=%d", e->aid);
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
        wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "[F] Client disconnesso da AP: AID=%d", e->aid);
        break;
    }
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "[F] Avvio STA Wi-Fi");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGW(TAG, "Wi-Fi STA disconnected, retrying...");
        esp_wifi_connect();
        break;
    default:
        break;
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "[F] STA Wi-Fi ha ottenuto IP: %s", ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.ip));
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "[F] Ethernet got IP: %s (netmask: %s, gateway: %s)", 
                 ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.ip),
                 ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.netmask),
                 ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.gw));
    }
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "[F] Collegamento Ethernet attivo");
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "[F] Collegamento Ethernet inattivo");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "[F] Driver Ethernet avviato");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGW(TAG, "[F] Driver Ethernet fermato");
        break;
    default:
        break;
    }
}

static esp_err_t init_event_loop(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // [F] WiFi disabled due to linker issues
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    return ESP_OK;
}

static esp_err_t start_wifi(void)
{
    // Use empty initializer to get default values for all fields
    wifi_init_config_t cfg = { 0 };
    // Now override only the essential ones that would be set by WIFI_INIT_CONFIG_DEFAULT()
    cfg.static_rx_buf_num = 16;
    cfg.dynamic_rx_buf_num = 32;
    cfg.tx_buf_type = 0;
    cfg.static_tx_buf_num = 16;
    cfg.dynamic_tx_buf_num = 32;
    cfg.rx_mgmt_buf_type = 1;
    cfg.rx_mgmt_buf_num = 10;
    cfg.espnow_max_encrypt_num = 2;
    cfg.magic = WIFI_INIT_CONFIG_MAGIC;
    
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "Wi-Fi init failed");
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    s_netif_ap = esp_netif_create_default_wifi_ap();
#if CONFIG_APP_WIFI_STA_ENABLE
    s_netif_sta = esp_netif_create_default_wifi_sta();
#endif

    wifi_config_t ap_cfg = { 0 };
    snprintf((char *)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid), "%s", CONFIG_APP_WIFI_AP_SSID);
    ap_cfg.ap.ssid_len = strlen(CONFIG_APP_WIFI_AP_SSID);
    snprintf((char *)ap_cfg.ap.password, sizeof(ap_cfg.ap.password), "%s", CONFIG_APP_WIFI_AP_PASSWORD);
    ap_cfg.ap.channel = CONFIG_APP_WIFI_AP_CHANNEL;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = strlen(CONFIG_APP_WIFI_AP_PASSWORD) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    if (ap_cfg.ap.authmode == WIFI_AUTH_OPEN) {
        ap_cfg.ap.password[0] = '\0';
    }

#if CONFIG_APP_WIFI_STA_ENABLE
    wifi_config_t sta_cfg = { 0 };
    snprintf((char *)sta_cfg.sta.ssid, sizeof(sta_cfg.sta.ssid), "%s", CONFIG_APP_WIFI_STA_SSID);
    snprintf((char *)sta_cfg.sta.password, sizeof(sta_cfg.sta.password), "%s", CONFIG_APP_WIFI_STA_PASSWORD);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_mode_t mode = WIFI_MODE_APSTA;
#else
    wifi_mode_t mode = WIFI_MODE_AP;
#endif

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(mode), TAG, "Wi-Fi set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg), TAG, "Wi-Fi AP config failed");

#if CONFIG_APP_WIFI_STA_ENABLE
    if (strlen(CONFIG_APP_WIFI_STA_SSID) > 0) {
        ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg), TAG, "Wi-Fi STA config failed");
    }
#endif

    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "Wi-Fi start failed");
    ESP_LOGI(TAG, "[F] Wi-Fi avviato: AP SSID=%s, canale=%d", CONFIG_APP_WIFI_AP_SSID, CONFIG_APP_WIFI_AP_CHANNEL);

    return ESP_OK;
}

static esp_err_t start_ethernet(void)
{
#if CONFIG_APP_ETH_ENABLED
    // Check for GPIO conflicts
    if (CONFIG_APP_ETH_MDC_GPIO == CONFIG_APP_MDB_RX_GPIO) {
        ESP_LOGW(TAG, "[F] ATTENZIONE: Conflitto GPIO! ETH_MDC_GPIO (%d) e MDB_RX_GPIO (%d) usano lo stesso pin", 
                 CONFIG_APP_ETH_MDC_GPIO, CONFIG_APP_MDB_RX_GPIO);
    }

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif_eth = esp_netif_new(&netif_cfg);
    if (!s_netif_eth) {
        ESP_LOGE(TAG, "[F] Impossibile allocare Ethernet netif");
        return ESP_FAIL;
    }

    // Enable DHCP for Ethernet
    ESP_RETURN_ON_ERROR(esp_netif_dhcpc_start(s_netif_eth), TAG, "DHCP client start failed");

    // Manual PHY reset if reset GPIO is configured
    if (CONFIG_APP_ETH_PHY_RST_GPIO >= 0) {
        ESP_LOGI(TAG, "[F] Reset PHY Ethernet su GPIO %d", CONFIG_APP_ETH_PHY_RST_GPIO);
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << CONFIG_APP_ETH_PHY_RST_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "PHY reset GPIO config failed");
        
        // Reset sequence: low -> delay -> high
        gpio_set_level(CONFIG_APP_ETH_PHY_RST_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay
        gpio_set_level(CONFIG_APP_ETH_PHY_RST_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(50)); // Wait 50ms after reset before initializing PHY
    }

    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    
    // Update PHY config based on board specific configuration
    phy_config.phy_addr = CONFIG_APP_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_APP_ETH_PHY_RST_GPIO;
    
    // Init vendor specific MAC config to default
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // Update vendor specific MAC config based on board configuration
    esp32_emac_config.smi_gpio.mdc_num = CONFIG_APP_ETH_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = CONFIG_APP_ETH_MDIO_GPIO;
    
    ESP_LOGI(TAG, "[F] Inizializzazione Ethernet: MDC=%d, MDIO=%d, PHY_ADDR=%d, RST_GPIO=%d",
             CONFIG_APP_ETH_MDC_GPIO, CONFIG_APP_ETH_MDIO_GPIO, 
             CONFIG_APP_ETH_PHY_ADDR, CONFIG_APP_ETH_PHY_RST_GPIO);
    
    // Create new ESP32 Ethernet MAC instance
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    if (!mac) {
        ESP_LOGE(TAG, "[F] Impossibile creare Ethernet MAC");
        return ESP_FAIL;
    }
    
    // Create new PHY instance (IP101 for ESP32-P4 Module DEV KIT)
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
    if (!phy) {
        ESP_LOGE(TAG, "[F] Impossibile creare Ethernet PHY");
        mac->del(mac);
        return ESP_FAIL;
    }
    
    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_err_t ret = esp_eth_driver_install(&eth_config, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[F] Installazione driver Ethernet fallita: %s", esp_err_to_name(ret));
        phy->del(phy);
        mac->del(mac);
        return ret;
    }
    
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_RETURN_ON_ERROR(esp_netif_attach(s_netif_eth, glue), TAG, "Netif attach failed");
    
    ret = esp_eth_start(eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[F] Avvio Ethernet fallito: %s", esp_err_to_name(ret));
        esp_eth_del_netif_glue(glue);
        esp_eth_driver_uninstall(eth_handle);
        phy->del(phy);
        mac->del(mac);
        return ret;
    }
    
    ESP_LOGI(TAG, "[F] Ethernet avviato (indirizzo PHY %d, DHCP abilitato)", CONFIG_APP_ETH_PHY_ADDR);
#else
    ESP_LOGI(TAG, "[F] Ethernet disabilitato (CONFIG_APP_ETH_ENABLED=n)");
#endif
    return ESP_OK;
}

static void ip_to_str(esp_netif_t *netif, char *out, size_t len)
{
    if (!netif || !out) {
        return;
    }
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(netif, &info) == ESP_OK) {
        ip4addr_ntoa_r((const ip4_addr_t *)&info.ip, out, len);
    }
}

static esp_err_t perform_ota(const char *url)
{
    if (!url || strlen(url) == 0) {
        ESP_LOGE(TAG, "OTA URL missing");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting OTA from %s", url);
    esp_http_client_config_t client_cfg = {
        .url = url,
        .timeout_ms = 15000,
        .cert_pem = NULL,
        .skip_cert_common_name_check = true,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &client_cfg,
    };

    esp_err_t ret = esp_https_ota(&ota_cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA successful. Rebooting into new image...");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char ap_ip[16] = "0.0.0.0";
    char sta_ip[16] = "0.0.0.0";
    char eth_ip[16] = "0.0.0.0";
    ip_to_str(s_netif_ap, ap_ip, sizeof(ap_ip));
    ip_to_str(s_netif_sta, sta_ip, sizeof(sta_ip));
    ip_to_str(s_netif_eth, eth_ip, sizeof(eth_ip));

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();

    char resp[512];
    int len = snprintf(resp, sizeof(resp),
                       "{\n"
                       "  \"partition_running\": \"%s\",\n"
                       "  \"partition_boot\": \"%s\",\n"
                       "  \"ip_ap\": \"%s\",\n"
                       "  \"ip_sta\": \"%s\",\n"
                       "  \"ip_eth\": \"%s\"\n"
                       "}\n",
                       running ? running->label : "?",
                       boot ? boot->label : "?",
                       ap_ip, sta_ip, eth_ip);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, len);
}

static esp_err_t ota_post_handler(httpd_req_t *req)
{
    char query[256];
    char url[200] = {0};

    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "url", url, sizeof(url));
    }
    if (strlen(url) == 0 && strlen(CONFIG_APP_OTA_DEFAULT_URL) > 0) {
        snprintf(url, sizeof(url), "%s", CONFIG_APP_OTA_DEFAULT_URL);
    }

    esp_err_t err = perform_ota(url);
    if (err != ESP_OK) {
        const char *msg = "OTA failed\n";
        httpd_resp_set_status(req, "500");
        httpd_resp_send(req, msg, strlen(msg));
        return ESP_OK;
    }

    const char *msg = "OTA started\n";
    httpd_resp_send(req, msg, strlen(msg));
    return ESP_OK;
}

static esp_err_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = CONFIG_APP_HTTP_PORT;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&s_httpd, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_get_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_httpd, &status_uri);

    httpd_uri_t ota_uri = {
        .uri = "/ota",
        .method = HTTP_POST,
        .handler = ota_post_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_httpd, &ota_uri);

    ESP_LOGI(TAG, "[F] Server HTTP avviato sulla porta %d", config.server_port);
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

esp_err_t init_run_factory(void)
{
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_spiffs());
    log_partitions();
    ESP_ERROR_CHECK(init_event_loop());
    // WiFi temporarily disabled due to linker issues (will be re-enabled after resolution)
    // ESP_ERROR_CHECK(start_wifi());
    // Ethernet initialization - continue even if it fails (non-critical for factory setup)
    esp_err_t eth_ret = start_ethernet();
    if (eth_ret != ESP_OK) {
        ESP_LOGW(TAG, "[F] Ethernet non disponibile, continuo senza Ethernet");
    }
    ESP_ERROR_CHECK(start_http_server());

    ESP_ERROR_CHECK(init_i2c());
    ESP_ERROR_CHECK(init_boot_button());
    ESP_ERROR_CHECK(init_ws2812());
    ESP_ERROR_CHECK(init_uart_rs232());
    ESP_ERROR_CHECK(init_uart_rs485());
    ESP_ERROR_CHECK(init_uart_mdb());
    ESP_ERROR_CHECK(init_pwm());

    log_sht40_stub();
    return ESP_OK;
}

led_strip_handle_t init_get_ws2812_handle(void)
{
    return s_led_strip;
}
