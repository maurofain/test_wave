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
#include "nvs.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "lwip/ip4_addr.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "init.h"
#include "tasks.h"
#include "app_version.h"
#include "aux_gpio.h"
#include "led.h"
#include "device_config.h"
#include "mdb.h"
extern esp_err_t cctalk_driver_init(void); /* forward decl - header in components/cctalk */
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
#include "device_activity.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "INIT";

/*
 * Forzatura temporanea: disabilita SEMPRE la parte video (LVGL + LCD + touch + backlight).
 *
 * Impostare a 1 per modalità headless forzata, indipendentemente da /config.
 * Impostare a 0 per comportamento normale basato su cfg->display.enabled.
 */
#define FORCE_VIDEO_DISABLED 0

static esp_netif_t *s_netif_ap;
static esp_netif_t *s_netif_sta;
static esp_netif_t *s_netif_eth;
static esp_eth_handle_t s_eth_handle;
static esp_lcd_touch_handle_t s_touch_handle;
static bool s_error_lock_active = false;
static uint32_t s_consecutive_reboots = 0;

#define BOOT_GUARD_NAMESPACE "boot_guard"
#define BOOT_GUARD_KEY_CONSEC "consecutive"
#define BOOT_GUARD_KEY_CRASH_PENDING "crash_pending"
#define BOOT_GUARD_KEY_CRASH_REASON "crash_reason"
#define BOOT_GUARD_REBOOT_LIMIT 3

static bool is_unclean_reset_reason(esp_reset_reason_t reason)
{
    switch (reason)
    {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
        case ESP_RST_UNKNOWN:
            return true;
        default:
            return false;
    }
}

static esp_err_t update_boot_reboot_guard(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[M] Apertura NVS boot guard fallita: %s", esp_err_to_name(ret));
        return ret;
    }

    uint32_t consecutive = 0;
    ret = nvs_get_u32(handle, BOOT_GUARD_KEY_CONSEC, &consecutive);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "[M] Lettura counter reboot fallita: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    esp_reset_reason_t reason = esp_reset_reason();
    if (is_unclean_reset_reason(reason))
    {
        consecutive++;
    }
    else
    {
        consecutive = 0;
    }

    ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CONSEC, consecutive);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[M] Salvataggio counter reboot fallito: %s", esp_err_to_name(ret));
        return ret;
    }

    s_consecutive_reboots = consecutive;
    s_error_lock_active = (consecutive >= BOOT_GUARD_REBOOT_LIMIT);

    ESP_LOGW(TAG, "[M] boot_guard: reset_reason=%d consecutive_reboots=%lu limit=%d",
             (int)reason, (unsigned long)s_consecutive_reboots, BOOT_GUARD_REBOOT_LIMIT);

    if (s_error_lock_active)
    {
        ESP_LOGE(TAG, "[M] ERROR_LOCK attivo: troppi reboot consecutivi");
    }

    return ESP_OK;
}

static esp_err_t update_crash_pending_record(void)
{
    esp_reset_reason_t reason = esp_reset_reason();
    if (!is_unclean_reset_reason(reason))
    {
        return ESP_OK;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[M] [C] apertura NVS crash record fallita: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CRASH_PENDING, 1);
    if (ret == ESP_OK)
    {
        ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CRASH_REASON, (uint32_t)reason);
    }
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret == ESP_OK)
    {
        ESP_LOGW(TAG, "[M] [C] crash pending registrato: reason=%d", (int)reason);
    }
    return ret;
}

static void build_deviceactivity_url(char *out, size_t out_len, const char *base_url)
{
    if (!out || out_len == 0)
    {
        return;
    }
    out[0] = '\0';
    if (!base_url || base_url[0] == '\0')
    {
        return;
    }

    size_t bl = strlen(base_url);
    if (bl > 0 && base_url[bl - 1] == '/')
    {
        snprintf(out, out_len, "%sapi/deviceactivity", base_url);
    }
    else
    {
        snprintf(out, out_len, "%s/api/deviceactivity", base_url);
    }
}

