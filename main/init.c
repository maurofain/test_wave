#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
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
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "init.h"
#include "tasks.h"
#include "aux_gpio.h"
#include "led.h"
#include "device_config.h"
#include "mdb.h"
#include "web_ui.h"
#include "sdkconfig.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "bsp/display.h"
#include "lvgl.h"
#include "io_expander.h"
#include "eeprom_24lc16.h"
#include "pwm.h"
#include "rs232.h"
#include "rs485.h"
#include "serial_test.h"
#include "remote_logging.h"
#include "sd_card.h"
#include "sht40.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "INIT";

static esp_netif_t *s_netif_ap;
static esp_netif_t *s_netif_sta;
static esp_netif_t *s_netif_eth;
static esp_eth_handle_t s_eth_handle;
static esp_lcd_touch_handle_t s_touch_handle;

// -----------------------------------------------------------------------------
// Rete + HTTP + OTA (factory)
// -----------------------------------------------------------------------------

static void nvs_list_entries(void)
{
    nvs_iterator_t it = NULL;
    esp_err_t ret = nvs_entry_find(NULL, NULL, NVS_TYPE_ANY, &it);

    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "=== NVS vuota (no entries) ===");
        return;
    }

    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Avviso lettura NVS: %s (verranno usate le defaults)", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "=== Elenco NVS Entries ===");
    int count = 0;

    while (it != NULL)
    {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        count++;
        ESP_LOGI(TAG, "  [%d] chiave='%s', tipo=%d", count, info.key, info.type);
        ret = nvs_entry_next(&it);
        if (ret != ESP_OK)
            break;
    }

    ESP_LOGI(TAG, "=== Totale: %d voci ===", count);
}

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Inizializzazione NVS fallita");

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
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[M] Montaggio SPIFFS fallito: %s", esp_err_to_name(ret));
        return ret;
    }
    size_t total = 0, used = 0;
    if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK)
    {
        ESP_LOGI(TAG, "[M] SPIFFS montato: totale=%u, usato=%u", (unsigned)total, (unsigned)used);
    }

    // Elenca file in SPIFFS
    DIR *dir = opendir("/spiffs");
    if (dir)
    {
        ESP_LOGI(TAG, "[M] === Elenco file SPIFFS ===");
        struct dirent *entry;
        int file_count = 0;
        while ((entry = readdir(dir)) != NULL)
        {
            file_count++;
            char filepath[300];
            snprintf(filepath, sizeof(filepath), "/spiffs/%s", entry->d_name);
            struct stat st;
            if (stat(filepath, &st) == 0)
            {
                ESP_LOGI(TAG, "[M]   [%d] %s (%ld bytes)", file_count, entry->d_name, st.st_size);
            }
            else
            {
                ESP_LOGI(TAG, "[M]   [%d] %s", file_count, entry->d_name);
            }
        }
        closedir(dir);
        ESP_LOGI(TAG, "[M] === Totale file: %d ===", file_count);
    }

    // Leggi e mostra tasks.csv
    FILE *f = fopen("/spiffs/tasks.csv", "r");
    if (f)
    {
        ESP_LOGI(TAG, "[M] === Contenuto tasks.csv ===");
        char line[256];
        int line_num = 0;
        while (fgets(line, sizeof(line), f))
        {
            line_num++;
            // Rimuovi newline
            line[strcspn(line, "\r\n")] = 0;
            ESP_LOGI(TAG, "[M]   [%d] %s", line_num, line);
        }
        fclose(f);
        ESP_LOGI(TAG, "[M] ================================");
    }
    else
    {
        ESP_LOGW(TAG, "[M] File tasks.csv non trovato");
    }

    return ESP_OK;
}

