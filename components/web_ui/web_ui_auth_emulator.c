#include "web_ui_internal.h"
#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_log.h"
#include "cJSON.h"
#include "fsm.h"
#include "web_ui_programs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "WEB_UI_EMU";

static const web_ui_program_entry_t *find_program_by_id(uint8_t program_id)
{
    const web_ui_program_table_t *table = web_ui_program_table_get();
    if (!table) {
        return NULL;
    }

    for (uint8_t i = 0; i < table->count; ++i) {
        const web_ui_program_entry_t *entry = &table->programs[i];
        if (entry->program_id == program_id) {
            return entry;
        }
    }

    return NULL;
}

#define WEB_UI_PROTECTED_PASSWORD "factoryUser"
#define WEB_UI_PASSWORD_FILE "/spiffs/ui_password.txt"

static char s_boot_password[64] = {0};
static bool s_boot_password_loaded = false;

static void web_ui_load_boot_password(void)
{
    if (s_boot_password_loaded) {
        return;
    }

    snprintf(s_boot_password, sizeof(s_boot_password), "%s", WEB_UI_PROTECTED_PASSWORD);

    FILE *file = fopen(WEB_UI_PASSWORD_FILE, "r");
    if (!file) {
        s_boot_password_loaded = true;
        return;
    }

    char line[64] = {0};
    if (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) >= 4) {
            snprintf(s_boot_password, sizeof(s_boot_password), "%s", line);
        }
    }

    fclose(file);
    s_boot_password_loaded = true;
}

const char *web_ui_boot_password_get(void)
{
    web_ui_load_boot_password();
    return s_boot_password;
}

esp_err_t web_ui_boot_password_set(const char *new_password)
{
    if (!new_password || strlen(new_password) < 4 || strlen(new_password) >= sizeof(s_boot_password)) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(WEB_UI_PASSWORD_FILE, "w");
    if (!file) {
        return ESP_FAIL;
    }

    size_t len = strlen(new_password);
    size_t written = fwrite(new_password, 1, len, file);
    fclose(file);
    if (written != len) {
        return ESP_FAIL;
    }

    snprintf(s_boot_password, sizeof(s_boot_password), "%s", new_password);
    s_boot_password_loaded = true;
    return ESP_OK;
}

bool web_ui_has_valid_password(httpd_req_t *req)
{
    if (!req) {
        return false;
    }

    char query[128] = {0};
    char password[64] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
        return false;
    }
    if (httpd_query_key_value(query, "pwd", password, sizeof(password)) != ESP_OK) {
        return false;
    }

    return strcmp(password, web_ui_boot_password_get()) == 0;
}

