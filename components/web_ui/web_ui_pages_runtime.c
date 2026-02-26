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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "WEB_UI_PAGES_RUNTIME"

esp_err_t root_get_handler(httpd_req_t *req)
{
    bool show_test = web_ui_feature_enabled(WEB_UI_FEATURE_HOME_TEST);
    bool show_tasks = web_ui_feature_enabled(WEB_UI_FEATURE_HOME_TASKS);
    bool show_httpservices = web_ui_feature_enabled(WEB_UI_FEATURE_HOME_HTTP_SERVICES);
    bool show_emulator = web_ui_feature_enabled(WEB_UI_FEATURE_HOME_EMULATOR);
    bool show_programs = web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS);
    const char *profile_label = web_ui_profile_view_label();
    const bool is_factory_view = (strcmp(profile_label, "Factory View") == 0);
    const char *home_title = is_factory_view ? "Factory Console" : "MH1001 control";

    const char *extra_style =
        ".card{background:white;padding:25px;margin:20px 0;border-radius:12px;box-shadow:0 4px 15px rgba(0,0,0,0.1);transition:.3s}"
        ".card:hover{transform:translateY(-5px)}h2{color:#2c3e50;border-bottom:3px solid #3498db;padding-bottom:10px;margin-top:0}"
        ".grid{display:grid;grid-template-columns:1fr 1fr;gap:20px}@media(max-width:600px){.grid{grid-template-columns:1fr}}"
        ".btn-link{display:flex;align-items:center;padding:20px;background:#3498db;color:white;text-decoration:none;border-radius:8px;font-weight:bold;font-size:18px;transition:.3s;gap:15px}"
        ".btn-link:hover{background:#2980b9;box-shadow:0 4px 8px rgba(0,0,0,0.2)}"
        ".btn-config{background:#27ae60}.btn-config:hover{background:#219150}"
        ".btn-test{background:#e67e22}.btn-test:hover{background:#d35400}"
        ".btn-emu{background:#8e44ad}.btn-emu:hover{background:#7d3c98}"
        ".btn-ota{background:#e74c3c}.btn-ota:hover{background:#c0392b}.icon{font-size:30px}"
        ".btn-reboot{display:inline-block;padding:10px 20px;background:#2c3e50;color:white;text-decoration:none;border-radius:5px;margin-top:10px;font-weight:bold}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, home_title, extra_style, false);

    const char *test_link = show_test
        ? "<a href='/test' class='btn-link btn-test'><span class='icon'>🔧</span><span>Test Hardware</span></a>"
        : "";
    const char *tasks_link = show_tasks
        ? "<a href='/tasks' class='btn-link'><span class='icon'>📋</span><span>Editor CSV</span></a>"
        : "";
    const char *httpservices_link = show_httpservices
        ? "<a href='/httpservices' class='btn-link'><span class='icon'>🔐</span><span>HTTP Services</span></a>"
        : "";
    const char *emulator_link = show_emulator
        ? "<a href='/emulator' class='btn-link btn-emu'><span class='icon'>🕹️</span><span>Emulatore</span></a>"
        : "";
    const char *programs_link = show_programs
        ? "<a href='/config/programs' class='btn-link'><span class='icon'>📊</span><span>Tabella Programmi</span></a>"
        : "";
    const char *reboot_factory_crash_link = is_factory_view
        ? "<a href='#' onclick=\"if(confirm('Confermi crash intenzionale?')){fetch('/api/debug/crash',{method:'POST'});setTimeout(()=>location.reload(),600);} return false;\" class='btn-reboot' style='background:#e74c3c;'>Crash</a>"
        : "";
    const char *reboot_app_restore_link = !is_factory_view
        ? "<a href='#' onclick=\"if(confirm('Confermi il ripristino di OTA_x con la versione Factory?')){fetch('/api/debug/restore',{method:'POST'}).then(r=>r.text()).then(t=>alert(t)).catch(()=>alert('Errore restore'));} return false;\" class='btn-reboot' style='background:#f39c12;'>Restore</a>"
        : "";

    int body_len = snprintf(NULL, 0,
        "<div class='container'><div class='grid'>"
        "<a href='/config' class='btn-link btn-config'><span class='icon'>⚙️</span><span>Configurazione</span></a>"
        "<a href='/stats' class='btn-link'><span class='icon'>📈</span><span>Statistiche</span></a>"
        "<a href='/files' class='btn-link'><span class='icon'>📁</span><span>File Manager</span></a>"
        "%s%s%s%s%s"
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
        "%s%s"
        "</div>"
        "</div></div></div></div>",
        test_link,
        tasks_link,
        httpservices_link,
        emulator_link,
        programs_link,
        profile_label,
        reboot_factory_crash_link,
        reboot_app_restore_link);

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
        "%s%s%s%s%s"
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
        "%s%s"
        "</div>"
        "</div></div></div></div>",
        test_link,
        tasks_link,
        httpservices_link,
        emulator_link,
        programs_link,
        profile_label,
        reboot_factory_crash_link,
        reboot_app_restore_link);

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
        "<div class='sm-row' id='sm_cpu0_r' style='display:none'>"
          "<span class='sm-lbl'>Core 0</span>"
          "<div class='sm-bar'><div class='sm-fill sm-cpu' id='sm_c0'></div></div>"
          "<span class='sm-val' id='sm_c0v'>\xe2\x80\x94</span></div>"
        "<div class='sm-row' id='sm_cpu1_r' style='display:none'>"
          "<span class='sm-lbl'>Core 1</span>"
          "<div class='sm-bar'><div class='sm-fill sm-cpu' id='sm_c1'></div></div>"
          "<span class='sm-val' id='sm_c1v'>\xe2\x80\x94</span></div>"
        "<div class='sm-row'><span class='sm-lbl'>Uptime</span>"
          "<div class='sm-bar'></div>"
          "<span class='sm-val' id='sm_up'>\xe2\x80\x94</span></div>"
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
        "var smOpen=false,smTimer=null;"
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
        "if(d.cpu.available&&d.cpu.core0_pct>=0){"
        "  document.getElementById('sm_cpu0_r').style.display='';"
        "  document.getElementById('sm_cpu1_r').style.display='';"
        "  document.getElementById('sm_c0').style.width=d.cpu.core0_pct+'%';"
        "  document.getElementById('sm_c0v').textContent=d.cpu.core0_pct+'%';"
        "  document.getElementById('sm_c1').style.width=d.cpu.core1_pct+'%';"
        "  document.getElementById('sm_c1v').textContent=d.cpu.core1_pct+'%';"
        "}"
        "document.getElementById('sm_up').textContent=fu(d.uptime_s);"
        "}catch(e){}"
        "}"
        "window.smToggle=function(){"
        "smOpen=!smOpen;"
        "document.getElementById('sm_body').style.display=smOpen?'':'none';"
        "document.getElementById('sm_icon').textContent=smOpen?'\xe2\x96\xbe':'\xe2\x96\xb8';"
        "try{localStorage.setItem('sm_open',smOpen?'1':'0');}catch(e){}"
        "if(smOpen){sm();smTimer=setInterval(sm,1000);}"
        "else{clearInterval(smTimer);smTimer=null;}"
        "};"
        "try{if(localStorage.getItem('sm_open')==='1')window.smToggle();}catch(e){}"
        "})();</script>"
        "</div></div>";
    httpd_resp_sendstr_chunk(req, sysmon_widget);
