#include "web_ui_internal.h"
#include "web_ui_programs.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

#define TAG "WEB_UI_PROGRAMS_PAGE"

esp_err_t programs_page_handler(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "404 Non Trovato");
        return httpd_resp_send(req, "404 Non Trovato", -1);
    }

    ESP_LOGI(TAG, "[C] GET /config/programs");

    const char *extra_style =
        ".section{background:white;padding:20px;margin:20px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}"
        "table{width:100%;border-collapse:collapse;margin-top:15px}"
        "th,td{padding:8px;border:1px solid #ddd;text-align:left;color:#333}"
        "th{background:#34495e;color:white}"
        "input[type=text],input[type=number]{width:100%;padding:6px;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}"
        "input[type=checkbox]{transform:scale(1.2)}"
        "button{padding:10px 16px;background:#27ae60;color:white;border:none;border-radius:5px;cursor:pointer;font-weight:bold}"
        "button:hover{background:#229954}"
        ".btn-secondary{background:#3498db}.btn-secondary:hover{background:#2c80b7}"
        ".status{margin:12px 0;padding:10px;border-radius:5px;display:none}"
        ".status.ok{display:block;background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
        ".status.err{display:block;background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Tabella Programmi", extra_style, true);

    const char *body =
        "<div class='container'><div class='section'>"
        "<h2>📊 Editor Tabella Programmi (FACTORY)</h2>"
        "<p>Imposta nome, abilitazione, prezzo, durata e relay mask per ogni programma.</p>"
        "<div id='status' class='status'></div>"
        "<table><thead><tr><th>ID</th><th>Nome</th><th>Abilitato</th><th>Prezzo</th><th>Durata (s)</th><th>Pausa Max (s)</th><th>Relay mask</th></tr></thead><tbody id='programRows'></tbody></table>"
        "<div style='display:flex;gap:10px;margin-top:14px;'>"
        "<button onclick='savePrograms()'>💾 Salva tabella</button>"
        "<button class='btn-secondary' onclick='loadPrograms()'>🔄 Ricarica</button>"
        "</div>"
        "</div></div>"
        "<script>"
        "let programs=[];"
        "function showStatus(msg,ok){const s=document.getElementById('status');s.textContent=msg;s.className='status '+(ok?'ok':'err');}"
        "function rowHtml(p,idx){"
        "return `<tr>`+"
        "`<td><input type='number' min='1' max='255' value='${p.program_id||idx+1}' onchange='programs[${idx}].program_id=parseInt(this.value||0,10)'></td>`+"
        "`<td><input type='text' value='${(p.name||'').replace(/\"/g,'&quot;')}' onchange='programs[${idx}].name=this.value'></td>`+"
        "`<td style='text-align:center'><input type='checkbox' ${p.enabled?'checked':''} onchange='programs[${idx}].enabled=this.checked'></td>`+"
        "`<td><input type='number' min='0' max='65535' value='${p.price_units||0}' onchange='programs[${idx}].price_units=parseInt(this.value||0,10)'></td>`+"
        "`<td><input type='number' min='0' max='65535' value='${p.duration_sec||0}' onchange='programs[${idx}].duration_sec=parseInt(this.value||0,10)'></td>`+"
        "`<td><input type='number' min='0' max='65535' value='${p.pause_max_suspend_sec||0}' onchange='programs[${idx}].pause_max_suspend_sec=parseInt(this.value||0,10)'></td>`+"
        "`<td><input type='number' min='0' max='65535' value='${p.relay_mask||0}' onchange='programs[${idx}].relay_mask=parseInt(this.value||0,10)'></td>`+"
        "`</tr>`;"
        "}"
        "function render(){const b=document.getElementById('programRows');b.innerHTML='';programs.forEach((p,i)=>{b.insertAdjacentHTML('beforeend',rowHtml(p,i));});}"
        "async function loadPrograms(){try{const r=await fetch('/api/programs');if(!r.ok)throw new Error('HTTP '+r.status);const data=await r.json();programs=data.programs||[];render();showStatus('Tabella programmi caricata',true);}catch(e){showStatus('Errore caricamento: '+e.message,false);}}"
        "async function savePrograms(){try{const r=await fetch('/api/programs/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({programs})});if(!r.ok){const t=await r.text();throw new Error(t||('HTTP '+r.status));}showStatus('Tabella programmi salvata',true);}catch(e){showStatus('Errore salvataggio: '+e.message,false);}}"
        "window.addEventListener('load',loadPrograms);"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t api_programs_get(httpd_req_t *req)
{
    char *json = web_ui_program_table_to_json();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

esp_err_t api_programs_save(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "404 Non Trovato");
        return httpd_resp_send(req, "404 Non Trovato", -1);
    }

    if (req->content_len <= 0 || req->content_len > 16384) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char *payload = calloc(1, (size_t)req->content_len + 1);
    if (!payload) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, payload + received, req->content_len - received);
        if (r <= 0) {
            free(payload);
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "Errore lettura payload", -1);
        }
        received += r;
    }

    char err_msg[128] = {0};
    esp_err_t err = web_ui_program_table_update_from_json(payload, (size_t)received, err_msg, sizeof(err_msg));
    free(payload);

    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, err_msg[0] ? err_msg : "Errore validazione", -1);
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
}