static esp_err_t try_send_pending_crash_record(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[M] [C] apertura NVS preboot send fallita: %s", esp_err_to_name(ret));
        return ret;
    }

    uint32_t pending = 0;
    uint32_t crash_reason = 0;
    if (nvs_get_u32(handle, BOOT_GUARD_KEY_CRASH_PENDING, &pending) != ESP_OK || pending == 0)
    {
        nvs_close(handle);
        return ESP_OK;
    }

    (void)nvs_get_u32(handle, BOOT_GUARD_KEY_CRASH_REASON, &crash_reason);
    nvs_close(handle);

    device_config_t *cfg = device_config_get();
    if (!cfg || !cfg->server.enabled || cfg->server.url[0] == '\0')
    {
        ESP_LOGW(TAG, "[M] [C] preboot crash send saltato: server non configurato");
        return ESP_OK;
    }

    char url[192];
    build_deviceactivity_url(url, sizeof(url), cfg->server.url);
    if (url[0] == '\0')
    {
        ESP_LOGW(TAG, "[M] [C] preboot crash send saltato: URL non valida");
        return ESP_OK;
    }

    char body[320];
    snprintf(body, sizeof(body),
             "{\"activityid\":999,\"status\":\"CRASH\",\"serial\":\"%s\",\"reason\":%lu,\"reboots\":%lu}",
             cfg->server.serial,
             (unsigned long)crash_reason,
             (unsigned long)s_consecutive_reboots);

    esp_http_client_config_t http_cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 3000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client)
    {
        ESP_LOGE(TAG, "[M] [C] preboot crash send: client init fallita");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, (int)strlen(body));

    esp_err_t http_ret = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (http_ret == ESP_OK && status >= 200 && status < 300)
    {
        ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
        if (ret == ESP_OK)
        {
            ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CRASH_PENDING, 0);
            if (ret == ESP_OK)
            {
                ret = nvs_commit(handle);
            }
            nvs_close(handle);
        }
        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "[M] [C] preboot crash send OK (status=%d), pending cleared", status);
        }
        return ret;
    }

    ESP_LOGW(TAG, "[M] [C] preboot crash send fallito (err=%s status=%d), pending mantenuto",
             esp_err_to_name(http_ret), status);
    return ESP_OK;
}

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
    
    // Applica l'offset del fuso orario
    if (cfg->ntp.timezone_offset != 0) {
        // Regola l'ora aggiungendo l'offset (in secondi)
        tv->tv_sec += (cfg->ntp.timezone_offset * 3600);
        settimeofday(tv, NULL);
        ESP_LOGI(TAG, "[NTP] Offset del fuso orario applicato: %+d ore", cfg->ntp.timezone_offset);
    }
    
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    ESP_LOGI(TAG, "[NTP] Time synchronized: %04d-%02d-%02d %02d:%02d:%02d (UTC%+d)",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, cfg->ntp.timezone_offset);
    
    // Quando la sincronizzazione NTP ha successo, aumenta l'intervallo a 1 ora (3600000 ms)
    // Evita tentativi continui quando l'ora è già corretta
    sntp_set_sync_interval(3600000);
    ESP_LOGI(TAG, "[NTP] Intervallo di sync impostato a 1 ora (ora sincronizzata)");
}