esp_err_t web_ui_send_password_required(httpd_req_t *req, const char *title, const char *target)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_status(req, "401 Unauthorized");

    char html[2048] = {0};
    snprintf(html, sizeof(html),
             "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>%s</title></head>"
             "<body style='font-family:Arial;background:#f5f5f5;padding:24px;'>"
             "<div style='max-width:440px;margin:40px auto;background:#fff;border-radius:10px;padding:20px;box-shadow:0 8px 24px rgba(0,0,0,.12)'>"
             "<h2 style='margin-top:0;color:#2c3e50;'>🔒 Password richiesta</h2>"
             "<p style='color:#566573;'>Inserire la password per continuare.</p>"
             "<form method='GET' action='%s' style='display:flex;flex-direction:column;gap:10px;'>"
             "<input name='pwd' type='password' autocomplete='current-password' placeholder='Password' style='padding:10px;border:1px solid #ccc;border-radius:6px;' required autofocus>"
             "<div style='display:flex;justify-content:flex-end;gap:8px;'>"
             "<a href='/' style='padding:10px 14px;background:#7f8c8d;color:#fff;text-decoration:none;border-radius:6px;'>Annulla</a>"
             "<button type='submit' style='padding:10px 14px;background:#3498db;color:white;border:none;border-radius:6px;cursor:pointer;'>Continua</button>"
             "</div>"
             "</form>"
             "</div></body></html>",
             title,
             target);

    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t emulator_page_handler_local(httpd_req_t *req)
{
    if (!web_ui_has_valid_password(req)) {
        return web_ui_send_password_required(req, "Emulator", "/emulator");
    }

    const char *extra_style =
        ".emu-wrap{width:100%;max-width:2048px;margin:16px auto;padding:0 8px;box-sizing:border-box;display:flex;gap:12px;align-items:stretch}"
        ".emu-panel{flex:0 0 700px;min-width:700px;height:1024px;background:#111;border-radius:16px;padding:14px;box-shadow:0 4px 20px rgba(0,0,0,0.25);box-sizing:border-box}"
        ".emu-layout{height:100%;display:flex;gap:14px}"
        ".emu-left{width:30%;display:grid;grid-template-rows:repeat(8,1fr);gap:10px}"
        ".prog-btn{border:none;border-radius:10px;background:#2c3e50;color:#fff;font-size:20px;font-weight:bold;cursor:pointer}"
        ".prog-btn.active{background:#3498db}"
        ".prog-btn:disabled{opacity:.45;cursor:not-allowed}"
        ".emu-main{width:60%;display:flex;flex-direction:column;gap:14px}"
        ".credit-box{background:#fdfdfd;border-radius:12px;padding:20px;flex:0 0 38%;display:flex;align-items:center;justify-content:center;flex-direction:column;transition:background .2s,color .2s}"
        ".credit-box.state-credit{background:#f1c40f;color:#1f2937}"
        ".credit-box.state-running{background:#e74c3c;color:#fff}"
        ".credit-label{font-size:22px;color:#2c3e50}"
        ".credit-value{font-size:110px;font-weight:bold;line-height:1;color:#111}"
        ".elapsed-value{margin-top:10px;font-size:22px;font-weight:600}"
        ".pause-value{margin-top:4px;font-size:18px;font-weight:500}"
        ".msg-box{background:#fdfdfd;border-radius:12px;padding:16px;flex:1;font-size:18px;color:#2c3e50;display:block;overflow:auto;line-height:1.35}"
        ".emu-gauge{width:10%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:10px}"
        ".gauge-title{color:#fff;font-weight:bold;font-size:18px;text-align:center}"
        ".gauge-track{position:relative;width:100%;height:92%;border-radius:12px;overflow:hidden;border:2px solid #d9d9d9;background:linear-gradient(to top,#c0392b 0%,#c0392b 20%,#27ae60 20%,#27ae60 100%)}"
        ".gauge-mask{position:absolute;left:0;top:0;width:100%;height:0%;background:#111;transition:height .25s linear}"
        ".gauge-text{color:#fff;font-size:20px;font-weight:bold}"
        ".emu-electrical{flex:1;min-width:280px;background:#ffffff;border-radius:16px;padding:12px;box-shadow:0 4px 16px rgba(0,0,0,0.12);box-sizing:border-box}"
        ".side-title{margin:0 0 10px 0;color:#2c3e50;font-size:22px}"
        ".coin-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:14px}"
        ".coin-btn{border:none;border-radius:8px;background:#27ae60;color:#fff;padding:12px 8px;font-size:18px;font-weight:bold;cursor:pointer}"
        ".coin-btn:hover{background:#219150}"
        ".relay-grid{display:grid;grid-template-columns:repeat(5,1fr);gap:8px}"
        ".relay{border-radius:8px;background:#d5d8dc;color:#2c3e50;padding:10px 4px;text-align:center;font-weight:bold;font-size:14px}"
        ".relay.on{background:#f39c12;color:#fff}"
        ".side-note{margin-top:12px;font-size:13px;color:#566573;line-height:1.4}"
        "@media(max-width:1250px){.emu-wrap{flex-direction:column;align-items:center}.emu-panel{flex:none;width:100%;min-width:0;height:920px}.emu-electrical{width:100%;flex:none}.credit-value{font-size:82px}.msg-box{font-size:24px}}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Emulator", extra_style, false);

    const char *body =
        "<div class='emu-wrap'>"
        "<div class='emu-panel'><div class='emu-layout'>"
        "<div class='emu-left'>"
        "<button class='prog-btn' data-id='1'>Programma 1</button>"
        "<button class='prog-btn' data-id='2'>Programma 2</button>"
        "<button class='prog-btn' data-id='3'>Programma 3</button>"
        "<button class='prog-btn' data-id='4'>Programma 4</button>"
        "<button class='prog-btn' data-id='5'>Programma 5</button>"
        "<button class='prog-btn' data-id='6'>Programma 6</button>"
        "<button class='prog-btn' data-id='7'>Programma 7</button>"
        "<button class='prog-btn' data-id='8'>Programma 8</button>"
        "</div>"
        "<div class='emu-main'>"
        "<div id='creditBox' class='credit-box'><div class='credit-label'>Credito (coin)</div><div id='credit' class='credit-value'>0</div><div id='elapsed' class='elapsed-value'>Tempo: 00:00</div><div id='pauseElapsed' class='pause-value'>In pausa: 00:00</div></div>"
        "<div id='msg' class='msg-box'>Nessun evento in coda</div>"
        "</div>"
        "<div class='emu-gauge'>"
        "<div class='gauge-track'><div id='gmask' class='gauge-mask'></div></div>"
        "<div id='gtext' class='gauge-text'>--</div>"
        "</div>"
        "</div></div>"
        "<div class='emu-electrical'>"
        "<h2 class='side-title'>Quadro elettrico</h2>"
        "<h3 class='side-title' style='font-size:18px;margin-top:4px;'>Ricarica Coin</h3>"
        "<div class='coin-grid'>"
        "<button class='coin-btn' data-coin='1'>+1</button>"
        "<button class='coin-btn' data-coin='5'>+5</button>"
        "<button class='coin-btn' data-coin='10'>+10</button>"
        "</div>"
        "<h3 class='side-title' style='font-size:18px;'>Relay</h3>"
        "<div id='relayGrid' class='relay-grid'>"
        "<div class='relay' data-relay='1'>R1</div>"
        "<div class='relay' data-relay='2'>R2</div>"
        "<div class='relay' data-relay='3'>R3</div>"
        "<div class='relay' data-relay='4'>R4</div>"
        "<div class='relay' data-relay='5'>R5</div>"
        "<div class='relay' data-relay='6'>R6</div>"
        "<div class='relay' data-relay='7'>R7</div>"
        "<div class='relay' data-relay='8'>R8</div>"
        "<div class='relay' data-relay='9'>R9</div>"
        "<div class='relay' data-relay='10'>R10</div>"
        "</div>"
        "<div class='side-note'>Layout operativo: pannello utente 800x1280 a sinistra, quadro elettrico a destra con ricarica credito e stato relay.</div>"
        "</div>"
        "</div>"
        "<script>"
        "(function(){"
        "const buttons=[...document.querySelectorAll('.prog-btn')];"
        "const coinButtons=[...document.querySelectorAll('.coin-btn')];"
        "const relays=[...document.querySelectorAll('.relay')];"
        "const creditBox=document.getElementById('creditBox');"
        "const creditEl=document.getElementById('credit');"
        "const elapsedEl=document.getElementById('elapsed');"
        "const pauseElapsedEl=document.getElementById('pauseElapsed');"
        "const msgEl=document.getElementById('msg');"
        "const maskEl=document.getElementById('gmask');"
        "const gaugeText=document.getElementById('gtext');"
        "let credit=0;"
        "let activeProgram=0;"
        "let fsmState='unknown';"
        "let runningElapsedMs=0;"
        "let runningElapsedSyncAt=0;"
        "let pauseElapsedMs=0;"
        "let pauseElapsedSyncAt=0;"
        "let programsById={};"
        "let programMetaByButton={};"
        "function escapeHtml(s){return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}"
        "function renderQueueMessages(messages){if(!Array.isArray(messages)||messages.length===0){msgEl.textContent='Nessun evento in coda';return;}msgEl.innerHTML=messages.map(function(m){return '<div>• '+escapeHtml(m)+'</div>';}).join('');}"
        "function applyRelayIndicators(relayItems){if(!Array.isArray(relayItems))return;const byRelay={};relayItems.forEach(function(item){if(item&&typeof item.relay_number==='number'){byRelay[item.relay_number]=!!item.status;}});relays.forEach(function(relay){const relayNumber=parseInt(relay.dataset.relay||'0',10);if(!relayNumber)return;if(Object.prototype.hasOwnProperty.call(byRelay,relayNumber)){relay.classList.toggle('on',!!byRelay[relayNumber]);}});}"
        "function formatElapsed(ms){const t=Math.max(0,Math.floor(ms/1000));const mm=String(Math.floor(t/60)).padStart(2,'0');const ss=String(t%60).padStart(2,'0');return mm+':'+ss;}"
        "function updateCreditStateUi(){if(!creditBox)return;creditBox.classList.remove('state-credit','state-running');if(fsmState==='credit')creditBox.classList.add('state-credit');if(fsmState==='running')creditBox.classList.add('state-running');}"
        /* updateElapsedUi: aggiorna timer E gauge (tempo rimanente interpolato a 200ms).
         * La barra scende da piena (avvio programma) a vuota (fine programma).
         * Il testo mostra i secondi rimanenti 0-999. */
        "function updateElapsedUi(){if(!elapsedEl)return;let shown=runningElapsedMs;if(fsmState==='running'&&runningElapsedSyncAt){shown += (Date.now()-runningElapsedSyncAt);}elapsedEl.textContent='Tempo: '+formatElapsed(shown);if(pauseElapsedEl){let pauseShown=pauseElapsedMs;if(fsmState==='paused'&&pauseElapsedSyncAt){pauseShown += (Date.now()-pauseElapsedSyncAt);}pauseElapsedEl.textContent='In pausa: '+formatElapsed(pauseShown);}if((fsmState==='running'||fsmState==='paused')&&activeProgram){const p=programMetaByButton[activeProgram];const dur=(p&&p.duration_sec)?p.duration_sec:0;if(dur>0){const elSec=Math.max(0,shown/1000);const remSec=Math.max(0,Math.min(999,Math.round(dur-elSec)));const pct=Math.max(0,Math.min(100,(remSec/dur)*100));if(gaugeText)gaugeText.textContent=remSec+'s';if(maskEl)maskEl.style.height=(100-pct)+'%';}}}"
        "function syncFromFsm(js){if(js&&typeof js.credit_cents==='number'){credit=Math.max(0,Math.floor(js.credit_cents));}if(js&&typeof js.state==='string'){fsmState=js.state;}if(js&&typeof js.running_elapsed_ms==='number'){runningElapsedMs=Math.max(0,Math.floor(js.running_elapsed_ms));runningElapsedSyncAt=Date.now();}if(js&&typeof js.pause_elapsed_ms==='number'){pauseElapsedMs=Math.max(0,Math.floor(js.pause_elapsed_ms));pauseElapsedSyncAt=Date.now();}if(fsmState!=='running'&&fsmState!=='paused'){activeProgram=0;buttons.forEach(function(b){b.classList.remove('active');});runningElapsedSyncAt=0;pauseElapsedSyncAt=0;}if(js&&Array.isArray(js.relays)){applyRelayIndicators(js.relays);}updateCreditStateUi();updateElapsedUi();render();renderQueueMessages(js&&js.messages?js.messages:[]);}"
        "async function refreshQueueMessages(){try{const r=await fetch('/api/emulator/fsm/messages');if(!r.ok)return;const js=await r.json();syncFromFsm(js);}catch(e){console.warn('fsm messages fetch failed',e);}}"
        "async function loadProgramsMeta(){try{const r=await fetch('/api/programs');if(!r.ok)return;const js=await r.json();const arr=Array.isArray(js.programs)?js.programs:[];programsById={};programMetaByButton={};arr.forEach(function(p){if(!p||typeof p.program_id!=='number')return;programsById[p.program_id]=p;});buttons.forEach(function(btn){const id=parseInt(btn.dataset.id||'0',10);const p=programsById[id];if(!p){btn.disabled=true;btn.textContent='Programma '+id;return;}programMetaByButton[id]=p;btn.textContent=p.name||('Programma '+id);btn.dataset.price=String(p.price_units||0);btn.dataset.enabled=p.enabled?'1':'0';});render();}catch(e){console.warn('load programs failed',e);}}"
        "function dispatchHardwareCommand(type,payload){"
        "  const detail={type:type,payload:payload,timestamp:Date.now()};"
        "  window.dispatchEvent(new CustomEvent('emulator:hardware-command',{detail:detail}));"
        "  console.log('[EMULATOR_CMD]',detail);"
        "}"
        "function updateProgramAvailability(){"
        "  buttons.forEach(function(btn){const id=parseInt(btn.dataset.id||'0',10);const p=programMetaByButton[id];if(!p){btn.disabled=true;return;}const needed=(p.price_units||0);const enabled=!!p.enabled&&credit>=needed;btn.disabled=!enabled;});"
        "}"
        "function updateVirtualRelay(relayNumber,status,duration){"
        "  fetch('/api/emulator/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay_number:relayNumber,status:status,duration:duration||0})}).catch(function(e){console.warn('virtual relay update failed',e);});"
        "}"
        /* render: mostra credito in coin interi; il gauge tempo rimanente
         * viene aggiornato da updateElapsedUi con interpolazione a 200ms. */
        "function render(){creditEl.textContent=String(Math.max(0,credit));if(!(fsmState==='running'||fsmState==='paused')||!activeProgram){if(gaugeText)gaugeText.textContent='--';if(maskEl)maskEl.style.height='100%';}updateProgramAvailability();}"
        "buttons.forEach(function(btn){btn.addEventListener('click',async function(){const programId=parseInt(btn.dataset.id||'0',10);if(!programId)return;try{if(activeProgram===programId&&(fsmState==='running'||fsmState==='paused')){dispatchHardwareCommand('program_pause_toggle',{program:programId,current_credit:credit,state:fsmState});const rp=await fetch('/api/emulator/program/pause_toggle',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({program_id:programId})});const jp=await rp.json().catch(function(){return {};});if(!rp.ok){msgEl.textContent=(jp&&jp.error)?jp.error:('Errore pausa/ripresa: HTTP '+rp.status);return;}msgEl.textContent=(jp&&jp.state==='paused')?('Programma '+programId+' in pausa'):('Programma '+programId+' ripreso');await refreshQueueMessages();return;}dispatchHardwareCommand('program_start',{program:programId,current_credit:credit});const r=await fetch('/api/emulator/program/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({program_id:programId})});const js=await r.json().catch(function(){return {};});if(!r.ok){msgEl.textContent=(js&&js.error)?js.error:('Errore avvio programma: HTTP '+r.status);return;}buttons.forEach(function(b){b.classList.remove('active');});btn.classList.add('active');activeProgram=programId;msgEl.textContent='Programma '+programId+' in running';await refreshQueueMessages();}catch(e){msgEl.textContent='Errore programma: '+(e&&e.message?e.message:e);}});});"
        "coinButtons.forEach(function(btn){btn.addEventListener('click',async function(){const delta=parseInt(btn.dataset.coin||'0',10);dispatchHardwareCommand('coin_add',{value:delta,current_credit:credit});try{const r=await fetch('/api/emulator/coin',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({coin:delta})});if(!r.ok){msgEl.textContent='Errore invio coin: HTTP '+r.status;return;}await refreshQueueMessages();}catch(e){msgEl.textContent='Errore invio coin: '+(e&&e.message?e.message:e);}});});"
        "relays.forEach(function(relay){relay.addEventListener('click',async function(){const relayNumber=parseInt(relay.dataset.relay||'0',10);const nextState=!relay.classList.contains('on');updateVirtualRelay(relayNumber,nextState,0);msgEl.textContent='Comando Relay R'+relayNumber+' '+(nextState?'ON':'OFF');dispatchHardwareCommand('relay_toggle',{relay:relayNumber,status:nextState});await refreshQueueMessages();});});"
        "render();"
        "loadProgramsMeta();"
        "refreshQueueMessages();"
        "setInterval(refreshQueueMessages,700);"
        "setInterval(updateElapsedUi,200);"
        "})();"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

