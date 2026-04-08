#include "web_ui_internal.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "device_config.h"
#include "esp_ota_ops.h"
#include "init.h"
#include "mdb.h"
#include "sd_card.h"
#include "tasks.h"
#include "app_version.h"
#include "http_services.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "WEB_UI_PAGES_RUNTIME"

/**
 * @brief Renderizza la home principale della Web UI.
 *
 * La pagina viene servita dal filesystem esterno della Web UI.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la pagina viene inviata correttamente.
 */
esp_err_t root_get_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "index.html", "text/html; charset=utf-8");
}

/**
 * @brief Espone lo stato runtime dispositivo in formato JSON.
 *
 * Include partizioni OTA, IP di rete, stato Web UI, stato MDB, stato SD,
 * telemetria ambiente e configurazione corrente serializzata.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la risposta JSON è inviata; ESP_FAIL su errore memoria.
 */
esp_err_t status_get_handler(httpd_req_t *req)
{
    esp_netif_t *ap, *sta, *eth;
    init_get_netifs(&ap, &sta, &eth);
    char ap_ip[16] = "0.0.0.0", sta_ip[16] = "0.0.0.0", eth_ip[16] = "0.0.0.0";
    ip_to_str(ap, ap_ip, 16);
    ip_to_str(sta, sta_ip, 16);
    ip_to_str(eth, eth_ip, 16);
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    const mdb_status_t *mdb = mdb_get_status();

    bool sd_mounted = sd_card_is_mounted();
    bool sd_present = sd_card_is_present();
    uint64_t sd_total_kb = 0;
    uint64_t sd_used_kb = 0;
    if (sd_mounted) {
        sd_total_kb = sd_card_get_total_size();
    }

    char *config_json = device_config_to_json(device_config_get());
    device_config_t *cfg = device_config_get();

    bool remote_enabled = http_services_is_remote_enabled();
    bool remote_online = http_services_is_remote_online();
    bool remote_token = http_services_has_auth_token();

    /* [C] Estrai stato dei 4 agenti principali (Network, USB Scanner, CCTALK, MDB) */
    bool http_services_online = false, usb_scanner_online = false;
    bool cctalk_online = false, mdb_online_agent = false;
    {
        size_t agent_count = 0;
        const init_agent_status_t *table = init_agent_status_get_table(&agent_count);
        if (table && agent_count > 0) {
            /* Cicla nella tabella per trovare i 4 agenti (salta primo elemento che è AGN_ID_NONE) */
            for (size_t i = 1; i < agent_count; i++) {
                /* state=1 significa OK (non-zero), state=0 significa KO */
                int is_ok = (table[i].state != 0) ? 1 : 0;
                switch (table[i].agn_value) {
                    case AGN_ID_HTTP_SERVICES:
                        http_services_online = !!is_ok;
                        break;
                    case AGN_ID_USB_CDC_SCANNER:
                        usb_scanner_online = !!is_ok;
                        break;
                    case AGN_ID_CCTALK:
                        cctalk_online = !!is_ok;
                        break;
                    case AGN_ID_MDB:
                        mdb_online_agent = !!is_ok;
                        break;
                    default:
                        break;
                }
            }
        }
    }

    const size_t resp_cap = 5120;
    char *resp = malloc(resp_cap);
    if (!resp) {
        if (config_json) free(config_json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    snprintf(resp, resp_cap,
             "{\"partition_running\":\"%s\",\"partition_boot\":\"%s\",\"ip_ap\":\"%s\",\"ip_sta\":\"%s\",\"ip_eth\":\"%s\","
             "\"web\":{\"running\":%s},"
             "\"remote\":{\"enabled\":%s,\"online\":%s,\"token\":%s},"
             "\"mdb\":{\"coin_online\":%s,\"coin_state\":%d,\"credit\":%u},"
             "\"sd\":{\"mounted\":%s,\"present\":%s,\"total_kb\":%llu,\"used_kb\":%llu,\"last_error\":\"%s\"},"
             "\"env\":{\"temp\":%.1f,\"hum\":%.1f},"
             "\"sensors\":{"
               "\"io_expander\":%d,\"led_strip\":%d,\"rs232\":%d,\"rs485\":%d,\"mdb\":%d,"
               "\"temperature\":%d,\"cctalk\":%d,\"sd_card\":%d,\"eeprom\":%d,"
               "\"pwm1\":%d,\"pwm2\":%d,\"remote_logging\":%d"
             "},"
             "\"agents\":{"
               "\"http_services\":%s,\"usb_scanner\":%s,\"cctalk\":%s,\"mdb\":%s"
             "},"
             "\"config\":%s}",
             running ? running->label : "?", boot ? boot->label : "?", ap_ip, sta_ip, eth_ip,
             web_ui_is_running() ? "true" : "false",
             remote_enabled ? "true" : "false",
             remote_online ? "true" : "false",
             remote_token ? "true" : "false",
             mdb->coin.is_online ? "true" : "false", mdb->coin.state, (unsigned int)mdb->coin.credit_cents,
             sd_mounted ? "true" : "false", sd_present ? "true" : "false",
             (unsigned long long)sd_total_kb, (unsigned long long)sd_used_kb,
             sd_card_get_last_error(),
             tasks_get_temperature(), tasks_get_humidity(),
             cfg->sensors.io_expander_enabled, cfg->sensors.led_enabled, cfg->sensors.rs232_enabled, cfg->sensors.rs485_enabled, cfg->sensors.mdb_enabled,
             cfg->sensors.temperature_enabled, cfg->sensors.cctalk_enabled, cfg->sensors.sd_card_enabled, cfg->sensors.eeprom_enabled,
             cfg->sensors.pwm1_enabled, cfg->sensors.pwm2_enabled, cfg->remote_log.use_broadcast,
             http_services_online ? "true" : "false",
             usb_scanner_online ? "true" : "false",
             cctalk_online ? "true" : "false",
             mdb_online_agent ? "true" : "false",
             config_json ? config_json : "{}");

    if (config_json) free(config_json);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

/**
 * @brief Renderizza la pagina statistiche con aggiornamento periodico lato client.
 *
 * La pagina legge `/status` via JavaScript e mostra rete, firmware, SD, driver,
 * ambiente e stato MDB in sezioni dedicate.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la pagina viene inviata correttamente.
 */
esp_err_t stats_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "stats.html", "text/html; charset=utf-8");
}

/**
 * @brief Renderizza la pagina editor dei task applicativi.
 *
 * L'interfaccia consente caricamento, modifica, salvataggio e applicazione della
 * tabella task su `tasks.json` tramite endpoint `/api/tasks*`.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la pagina viene inviata correttamente.
 */
esp_err_t tasks_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "tasks.html", "text/html; charset=utf-8");
}

/**
 * @brief Renderizza la pagina di test delle chiamate HTTP services.
 *
 * La pagina include login, gestione token/JWT e pulsanti per invocare endpoint
 * remoti/locali con tracciamento richiesta/risposta.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la pagina viene inviata correttamente.
 */
esp_err_t httpservices_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "httpservices.html", "text/html; charset=utf-8");
}

/**
 * @brief Renderizza l'indice interattivo degli endpoint API disponibili.
 *
 * Espone una tabella con metodi/URI e azioni rapide (apertura o invocazione)
 * per semplificare test manuali e diagnostica.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la pagina viene inviata correttamente.
 */
esp_err_t api_index_page_handler(httpd_req_t *req)
{
    return webpages_send_external_or_error(req, "api.html", "text/html; charset=utf-8");
}