static void init_sntp(void)
{
    device_config_t *cfg = device_config_get();

    ESP_LOGI(TAG, "[NTP] Initializing SNTP with servers: %s, %s", cfg->ntp.server1, cfg->ntp.server2);

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, cfg->ntp.server1);
    esp_sntp_setservername(1, cfg->ntp.server2);
    
    // Imposta la callback da chiamare alla sincronizzazione NTP riuscita
    esp_sntp_set_time_sync_notification_cb(ntp_sync_callback);
    
    esp_sntp_init();

    // Imposta la modalità di sincronizzazione su "immediata" per velocizzare la sync iniziale
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);

    // Avvia con intervallo di 5 minuti per i tentativi di sincronizzazione iniziali
    sntp_set_sync_interval(300000);

    ESP_LOGI(TAG, "[NTP] SNTP inizializzato - la sincronizzazione avverrà in background");

    // Nota: rimosso il loop di attesa bloccante — la sincronizzazione NTP è asincrona
    // Il sistema può proseguire l'inizializzazione mentre NTP si sincronizza in background
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
    
    // Riavvia SNTP per forzare la sincronizzazione
    esp_sntp_restart();
    
    ESP_LOGI(TAG, "[NTP] Richiesta di sincronizzazione inviata — completamento in background");
    
    // Nota: rimosso il wait bloccante — la sincronizzazione è asincrona
    // L'interfaccia web può interrogare lo stato se necessario
    
    return ESP_OK;
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    static bool config_initialized = false;
    if ((event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) ||
        (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP))
    {
        if (!config_initialized) {
            ESP_LOGI(TAG, "[INIT] Inizializzo configurazione device dopo DHCP");
            ESP_ERROR_CHECK(device_config_init());
            config_initialized = true;
        }
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "[M] STA Wi-Fi ha ottenuto IP: %s", ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.ip));
        } else if (event_id == IP_EVENT_ETH_GOT_IP) {
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
    // Inizializza sempre netif/event loop prima di eventuali socket (HTTP server, etc.)
    // per evitare assert lwIP "Invalid mbox" se Ethernet è disabilitato/non avviato.
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        return ret;
    }

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

    // Crea istanza MAC Ethernet ESP32
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    // Crea istanza PHY (IP101 per ESP32-P4 Module DEV KIT)
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

    // esp_netif/event loop sono inizializzati in init_event_loop(); qui non li reinizializziamo.

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
        char lvgl_time_not_available[48] = {0};
        device_config_get_ui_text_scoped("lvgl", "time_not_available", "Ora non disponibile", lvgl_time_not_available, sizeof(lvgl_time_not_available));
        lv_label_set_text(time_label, lvgl_time_not_available);
        return;
    }
    
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%d/%m/%Y %H:%M:%S", &timeinfo);
    lv_label_set_text(time_label, time_str);

    // Layout verticale: nessuna rotazione artificiale del widget
    lv_obj_set_style_transform_angle(time_label, 0, LV_PART_MAIN);
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
    
    // Aggiorna l'orario iniziale
    update_time_display(NULL);
    
    // Crea un timer per aggiornare l'orario ogni secondo
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

    // Forza orientamento verticale (portrait)
    bsp_display_rotate(disp, LV_DISPLAY_ROTATION_0);

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

    // Inizializza il touch
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
    ESP_ERROR_CHECK(update_boot_reboot_guard());
    ESP_ERROR_CHECK(update_crash_pending_record());

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

    // Tentativo rapido pre-boot di invio crash (best-effort, timeout corto)
    ESP_ERROR_CHECK(try_send_pending_crash_record());

    if (s_error_lock_active)
    {
        return ESP_ERR_INVALID_STATE;
    }



    // Inizializza GPIO ausiliari
    aux_gpio_init();

    // Display + LVGL (minimal screen) - skip se headless
    device_config_t *cfg = device_config_get();

#if FORCE_VIDEO_DISABLED
    /* Override runtime: blocca ogni inizializzazione video in questa build */
    if (cfg->display.enabled) {
        ESP_LOGW(TAG, "[M] FORCE_VIDEO_DISABLED attivo: disabilito display da runtime");
    }
    cfg->display.enabled = false;
#endif

    if (cfg->display.enabled) {
        esp_err_t disp_ret = init_display_lvgl_minimal();
        if (disp_ret != ESP_OK)
        {
            ESP_LOGW(TAG, "[M] Display/LVGL init failed: %s", esp_err_to_name(disp_ret));
        }
    } else {
        ESP_LOGI(TAG, "[M] Display disabilitato da config: salto init LVGL/display (modalità headless)");
    }

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
    esp_err_t web_ret = web_ui_init();
    if (web_ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[M] web_ui_init fallita: %s", esp_err_to_name(web_ret));
        return web_ret;
    }
    ESP_LOGI(TAG, "[M] Web UI avviata correttamente");

#if COMPILE_APP
    // Carica la tabella activity da SPIFFS (solo build APP)
    ESP_ERROR_CHECK(device_activity_init());
