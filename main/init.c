#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
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
#include "led_strip.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "init.h"
#include "led.h"
#include "device_config.h"
#include "mdb.h"
#include "web_ui.h"
#include "sdkconfig.h"
#include "io_expander.h"
#include "pwm.h"
#include "rs232.h"
#include "rs485.h"
#include "serial_test.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "INIT";

static esp_netif_t *s_netif_ap;
static esp_netif_t *s_netif_sta;
static esp_netif_t *s_netif_eth;
static esp_eth_handle_t s_eth_handle;

// -----------------------------------------------------------------------------
// Rete + HTTP + OTA (factory)
// -----------------------------------------------------------------------------

static void nvs_list_entries(void)
{
    nvs_iterator_t it = NULL;
    esp_err_t ret = nvs_entry_find(NULL, NULL, NVS_TYPE_ANY, &it);
    
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "=== NVS vuota (no entries) ===");
        return;
    }
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Avviso lettura NVS: %s (verranno usate le defaults)", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "=== Elenco NVS Entries ===");
    int count = 0;
    
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        count++;
        ESP_LOGI(TAG, "  [%d] key='%s', type=%d", count, info.key, info.type);
        ret = nvs_entry_next(&it);
        if (ret != ESP_OK) break;
    }
    
    ESP_LOGI(TAG, "=== Totale: %d entries ===", count);
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "NVS init failed");
    
    // Mostra l'elenco dei file/entries nella NVS
    nvs_list_entries();
    
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
    
    // Elenca file in SPIFFS
    DIR *dir = opendir("/spiffs");
    if (dir) {
        ESP_LOGI(TAG, "[F] === Elenco file SPIFFS ===");
        struct dirent *entry;
        int file_count = 0;
        while ((entry = readdir(dir)) != NULL) {
            file_count++;
            char filepath[300];
            snprintf(filepath, sizeof(filepath), "/spiffs/%s", entry->d_name);
            struct stat st;
            if (stat(filepath, &st) == 0) {
                ESP_LOGI(TAG, "[F]   [%d] %s (%ld bytes)", file_count, entry->d_name, st.st_size);
            } else {
                ESP_LOGI(TAG, "[F]   [%d] %s", file_count, entry->d_name);
            }
        }
        closedir(dir);
        ESP_LOGI(TAG, "[F] === Totale file: %d ===", file_count);
    }
    
    // Leggi e mostra tasks.csv
    FILE *f = fopen("/spiffs/tasks.csv", "r");
    if (f) {
        ESP_LOGI(TAG, "[F] === Contenuto tasks.csv ===");
        char line[256];
        int line_num = 0;
        while (fgets(line, sizeof(line), f)) {
            line_num++;
            // Rimuovi newline
            line[strcspn(line, "\r\n")] = 0;
            ESP_LOGI(TAG, "[F]   [%d] %s", line_num, line);
        }
        fclose(f);
        ESP_LOGI(TAG, "[F] ================================");
    } else {
        ESP_LOGW(TAG, "[F] File tasks.csv non trovato");
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
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        ESP_LOGI(TAG, "[F] Ethernet Got IP Address");
        ESP_LOGI(TAG, "[F] ~~~~~~~~~~~");
        ESP_LOGI(TAG, "[F] ETHIP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "[F] ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "[F] ETHGW:" IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "[F] ~~~~~~~~~~~");

        if (s_netif_eth) {
            struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(s_netif_eth);
            if (lwip_netif) {
                etharp_gratuitous(lwip_netif);
                ESP_LOGI(TAG, "[F] Gratuitous ARP inviato");
            }
        }
    }
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        {
            uint8_t mac_addr[6] = {0};
            esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(TAG, "[F] Collegamento Ethernet attivo");
            ESP_LOGI(TAG, "[F] Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                     mac_addr[0], mac_addr[1], mac_addr[2],
                     mac_addr[3], mac_addr[4], mac_addr[5]);
        }
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
    // Nota: esp_netif_init() e esp_event_loop_create_default() vengono chiamati in start_ethernet()
    // DOPO aver inizializzato il driver Ethernet, come nell'esempio funzionante
    // Questa funzione ora è vuota perché tutto viene fatto in start_ethernet()
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

static esp_eth_handle_t eth_init_internal(void)
{
    esp_err_t ret = ESP_OK;
    // Init common MAC and PHY configs to default
    // Nota: Il reset PHY viene gestito automaticamente dal driver quando phy_config.reset_gpio_num è configurato
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

    // Create new ESP32 Ethernet MAC instance
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    // Create new PHY instance (IP101 for ESP32-P4 Module DEV KIT)
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&eth_config, &eth_handle) == ESP_OK, ESP_FAIL,
                      err, TAG, "Ethernet driver install failed");

    return eth_handle;
err:
    if (eth_handle != NULL) {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    return NULL;
}