esp_err_t api_emulator_coin_event(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (!fsm_event_queue_init(0)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "FSM queue non disponibile", -1);
    }

    if (req->content_len <= 0 || req->content_len > 256) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char payload[256] = {0};
    int ret = httpd_req_recv(req, payload, req->content_len);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Errore lettura payload", -1);
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    cJSON *coin = cJSON_GetObjectItem(root, "coin");
    if (!cJSON_IsNumber(coin) || coin->valueint <= 0) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Campo coin obbligatorio e > 0", -1);
    }

    int value = coin->valueint;
    cJSON_Delete(root);

    {
        fsm_input_event_t ev = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PAYMENT_ACCEPTED,
            .type = FSM_INPUT_EVENT_COIN,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = value,
            .value_u32 = 0,
            .aux_u32 = 0,
            .text = {0},
        };
        strncpy(ev.text, "coin_from_emulator", sizeof(ev.text)-1);
        if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
            ESP_LOGW(TAG, "Queue FSM piena/non disponibile per coin=%d", value);
            httpd_resp_set_status(req, "503 Service Unavailable");
            return httpd_resp_send(req, "Coda FSM piena", -1);
        }
    }

    char response[96];
    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"coin\":%d}", value);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}

esp_err_t api_emulator_fsm_messages_get(httpd_req_t *req)
{
    if (req->method != HTTP_GET) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    (void)fsm_event_queue_init(0);

    char messages[16][FSM_EVENT_TEXT_MAX_LEN] = {{0}};
    size_t count = fsm_pending_messages_copy(messages, 16);
    fsm_ctx_t snapshot = {0};
    bool has_snapshot = fsm_runtime_snapshot(&snapshot);

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!root || !arr) {
        if (root) cJSON_Delete(root);
        if (arr) cJSON_Delete(arr);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    for (size_t i = 0; i < count; ++i) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(messages[i]));
    }
    cJSON_AddNumberToObject(root, "count", (double)count);
    cJSON_AddNumberToObject(root, "credit_cents", has_snapshot ? (double)snapshot.credit_cents : 0.0);
    cJSON_AddStringToObject(root, "state", has_snapshot ? fsm_state_to_string(snapshot.state) : "unknown");
    cJSON_AddNumberToObject(root, "running_elapsed_ms", has_snapshot ? (double)snapshot.running_elapsed_ms : 0.0);
    cJSON_AddNumberToObject(root, "pause_elapsed_ms", has_snapshot ? (double)snapshot.pause_elapsed_ms : 0.0);

    cJSON *relays = cJSON_CreateArray();
    if (!relays) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    for (uint8_t relay = 1; relay <= WEB_UI_VIRTUAL_RELAY_MAX; ++relay) {
        web_ui_virtual_relay_state_t state = {0};
        if (!web_ui_virtual_relay_get(relay, &state)) {
            continue;
        }
        cJSON *item = cJSON_CreateObject();
        if (!item) {
            continue;
        }
        cJSON_AddNumberToObject(item, "relay_number", relay);
        cJSON_AddBoolToObject(item, "status", state.status);
        cJSON_AddNumberToObject(item, "duration", (double)state.duration_ms);
        cJSON_AddItemToArray(relays, item);
    }
    cJSON_AddItemToObject(root, "relays", relays);

    cJSON_AddItemToObject(root, "messages", arr);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t send_ret = httpd_resp_send(req, json, -1);
    cJSON_free(json);
    return send_ret;
}