static void log_partitions(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    ESP_LOGI(TAG, "[M] Partizione in esecuzione: %s (tipo %d, sottotipo %d)", running ? running->label : "?", running ? running->type : -1, running ? running->subtype : -1);
    ESP_LOGI(TAG, "[M] Partizione boot      : %s (tipo %d, sottotipo %d)", boot ? boot->label : "?", boot ? boot->type : -1, boot ? boot->subtype : -1);
}

static bool check_internet_access(void)
{
    ESP_LOGI(TAG, "[NTP] Checking internet access by HTTP request to http://www.google.com...");

    esp_http_client_config_t config = {
        .url = "http://www.google.com",
        .timeout_ms = 5000,
        .method = HTTP_METHOD_HEAD,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGW(TAG, "[NTP] Failed to initialize HTTP client");
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);

    if (err == ESP_OK && status_code == 200) {
        ESP_LOGI(TAG, "[NTP] Internet access confirmed (HTTP status: %d)", status_code);
        return true;
    } else {
        ESP_LOGW(TAG, "[NTP] No internet access detected (err: %s, status: %d)", esp_err_to_name(err), status_code);
        return false;
    }
}

static void ntp_sync_callback(struct timeval *tv)
{
    device_config_t *cfg = device_config_get();
    
    // Apply timezone offset
    if (cfg->ntp.timezone_offset != 0) {
        // Adjust time by timezone offset (in seconds)
        tv->tv_sec += (cfg->ntp.timezone_offset * 3600);
        settimeofday(tv, NULL);
        ESP_LOGI(TAG, "[NTP] Applied timezone offset: %+d hours", cfg->ntp.timezone_offset);
    }
    
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "[NTP] Time synchronized: %04d-%02d-%02d %02d:%02d:%02d (UTC%+d)",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, cfg->ntp.timezone_offset);
    
    // When NTP sync succeeds, increase interval to 1 hour (3600000 ms)
    // This prevents continuous sync attempts when time is already correct
    sntp_set_sync_interval(3600000);
    ESP_LOGI(TAG, "[NTP] Sync interval changed to 1 hour (time is synchronized)");
}

static void init_sntp(void)
{
    device_config_t *cfg = device_config_get();

    ESP_LOGI(TAG, "[NTP] Initializing SNTP with servers: %s, %s", cfg->ntp.server1, cfg->ntp.server2);

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, cfg->ntp.server1);
    esp_sntp_setservername(1, cfg->ntp.server2);
    
    // Set callback for successful NTP sync
    esp_sntp_set_time_sync_notification_cb(ntp_sync_callback);
    
    esp_sntp_init();

    // Set sync mode to immediate for faster initial sync
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    // Start with 5 minute interval for initial sync attempts
    sntp_set_sync_interval(300000);

    ESP_LOGI(TAG, "[NTP] SNTP initialized - synchronization will happen in background");

    // Note: Removed blocking wait loop - NTP sync will happen asynchronously
    // The system can continue initialization while NTP syncs in background
}

/**
 * @brief Forza la sincronizzazione NTP manuale
 * @return ESP_OK se la sincronizzazione è iniziata, ESP_FAIL altrimenti
 */
esp_err_t init_sync_ntp(void)
{
    device_config_t *cfg = device_config_get();
    
    if (!cfg->ntp_enabled) {
        ESP_LOGW(TAG, "[NTP] NTP is disabled in configuration");
        return ESP_FAIL;
    }
    
    if (!check_internet_access()) {
        ESP_LOGW(TAG, "[NTP] No internet access, cannot sync NTP");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "[NTP] Forcing NTP synchronization...");
    
    // Restart SNTP to force sync
    esp_sntp_restart();
    
    ESP_LOGI(TAG, "[NTP] NTP sync request sent - will complete in background");
    
    // Note: Removed blocking wait - sync happens asynchronously
    // The web interface can poll the status if needed
    
    return ESP_OK;
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "[M] STA Wi-Fi ha ottenuto IP: %s", ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.ip));
    }
    if (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        const esp_netif_ip_info_t *ip_info = &event->ip_info;
        ESP_LOGI(TAG, "[M] Ethernet ha ottenuto l'indirizzo IP");
        ESP_LOGI(TAG, "[M] ~~~~~~~~~~~");
        ESP_LOGI(TAG, "[M] ETH IP:" IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "[M] ETH MASCHERA:" IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "[M] ETH GATEWAY:" IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "[M] ~~~~~~~~~~~");

        if (s_netif_eth)
        {
            struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(s_netif_eth);
            if (lwip_netif)
            {
                etharp_gratuitous(lwip_netif);
                ESP_LOGI(TAG, "[M] Gratuitous ARP inviato");
            }
        }

        // Check internet and init NTP after getting IP
        device_config_t *cfg = device_config_get();
        if (cfg->ntp_enabled && check_internet_access())
        {
            init_sntp();
        }
    }
}