static esp_err_t start_ethernet(void)
{
#if CONFIG_APP_ETH_ENABLED
    esp_err_t ret = ESP_OK;
    
    // Check for GPIO conflicts
    if (CONFIG_APP_ETH_MDC_GPIO == CONFIG_APP_MDB_RX_GPIO) {
        ESP_LOGW(TAG, "[F] ATTENZIONE: Conflitto GPIO! ETH_MDC_GPIO (%d) e MDB_RX_GPIO (%d) usano lo stesso pin", 
                 CONFIG_APP_ETH_MDC_GPIO, CONFIG_APP_MDB_RX_GPIO);
    }

    ESP_LOGI(TAG, "[F] Inizializzazione Ethernet: MDC=%d, MDIO=%d, PHY_ADDR=%d, RST_GPIO=%d",
             CONFIG_APP_ETH_MDC_GPIO, CONFIG_APP_ETH_MDIO_GPIO, 
             CONFIG_APP_ETH_PHY_ADDR, CONFIG_APP_ETH_PHY_RST_GPIO);

    // Init Ethernet driver to default and install it
    esp_eth_handle_t eth_handle = eth_init_internal();
    if (!eth_handle) {
        ESP_LOGE(TAG, "[F] Installazione driver Ethernet fallita");
        return ESP_FAIL;
    }
    
    // Salva l'handle Ethernet per riferimento futuro
    s_eth_handle = eth_handle;
    
    // Inizializza esp_netif e event loop DOPO aver installato il driver (come nell'esempio funzionante)
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "esp_event_loop_create_default failed");
    
    // Create netif AFTER installing the driver and initializing netif (come nell'esempio funzionante)
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif_eth = esp_netif_new(&netif_cfg);
    if (!s_netif_eth) {
        ESP_LOGE(TAG, "[F] Impossibile allocare Ethernet netif");
        esp_eth_driver_uninstall(eth_handle);
        return ESP_FAIL;
    }
    
    // Create glue and attach netif
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_RETURN_ON_ERROR(esp_netif_attach(s_netif_eth, glue), TAG, "Netif attach failed");

    // Allinea MAC del netif a quello del driver Ethernet
    uint8_t mac_addr[6] = {0};
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
    ESP_RETURN_ON_ERROR(esp_netif_set_mac(s_netif_eth, mac_addr), TAG, "esp_netif_set_mac failed");
    ESP_LOGI(TAG, "[F] Netif MAC impostato a %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    
    // Register event handlers AFTER creating netif (come nel progetto factory)
    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL), TAG, "register ETH_EVENT failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL), TAG, "register IP_EVENT failed");
    
    // Applica configurazione IP statica se DHCP è disabilitato
    device_config_t *cfg = device_config_get();
    if (!cfg->eth.dhcp_enabled && strlen(cfg->eth.ip) > 0) {
        esp_netif_dhcpc_stop(s_netif_eth);
        esp_netif_ip_info_t ip_info;
        ip_info.ip.addr = ipaddr_addr(cfg->eth.ip);
        ip_info.gw.addr = ipaddr_addr(cfg->eth.gateway);
        ip_info.netmask.addr = ipaddr_addr(cfg->eth.subnet);
        esp_netif_set_ip_info(s_netif_eth, &ip_info);
        ESP_LOGI(TAG, "[F] Ethernet IP statico: %s", cfg->eth.ip);
    }

    // Start Ethernet driver (come nel progetto factory)
    ret = esp_eth_start(eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[F] Avvio Ethernet fallito: %s", esp_err_to_name(ret));
        esp_eth_del_netif_glue(glue);
        esp_eth_driver_uninstall(eth_handle);
        return ret;
    }
    
    ESP_LOGI(TAG, "[F] Ethernet avviato (indirizzo PHY %d, DHCP abilitato)", CONFIG_APP_ETH_PHY_ADDR);
#else
    ESP_LOGI(TAG, "[F] Ethernet disabilitato (CONFIG_APP_ETH_ENABLED=n)");
#endif
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
    
    // Inizializza configurazione device PRIMA degli altri moduli
    ESP_ERROR_CHECK(device_config_init());
    device_config_t *cfg = device_config_get();

    // Ethernet - continua anche se fallisce
    if (cfg->eth.enabled) {
        esp_err_t eth_ret = start_ethernet(); // TODO: aggiornare start_ethernet per usare cfg
        if (eth_ret != ESP_OK) {
            ESP_LOGW(TAG, "[F] Ethernet non disponibile, continuo senza");
        }
    } else {
        ESP_LOGI(TAG, "[F] Ethernet disabilitato da config");
    }
    
    // Inizializza monitoraggio seriale per i test
    serial_test_init();

    // Inizializza e avvia Web UI (Server + Handler)
    ESP_ERROR_CHECK(web_ui_init());

    // Inizializzazioni condizionali basate su NVS
    if (cfg->sensors.io_expander_enabled) ESP_ERROR_CHECK(io_expander_init());
    if (cfg->sensors.led_enabled) ESP_ERROR_CHECK(led_init());
    if (cfg->sensors.rs232_enabled) ESP_ERROR_CHECK(rs232_init());
    if (cfg->sensors.rs485_enabled) ESP_ERROR_CHECK(rs485_init());
    if (cfg->sensors.mdb_enabled) {
        ESP_ERROR_CHECK(mdb_init());
        ESP_ERROR_CHECK(mdb_start_engine());
    }
    if (cfg->sensors.pwm1_enabled || cfg->sensors.pwm2_enabled) ESP_ERROR_CHECK(pwm_init());

    return ESP_OK;
}

led_strip_handle_t init_get_ws2812_handle(void)
{
    return led_get_handle();
}

void init_get_netifs(esp_netif_t **ap, esp_netif_t **sta, esp_netif_t **eth)
{
    if (ap) *ap = s_netif_ap;
    if (sta) *sta = s_netif_sta;
    if (eth) *eth = s_netif_eth;
}
