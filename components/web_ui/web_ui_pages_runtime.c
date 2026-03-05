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
 * La pagina compone dinamicamente i collegamenti disponibili in base al profilo
 * attivo (Factory/App), alle feature abilitate e alla partizione in esecuzione.
 *
 * @param req Richiesta HTTP GET.
 * @return ESP_OK se la pagina viene inviata correttamente.
 */
esp_err_t root_get_handler(httpd_req_t *req)
{
#if WEB_UI_USE_EMBEDDED_PAGES == 0
    return webpages_send_external_or_error(req, "index.html", "text/html; charset=utf-8");
#else
    esp_err_t ext_page_ret = webpages_try_send_external(req, "index.html", "text/html; charset=utf-8");
    if (ext_page_ret == ESP_OK) {
        return ESP_OK;
    }

    bool show_test = web_ui_feature_enabled(WEB_UI_FEATURE_HOME_TEST);
    bool show_tasks = web_ui_feature_enabled(WEB_UI_FEATURE_HOME_TASKS);
    bool show_httpservices = web_ui_feature_enabled(WEB_UI_FEATURE_HOME_HTTP_SERVICES);
    bool show_emulator = web_ui_feature_enabled(WEB_UI_FEATURE_HOME_EMULATOR);
    bool show_programs = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS);
    const bool maintainer_active = web_ui_factory_features_override_get();
    const esp_partition_t *running = esp_ota_get_running_partition();
    const bool is_running_ota = running &&
        (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
         running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1);
    const char *profile_label = web_ui_profile_view_label();
    const bool is_factory_view = (strcmp(profile_label, "Factory View") == 0);
    const char *home_title = is_factory_view ? "Factory Console" : "MH1001 control";

    const char *extra_style = WEBPAGE_HOME_EXTRA_STYLE;

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, home_title, extra_style, true);

    const char *test_link = show_test ? WEBPAGE_HOME_LINK_TEST : "";
    const char *tasks_link = show_tasks ? WEBPAGE_HOME_LINK_TASKS : "";
    const char *httpservices_link = show_httpservices ? WEBPAGE_HOME_LINK_HTTPSERVICES : "";
    const char *emulator_link = show_emulator ? WEBPAGE_HOME_LINK_EMULATOR : "";
    const char *mantainer_link = (!is_factory_view && show_emulator && !maintainer_active) ? WEBPAGE_HOME_LINK_MANTAINER : "";
    const char *programs_link = show_programs ? WEBPAGE_HOME_LINK_PROGRAMS : "";
    const char *logs_link = WEBPAGE_HOME_LINK_LOGS;
    const char *reboot_factory_crash_link = is_factory_view ? WEBPAGE_HOME_LINK_REBOOT_FACTORY_CRASH : "";
    const char *reboot_app_restore_link = !is_factory_view ? WEBPAGE_HOME_LINK_REBOOT_APP_RESTORE : "";
    const char *promote_factory_link = (is_factory_view && is_running_ota) ? WEBPAGE_HOME_LINK_PROMOTE_FACTORY : "";
    const char *service_status_widget = WEBPAGE_HOME_SERVICE_STATUS_WIDGET;

    int body_len = snprintf(NULL, 0,
        "<div class='container'><div class='grid'>"
        "<a href='/config' class='btn-link btn-config'><span class='icon'>⚙️</span><span>Configurazione</span></a>"
        "<a href='/stats' class='btn-link'><span class='icon'>📈</span><span>Statistiche</span></a>"
        "<a href='/files' class='btn-link'><span class='icon'>📁</span><span>File Manager</span></a>"
        "%s%s%s%s%s%s%s"
        "<a href='/ota' class='btn-link btn-ota'><span class='icon'>🔄</span><span>Update OTA</span></a>"
        "</div>"
        "<div class='card'><h2>ℹ️ Informazioni</h2>"
        "<p><strong>Profilo:</strong> %s</p>"
        "<p>Benvenuti nell'interfaccia di configurazione e test.</p>"
        "<div style='margin-top:20px; border-top:1px solid #eee; padding-top:15px;'>"
        "<h3 style='margin:0 0 10px 0;color:#2c3e50;'>Reboot</h3>"
        "<div style='display:flex;flex-wrap:wrap;gap:8px;'>"
        "<a href='#' onclick=\"return window.goProtectedPath('/reboot/factory');\" class='btn-reboot' style='background:#c0392b;'>FACTORY</a>"
        "<a href='/reboot/app_last' class='btn-reboot' style='background:#27ae60;'>APP LAST</a>"
        "<a href='/reboot/ota0' class='btn-reboot' style='background:#2980b9;'>OTA0</a>"
        "<a href='/reboot/ota1' class='btn-reboot' style='background:#8e44ad;'>OTA1</a>"
        "<div style='margin-left:auto;display:flex;flex-wrap:wrap;gap:8px;'>"
        "<a href='/api' class='btn-reboot' style='background:#3498db;'>API</a>"
        "%s%s%s"
        "</div>"
        "</div></div></div>"
        "%s"
        "</div>",
        test_link,
        tasks_link,
        httpservices_link,
        emulator_link,
        mantainer_link,
        programs_link,
        logs_link,
        profile_label,
        reboot_factory_crash_link,
        reboot_app_restore_link,
        promote_factory_link,
        service_status_widget);

    if (body_len < 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    char *body = malloc((size_t)body_len + 1);
    if (!body) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    snprintf(body, (size_t)body_len + 1,
        "<div class='container'><div class='grid'>"
        "<a href='/config' class='btn-link btn-config'><span class='icon'>⚙️</span><span>Configurazione</span></a>"
        "<a href='/stats' class='btn-link'><span class='icon'>📈</span><span>Statistiche</span></a>"
        "<a href='/files' class='btn-link'><span class='icon'>📁</span><span>File Manager</span></a>"
        "%s%s%s%s%s%s%s"
        "<a href='/ota' class='btn-link btn-ota'><span class='icon'>🔄</span><span>Update OTA</span></a>"
        "</div>"
        "<div class='card'><h2>ℹ️ Informazioni</h2>"
        "<p><strong>Profilo:</strong> %s</p>"
        "<p>Benvenuti nell'interfaccia di configurazione e test.</p>"
        "<div style='margin-top:20px; border-top:1px solid #eee; padding-top:15px;'>"
        "<h3 style='margin:0 0 10px 0;color:#2c3e50;'>Reboot</h3>"
        "<div style='display:flex;flex-wrap:wrap;gap:8px;'>"
        "<a href='#' onclick=\"return window.goProtectedPath('/reboot/factory');\" class='btn-reboot' style='background:#c0392b;'>FACTORY</a>"
        "<a href='/reboot/app_last' class='btn-reboot' style='background:#27ae60;'>APP LAST</a>"
        "<a href='/reboot/ota0' class='btn-reboot' style='background:#2980b9;'>OTA0</a>"
        "<a href='/reboot/ota1' class='btn-reboot' style='background:#8e44ad;'>OTA1</a>"
        "<div style='margin-left:auto;display:flex;flex-wrap:wrap;gap:8px;'>"
        "<a href='/api' class='btn-reboot' style='background:#3498db;'>API</a>"
        "%s%s%s"
        "</div>"
        "</div></div></div>"
        "%s"
        "</div>",
        test_link,
        tasks_link,
        httpservices_link,
        emulator_link,
        mantainer_link,
        programs_link,
        logs_link,
        profile_label,
        reboot_factory_crash_link,
        reboot_app_restore_link,
        promote_factory_link,
        service_status_widget);

    httpd_resp_sendstr_chunk(req, body);
    free(body);
#if DNA_SYS_MONITOR == 0
    static const char sysmon_widget[] =
        "<div class='container'><div class='card' id='sm_card'>"
        "<h2 id='sm_hdr' onclick='smToggle()' style='cursor:pointer;user-select:none;margin-bottom:0'>"
          "\xf0\x9f\x92\xbb Sistema<span id='sm_icon' style='float:right;font-size:14px;margin-top:4px'>\xe2\x96\xb8</span></h2>"
        "<div id='sm_body' style='display:none;margin-top:12px'>"
        "<div class='sm-row'><span class='sm-lbl'>DRAM</span>"
          "<div class='sm-bar'><div class='sm-fill' id='sm_dram'></div></div>"
          "<span class='sm-val' id='sm_dram_v'>\xe2\x80\x94</span></div>"
        "<div class='sm-row'><span class='sm-lbl'>SPIRAM</span>"
          "<div class='sm-bar'><div class='sm-fill' id='sm_spi'></div></div>"
          "<span class='sm-val' id='sm_spi_v'>\xe2\x80\x94</span></div>"
                "<div class='sm-row' id='sm_cpu0_r'>"
          "<span class='sm-lbl'>Core 0</span>"
          "<div class='sm-bar'><div class='sm-fill sm-cpu' id='sm_c0'></div></div>"
          "<span class='sm-val' id='sm_c0v'>\xe2\x80\x94</span></div>"
                "<div class='sm-row' id='sm_cpu1_r'>"
          "<span class='sm-lbl'>Core 1</span>"
          "<div class='sm-bar'><div class='sm-fill sm-cpu' id='sm_c1'></div></div>"
          "<span class='sm-val' id='sm_c1v'>\xe2\x80\x94</span></div>"
        "<div class='sm-row'><span class='sm-lbl'>Uptime</span>"
          "<div class='sm-bar'></div>"
          "<span class='sm-val' id='sm_up'>\xe2\x80\x94</span></div>"
                "<div style='margin-top:12px;padding-top:10px;border-top:1px solid #ecf0f1'>"
                "<div style='display:flex;align-items:center;justify-content:space-between;gap:8px;margin-bottom:6px'>"
                "<span style='font-size:12px;font-weight:bold;color:#34495e'>RunTime Stats</span>"
                "<label style='font-size:12px;color:#555;display:flex;align-items:center;gap:6px;cursor:pointer'>"
                "<input type='checkbox' id='sm_rts_sw'> Refresh 2s</label></div>"
                "<textarea id='sm_rts_box' readonly "
                "style='width:100%;min-height:170px;resize:vertical;font-family:monospace;font-size:11px;line-height:1.35;padding:8px;border:1px solid #dfe6e9;border-radius:6px;background:#f8fafc;color:#2d3436'></textarea>"
                "</div>"
        "</div>"
        "<style>"
        ".sm-row{display:flex;align-items:center;gap:10px;padding:5px 0;border-bottom:1px solid #ecf0f1}"
        ".sm-lbl{width:60px;font-size:12px;font-weight:bold;color:#34495e;flex-shrink:0}"
        ".sm-bar{flex:1;background:#ecf0f1;border-radius:4px;height:13px;overflow:hidden}"
        ".sm-fill{height:100%;background:#3498db;border-radius:4px;width:0%;transition:width .4s}"
        ".sm-cpu{background:#e67e22}"
        ".sm-val{width:130px;font-size:12px;font-family:monospace;color:#555;text-align:right;flex-shrink:0}"
        "</style>"
        "<script>(function(){"
        "var smOpen=false,smTimer=null,smRtTimer=null,smRtBound=false;"
        "function fb(n){return n>=1048576?(n/1048576).toFixed(1)+' MB':n>=1024?(n/1024).toFixed(0)+' KB':n+' B';}"
        "function fu(s){var d=Math.floor(s/86400),h=Math.floor((s%86400)/3600),m=Math.floor((s%3600)/60),ss=s%60;"
        "return(d?d+'g ':'')+h+'h '+m+'m '+ss+'s';}"
        "async function sm(){"
        "try{var r=await fetch('/api/sysinfo');if(!r.ok)return;var d=await r.json();"
        "var dp=Math.round((1-d.heap.dram_free/d.heap.dram_total)*100);"
        "var sp=Math.round((1-d.heap.spiram_free/d.heap.spiram_total)*100);"
        "document.getElementById('sm_dram').style.width=dp+'%';"
        "document.getElementById('sm_dram_v').textContent=fb(d.heap.dram_free)+' lib ('+dp+'%)';"
        "document.getElementById('sm_spi').style.width=sp+'%';"
        "document.getElementById('sm_spi_v').textContent=fb(d.heap.spiram_free)+' lib ('+sp+'%)';"
        "var c0=(d&&d.cpu&&typeof d.cpu.core0_pct==='number'&&d.cpu.core0_pct>=0)?d.cpu.core0_pct:-1;"
        "var c1=(d&&d.cpu&&typeof d.cpu.core1_pct==='number'&&d.cpu.core1_pct>=0)?d.cpu.core1_pct:-1;"
        "var c0p=(c0<0)?0:Math.max(0,Math.min(100,c0));"
        "var c1p=(c1<0)?0:Math.max(0,Math.min(100,c1));"
        "document.getElementById('sm_c0').style.width=c0p+'%';"
        "document.getElementById('sm_c0v').textContent=(c0<0?'n/d':(Math.round(c0*10)/10+'%'));"
        "document.getElementById('sm_c1').style.width=c1p+'%';"
        "document.getElementById('sm_c1v').textContent=(c1<0?'n/d':(Math.round(c1*10)/10+'%'));"
        "document.getElementById('sm_up').textContent=fu(d.uptime_s);"
        "}catch(e){}"
        "}"
        "async function smRt(){"
        "try{var r=await fetch('/api/runtime_stats',{cache:'no-store'});if(!r.ok)return;var t=await r.text();"
        "var box=document.getElementById('sm_rts_box');if(box)box.value=t||'';}catch(e){}"
        "}"
        "function smRtToggle(){"
        "var sw=document.getElementById('sm_rts_sw');"
        "if(!sw||!smOpen||!sw.checked){if(smRtTimer){clearInterval(smRtTimer);smRtTimer=null;}return;}"
        "if(!smRtTimer){smRtTimer=setInterval(smRt,2000);}"
        "}"
        "function smRtBind(){"
        "if(smRtBound)return;"
        "var sw=document.getElementById('sm_rts_sw');if(!sw)return;"
        "try{sw.checked=localStorage.getItem('sm_rts_sw')==='1';}catch(e){}"
        "sw.addEventListener('change',function(){"
        "try{localStorage.setItem('sm_rts_sw',sw.checked?'1':'0');}catch(e){}"
        "if(sw.checked){smRt();}"
        "smRtToggle();"
        "});"
        "smRtBound=true;"
        "}"
        "window.smToggle=function(){"
        "smOpen=!smOpen;"
        "document.getElementById('sm_body').style.display=smOpen?'':'none';"
        "document.getElementById('sm_icon').textContent=smOpen?'\xe2\x96\xbe':'\xe2\x96\xb8';"
        "try{localStorage.setItem('sm_open',smOpen?'1':'0');}catch(e){}"
        "if(smOpen){sm();smTimer=setInterval(sm,1000);smRtBind();smRt();smRtToggle();}"
        "else{clearInterval(smTimer);smTimer=null;clearInterval(smRtTimer);smRtTimer=null;}"
        "};"
        "try{if(localStorage.getItem('sm_open')==='1')window.smToggle();}catch(e){}"
        "})();</script>"
        "</div></div>";
    httpd_resp_sendstr_chunk(req, sysmon_widget);
#endif /* DNA_SYS_MONITOR == 0 */
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
#endif
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

    bool remote_enabled = http_services_is_remote_enabled();
    bool remote_online = http_services_is_remote_online();
    bool remote_token = http_services_has_auth_token();

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
             "\"mdb\":{\"coin_online\":%s,\"coin_state\":%d,\"credit\":%lu},"
             "\"sd\":{\"mounted\":%s,\"present\":%s,\"total_kb\":%llu,\"used_kb\":%llu,\"last_error\":\"%s\"},"
             "\"env\":{\"temp\":%.1f,\"hum\":%.1f},"
             "\"dna\":{"
               "\"pwm\":%d,\"led_strip\":%d,\"rs232\":%d,\"rs485\":%d,\"mdb\":%d,"
               "\"sht40\":%d,\"cctalk\":%d,\"sd_card\":%d,\"ethernet\":%d,"
               "\"wifi\":%d,\"usb_scanner\":%d,\"io_expander\":%d,\"remote_logging\":%d"
             "},"
             "\"config\":%s}",
             running ? running->label : "?", boot ? boot->label : "?", ap_ip, sta_ip, eth_ip,
             web_ui_is_running() ? "true" : "false",
             remote_enabled ? "true" : "false",
             remote_online ? "true" : "false",
             remote_token ? "true" : "false",
             mdb->coin.is_online ? "true" : "false", mdb->coin.state, mdb->coin.credit_cents,
             sd_mounted ? "true" : "false", sd_present ? "true" : "false",
             (unsigned long long)sd_total_kb, (unsigned long long)sd_used_kb,
             sd_card_get_last_error(),
             tasks_get_temperature(), tasks_get_humidity(),
             DNA_PWM, DNA_LED_STRIP, DNA_RS232, DNA_RS485, DNA_MDB,
             DNA_SHT40, DNA_CCTALK, DNA_SD_CARD, DNA_ETHERNET,
             DNA_WIFI, DNA_USB_SCANNER, DNA_IO_EXPANDER, DNA_REMOTE_LOGGING,
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
#if WEB_UI_USE_EMBEDDED_PAGES == 0
    return webpages_send_external_or_error(req, "stats.html", "text/html; charset=utf-8");
#else
    esp_err_t ext_page_ret = webpages_try_send_external(req, "stats.html", "text/html; charset=utf-8");
    if (ext_page_ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[C] GET /stats");
    const char *extra_style = WEBPAGE_STATS_EXTRA_STYLE;

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Statistiche Device", extra_style, true);

    const char *body = WEBPAGE_STATS_BODY;

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
#endif
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
#if WEB_UI_USE_EMBEDDED_PAGES == 0
    return webpages_send_external_or_error(req, "tasks.html", "text/html; charset=utf-8");
#else
    esp_err_t ext_page_ret = webpages_try_send_external(req, "tasks.html", "text/html; charset=utf-8");
    if (ext_page_ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[C] GET /tasks");

    const char *extra_style = WEBPAGE_TASKS_EXTRA_STYLE;

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Editor Tasks", extra_style, true);

    const char *body = WEBPAGE_TASKS_BODY;

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
#endif
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
#if WEB_UI_USE_EMBEDDED_PAGES == 0
    return webpages_send_external_or_error(req, "httpservices.html", "text/html; charset=utf-8");
#else
    esp_err_t ext_page_ret = webpages_try_send_external(req, "httpservices.html", "text/html; charset=utf-8");
    if (ext_page_ret == ESP_OK) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "[C] GET /httpservices");

    const char *extra_style = WEBPAGE_HTTPSERVICES_EXTRA_STYLE;

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "HTTP Services", extra_style, true);

    const char *body = WEBPAGE_HTTPSERVICES_BODY;

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
#endif
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
#if WEB_UI_USE_EMBEDDED_PAGES == 0
    return webpages_send_external_or_error(req, "api.html", "text/html; charset=utf-8");
#else
    esp_err_t ext_page_ret = webpages_try_send_external(req, "api.html", "text/html; charset=utf-8");
    if (ext_page_ret == ESP_OK) {
        return ESP_OK;
    }

    if (send_http_log) ESP_LOGI(TAG, "[C] GET /api (index)");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    const char *extra_style = WEBPAGE_API_INDEX_EXTRA_STYLE;
    send_head(req, "API Endpoints", extra_style, true);

    const char *body = WEBPAGE_API_INDEX_BODY;

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
#endif
}