esp_err_t api_emulator_relay_control(httpd_req_t *req)
{
    if (req->content_len <= 0 || req->content_len > 1024) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char *payload = calloc(1, (size_t)req->content_len + 1);
    if (!payload) {
        httpd_resp_send_500(req);
        return ESP_ERR_NO_MEM;
    }

    int received = 0;
    while (received < req->content_len) {
        int r = httpd_req_recv(req, payload + received, req->content_len - received);
        if (r <= 0) {
            free(payload);
            httpd_resp_set_status(req, "400 Bad Request");
            return httpd_resp_send(req, "Errore lettura payload", -1);
        }
        received += r;
    }

    cJSON *root = cJSON_Parse(payload);
    free(payload);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    cJSON *relay_number = cJSON_GetObjectItem(root, "relay_number");
    cJSON *status = cJSON_GetObjectItem(root, "status");
    cJSON *duration = cJSON_GetObjectItem(root, "duration");

    if (!cJSON_IsNumber(relay_number) || !cJSON_IsBool(status) || !cJSON_IsNumber(duration)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Campi relay_number/status/duration obbligatori", -1);
    }

    esp_err_t err = web_ui_virtual_relay_control((uint8_t)relay_number->valueint, cJSON_IsTrue(status), (uint32_t)duration->valueint);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "relay_number fuori range", -1);
    }

    char *json = web_ui_virtual_relays_to_json();
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json, strlen(json));
    free(json);
    return ret;
}

esp_err_t api_security_password(httpd_req_t *req)
{
    if (!web_ui_feature_enabled(WEB_UI_FEATURE_ENDPOINT_PROGRAMS)) {
        httpd_resp_set_status(req, "404 Non Trovato");
        return httpd_resp_send(req, "404 Non Trovato", -1);
    }

    if (req->method == HTTP_GET) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"status\":\"ok\",\"editable\":true}", -1);
    }

    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (req->content_len <= 0 || req->content_len > 1024) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char payload[1024] = {0};
    int len = httpd_req_recv(req, payload, req->content_len);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Errore lettura payload", -1);
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    cJSON *current = cJSON_GetObjectItem(root, "current_password");
    cJSON *next = cJSON_GetObjectItem(root, "new_password");
    if (!cJSON_IsString(current) || !cJSON_IsString(next)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Campi current_password/new_password obbligatori", -1);
    }

    if (strcmp(current->valuestring, web_ui_boot_password_get()) != 0) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "403 Forbidden");
        return httpd_resp_send(req, "Password attuale non valida", -1);
    }

    esp_err_t err = web_ui_boot_password_set(next->valuestring);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Nuova password non valida o errore salvataggio", -1);
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
}