#endif /* DNA_SYS_MONITOR == 0 */
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

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

    char *config_json = device_config_to_json(device_config_get());

    char *resp = malloc(4096);
    if (!resp) {
        if (config_json) free(config_json);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    snprintf(resp, 4096,
             "{\"partition_running\":\"%s\",\"partition_boot\":\"%s\",\"ip_ap\":\"%s\",\"ip_sta\":\"%s\",\"ip_eth\":\"%s\","
             "\"web\":{\"running\":%s},"
             "\"mdb\":{\"coin_online\":%s,\"coin_state\":%d,\"credit\":%lu},"
             "\"sd\":{\"mounted\":%s,\"present\":%s,\"total_kb\":%llu,\"used_kb\":%llu,\"last_error\":\"%s\"},"
             "\"env\":{\"temp\":%.1f,\"hum\":%.1f},"
             "\"config\":%s}",
             running ? running->label : "?", boot ? boot->label : "?", ap_ip, sta_ip, eth_ip,
             web_ui_is_running() ? "true" : "false",
             mdb->coin.is_online ? "true" : "false", mdb->coin.state, mdb->coin.credit_cents,
             sd_card_is_mounted() ? "true" : "false", sd_card_is_present() ? "true" : "false",
             sd_card_get_total_size(), sd_card_get_used_size(),
             sd_card_get_last_error(),
             tasks_get_temperature(), tasks_get_humidity(),
             config_json ? config_json : "{}");

    if (config_json) free(config_json);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    return ret;
}

esp_err_t stats_page_handler(httpd_req_t *req)
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
        "<div class='section'><h2> SD Card</h2><div id='sd_card'>Caricamento...</div></div>"
        "<div class='section'><h2>🔌 Stato Driver</h2><div id='sensors'>Caricamento...</div></div>"
        "<div class='section'><h2>️ Ambiente</h2><div id='env_info'>Caricamento...</div></div>"
        "<div class='section'><h2>🎰 MDB Status</h2><div id='mdb_info'>Caricamento...</div></div>"
        "</div>"
        "<script>"
        "async function loadStats(){"
        "try{"
        "const r=await fetch('/status');if(!r.ok)throw new Error('Status Error');const status=await r.json();"
        "const config=status.config;"
        "document.getElementById('network').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Indirizzo IP Ethernet</span><span class='stat-value'>${status.ip_eth||'---'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Indirizzo IP WiFi STA</span><span class='stat-value'>${status.ip_sta||'---'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Indirizzo IP WiFi AP</span><span class='stat-value'>${status.ip_ap||'---'}</span></div>`;"
        "document.getElementById('partitions').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Partizione Corrente</span><span class='stat-value'>${status.partition_running||'?'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Partizione al Boot</span><span class='stat-value'>${status.partition_boot||'?'}</span></div>`;"
        "const sd=status.sd;"
        "let sd_status_text='Non Trovata'; let sd_badge='badge-off';"
        "if(sd.mounted){sd_status_text='Montata'; sd_badge='badge-on';}"
        "else if(sd.present){sd_status_text='Presente (Non Montata)'; sd_badge='badge-on';}"
        "document.getElementById('sd_card').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Stato</span><span class='badge ${sd_badge}'>${sd_status_text}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Spazio Totale</span><span class='stat-value'>${sd.mounted?(sd.total_kb/1024).toFixed(1)+' MB':'---'}</span></div>"
        "<div class='stat-row'><span class='stat-label'>Spazio Usato</span><span class='stat-value'>${sd.mounted?(sd.used_kb/1024).toFixed(1)+' MB ('+((sd.used_kb/sd.total_kb)*100).toFixed(1)+'%)':'---'}</span></div>`;"
        "const env=status.env;"
        "document.getElementById('env_info').innerHTML="
        "`<div class='stat-row'><span class='stat-label'>Temperatura</span><span class='stat-value'>${env.temp.toFixed(1)} °C</span></div>"
        "<div class='stat-row'><span class='stat-label'>Umidità</span><span class='stat-value'>${env.hum.toFixed(1)} %</span></div>`;"
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