static void eth_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
    {
        uint8_t mac_addr[6] = {0};
        esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "[M] Collegamento Ethernet attivo");
        ESP_LOGI(TAG, "[M] Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2],
                 mac_addr[3], mac_addr[4], mac_addr[5]);
    }
    break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "[M] Collegamento Ethernet inattivo");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "[M] Driver Ethernet avviato");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGW(TAG, "[M] Driver Ethernet fermato");
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

static esp_eth_handle_t eth_init_internal(void)
{
    esp_err_t ret __attribute__((unused)) = ESP_OK;
    // Inizializza le config comuni MAC e PHY ai valori di default
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
    // Inizializza il driver Ethernet ai valori di default e installalo
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_GOTO_ON_FALSE(esp_eth_driver_install(&eth_config, &eth_handle) == ESP_OK, ESP_FAIL,
                      err, TAG, "Installazione driver Ethernet fallita");

    return eth_handle;
err:
    if (eth_handle != NULL)
    {
        esp_eth_driver_uninstall(eth_handle);
    }
    if (mac != NULL)
    {
        mac->del(mac);
    }
    if (phy != NULL)
    {
        phy->del(phy);
    }
    return NULL;
}

static esp_err_t start_ethernet(void)
{
#if CONFIG_APP_ETH_ENABLED
    esp_err_t ret = ESP_OK;

    // Check for GPIO conflicts
    if (CONFIG_APP_ETH_MDC_GPIO == CONFIG_APP_MDB_RX_GPIO)
    {
        ESP_LOGW(TAG, "[M] ATTENZIONE: Conflitto GPIO! ETH_MDC_GPIO (%d) e MDB_RX_GPIO (%d) usano lo stesso pin",
                 CONFIG_APP_ETH_MDC_GPIO, CONFIG_APP_MDB_RX_GPIO);
    }

    ESP_LOGI(TAG, "[M] Inizializzazione Ethernet: MDC=%d, MDIO=%d, PHY_ADDR=%d, RST_GPIO=%d",
             CONFIG_APP_ETH_MDC_GPIO, CONFIG_APP_ETH_MDIO_GPIO,
             CONFIG_APP_ETH_PHY_ADDR, CONFIG_APP_ETH_PHY_RST_GPIO);

    // Inizializza il driver Ethernet ai valori di default e installalo
    esp_eth_handle_t eth_handle = eth_init_internal();
    if (!eth_handle)
    {
        ESP_LOGE(TAG, "[M] Installazione driver Ethernet fallita");
        return ESP_FAIL;
    }

    // Salva l'handle Ethernet per riferimento futuro
    s_eth_handle = eth_handle;

    // Inizializza esp_netif e loop eventi DOPO aver installato il driver (come nell'esempio funzionante)
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init fallito");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "esp_event_loop_create_default fallito");

    // Crea netif DOPO l'installazione del driver e l'inizializzazione di netif (come nell'esempio funzionante)
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif_eth = esp_netif_new(&netif_cfg);
    if (!s_netif_eth)
    {
        ESP_LOGE(TAG, "[M] Impossibile allocare Ethernet netif");
        esp_eth_driver_uninstall(eth_handle);
        return ESP_FAIL;
    }

    // Crea glue e aggancia netif
    esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
    ESP_RETURN_ON_ERROR(esp_netif_attach(s_netif_eth, glue), TAG, "Aggancio Netif fallito");

    // Allinea MAC del netif a quello del driver Ethernet
    uint8_t mac_addr[6] = {0};
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
    ESP_RETURN_ON_ERROR(esp_netif_set_mac(s_netif_eth, mac_addr), TAG, "esp_netif_set_mac fallito");
    ESP_LOGI(TAG, "[M] Netif MAC impostato a %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);

    // Registra i gestori di eventi DOPO la creazione del netif (come nel progetto factory)
    ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL), TAG, "registrazione ETH_EVENT fallita");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL), TAG, "registrazione IP_EVENT fallita");

    // Applica configurazione IP statica se DHCP è disabilitato
    device_config_t *cfg = device_config_get();
    if (!cfg->eth.dhcp_enabled && strlen(cfg->eth.ip) > 0)
    {
        esp_netif_dhcpc_stop(s_netif_eth);
        esp_netif_ip_info_t ip_info;
        ip_info.ip.addr = ipaddr_addr(cfg->eth.ip);
        ip_info.gw.addr = ipaddr_addr(cfg->eth.gateway);
        ip_info.netmask.addr = ipaddr_addr(cfg->eth.subnet);
        esp_netif_set_ip_info(s_netif_eth, &ip_info);
        ESP_LOGI(TAG, "[M] Ethernet IP statico: %s", cfg->eth.ip);
    }

    // Start Ethernet driver (come nel progetto factory)
    ret = esp_eth_start(eth_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[M] Avvio Ethernet fallito: %s", esp_err_to_name(ret));
        esp_eth_del_netif_glue(glue);
        esp_eth_driver_uninstall(eth_handle);
        return ret;
    }

    ESP_LOGI(TAG, "[M] Ethernet avviato (indirizzo PHY %d, DHCP abilitato)", CONFIG_APP_ETH_PHY_ADDR);