esp_err_t api_emulator_program_start(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (!fsm_event_queue_init(0)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "FSM queue non disponibile", -1);
    }

    if (req->content_len <= 0 || req->content_len > 256) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Payload non valido", -1);
    }

    char payload[256] = {0};
    int ret = httpd_req_recv(req, payload, req->content_len);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Errore lettura payload", -1);
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "JSON non valido", -1);
    }

    cJSON *program_id = cJSON_GetObjectItem(root, "program_id");
    if (!cJSON_IsNumber(program_id) || program_id->valueint <= 0 || program_id->valueint > 255) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_send(req, "Campo program_id obbligatorio e valido", -1);
    }

    uint8_t pid = (uint8_t)program_id->valueint;
    cJSON_Delete(root);

    const web_ui_program_entry_t *entry = find_program_by_id(pid);
    if (!entry) {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Programma non trovato\"}", -1);
    }

    if (!entry->enabled) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Programma disabilitato\"}", -1);
    }

    fsm_ctx_t snapshot = {0};
    bool has_snapshot = fsm_runtime_snapshot(&snapshot);
    if (!has_snapshot) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"FSM snapshot non disponibile\"}", -1);
    }

    if (snapshot.credit_cents < (int32_t)entry->price_units) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Credito insufficiente per il programma\"}", -1);
    }

    for (uint8_t relay = 1; relay <= WEB_UI_VIRTUAL_RELAY_MAX; ++relay) {
        bool on = ((entry->relay_mask & (1U << (relay - 1))) != 0);
        (void)web_ui_virtual_relay_control(relay, on, (uint32_t)entry->duration_sec * 1000U);
    }

    fsm_input_event_t event = {
        .from = AGN_ID_WEB_UI,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PROGRAM_SELECTED,

        .type = FSM_INPUT_EVENT_PROGRAM_SELECTED,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = (int32_t)entry->price_units,
        .value_u32 = (uint32_t)entry->pause_max_suspend_sec * 1000U,
        .aux_u32 = (uint32_t)entry->duration_sec * 1000U,
        .text = {0},
    };
    snprintf(event.text, sizeof(event.text), "%s", entry->name);

    if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
    }

    char response[256];
    snprintf(response, sizeof(response),
             "{\"status\":\"ok\",\"program_id\":%u,\"name\":\"%s\",\"price_units\":%u,\"duration_sec\":%u,\"pause_max_suspend_sec\":%u,\"relay_mask\":%u}",
             (unsigned)pid,
             entry->name,
             (unsigned)entry->price_units,
             (unsigned)entry->duration_sec,
             (unsigned)entry->pause_max_suspend_sec,
             (unsigned)entry->relay_mask);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}