esp_err_t tasks_page_handler(httpd_req_t *req)
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
        "<p style='margin:8px 0 14px 0;color:#566573;'>Questa tabella modifica solo i task applicativi da <code>tasks.json</code>. I task di sistema FreeRTOS (es. HTTP server, lwIP, idle) non sono mostrati qui.</p>"
        "<div id='status'></div>"
        "<table id='tasksTable'><thead><tr>"
        "<th>Nome</th><th>Stato</th><th>Priorità</th><th>Core</th><th>Periodo (ms)</th><th>Stack Words</th><th>Stack Mem</th>"
        "</tr></thead><tbody id='tasksBody'>Caricamento...</tbody></table>"
        "<div style='display:flex; gap:10px;'>"
        "<button type='button' class='btn-add' onclick='addRow()'>➕ Aggiungi Task</button>"
        "<button type='button' onclick='saveTasks()'>💾 Salva</button>"
        "<button type='button' onclick='applyTasks()' style='background:#e67e22'>🚀 Applica</button>"
        "<button type='button' onclick='loadTasks()' style='background:#3498db'>🔄 Aggiorna Dati</button>"
        "</div>"
        "</div></div>"
        "<script>"
        "let tasks=[];"
        "const S=['idle','run','pause'],K=['spiram','internal'];"
        "function decode(raw){return raw.map(t=>({name:t.n,state:S[t.s]||'idle',priority:t.p,core:t.c,period_ms:t.m,stack_words:t.w,stack_caps:K[t.k]||'spiram'}));}"
        "function encode(t){const Se={idle:0,run:1,pause:2},Ke={spiram:0,internal:1};return({n:t.name,s:Se[t.state]??0,p:t.priority,c:t.core,m:t.period_ms,w:t.stack_words,k:Ke[t.stack_caps]??0});}"
        "async function loadTasks(){"
        "try{const r=await fetch('/api/tasks');if(!r.ok)throw new Error('HTTP '+r.status);"
        "tasks=decode(await r.json());renderTable();}catch(e){showStatus('Errore caricamento tasks: '+e.message,'error');}}"
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
        "<td><input type='number' value='${task.stack_words}' min='512' step='512' onchange='tasks[${idx}].stack_words=parseInt(this.value)'></td>"
        "<td><select onchange='tasks[${idx}].stack_caps=this.value'>"
        "<option value='spiram' ${(task.stack_caps||'spiram')==='spiram'?'selected':''}>SPIRAM</option>"
        "<option value='internal' ${task.stack_caps==='internal'?'selected':''}>DRAM</option>"
        "</select></td>`;});"
        "}"
        "function addRow(){tasks.push({name:'new_task',state:'idle',priority:4,core:0,period_ms:100,stack_words:2048,stack_caps:'spiram'});renderTable();}"
        "async function saveTasks(){"
        "try{const r=await fetch('/api/tasks/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(tasks.map(encode))});"
        "if(r.ok){showStatus('✅ Tasks salvate con successo!','success');}else{showStatus('❌ Errore durante il salvataggio','error');}}catch(e){showStatus('❌ Errore: '+e,'error');}}"
        "async function applyTasks(){"
        "try{if(!confirm('Salvare e applicare le modifiche ai task attivi?')) return;"
        "const rs=await fetch('/api/tasks/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(tasks.map(encode))});"
        "if(!rs.ok) throw new Error('Salvataggio fallito');"
        "const ra=await fetch('/api/tasks/apply',{method:'POST'});"
        "if(ra.ok){showStatus('🚀 Tasks applicati con successo!','success');}else{showStatus('❌ Errore durante l\\'applicazione','error');}}catch(e){showStatus('❌ Errore: '+e,'error');}}"
        "function showStatus(msg,type){const s=document.getElementById('status');s.textContent=msg;s.className=type;setTimeout(()=>s.className='',3000);}"
        "window.addEventListener('load',loadTasks);"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t httpservices_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /httpservices");

    const char *extra_style =
        ".section.collapsed > :not(h2){display:none !important;}"
        ".section h2 .section-toggle-icon{position:absolute; right:12px; top:6px; font-size:18px; color:#95a5a6;}"
        ".test-item{display:flex;align-items:center;justify-content:space-between;padding:12px;margin:8px 0;background:#ecf0f1;border-radius:6px}"
        ".test-controls{display:flex;gap:8px;align-items:center}"
        "input[type=text], input[type=password]{padding:6px;border:1px solid #bdc3c7;border-radius:4px;color:#333;}"
        "input[readonly]{background:#f6f8fa; font-family:monospace;}"
        "/* button visual feedback */"
        ".test-controls button{transition:background-color .12s ease, transform .06s ease; cursor:pointer}"
        ".test-controls button:hover{filter:brightness(0.96); transform:translateY(-1px)}"
        ".test-controls button:active{transform:translateY(1px); filter:brightness(0.9)}"
        ".test-controls button:disabled{opacity:0.6; cursor:not-allowed}"
        ".hs-waiting{background:#fffacd} .section#section_calls{max-width:960px; margin-left:auto; margin-right:auto;}"
        "";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "HTTP Services", extra_style, true);

    const char *body =
        "<div class='container'>"
        "<div style='text-align:right; margin-bottom:8px;'><button onclick='location.reload()' style='background:#3498db; color:#fff; padding:6px 10px; border-radius:4px;'>🔄 Aggiorna Pagina</button></div>"
        "<!-- Top request/response boxes -->"
        "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px'>"
        "  <div><label style='font-weight:bold;display:block;margin-bottom:6px'>Richiesta</label><textarea id='hs_request' rows='6' style='width:100%;font-family:monospace;padding:8px;border:1px solid #ddd;border-radius:4px' readonly></textarea></div>"
        "  <div><label style='font-weight:bold;display:block;margin-bottom:6px'>Risposta</label><textarea id='hs_response' rows='6' style='width:100%;font-family:monospace;padding:8px;border:1px solid #ddd;border-radius:4px' readonly></textarea></div>"
        "</div>"
        "<!-- Token visualizer -->"
        "<div style='margin-top:8px'>"
        "  <div class='token-info' style='padding:10px;background:#f5fbff;border:1px solid #e1eef6;border-radius:6px;font-family:monospace'>"
        "    <div style='display:flex;gap:10px;align-items:center;flex-wrap:wrap'><strong style='width:80px'>Token:</strong><input id='hs_token_value' type='text' readonly value='' style='flex:1;min-width:280px;padding:6px;border:1px solid #cfe3ef;border-radius:4px;color:#1b4f72;background:#fff;font-family:monospace'><button id='hs_token_copy' onclick='copyToken()' style='margin-left:8px;padding:6px 10px;border-radius:4px;background:#3498db;color:#fff;border:none;cursor:pointer'>Copia</button><button onclick='localStorage.removeItem(\"httpservices_token\"); showToken(\"\"); document.getElementById(\"hs_response\").value=\"\";' style='padding:6px 8px;border-radius:4px;background:#7f8c8d;color:#fff;border:none;cursor:pointer'>Rimuovi</button></div>"
        "    <div style='margin-top:8px;display:flex;gap:12px;align-items:flex-start'>"
        "      <div style='min-width:160px'><strong>Is JWT</strong><div id='hs_token_isjwt' style='margin-top:6px'>—</div></div>"
        "      <div id='hs_jwt_sections' style='display:none;flex:1'>"
        "        <div style='margin-bottom:8px'><strong>Header</strong><pre id='hs_jwt_header' style='white-space:pre-wrap;background:#fff;padding:8px;border:1px solid #e6eef3;border-radius:4px;max-height:120px;overflow:auto'></pre></div>"
        "        <div><strong>Payload</strong><pre id='hs_jwt_payload' style='white-space:pre-wrap;background:#fff;padding:8px;border:1px solid #e6eef3;border-radius:4px;max-height:220px;overflow:auto'></pre></div>"
        "      </div>"
        "    </div>"
        "  </div>"
        "</div>"

        "<div id='section_httpservices' class='section'><h2 onclick='var s=this.parentElement;s.classList.toggle(\"collapsed\");var i=this.querySelector(\".section-toggle-icon\"); if(i) i.innerText = s.classList.contains(\"collapsed\")?\"▸\":\"▾\";' tabindex='0'>🔐 Login + Chiamate HTTP<span class='section-toggle-icon'>▾</span></h2>"
        "<div class='test-item'><span>Serial:</span><div class='test-controls'><input id='hs_serial' type='text' placeholder='AD-34-DFG-333' style='width:220px'></div></div>"
        "<div class='test-item'><span>Password (MD5):</span><div class='test-controls'><input id='hs_password' type='text' placeholder='md5(...)' style='width:220px'></div></div>"
        "<div class='test-item'><div class='test-controls'><button id='hs_send' onclick='submitLogin()' style='background:#27ae60; color:#fff; padding:8px 12px; border-radius:4px;'>Invia</button></div></div>"

        "<div style='margin:8px 0; padding:8px; border-radius:6px; background:#fbfcfd'>"

        "<div class='test-item'><span>POST /api/keepalive</span><div class='test-controls'><button id='hs_keepalive_btn_1' type='button' onclick='window.hsKeepaliveClick && window.hsKeepaliveClick(this); return false;' style='background:#2980b9;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/getconfig</span><div class='test-controls'><button onclick='callGetConfig()' style='background:#2980b9;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/getimages</span><div class='test-controls'><button onclick='callGetImages()' style='background:#2980b9;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/gettranslations</span><div class='test-controls'><button onclick='callGetTranslations()' style='background:#2980b9;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/getfirmware</span><div class='test-controls'><button onclick='callGetFirmware()' style='background:#2980b9;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/payment</span><div class='test-controls'><button onclick='callPayment()' style='background:#16a085;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/paymentoffline</span><div class='test-controls'><button onclick='callPaymentOffline()' style='background:#16a085;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/serviceused</span><div class='test-controls'><button onclick='callServiceUsed()' style='background:#16a085;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/getcustomers</span><div class='test-controls'><input id='customers_code' value='00011486026039906' style='padding:6px;border:1px solid #ddd;border-radius:4px;width:220px'> <button onclick='if(window.callGetCustomers){window.callGetCustomers(this);}else{var r=document.getElementById(\"hs_response\"); if(r) r.value=\"Errore: callGetCustomers non caricata\";}' style='background:#8e44ad;color:#fff;padding:6px 10px;border-radius:4px;margin-left:8px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/getoperators</span><div class='test-controls'><button onclick='callGetOperators()' style='background:#8e44ad;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"


        "<div class='test-item'><span>POST /api/activity</span><div class='test-controls'><button onclick='callActivity()' style='background:#f39c12;color:#fff;padding:6px 10px;border-radius:4px'>Invia</button></div></div>"

        "</div></div>"



        "</div></div>"


        "</div>"

        "<script>"
        "window.callGetCustomers = async function(btn){ const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='\\u23f3 Inviando...'; } const reqEl=document.getElementById('hs_request'); const respEl=document.getElementById('hs_response'); try{ const codeEl=document.getElementById('customers_code'); const code=(codeEl&&codeEl.value)?codeEl.value.trim():'*'; const payload=JSON.stringify({ Code: (code||'*'), Telephone: '' }); const token=(localStorage.getItem('httpservices_token')||''); const headers={ 'Content-Type':'application/json', 'X-Request-Date': new Date().toUTCString() }; if(token) headers['Authorization']='Bearer '+token; if(reqEl){ let rq='POST /api/getcustomers\\nContent-Type: application/json\\nX-Request-Date: '+headers['X-Request-Date']; if(headers['Authorization']) rq += '\\nAuthorization: '+headers['Authorization']; rq += '\\n\\n'+payload; reqEl.value=rq; } if(respEl){ respEl.value='Invio in corso...'; respEl.classList.add('hs-waiting'); } const res=await fetch('/api/getcustomers',{ method:'POST', headers:headers, body:payload, cache:'no-store' }); const txt=await res.text().catch(()=>('')); if(respEl){ respEl.classList.remove('hs-waiting'); respEl.value=txt || ('HTTP '+res.status); } return {ok:res.ok,status:res.status,text:txt}; }catch(e){ if(respEl){ respEl.classList.remove('hs-waiting'); respEl.value='Errore: '+e; } return {ok:false,status:0,text:String(e)}; } finally { if(btn){ btn.disabled=false; btn.innerHTML=orig; } } };"
        "window.hsKeepaliveClick = async function(btn){ const caller = document.activeElement; btn = btn || ((caller && caller.tagName==='BUTTON')?caller:null); const orig = btn?btn.innerHTML:null; const reqEl = document.getElementById('hs_request'); const resp = document.getElementById('hs_response'); if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } try{ const payload = JSON.stringify({ status: 'OK', inputstates: '00010001', outputstates: '00000000', temperature: 2000, humidity: 4700, subdevices: [{ code: 'QRC', status: 'NOT FOUND' }, { code: 'PRN1', status: 'OK' }] }); const tokenEl = document.getElementById('hs_token_value'); const token = (tokenEl && tokenEl.value) ? tokenEl.value : (localStorage.getItem('httpservices_token') || ''); const headers = { 'Content-Type':'application/json', 'X-Request-Date': new Date().toUTCString() }; if(token) headers['Authorization'] = 'Bearer ' + token; if(reqEl){ let rq = 'POST /api/keepalive\\nContent-Type: application/json\\nX-Request-Date: ' + headers['X-Request-Date']; if(headers['Authorization']) rq += '\\nAuthorization: ' + headers['Authorization']; rq += '\\n\\n' + payload; reqEl.value = rq; } if(resp){ resp.value = '--> POST /api/keepalive\\nInvio in corso...'; resp.classList.add('hs-waiting'); } const res = await fetch('/api/keepalive', { method:'POST', headers: headers, body: payload, cache:'no-store' }); const txt = await res.text().catch(()=>('')); if(resp){ resp.classList.remove('hs-waiting'); resp.value = txt || ('HTTP ' + res.status); } }catch(e){ if(resp){ resp.classList.remove('hs-waiting'); resp.value = 'Errore keepalive: ' + e; } } finally{ if(resp) resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } };"
        "window.callKeepalive = window.hsKeepaliveClick;"
        "window.addEventListener('DOMContentLoaded', async function(){ const stored = localStorage.getItem('httpservices_token'); try{ if(stored){ const respEl = document.getElementById('hs_response'); if(respEl) respEl.value = 'Token (localStorage): ' + stored; try{ showToken(stored); }catch(e){} } const r = await fetch('/api/config'); if(r.ok){ const c = await r.json(); if(c.server){ if(c.server.serial) document.getElementById('hs_serial').value = c.server.serial; if(c.server.password) document.getElementById('hs_password').value = c.server.password; } } }catch(e){} });"

        "async function submitLogin(){ const s = document.getElementById('hs_serial').value || ''; const p = document.getElementById('hs_password').value || ''; const btn = document.getElementById('hs_send'); const origBtn = btn ? btn.innerHTML : null; const reqEl = document.getElementById('hs_request'); const respEl = document.getElementById('hs_response'); if(respEl) { respEl.value = ''; respEl.classList.remove('hs-waiting'); } if(btn){ btn.disabled = true; btn.innerHTML = '⏳ Inviando...'; } if(!s || !p){ if(reqEl) reqEl.value = 'POST /api/login\\n\\n{}'; if(btn){ btn.disabled = false; btn.innerHTML = origBtn; } if(respEl) respEl.value = 'Compila serial e password'; return; } try{ const body = JSON.stringify({ serial: s, password: p }); const headers = { 'Content-Type': 'application/json', 'X-Request-Date': new Date().toUTCString() }; if(reqEl) reqEl.value = 'POST /api/login\\nContent-Type: application/json\\nX-Request-Date: ' + headers['X-Request-Date'] + '\\n\\n' + body; if(respEl){ respEl.value = ''; respEl.classList.add('hs-waiting'); } const res = await fetch('/api/login', { method: 'POST', headers: headers, body: body }); const txt = await res.text().catch(()=>('')); let j = null; try{ j = JSON.parse(txt); }catch(e){} if(res.ok){ if(respEl){ respEl.value = (j?JSON.stringify(j,null,2):txt); } if(j && j.access_token){ try{ showToken(j.access_token); }catch(e){}  localStorage.setItem('httpservices_token', j.access_token); if(respEl) respEl.value += '\\n\\nToken: ' + j.access_token; } } else { if(respEl) respEl.value = txt || ('HTTP ' + res.status); } }catch(e){ console.error(e); const respEl2 = document.getElementById('hs_response'); if(respEl2) respEl2.value = 'Errore di rete: ' + e; } finally { if(respEl) respEl.classList.remove('hs-waiting'); if(btn){ btn.disabled = false; btn.innerHTML = origBtn; } } }"





        "// Token/JWT helper functions - showToken, copyToken, base64UrlDecode"
        "function base64UrlDecode(input){ try{ input = input.replace(/-/g,'+').replace(/_/g,'/'); while(input.length%4) input += '='; var decoded = atob(input); try{ return decodeURIComponent(Array.prototype.map.call(decoded, function(c){ return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2); }).join('')); }catch(e){ return decoded; } }catch(e){ return null; } }"
        "function showToken(token){ try{ var v=document.getElementById('hs_token_value'); var isjwt=document.getElementById('hs_token_isjwt'); var hdr=document.getElementById('hs_jwt_header'); var pl=document.getElementById('hs_jwt_payload'); var sect=document.getElementById('hs_jwt_sections'); if(v){ if(typeof v.value !== 'undefined') v.value = token || ''; else v.textContent = token || '—'; } var parts = token?token.split('.') : []; var isJwt = (parts.length===3); if(isjwt) isjwt.textContent = isJwt? 'Yes' : 'No'; if(isJwt){ var h = base64UrlDecode(parts[0]); var p = base64UrlDecode(parts[1]); if(h && hdr){ try{ hdr.textContent = JSON.stringify(JSON.parse(h), null, 2); }catch(e){ hdr.textContent = h; } } if(p && pl){ try{ pl.textContent = JSON.stringify(JSON.parse(p), null, 2); }catch(e){ pl.textContent = p; } } if(sect) sect.style.display='block'; } else { if(hdr) hdr.textContent=''; if(pl) pl.textContent=''; if(sect) sect.style.display='none'; } }catch(e){} }"
        "function copyToken(){ try{ var el = document.getElementById('hs_token_value') || {value:'',textContent:''}; var t = (typeof el.value !== 'undefined' ? el.value : el.textContent) || ''; if(!t) return; if(navigator.clipboard && navigator.clipboard.writeText){ navigator.clipboard.writeText(t); alert('Token copiato'); } else { var ta=document.createElement('textarea'); ta.value=t; document.body.appendChild(ta); ta.select(); document.execCommand('copy'); ta.remove(); alert('Token copiato'); } }catch(e){} }"
        "// Generic POST helper that injects Authorization from Token textbox when available"
        "async function doPostLocal(path, body){ try{ const t = localStorage.getItem('httpservices_token') || ''; const headers = {'Content-Type':'application/json','X-Request-Date': new Date().toUTCString()}; if(t) headers['Authorization'] = 'Bearer ' + t; const reqEl = document.getElementById('hs_request'); const respEl = document.getElementById('hs_response'); const payload = body || '{}'; if(reqEl){ let rq = 'POST ' + path + '\\nContent-Type: application/json\\nX-Request-Date: ' + headers['X-Request-Date']; if(headers['Authorization']) rq += '\\nAuthorization: ' + headers['Authorization']; rq += '\\n\\n' + payload; reqEl.value = rq; } if(respEl){ respEl.value = ''; respEl.classList.add('hs-waiting'); } const res = await fetch(path, { method: 'POST', headers: headers, body: payload }); const txt = await res.text().catch(()=>('')); if(respEl){ respEl.classList.remove('hs-waiting'); respEl.value = txt || ('HTTP ' + res.status); } return {ok:res.ok, status: res.status, text: txt}; }catch(e){ const respEl = document.getElementById('hs_response'); if(respEl){ respEl.classList.remove('hs-waiting'); respEl.value = 'Errore: ' + e; } return {ok:false, status:0, text: String(e)}; } }"

        "async function callGetConfig(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const r = await doPostLocal('/api/getconfig'); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "async function callGetImages(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const r = await doPostLocal('/api/getimages'); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "async function callGetTranslations(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const r = await doPostLocal('/api/gettranslations'); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "async function callGetFirmware(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const r = await doPostLocal('/api/getfirmware'); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "async function callPayment(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const sample = JSON.stringify({ amount: 100 }); const r = await doPostLocal('/api/payment', sample); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "async function callPaymentOffline(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const sample = JSON.stringify({ amount: 100 }); const r = await doPostLocal('/api/paymentoffline', sample); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "async function callServiceUsed(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const sample = JSON.stringify({ service: 'test' }); const r = await doPostLocal('/api/serviceused', sample); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "window.callGetCustomers = async function(btn){ const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='\u23f3 Inviando...'; } const resp = document.getElementById('hs_response'); const req = document.getElementById('hs_request'); try{ if(resp) resp.value = 'Invio in corso...'; const codeEl = document.getElementById('customers_code'); const code = (codeEl && codeEl.value) ? codeEl.value.trim() : '*'; const payload = JSON.stringify({ Code: code || '*', Telephone: '' }); if(req) req.value = 'POST /api/getcustomers\\nContent-Type: application/json\\n\\n' + payload; const r = await doPostLocal('/api/getcustomers', payload); if(resp) resp.value = r.text || ('HTTP ' + r.status); }catch(e){ if(resp) resp.value = 'Errore: ' + e; else alert('Errore: ' + e); } finally{ if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "async function callGetOperators(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const r = await doPostLocal('/api/getoperators'); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"
        "async function callActivity(){ const caller = document.activeElement; const btn = (caller && caller.tagName==='BUTTON')?caller:null; const orig = btn?btn.innerHTML:null; if(btn){ btn.disabled=true; btn.innerHTML='⏳ Inviando...'; } const resp = document.getElementById('hs_response'); try{ resp.value = 'Invio in corso...'; const r = await doPostLocal('/api/activity', JSON.stringify({ action: 'test' })); resp.value = r.text || ('HTTP ' + r.status); }catch(e){ resp.value = 'Errore: ' + e; } finally{ resp.scrollTop = 0; if(btn){ btn.disabled=false; btn.innerHTML = orig; } } }"

        "function clearResponse(){ try{ document.getElementById('hs_response').value = ''; }catch(e){} }"

        "</script>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t api_index_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /api (index)");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    const char *extra_style =
        "table{width:100%;border-collapse:collapse}th,td{padding:8px;border-bottom:1px solid #e5e7eb;text-align:left;vertical-align:top}"
        "th{background:#f8fafc;position:sticky;top:0}"
        ".api-btn{padding:6px 10px;border:0;border-radius:6px;cursor:pointer;background:#3498db;color:#fff;font-weight:bold}"
        ".api-btn:hover{background:#2d7fb8}.mono{font-family:monospace}"
        ".hint{color:#6b7280;font-size:12px}";
    send_head(req, "API Endpoints", extra_style, true);

    const char *body =
        "<div class='container'><div class='card'><h2>📡 API Endpoints</h2>"
        "<p class='hint'>Tutte le API sono azionabili. Per endpoint con parametri viene richiesto un popup (query o JSON).</p>"
        "<table>"
        "<tr><th>Method</th><th>URI</th><th>Description</th><th>Action</th></tr>"
        "<tr><td>GET</td><td class='mono'>/api</td><td>API index</td><td><a class='api-btn' href='/api'>Apri</a></td></tr>"
        "<tr><td>GET</td><td class='mono'>/status</td><td>Device status JSON</td><td><a class='api-btn' href='/status'>Apri</a></td></tr>"
        "<tr><td>GET</td><td class='mono'>/api/config</td><td>Current configuration</td><td><a class='api-btn' href='/api/config'>Apri</a></td></tr>"
        "<tr><td>GET</td><td class='mono'>/api/ui/texts</td><td>UI texts</td><td><a class='api-btn' href='/api/ui/texts'>Apri</a></td></tr>"
        "<tr><td>GET</td><td class='mono'>/api/ui/languages</td><td>UI languages</td><td><a class='api-btn' href='/api/ui/languages'>Apri</a></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/config/save</td><td>Save configuration (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/config/save',false,true,'{}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/config/backup</td><td>Backup configuration</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/config/backup',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/config/reset</td><td>Factory reset</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/config/reset',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/ntp/sync</td><td>Force NTP sync</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/ntp/sync',false,false,'')\">Invia</button></td></tr>"

        "<tr><td>GET</td><td class='mono'>/api/files/list</td><td>File list (query: storage)</td><td><button class='api-btn' onclick=\"apiCall('GET','/api/files/list',true,false,'','storage=spiffs')\">Apri</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/files/upload</td><td>Upload raw (usa /files per body binario)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/files/upload',true,false,'','storage=spiffs&name=test.txt')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/files/delete</td><td>Delete file (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/files/delete',false,true,'{\\\"storage\\\":\\\"spiffs\\\",\\\"name\\\":\\\"activity.json\\\"}')\">Invia</button></td></tr>"
        "<tr><td>GET</td><td class='mono'>/api/files/download</td><td>Download file (query)</td><td><button class='api-btn' onclick=\"apiCall('GET','/api/files/download',true,false,'','storage=spiffs&name=activity.json')\">Apri</button></td></tr>"

        "<tr><td>GET</td><td class='mono'>/api/programs</td><td>Programs table</td><td><a class='api-btn' href='/api/programs'>Apri</a></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/programs/save</td><td>Save programs (JSON, feature-gated)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/programs/save',false,true,'[]')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/security/password</td><td>Set UI password (JSON, feature-gated)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/security/password',false,true,'{\\\"password\\\":\\\"1234\\\"}')\">Invia</button></td></tr>"

        "<tr><td>POST</td><td class='mono'>/api/emulator/relay</td><td>Emulator relay control (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/emulator/relay',false,true,'{\\\"id\\\":1,\\\"on\\\":true}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/emulator/coin</td><td>Emulator coin event (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/emulator/coin',false,true,'{\\\"value\\\":100}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/emulator/program/start</td><td>Program start (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/emulator/program/start',false,true,'{\\\"program\\\":1}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/emulator/program/stop</td><td>Program stop</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/emulator/program/stop',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/emulator/program/pause_toggle</td><td>Program pause toggle</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/emulator/program/pause_toggle',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>GET</td><td class='mono'>/api/emulator/fsm/messages</td><td>FSM queue/snapshot</td><td><a class='api-btn' href='/api/emulator/fsm/messages'>Apri</a></td></tr>"

        "<tr><td>GET</td><td class='mono'>/api/tasks</td><td>Tasks CSV</td><td><a class='api-btn' href='/api/tasks'>Apri</a></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/tasks/save</td><td>Save tasks (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/tasks/save',false,true,'[]')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/tasks/apply</td><td>Apply tasks</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/tasks/apply',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/test/*</td><td>Run internal tests</td><td><button class='api-btn' onclick=\"alert('Endpoint wildcard: usare la pagina /test')\">Info</button></td></tr>"

        "<tr><td>GET</td><td class='mono'>/api/logs</td><td>Stored logs</td><td><a class='api-btn' href='/api/logs'>Apri</a></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/logs/receive</td><td>Receive external logs (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/logs/receive',false,true,'{\\\"level\\\":\\\"I\\\",\\\"tag\\\":\\\"EXT\\\",\\\"msg\\\":\\\"test\\\"}')\">Invia</button></td></tr>"
        "<tr><td>GET</td><td class='mono'>/api/logs/levels</td><td>Log levels</td><td><a class='api-btn' href='/api/logs/levels'>Apri</a></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/logs/level</td><td>Set log level (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/logs/level',false,true,'{\\\"tag\\\":\\\"*\\\",\\\"level\\\":\\\"INFO\\\"}')\">Invia</button></td></tr>"
        "<tr><td>OPTIONS</td><td class='mono'>/api/logs/*</td><td>CORS preflight</td><td><button class='api-btn' onclick=\"apiCall('OPTIONS','/api/logs/receive',false,false,'')\">Invia</button></td></tr>"

        "<tr><td>GET</td><td class='mono'>/api/debug/usb/enumerate</td><td>USB enumerate</td><td><a class='api-btn' href='/api/debug/usb/enumerate'>Apri</a></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/debug/usb/restart</td><td>Restart USB host</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/debug/usb/restart',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/debug/crash</td><td>Crash intenzionale (Factory)</td><td><button class='api-btn' onclick=\"if(confirm('Confermi crash intenzionale?')) apiCall('POST','/api/debug/crash',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/debug/restore</td><td>Restore factory su OTA inattiva (APP)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/debug/restore',false,false,'')\">Invia</button></td></tr>"

        "<tr><td>POST</td><td class='mono'>/api/login</td><td>Authenticate remote (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/login',false,true,'{\\\"serial\\\":\\\"AD-34-DFG-333\\\",\\\"password\\\":\\\"md5...\\\"}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/getconfig</td><td>Get remote config</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/getconfig',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/getimages</td><td>Fetch images</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/getimages',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/gettranslations</td><td>Fetch translations</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/gettranslations',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/getfirmware</td><td>Fetch firmware</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/getfirmware',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/payment</td><td>Payment request (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/payment',false,true,'{\\\"amount\\\":100}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/paymentoffline</td><td>Offline payment (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/paymentoffline',false,true,'{\\\"amount\\\":100}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/serviceused</td><td>Service used (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/serviceused',false,true,'{\\\"service\\\":\\\"test\\\"}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/getcustomers</td><td>Get customers (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/getcustomers',false,true,'{\\\"Code\\\":\\\"00011486026039906\\\"}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/getoperators</td><td>Get operators</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/getoperators',false,false,'')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/activity</td><td>Activity (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/activity',false,true,'{\\\"action\\\":\\\"test\\\"}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/deviceactivity</td><td>Device activity (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/deviceactivity',false,true,'{\\\"activityid\\\":999,\\\"status\\\":\\\"CRASH\\\"}')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/keepalive</td><td>Keepalive (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/keepalive',false,true,'{\\\"status\\\":\\\"OK\\\"}')\">Invia</button></td></tr>"

        "<tr><td>GET</td><td class='mono'>/api/remote/files/list</td><td>Remote file list (query)</td><td><button class='api-btn' onclick=\"apiCall('GET','/api/remote/files/list',true,false,'','storage=spiffs')\">Apri</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/remote/files/upload</td><td>Remote upload raw (usa /files per body binario)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/remote/files/upload',true,false,'','storage=spiffs&name=test.txt')\">Invia</button></td></tr>"
        "<tr><td>POST</td><td class='mono'>/api/remote/files/delete</td><td>Remote delete file (JSON)</td><td><button class='api-btn' onclick=\"apiCall('POST','/api/remote/files/delete',false,true,'{\\\"storage\\\":\\\"spiffs\\\",\\\"name\\\":\\\"activity.json\\\"}')\">Invia</button></td></tr>"
        "<tr><td>GET</td><td class='mono'>/api/remote/files/download</td><td>Remote download (query)</td><td><button class='api-btn' onclick=\"apiCall('GET','/api/remote/files/download',true,false,'','storage=spiffs&name=activity.json')\">Apri</button></td></tr>"

        "<tr><td>—</td><td>Note</td><td>Alcuni endpoint sono disponibili solo per profilo runtime (Factory/App) o feature flag.</td><td>—</td></tr>"
        "</table>"
        "<script>"
        "async function apiCall(method,path,needsQuery,needsBody,defaultBody,defaultQuery){"
        "  let url=path;"
        "  if(needsQuery){"
        "    const q=prompt('Inserisci query string (senza ?)',defaultQuery||'');"
        "    if(q===null) return;"
        "    const qq=(q||'').trim();"
        "    if(qq) url += (url.indexOf('?')>=0 ? '&' : '?') + qq;"
        "  }"
        "  let body=null;"
        "  const headers={};"
        "  if(needsBody){"
        "    const input=prompt('Inserisci JSON body',defaultBody||'{}');"
        "    if(input===null) return;"
        "    try{ JSON.parse(input||'{}'); }catch(e){ alert('JSON non valido'); return; }"
        "    body=input||'{}';"
        "    headers['Content-Type']='application/json';"
        "  }"
        "  try{"
        "    const r=await fetch(url,{method:method,headers:headers,body:body});"
        "    const t=await r.text();"
        "    alert('HTTP '+r.status+'\\n'+(t||''));"
        "  }catch(e){"
        "    alert('Errore chiamata: '+e);"
        "  }"
        "}"
        "</script>"
        "</div></div></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