#endif

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
#if COMPILE_APP
            cfg->sensors.io_expander_enabled = false;
#else
            return exp_ret;
#endif
        }
        if (exp_ret == ESP_OK) {
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
#endif
    }
    else
    {
        ESP_LOGI(TAG, "I/O Expander disabilitato da config");
    }

    if (cfg->sensors.led_enabled)
    {
        esp_err_t led_ret = led_init();
        if (led_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[M] Inizializzazione LED fallita (%s): periferica disabilitata a runtime", esp_err_to_name(led_ret));
            cfg->sensors.led_enabled = false;
        }
    }
    else
    {
        ESP_LOGI(TAG, "LED Strip disabilitato da config");
    }

    if (cfg->sensors.rs232_enabled)
    {
        esp_err_t rs232_ret = rs232_init();
        if (rs232_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[M] Inizializzazione RS232 fallita (%s): periferica disabilitata a runtime", esp_err_to_name(rs232_ret));
            cfg->sensors.rs232_enabled = false;
        } else {
            /* Avvia anche il driver CCtalk (se presente) che usa la stessa UART fisica */
            esp_err_t cctalk_ret = cctalk_driver_init();
            if (cctalk_ret != ESP_OK) {
                ESP_LOGW(TAG, "CCTALK driver non avviato: %s", esp_err_to_name(cctalk_ret));
            }
        }
    }
    else
    {
        ESP_LOGI(TAG, "UART RS232 disabilitato da config");
    }

    if (cfg->sensors.rs485_enabled)
    {
        esp_err_t rs485_ret = rs485_init();
        if (rs485_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[M] Inizializzazione RS485 fallita (%s): periferica disabilitata a runtime", esp_err_to_name(rs485_ret));
            cfg->sensors.rs485_enabled = false;
        }
    }
    else
    {
        ESP_LOGI(TAG, "UART RS485 disabilitato da config");
    }

    if (cfg->sensors.mdb_enabled)
    {
        esp_err_t mdb_ret = mdb_init();
        if (mdb_ret == ESP_OK)
        {
            mdb_ret = mdb_start_engine();
        }

        if (mdb_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[M] Inizializzazione MDB fallita (%s): periferica disabilitata a runtime", esp_err_to_name(mdb_ret));
            cfg->sensors.mdb_enabled = false;
        }
    }
    else
    {
        ESP_LOGI(TAG, "MDB Engine disabilitato da config");
    }

    if (cfg->sensors.pwm1_enabled || cfg->sensors.pwm2_enabled)
    {
        esp_err_t pwm_ret = pwm_init();
        if (pwm_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[M] Inizializzazione PWM fallita (%s): PWM1/PWM2 disabilitati a runtime", esp_err_to_name(pwm_ret));
            cfg->sensors.pwm1_enabled = false;
            cfg->sensors.pwm2_enabled = false;
        }
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
            ESP_LOGE(TAG, "[M] Inizializzazione SHT40 fallita! Sensore temperatura disabilitato a runtime");
            cfg->sensors.temperature_enabled = false;
        }
    }

    // Inizializza sempre il monitor hot-plug SD per rilevare inserimenti
    sd_card_init_monitor();

    if (cfg->sensors.sd_card_enabled)
    {
        esp_err_t sd_ret = sd_card_mount();
        if (sd_ret != ESP_OK)
        {
            ESP_LOGE(TAG, "[M] Inizializzazione SD Card fallita (%s): periferica disabilitata a runtime", esp_err_to_name(sd_ret));
            cfg->sensors.sd_card_enabled = false;
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

bool init_is_error_lock_active(void)
{
    return s_error_lock_active;
}

uint32_t init_get_consecutive_reboots(void)
{
    return s_consecutive_reboots;
}

esp_err_t init_mark_boot_completed(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "[M] [C] apertura NVS boot_completed fallita: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CONSEC, 0);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret == ESP_OK)
    {
        s_consecutive_reboots = 0;
        s_error_lock_active = false;
        ESP_LOGI(TAG, "[M] [C] boot completato: contatore reboot consecutivi azzerato");
    }

    return ret;
}