esp_err_t api_emulator_program_stop(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (!fsm_event_queue_init(0)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "FSM queue non disponibile", -1);
    }

    uint8_t pid = 0;
    if (req->content_len > 0 && req->content_len <= 256) {
        char payload[256] = {0};
        int ret = httpd_req_recv(req, payload, req->content_len);
        if (ret > 0) {
            cJSON *root = cJSON_Parse(payload);
            if (root) {
                cJSON *program_id = cJSON_GetObjectItem(root, "program_id");
                if (cJSON_IsNumber(program_id) && program_id->valueint > 0 && program_id->valueint <= 255) {
                    pid = (uint8_t)program_id->valueint;
                }
                cJSON_Delete(root);
            }
        }
    }

    for (uint8_t relay = 1; relay <= WEB_UI_VIRTUAL_RELAY_MAX; ++relay) {
        (void)web_ui_virtual_relay_control(relay, false, 0);
    }

    fsm_input_event_t event = {
        .from = AGN_ID_WEB_UI,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PROGRAM_STOP,

        .type = FSM_INPUT_EVENT_PROGRAM_STOP,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = (uint32_t)pid,
        .aux_u32 = 0,
        .text = {0},
    };
    snprintf(event.text, sizeof(event.text), "program_stop");

    if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
    }

    char response[128];
    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"program_id\":%u}", (unsigned)pid);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}

