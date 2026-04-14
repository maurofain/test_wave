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
#include "audio_player.h"
#include "cctalk.h"
#include "http_services.h"
#include "modbus_relay.h"
#include "usb_cdc_scanner.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "WEB_UI_PAGES_RUNTIME"

static device_component_status_t runtime_modbus_component_status(const device_config_t *cfg,
                                                                 const modbus_relay_status_t *status,
                                                                 bool status_available)
{
    if (!cfg || !cfg->sensors.rs485_enabled || !cfg->modbus.enabled) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (!status_available) {
        return DEVICE_COMPONENT_STATUS_OFFLINE;
    }

    if (status->running && status->poll_ok_count > 0 && status->last_error == ESP_OK) {
        return DEVICE_COMPONENT_STATUS_ONLINE;
    }

    if (status->initialized || status->running) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    return DEVICE_COMPONENT_STATUS_OFFLINE;
}

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
    modbus_relay_status_t modbus_status = {0};
    bool modbus_status_available = (modbus_relay_get_status(&modbus_status) == ESP_OK);

    bool remote_enabled = http_services_is_remote_enabled();
    bool remote_online = http_services_is_remote_online();
    bool remote_token = http_services_has_auth_token();
    bool scanner_connected = usb_cdc_scanner_is_connected();
    device_component_status_t mdb_component_status = mdb_get_component_status();
    device_component_status_t cctalk_component_status = cctalk_driver_get_component_status();
    device_component_status_t scanner_component_status = usb_cdc_scanner_get_component_status();
    device_component_status_t http_component_status = http_services_get_component_status();
    device_component_status_t audio_component_status = audio_player_get_component_status();
    device_component_status_t modbus_component_status = runtime_modbus_component_status(cfg,
                                                                                        &modbus_status,
                                                                                        modbus_status_available);

    time_t board_time_val = time(NULL);
    struct tm board_tm;
    localtime_r(&board_time_val, &board_tm);
    char board_time_str[32];
    strftime(board_time_str, sizeof(board_time_str), "%Y-%m-%d %H:%M:%S", &board_tm);

    const size_t resp_cap = 5120;
    char *resp = malloc(resp_cap);
    if (!resp) {
        if (config_json) {
            free(config_json);
        }
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    snprintf(resp, resp_cap,
             "{\"partition_running\":\"%s\",\"partition_boot\":\"%s\",\"ip_ap\":\"%s\",\"ip_sta\":\"%s\",\"ip_eth\":\"%s\","
             "\"web\":{\"running\":%s},"
             "\"remote\":{\"enabled\":%s,\"online\":%s,\"token\":%s,\"status\":\"%s\"},"
             "\"modbus\":{\"enabled\":%s,\"running\":%s,\"initialized\":%s,\"poll_ok\":%lu,\"poll_err\":%lu,\"last_error\":%ld,\"last_update_ms\":%lu,\"status\":\"%s\"},"
             "\"mdb\":{\"coin_online\":%s,\"coin_state\":%d,\"credit\":%u,\"status\":\"%s\","
                 "\"cashless_online\":%s,\"cashless_state\":%d,\"cashless_online_devices\":%u,\"cashless_active_device\":%u,"
                 "\"cashless_feature_level\":%u,\"cashless_last_response\":%u,\"cashless_session_open\":%s,"
                 "\"cashless_credit_cents\":%u,\"cashless_approved_price_cents\":%u,\"cashless_approved_revalue_cents\":%u,"
                 "\"cashless_revalue_limit_cents\":%u,\"cashless_revalue_status\":%u},"
             "\"sd\":{\"mounted\":%s,\"present\":%s,\"total_kb\":%llu,\"used_kb\":%llu,\"last_error\":\"%s\"},"
             "\"env\":{\"temp\":%.1f,\"hum\":%.1f},"
             "\"sensors\":{"
                 "\"io_expander\":%d,\"led_strip\":%d,\"rs232\":%d,\"rs485\":%d,\"mdb\":%d,"
                 "\"temperature\":%d,\"cctalk\":%d,\"sd_card\":%d,\"eeprom\":%d,"
                 "\"pwm1\":%d,\"pwm2\":%d,\"remote_logging\":%d"
             "},"
             "\"cctalk\":{\"enabled\":%s,\"online\":%s,\"status\":\"%s\"},"
             "\"scanner\":{\"enabled\":%s,\"connected\":%s,\"status\":\"%s\"},"
             "\"audio\":{\"enabled\":%s,\"playing\":%s,\"status\":\"%s\"},"
             "\"board_time\":\"%s\","
             "\"config\":%s}",
             running ? running->label : "?", boot ? boot->label : "?", ap_ip, sta_ip, eth_ip,
             web_ui_is_running() ? "true" : "false",
             remote_enabled ? "true" : "false",
             remote_online ? "true" : "false",
             remote_token ? "true" : "false",
             device_component_status_to_string(http_component_status),
             (cfg && cfg->modbus.enabled) ? "true" : "false",
             modbus_status.running ? "true" : "false",
             modbus_status.initialized ? "true" : "false",
             (unsigned long)modbus_status.poll_ok_count,
             (unsigned long)modbus_status.poll_err_count,
             (long)modbus_status.last_error,
             (unsigned long)modbus_status.last_update_ms,
             device_component_status_to_string(modbus_component_status),
             mdb->coin.is_online ? "true" : "false", mdb->coin.state, (unsigned int)mdb->coin.credit_cents,
             device_component_status_to_string(mdb_component_status),
             mdb->cashless.is_online ? "true" : "false",
             mdb->cashless.state,
             (unsigned)mdb->cashless.online_devices,
             (unsigned)mdb->cashless.active_device_index,
             (unsigned)mdb->cashless.feature_level,
             (unsigned)mdb->cashless.last_response_code,
             mdb->cashless.session_open ? "true" : "false",
             (unsigned)mdb->cashless.credit_cents,
             (unsigned)mdb->cashless.approved_price_cents,
             (unsigned)mdb->cashless.approved_revalue_cents,
             (unsigned)mdb->cashless.revalue_limit_cents,
             (unsigned)mdb->cashless.revalue_status,
             sd_mounted ? "true" : "false", sd_present ? "true" : "false",
             (unsigned long long)sd_total_kb, (unsigned long long)sd_used_kb,
             sd_card_get_last_error(),
             tasks_get_temperature(), tasks_get_humidity(),
             cfg->sensors.io_expander_enabled, cfg->sensors.led_enabled, cfg->sensors.rs232_enabled, cfg->sensors.rs485_enabled, cfg->sensors.mdb_enabled,
             cfg->sensors.temperature_enabled, cfg->sensors.cctalk_enabled, cfg->sensors.sd_card_enabled, cfg->sensors.eeprom_enabled,
             cfg->sensors.pwm1_enabled, cfg->sensors.pwm2_enabled, cfg->remote_log.use_broadcast,
             cfg->sensors.cctalk_enabled ? "true" : "false",
             cctalk_driver_is_acceptor_online() ? "true" : "false",
             device_component_status_to_string(cctalk_component_status),
             cfg->scanner.enabled ? "true" : "false", scanner_connected ? "true" : "false",
             device_component_status_to_string(scanner_component_status),
             cfg->audio.enabled ? "true" : "false",
             audio_player_is_playing() ? "true" : "false",
             device_component_status_to_string(audio_component_status),
             board_time_str,
             config_json ? config_json : "{}"
             );

    if (config_json) {
        free(config_json);
    }

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