#else
    ESP_LOGI(TAG, "[M] Ethernet disabilitato (CONFIG_APP_ETH_ENABLED=n)");
#endif
    return ESP_OK;
}

// -----------------------------------------------------------------------------

// Public API
// -----------------------------------------------------------------------------

// Global variables for time display
static lv_obj_t *time_label = NULL;

static void update_time_display(lv_timer_t *timer)
{
    if (time_label == NULL) return;
    
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        lv_label_set_text(time_label, "Ora non disponibile");
        return;
    }
    
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%d/%m/%Y %H:%M:%S", &timeinfo);
    lv_label_set_text(time_label, time_str);

    // Rotazione del widget per layout orizzontale senza ruotare il display
    lv_obj_set_style_transform_angle(time_label, 900, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(time_label, lv_obj_get_width(time_label) / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(time_label, lv_obj_get_height(time_label) / 2, LV_PART_MAIN);
    lv_obj_center(time_label);
}

static void lvgl_show_minimal_screen(void)
{
    if (!bsp_display_lock(0))
    {
        ESP_LOGW(TAG, "[M] LVGL lock failed");
        return;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_make(0x10, 0x10, 0x10), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    time_label = lv_label_create(scr);
    lv_obj_set_style_text_color(time_label, lv_color_make(0xEE, 0xEE, 0xEE), LV_PART_MAIN);
    lv_obj_center(time_label);
    
    // Initial time update
    update_time_display(NULL);
    
    // Create timer to update time every second
    lv_timer_create(update_time_display, 1000, NULL);

    bsp_display_unlock();
}

static esp_err_t init_display_lvgl_minimal(void)
{
    ESP_LOGI(TAG, "Heap before display init:");
    ESP_LOGI(TAG, "  INTERNAL free: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "  DMA free: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
    ESP_LOGI(TAG, "  PSRAM free: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "  SPIRAM caps alloc free (8bit): %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = false,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        },
    };


    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (!disp)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Heap after LVGL init:");
    ESP_LOGI(TAG, "  INTERNAL free: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "  DMA free: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
    ESP_LOGI(TAG, "  PSRAM free: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "  SPIRAM caps alloc free (8bit): %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "brightness init failed");
    
    // Log delle dimensioni dopo rotazione
    lv_coord_t hor_res = lv_display_get_horizontal_resolution(disp);
    lv_coord_t ver_res = lv_display_get_vertical_resolution(disp);
    ESP_LOGI(TAG, "[M] Dimensioni display dopo rotazione: %dx%d", hor_res, ver_res);
    
    // Applica la luminosità dal config invece di accendere sempre al 100%
    device_config_t *device_cfg = device_config_get();
    ESP_RETURN_ON_ERROR(bsp_display_brightness_set(device_cfg->display.lcd_brightness), TAG, "brightness set failed");

    // Initialize touch
    bsp_touch_config_t touch_cfg = {
        .dummy = NULL,
    };
    ESP_RETURN_ON_ERROR(bsp_touch_new(&touch_cfg, &s_touch_handle), TAG, "touch init failed");
    
    // Verifica che il touch sia inizializzato correttamente
    if (s_touch_handle) {
        ESP_LOGI(TAG, "[M] Touch handle inizializzato: %p", s_touch_handle);
    } else {
        ESP_LOGE(TAG, "[M] Touch handle è NULL!");
    }
    
    // Pass touch handle to touchscreen task
    if (s_touch_handle) {
        ESP_LOGI(TAG, "Touch handle initialized successfully: %p", s_touch_handle);
        tasks_set_touchscreen_handle(s_touch_handle);
    } else {
        ESP_LOGE(TAG, "Touch handle is NULL after initialization");
    }

    lvgl_show_minimal_screen();
    return ESP_OK;
}

esp_err_t init_run_factory(void)
{
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(init_spiffs());
    log_partitions();
    ESP_ERROR_CHECK(init_event_loop());

#if defined(CONFIG_BSP_I2C_NUM)
    bsp_i2c_init();
#endif

    // Inizializza I2C e EEPROM prima della configurazione (essenziale per il boot)
#if defined(CONFIG_BSP_I2C_NUM)
    ESP_LOGW(TAG, "[M] Legacy I2C init skipped (BSP uses i2c_master)");
#else
    ESP_ERROR_CHECK(init_i2c_bus());
    ESP_ERROR_CHECK(eeprom_24lc16_init());
#endif

    // Inizializza configurazione device PRIMA degli altri moduli
    ESP_ERROR_CHECK(device_config_init());

    // Inizializza GPIO ausiliari
    aux_gpio_init();

    // Display + LVGL (minimal screen)
    esp_err_t disp_ret = init_display_lvgl_minimal();
    if (disp_ret != ESP_OK)
    {
        ESP_LOGW(TAG, "[M] Display/LVGL init failed: %s", esp_err_to_name(disp_ret));
    }

    device_config_t *cfg = device_config_get();

    // Ethernet - continua anche se fallisce
    if (cfg->eth.enabled)
    {
        esp_err_t eth_ret = start_ethernet(); // TODO: aggiornare start_ethernet per usare cfg
        if (eth_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "[M] Ethernet non disponibile, continuo senza");
        }
    }
    else
    {
        ESP_LOGI(TAG, "[M] Ethernet disabilitato da config");
    }

    // Inizializza monitoraggio seriale per i test
    serial_test_init();

    // Inizializza e avvia Web UI (Server + Handler)
    ESP_ERROR_CHECK(web_ui_init());

    // Inizializza Remote Logging
    ESP_ERROR_CHECK(remote_logging_init());

    // Inizializzazioni condizionali basate su NVS
    if (cfg->sensors.io_expander_enabled)
    {
#if defined(CONFIG_BSP_I2C_NUM)
        ESP_LOGW(TAG, "[M] I/O Expander skipped (legacy I2C disabled with BSP)");
#else
        // La porta I2C è già inizializzata sopra, io_expander_init la riutilizzerà
        esp_err_t exp_ret = io_expander_init();
        if (exp_ret != ESP_OK) {
            ESP_LOGW(TAG, "I/O Expander non disponibile o errore (%s): proseguo senza bloccare l'esecuzione", esp_err_to_name(exp_ret));
            return;
        }
        // Controllo GPIO3 solo se expander disponibile
        while (true) {
            int gpio3_value = io_get_pin(3) ? 1 : 0;
            ESP_LOGI(TAG, "Valore GPIO3: %d", gpio3_value);
            if (gpio3_value == 1) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
#endif
    }
    else
    {
        ESP_LOGI(TAG, "I/O Expander disabilitato da config");
    }

    if (cfg->sensors.led_enabled)
    {
        ESP_ERROR_CHECK(led_init());
    }
    else
    {
        ESP_LOGI(TAG, "LED Strip disabilitato da config");
    }

    if (cfg->sensors.rs232_enabled)
    {
        ESP_ERROR_CHECK(rs232_init());
    }
    else
    {
        ESP_LOGI(TAG, "UART RS232 disabilitato da config");
    }

    if (cfg->sensors.rs485_enabled)
    {
        ESP_ERROR_CHECK(rs485_init());
    }
    else
    {
        ESP_LOGI(TAG, "UART RS485 disabilitato da config");
    }

    if (cfg->sensors.mdb_enabled)
    {
        ESP_ERROR_CHECK(mdb_init());
        ESP_ERROR_CHECK(mdb_start_engine());
    }
    else
    {
        ESP_LOGI(TAG, "MDB Engine disabilitato da config");
    }

    if (cfg->sensors.pwm1_enabled || cfg->sensors.pwm2_enabled)
    {
        ESP_ERROR_CHECK(pwm_init());
    }
    else
    {
        ESP_LOGI(TAG, "PWM Hardware disabilitato da config");
    }

    if (cfg->sensors.temperature_enabled)
    {
        esp_err_t sht_ret = sht40_init();
        if (sht_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[M] Inizializzazione SHT40 fallita!");
        }
    }

    // Inizializza sempre il monitor hot-plug SD per rilevare inserimenti
    sd_card_init_monitor();

    if (cfg->sensors.sd_card_enabled)
    {
        esp_err_t sd_ret = sd_card_mount();
        if (sd_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "[M] Montaggio SD Card salvato, continuo senza");
        }
    }
    else
    {
        ESP_LOGI(TAG, "[M] SD Card disabilitata da config");
    }

    return ESP_OK;
}

led_strip_handle_t init_get_ws2812_handle(void)
{
    return led_get_handle();
}

void init_get_netifs(esp_netif_t **ap, esp_netif_t **sta, esp_netif_t **eth)
{
    if (ap)
        *ap = s_netif_ap;
    if (sta)
        *sta = s_netif_sta;
    if (eth)
        *eth = s_netif_eth;
}

void init_i2c_and_io_expander(void) {
#if defined(CONFIG_BSP_I2C_NUM)
    bsp_i2c_init();
#endif
    esp_err_t exp_ret = io_expander_init();
    if (exp_ret != ESP_OK) {
        ESP_LOGW(TAG, "I/O Expander non disponibile o errore (%s): proseguo senza bloccare l'esecuzione", esp_err_to_name(exp_ret));
        return;
    }
    // Controllo GPIO3 solo se expander disponibile
    while (true) {
        int gpio3_value = io_get_pin(3) ? 1 : 0;
        ESP_LOGI(TAG, "Valore GPIO3: %d", gpio3_value);
        if (gpio3_value == 1) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