esp_err_t api_emulator_program_pause_toggle(httpd_req_t *req)
{
    if (req->method != HTTP_POST) {
        httpd_resp_set_status(req, "405 Method Not Allowed");
        return httpd_resp_send(req, "Method not allowed", -1);
    }

    if (!fsm_event_queue_init(0)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "FSM queue non disponibile", -1);
    }

    uint8_t pid = 0;
    if (req->content_len > 0 && req->content_len <= 256) {
        char payload[256] = {0};
        int ret = httpd_req_recv(req, payload, req->content_len);
        if (ret > 0) {
            cJSON *root = cJSON_Parse(payload);
            if (root) {
                cJSON *program_id = cJSON_GetObjectItem(root, "program_id");
                if (cJSON_IsNumber(program_id) && program_id->valueint > 0 && program_id->valueint <= 255) {
                    pid = (uint8_t)program_id->valueint;
                }
                cJSON_Delete(root);
            }
        }
    }

    fsm_input_event_t event = {
        .from = AGN_ID_WEB_UI,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PROGRAM_PAUSE_TOGGLE,

        .type = FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = (uint32_t)pid,
        .aux_u32 = 0,
        .text = {0},
    };
    snprintf(event.text, sizeof(event.text), "program_pause_toggle");

    if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
    }

    fsm_ctx_t snapshot = {0};
    const char *state = "unknown";
    if (fsm_runtime_snapshot(&snapshot)) {
        state = fsm_state_to_string(snapshot.state);
    }

    char response[160];
    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"program_id\":%u,\"state\":\"%s\"}", (unsigned)pid, state);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}
