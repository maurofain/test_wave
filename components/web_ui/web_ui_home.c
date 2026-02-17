#include "web_ui_internal.h"
#include "web_ui.h"
#include "esp_log.h"
#include "device_config.h"

#define TAG "WEB_UI_HOME"

esp_err_t reboot_factory_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in Factory Mode...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_factory();
    return ESP_OK;
}

esp_err_t reboot_app_handler(httpd_req_t *req)
{
    httpd_resp_sendstr(req, "<html><body><h1>Reboot in Production Mode...</h1><p>Attendere il riavvio.</p></body></html>");
    vTaskDelay(pdMS_TO_TICKS(1000));
    device_config_reboot_app();
    return ESP_OK;
}

esp_err_t root_get_handler(httpd_req_t *req)
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
        "<a href='/httpservices' class='btn-link'><span class='icon'>🔐</span><span>HTTP Services</span></a>"
        "<a href='/tasks' class='btn-link'><span class='icon'>📋</span><span>Editor CSV</span></a>"
        "<a href='/ota' class='btn-link btn-ota'><span class='icon'>🔄</span><span>Update OTA</span></a>"
        "</div>"
        "<div class='card'><h2>ℹ️ Informazioni</h2>"
        "<p>Benvenuti nell'interfaccia di configurazione e test.</p>"
        "<div style='margin-top:20px; border-top:1px solid #eee; padding-top:15px; display:flex; justify-content:space-between; align-items:center; gap:10px;'>"
        "<div>"
        "<a href='/reboot/factory' class='btn-reboot' style='background:#c0392b;'>Reboot in Factory</a> "
        "<a href='/reboot/app' class='btn-reboot' style='background:#27ae60;'>Reboot in App</a>"
        "</div>"
        "<a href='/api' class='btn-reboot' style='background:#3498db;'>API</a>"
        "</div>"
        "</div>"
        "</div></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t api_index_page_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "[C] GET /api (index)");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "API Endpoints", NULL, true);

    const char *body =
        "<div class='container'><div class='card'><h2>📡 API Endpoints</h2>"
        "<table style='width:100%;border-collapse:collapse'>"
        "<tr><th style='text-align:left;padding:8px;border-bottom:1px solid #ddd'>Method</th><th style='text-align:left;padding:8px;border-bottom:1px solid #ddd'>URI</th><th style='text-align:left;padding:8px;border-bottom:1px solid #ddd'>Description</th></tr>"
        "<tr><td>GET</td><td><a href='/status'>/status</a></td><td>Device status JSON</td></tr>"
        "<tr><td>GET</td><td><a href='/api/config'>/api/config</a></td><td>Current configuration</td></tr>"
        "<tr><td>POST</td><td><a href='/api/config/save'>/api/config/save</a></td><td>Save configuration</td></tr>"
        "<tr><td>POST</td><td><a href='/api/config/backup'>/api/config/backup</a></td><td>Backup configuration</td></tr>"
        "<tr><td>POST</td><td><a href='/api/config/reset'>/api/config/reset</a></td><td>Factory reset</td></tr>"
        "<tr><td>POST</td><td><a href='/api/ntp/sync'>/api/ntp/sync</a></td><td>Force NTP sync</td></tr>"
        "<tr><td>GET</td><td><a href='/api/tasks'>/api/tasks</a></td><td>Tasks CSV</td></tr>"
        "<tr><td>POST</td><td><a href='/api/tasks/save'>/api/tasks/save</a></td><td>Save tasks</td></tr>"
        "<tr><td>POST</td><td><a href='/api/tasks/apply'>/api/tasks/apply</a></td><td>Apply tasks</td></tr>"
        "<tr><td>GET</td><td><a href='/api/test/endpoints'>/api/test/endpoints</a></td><td>Catalogo endpoint test (legacy + REST)</td></tr>"
        "<tr><td>POST</td><td><a href='/api/test/*'>/api/test/*</a></td><td>Run internal tests (legacy: /api/test/&lt;token&gt;)</td></tr>"
        "<tr><td>POST</td><td>/api/test/&lt;group&gt;/&lt;action&gt;</td><td>Run internal tests (REST: es. /api/test/led/start)</td></tr>"
        "<tr><td>GET</td><td><a href='/api/logs'>/api/logs</a></td><td>Stored logs</td></tr>"
        "<tr><td>GET</td><td><a href='/api/logs/levels'>/api/logs/levels</a></td><td>Log levels</td></tr>"
        "<tr><td>POST</td><td><a href='/api/logs/level'>/api/logs/level</a></td><td>Set log level</td></tr>"
        "<tr><td>GET</td><td><a href='/api/debug/usb/enumerate'>/api/debug/usb/enumerate</a></td><td>USB enumerate</td></tr>"
        "<tr><td>POST</td><td><a href='/api/debug/usb/restart'>/api/debug/usb/restart</a></td><td>Restart USB host</td></tr>"
        "<tr><td>POST</td><td><a href='/api/login'>/api/login</a></td><td>Authenticate (remote)</td></tr>"
        "<tr><td>POST</td><td><a href='/api/getconfig'>/api/getconfig</a></td><td>Get remote config</td></tr>"
        "<tr><td>POST</td><td><a href='/api/getimages'>/api/getimages</a></td><td>Fetch images</td></tr>"
        "<tr><td>POST</td><td><a href='/api/gettranslations'>/api/gettranslations</a></td><td>Fetch translations</td></tr>"
        "<tr><td>POST</td><td><a href='/api/getfirmware'>/api/getfirmware</a></td><td>Fetch firmware</td></tr>"
        "<tr><td>POST</td><td><a href='/api/payment'>/api/payment</a></td><td>Payment request</td></tr>"
        "<tr><td>POST</td><td><a href='/api/paymentoffline'>/api/paymentoffline</a></td><td>Offline payment</td></tr>"
        "<tr><td>POST</td><td><a href='/api/serviceused'>/api/serviceused</a></td><td>Service used</td></tr>"
        "<tr><td>POST</td><td><a href='/api/getcustomers'>/api/getcustomers</a></td><td>Get customers</td></tr>"
        "<tr><td>POST</td><td><a href='/api/getoperators'>/api/getoperators</a></td><td>Get operators</td></tr>"
        "<tr><td>POST</td><td><a href='/api/activity'>/api/activity</a></td><td>Activity</td></tr>"
        "<tr><td>POST</td><td><a href='/api/keepalive'>/api/keepalive</a></td><td>Keepalive</td></tr>"
        "</table></div></div></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}
