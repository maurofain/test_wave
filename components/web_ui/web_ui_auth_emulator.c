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


/**
 * @brief Carica la password di avvio dalla configurazione web.
 * 
 * @param [in/out] s_boot_password_loaded Indica se la password di avvio è già stata caricata.
 * @return void
 */
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


/**
 * @brief Imposta la password di avvio per l'interfaccia web.
 * 
 * @param [in] new_password Nuova password da impostare. Deve essere una stringa non vuota, con una lunghezza compresa tra 4 e 32 caratteri.
 * @return esp_err_t Errore generato dalla funzione.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_ERR_INVALID_ARG: Argomento non valido (password nulla o lunghezza non valida).
 *         - ESP_ERR_NO_MEM: Memoria insufficiente per memorizzare la nuova password.
 */
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


/**
 * @brief Controlla se la richiesta HTTP ha una password valida.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return true se la password è valida, false altrimenti.
 */
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


/**
 * @brief Invia una richiesta di password all'interfaccia utente web.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @param title Titolo della pagina di autenticazione.
 * @param target URL o risorsa a cui l'utente deve essere reindirizzato dopo l'autenticazione.
 * @return esp_err_t Codice di errore.
 */
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


/**
 * @brief Gestisce la richiesta per la pagina dell'emulatore.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * 
 * @note Questa funzione verifica se la password della web UI è valida prima di procedere.
 */
esp_err_t emulator_page_handler_local(httpd_req_t *req)
{
    if (!web_ui_has_valid_password(req)) {
        return web_ui_send_password_required(req, "Emulator", "/emulator");
    }

    const char *extra_style =
        ".emu-wrap{width:100%;max-width:2048px;margin:16px auto;padding:0 8px;box-sizing:border-box;display:flex;gap:12px;align-items:stretch}"
        ".emu-panel{flex:0 0 720px;min-width:720px;height:1280px;background:#111;border-radius:16px;padding:14px;box-shadow:0 4px 20px rgba(0,0,0,0.25);box-sizing:border-box}"
        ".emu-layout{height:100%;display:grid;grid-template-columns:150px 1fr 64px 150px;column-gap:10px;align-items:stretch}"
        ".emu-left,.emu-right{min-width:0;display:grid;grid-template-rows:repeat(5,1fr);gap:10px}"
        ".prog-btn{width:75%;justify-self:center;border:none;border-radius:10px;background:#6c3483 !important;color:#fff;font-size:30px;font-weight:bold;cursor:pointer;padding:6px 4px;line-height:1.1}"
        ".prog-btn.active{background:#1e8b45 !important}"
        ".prog-btn:disabled{opacity:.45;cursor:not-allowed;pointer-events:none}"
        ".emu-main{min-width:0;display:block}"
        ".credit-box{position:relative;background:#202048;border-radius:8px;padding:20px;height:100%;display:flex;align-items:center;justify-content:center;flex-direction:column;transition:background .2s,color .2s}"
        ".credit-box.state-credit{background:#202048;color:#fff}"
        ".credit-box.state-running{background:#1e6b35;color:#fff}"
        ".credit-box.state-paused{background:#7a4200;color:#fff}"
        ".datetime{position:absolute;top:10px;right:12px;color:#888899;font-size:32px;font-weight:700;line-height:1.1;text-align:right;white-space:pre-line;font-family:'Courier New','DejaVu Sans Mono',monospace}"
        ".credit-label{font-size:32px;color:#888899}"
        ".credit-value{font-size:165px;font-weight:bold;line-height:1;color:#eee}"
        ".secs-value{font-size:165px;font-weight:bold;line-height:1;color:#eee;display:none}"
        ".credit-value-small{font-size:52px;font-weight:bold;line-height:1.2;color:#eee;margin-top:6px;display:block}"
        ".elapsed-value{margin-top:10px;font-size:32px;font-weight:600;color:#888899}"
        ".pause-value{margin-top:4px;font-size:32px;font-weight:500;color:#e67e22}"
        ".emu-gauge{min-width:0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px}"
        ".gauge-track{position:relative;width:100%;height:100%;border-radius:8px;overflow:hidden;border:3px solid #d9d9d9;background:linear-gradient(to top,#c0392b 0%,#c0392b 30%,#27ae60 30%,#27ae60 100%)}"
        ".gauge-mask{position:absolute;left:0;top:0;width:100%;height:100%;background:#111;transition:height .25s linear}"
        ".gauge-text{color:#fff;font-size:28px;font-weight:bold}"
        ".emu-electrical{flex:1;min-width:280px;background:#ffffff;border-radius:16px;padding:12px;box-shadow:0 4px 16px rgba(0,0,0,0.12);box-sizing:border-box}"
        ".side-title{margin:0 0 10px 0;color:#2c3e50;font-size:22px}"
        ".coin-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:14px}"
        ".coin-grid-2{display:grid;grid-template-columns:repeat(2,1fr);gap:8px;margin-bottom:14px}"
        ".coin-btn{border:none;border-radius:8px;background:#27ae60;color:#fff;padding:12px 8px;font-size:18px;font-weight:bold;cursor:pointer}"
        ".coin-btn:hover{background:#219150}"
        ".coin-btn.coin-btn-card{background:#2980b9}"
        ".coin-btn.coin-btn-card:hover{background:#1f6fa0}"
        ".coin-btn.coin-btn-cash{background:#e67e22}"
        ".coin-btn.coin-btn-cash:hover{background:#c96a12}"
        ".relay-grid{display:grid;grid-template-columns:repeat(5,1fr);gap:8px}"
        ".relay{border-radius:8px;background:#d5d8dc;color:#2c3e50;padding:10px 4px;text-align:center;font-weight:bold;font-size:14px}"
        ".relay.on{background:#f39c12;color:#fff}"
        ".msg-box{margin-top:12px;background:#f5f7fa;border-radius:10px;padding:12px;font-size:16px;color:#2c3e50;display:block;overflow:auto;line-height:1.35;max-height:220px}"
        ".side-note{margin-top:12px;font-size:13px;color:#566573;line-height:1.4}"
        "@media(max-width:1250px){.emu-wrap{flex-direction:column;align-items:center}.emu-panel{flex:none;width:720px;min-width:720px;height:1280px}.emu-electrical{width:720px;flex:none}.msg-box{max-height:260px}}";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    send_head(req, "Emulator", extra_style, false);

    const char *body =
        "<div class='emu-wrap'>"
        "<div class='emu-panel'><div class='emu-layout'>"
        "<div class='emu-left'>"
        "<button class='prog-btn' data-id='1'>1</button>"
        "<button class='prog-btn' data-id='2'>2</button>"
        "<button class='prog-btn' data-id='3'>3</button>"
        "<button class='prog-btn' data-id='4'>4</button>"
        "<button class='prog-btn' data-id='5'>5</button>"
        "</div>"
        "<div class='emu-main'>"
        "<div id='creditBox' class='credit-box'><div id='dateTime' class='datetime'>--/--/----\\n--:--:--</div><div id='creditLabel' class='credit-label'>Credito</div><div id='secsDisplay' class='secs-value'>--</div><div id='credit' class='credit-value'>0</div><div id='elapsed' class='elapsed-value'>Secondi   00:00</div><div id='pauseElapsed' class='pause-value'>Pausa: 00:00</div></div>"
        "</div>"
        "<div class='emu-gauge'>"
        "<div class='gauge-track'><div id='gmask' class='gauge-mask'></div></div>"
        "<div id='gtext' class='gauge-text'>--</div>"
        "</div>"
        "<div class='emu-right'>"
        "<button class='prog-btn' data-id='6'>6</button>"
        "<button class='prog-btn' data-id='7'>7</button>"
        "<button class='prog-btn' data-id='8'>8</button>"
        "<button class='prog-btn' data-id='9'>9</button>"
        "<button class='prog-btn' data-id='10'>10</button>"
        "</div>"
        "</div></div>"
        "<div class='emu-electrical'>"
        "<h2 class='side-title'>Quadro elettrico</h2>"
        "<h3 class='side-title' style='font-size:18px;margin-top:4px;'>&#128247; Crediti QR</h3>"
        "<div class='coin-grid'>"
        "<button class='coin-btn' data-coin='1' data-source='qr'>+1</button>"
        "<button class='coin-btn' data-coin='5' data-source='qr'>+5</button>"
        "<button class='coin-btn' data-coin='10' data-source='qr'>+10</button>"
        "</div>"
        "<h3 class='side-title' style='font-size:18px;margin-top:4px;'>&#128190; Crediti Tessera <label style='font-size:14px;margin-left:8px;'><input type=\"checkbox\" id=\"cardSwitch\"> Tessera ON</label></h3>"
        "<div class='coin-grid'>"
        "<button class='coin-btn coin-btn-card' data-coin='1' data-source='card'>+1</button>"
        "<button class='coin-btn coin-btn-card' data-coin='5' data-source='card'>+5</button>"
        "<button class='coin-btn coin-btn-card' data-coin='10' data-source='card'>+10</button>"
        "</div>"
        "<h3 class='side-title' style='font-size:18px;margin-top:4px;'>&#129689; Monete</h3>"
        "<div class='coin-grid-2'>"
        "<button class='coin-btn coin-btn-cash' data-coin='1' data-source='cash'>1 coin</button>"
        "<button class='coin-btn coin-btn-cash' data-coin='2' data-source='cash'>2 coin</button>"
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
        "<div id='msg' class='msg-box'>Nessun evento in coda</div>"
        "<div class='side-note'>Layout operativo: pannello utente 720x1280 a sinistra (1-5 | counter | barra | 6-10), quadro elettrico a destra con ricarica credito e stato relay.</div>"
        "</div>"
        "</div>"
        "<script>"
        "(function(){"
        "const buttons=[...document.querySelectorAll('.prog-btn')];"
        "const coinButtons=[...document.querySelectorAll('.coin-btn')];"
        "const relays=[...document.querySelectorAll('.relay')];"
        "const creditBox=document.getElementById('creditBox');"
        "const creditEl=document.getElementById('credit');"
        "const creditLabel=document.getElementById('creditLabel');"
        "const dateTimeEl=document.getElementById('dateTime');"
        "const secsDisplay=document.getElementById('secsDisplay');"
        "const elapsedEl=document.getElementById('elapsed');"
        "const pauseElapsedEl=document.getElementById('pauseElapsed');"
        "const msgEl=document.getElementById('msg');"
        "const maskEl=document.getElementById('gmask');"
        "const gaugeText=document.getElementById('gtext');"
        "let credit=0,ecd=0,vcd=0;" "let cardSwitch=null;" "cardSwitch=document.getElementById('cardSwitch'); if(cardSwitch){cardSwitch.addEventListener('change',function(){if(msgEl)msgEl.textContent='Tessera '+(this.checked?'ON':'OFF');});}"
        "let activeProgram=0;"
        "let fsmState='unknown';"
        "let runningElapsedMs=0;"
        "let runningElapsedSyncAt=0;"
        "let pauseElapsedMs=0;"
        "let pauseElapsedSyncAt=0;"
        "let programsById={};"
        "let programMetaByButton={};"
        "let pendingStart=false;"
        "let runningTargetMs=0;"
        "function pad2(n){return String(n).padStart(2,'0');}"
        "function updateDateTimeUi(){if(!dateTimeEl)return;const d=new Date();dateTimeEl.textContent=pad2(d.getDate())+'/'+pad2(d.getMonth()+1)+'/'+d.getFullYear()+'\\n'+pad2(d.getHours())+':'+pad2(d.getMinutes())+':'+pad2(d.getSeconds());}"
        "function escapeHtml(s){return String(s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}"
        "function renderQueueMessages(messages){if(!Array.isArray(messages)||messages.length===0){msgEl.textContent='Nessun evento in coda';return;}msgEl.innerHTML=messages.map(function(m){return '<div>• '+escapeHtml(m)+'</div>';}).join('');}"
        "function applyRelayIndicators(relayItems){if(!Array.isArray(relayItems))return;const byRelay={};relayItems.forEach(function(item){if(item&&typeof item.relay_number==='number'){byRelay[item.relay_number]=!!item.status;}});relays.forEach(function(relay){const relayNumber=parseInt(relay.dataset.relay||'0',10);if(!relayNumber)return;if(Object.prototype.hasOwnProperty.call(byRelay,relayNumber)){relay.classList.toggle('on',!!byRelay[relayNumber]);}});}"
        "function formatElapsed(ms){const t=Math.max(0,Math.floor(ms/1000));const mm=String(Math.floor(t/60)).padStart(2,'0');const ss=String(t%60).padStart(2,'0');return mm+':'+ss;}"
        "function updateCreditStateUi(){if(!creditBox)return;creditBox.classList.remove('state-credit','state-running','state-paused');if(fsmState==='credit')creditBox.classList.add('state-credit');if(fsmState==='running')creditBox.classList.add('state-running');if(fsmState==='paused')creditBox.classList.add('state-paused');}"
        /* updateElapsedUi: aggiorna timer E gauge con formula tick.
         * total_ticks = price_units*60; rdu = total_ticks/dur (tick/s).
         * Gauge% = (total_ticks - rdu*elapsed) / total_ticks = (dur-elapsed)/dur.
         * Testo gauge e secsDisplay mostrano i secondi rimanenti. */
        "function updateElapsedUi(){if(!elapsedEl)return;let shown=runningElapsedMs;if(fsmState==='running'&&runningElapsedSyncAt){shown+=(Date.now()-runningElapsedSyncAt);}elapsedEl.textContent='Secondi   '+formatElapsed(shown);if(pauseElapsedEl){let pauseShown=pauseElapsedMs;if(fsmState==='paused'&&pauseElapsedSyncAt){pauseShown+=(Date.now()-pauseElapsedSyncAt);}pauseElapsedEl.textContent='Pausa: '+formatElapsed(pauseShown);}if((fsmState==='running'||fsmState==='paused')&&runningTargetMs>0){const remMs=Math.max(0,runningTargetMs-shown);const remSec=Math.round(remMs/1000);const pct=Math.max(0,Math.min(100,(remMs/runningTargetMs)*100));if(secsDisplay)secsDisplay.textContent=remSec+'s';if(gaugeText)gaugeText.textContent=remSec+'s';if(maskEl)maskEl.style.height=(100-pct)+'%';}}"
        "function syncFromFsm(js){if(js&&typeof js.credit_cents==='number'){credit=Math.max(0,Math.floor(js.credit_cents));}if(js&&typeof js.ecd_coins==='number'){ecd=Math.max(0,Math.floor(js.ecd_coins));}if(js&&typeof js.vcd_coins==='number'){vcd=Math.max(0,Math.floor(js.vcd_coins));}if(cardSwitch&&vcd===0){cardSwitch.checked=false;}if(js&&typeof js.state==='string'){fsmState=js.state;}if(fsmState==='running'||fsmState==='paused'){pendingStart=false;if(!activeProgram&&js&&js.running_price_units>0){activeProgram=-1;}}if(js&&typeof js.running_elapsed_ms==='number'){runningElapsedMs=Math.max(0,Math.floor(js.running_elapsed_ms));runningElapsedSyncAt=Date.now();}if(js&&typeof js.running_target_ms==='number'&&js.running_target_ms>0){runningTargetMs=Math.max(0,Math.floor(js.running_target_ms));}if(js&&typeof js.pause_elapsed_ms==='number'){pauseElapsedMs=Math.max(0,Math.floor(js.pause_elapsed_ms));pauseElapsedSyncAt=Date.now();}if(!pendingStart&&fsmState!=='running'&&fsmState!=='paused'){activeProgram=0;runningTargetMs=0;buttons.forEach(function(b){b.classList.remove('active');});runningElapsedSyncAt=0;pauseElapsedSyncAt=0;}if(js&&Array.isArray(js.relays)){applyRelayIndicators(js.relays);}updateCreditStateUi();updateElapsedUi();render();renderQueueMessages(js&&js.messages?js.messages:[]);}"
        "async function refreshQueueMessages(){try{const r=await fetch('/api/emulator/fsm/messages');if(!r.ok)return;const js=await r.json();syncFromFsm(js);}catch(e){console.warn('fsm messages fetch failed',e);}}"
        "async function loadProgramsMeta(){try{const r=await fetch('/api/programs');if(!r.ok)return;const js=await r.json();const arr=Array.isArray(js.programs)?js.programs:[];programsById={};programMetaByButton={};arr.forEach(function(p){if(!p||typeof p.program_id!=='number')return;programsById[p.program_id]=p;});buttons.forEach(function(btn){const id=parseInt(btn.dataset.id||'0',10);const p=programsById[id];btn.textContent=String(id);if(!p){btn.disabled=true;return;}programMetaByButton[id]=p;btn.dataset.price=String(p.price_units||0);btn.dataset.enabled=p.enabled?'1':'0';});render();}catch(e){console.warn('load programs failed',e);}}"
        "function dispatchHardwareCommand(type,payload){"
        "  const detail={type:type,payload:payload,timestamp:Date.now()};"
        "  window.dispatchEvent(new CustomEvent('emulator:hardware-command',{detail:detail}));"
        "  console.log('[EMULATOR_CMD]',detail);"
        "}"
        "function paintProgramButton(btn,isActive,isEnabled){btn.style.background=isActive?'#e74c3c':'#2c3e50';btn.style.opacity=isEnabled?'1':'0.45';btn.style.cursor=isEnabled?'pointer':'not-allowed';btn.style.pointerEvents=isEnabled?'auto':'none';}"
        "function updateProgramAvailability(){"
        "  if(credit<=0){activeProgram=0;}const canHighlight=(fsmState==='running'||fsmState==='paused')&&activeProgram>0&&credit>0;buttons.forEach(function(btn){const id=parseInt(btn.dataset.id||'0',10);const p=programMetaByButton[id];if(!p){btn.disabled=true;btn.classList.remove('active');paintProgramButton(btn,false,false);return;}const needed=(p.price_units||0);const enabled=!!p.enabled&&credit>0&&credit>=needed;btn.disabled=!enabled;if(!enabled){btn.classList.remove('active');paintProgramButton(btn,false,false);return;}if(canHighlight&&id===activeProgram){btn.classList.add('active');paintProgramButton(btn,true,true);}else{btn.classList.remove('active');paintProgramButton(btn,false,true);}});"
        "}"
        "function updateVirtualRelay(relayNumber,status,duration){"
        "  fetch('/api/emulator/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({relay_number:relayNumber,status:status,duration:duration||0})}).catch(function(e){console.warn('virtual relay update failed',e);});"
        "}"
        /* render: visualizzazione credit box secondo spec TODO#5:
         * - fase credit: credito in coin a carattere grande (100%);
         * - running/paused: secondi disponibili grandi (100%), credito
         *   piccolo (30%) se > 0 oppure nascosto se == 0;
         * - gauge: aggiornato da updateElapsedUi (interpolato a 200ms). */
        "function render(){const inProgram=(fsmState==='running'||fsmState==='paused')&&activeProgram;if(inProgram){if(creditLabel)creditLabel.textContent='Secondi';if(secsDisplay)secsDisplay.style.display='block';creditEl.className=credit>0?'credit-value credit-value-small':'credit-value';creditEl.style.display=credit>0?'block':'none';creditEl.textContent=credit>0?String(credit):'';}else{if(creditLabel){creditLabel.textContent='Credito';}if(secsDisplay){secsDisplay.style.display='none';secsDisplay.textContent='--';}creditEl.className='credit-value';creditEl.style.display='block';creditEl.textContent=String(Math.max(0,credit));if(gaugeText)gaugeText.textContent='--';if(maskEl)maskEl.style.height='100%';}updateProgramAvailability();}"
        "buttons.forEach(function(btn){btn.addEventListener('click',async function(){const programId=parseInt(btn.dataset.id||'0',10);if(!programId||btn.disabled)return;if(credit<=0){activeProgram=0;buttons.forEach(function(b){b.classList.remove('active');paintProgramButton(b,false,false);});msgEl.textContent='Credito insufficiente';return;}try{if(activeProgram===programId&&(fsmState==='running'||fsmState==='paused')){dispatchHardwareCommand('program_pause_toggle',{program:programId,current_credit:credit,state:fsmState});const rp=await fetch('/api/emulator/program/pause_toggle',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({program_id:programId})});const jp=await rp.json().catch(function(){return {};});if(!rp.ok){msgEl.textContent=(jp&&jp.error)?jp.error:('Errore pausa/ripresa: HTTP '+rp.status);return;}const wasPaused=(fsmState==='paused');fsmState=wasPaused?'running':'paused';if(!wasPaused){pauseElapsedMs=0;pauseElapsedSyncAt=0;}updateCreditStateUi();render();msgEl.textContent=(fsmState==='paused')?('Programma '+programId+' in pausa'):('Programma '+programId+' ripreso');await new Promise(function(r){setTimeout(r,250);});await refreshQueueMessages();return;}dispatchHardwareCommand('program_start',{program:programId,current_credit:credit});const r=await fetch('/api/emulator/program/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({program_id:programId})});const js=await r.json().catch(function(){return {};});if(!r.ok){msgEl.textContent=(js&&js.error)?js.error:('Errore avvio programma: HTTP '+r.status);return;}buttons.forEach(function(b){b.classList.remove('active');paintProgramButton(b,false,true);});btn.classList.add('active');paintProgramButton(btn,true,true);const newP=programMetaByButton[programId];const wasRunning=(fsmState==='running'||fsmState==='paused');if(wasRunning){const shownNow=runningElapsedMs+(runningElapsedSyncAt?Date.now()-runningElapsedSyncAt:0);const oldP=programMetaByButton[activeProgram];const oldPrice=(oldP&&oldP.price_units>0)?oldP.price_units:1;const newPrice=(newP&&newP.price_units>0)?newP.price_units:1;const newDurMs=newP&&newP.duration_sec?newP.duration_sec*1000:0;const remOld=Math.max(0,runningTargetMs-shownNow);const remNew=(runningTargetMs>0&&newDurMs>0)?Math.round(remOld*newDurMs*oldPrice/(runningTargetMs*newPrice)):newDurMs;runningTargetMs=shownNow+remNew;}else{runningElapsedMs=0;runningElapsedSyncAt=Date.now();runningTargetMs=newP&&newP.duration_sec?newP.duration_sec*1000:0;}activeProgram=programId;fsmState='running';pendingStart=true;updateCreditStateUi();render();msgEl.textContent='Programma '+programId+' in running';await new Promise(function(r){setTimeout(r,250);});await refreshQueueMessages();}catch(e){msgEl.textContent='Errore programma: '+(e&&e.message?e.message:e);}});});"
        "coinButtons.forEach(function(btn){btn.addEventListener('click',async function(){const delta=parseInt(btn.dataset.coin||'0',10);const src=btn.dataset.source||'qr';const srcLabel={'qr':'QR','card':'Tessera','cash':'Monete'}[src]||src;dispatchHardwareCommand('coin_add',{value:delta,source:src,current_credit:credit});if(src==='card'&&cardSwitch){cardSwitch.checked=true;msgEl.textContent='Tessera ON';}try{const r=await fetch('/api/emulator/coin',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({coin:delta,source:src})});if(!r.ok){msgEl.textContent='Errore invio coin: HTTP '+r.status;return;}msgEl.textContent=srcLabel+' +'+delta+' coin';await refreshQueueMessages();}catch(e){msgEl.textContent='Errore invio coin: '+(e&&e.message?e.message:e);}});});"
        "relays.forEach(function(relay){relay.addEventListener('click',async function(){const relayNumber=parseInt(relay.dataset.relay||'0',10);const nextState=!relay.classList.contains('on');updateVirtualRelay(relayNumber,nextState,0);msgEl.textContent='Comando Relay R'+relayNumber+' '+(nextState?'ON':'OFF');dispatchHardwareCommand('relay_toggle',{relay:relayNumber,status:nextState});await refreshQueueMessages();});});"
        "render();"
        "updateDateTimeUi();"
        "loadProgramsMeta();"
        "refreshQueueMessages();"
        "setInterval(refreshQueueMessages,700);"
        "setInterval(updateElapsedUi,200);"
        "setInterval(updateProgramAvailability,150);"
        "setInterval(updateDateTimeUi,1000);"
        "})();"
        "</script></body></html>";

    httpd_resp_sendstr_chunk(req, body);
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}


/**
 * @brief Gestisce gli eventi coin dell'emulatore.
 * 
 * Questa funzione gestisce gli eventi coin dell'emulatore. 
 * Se il metodo della richiesta non è HTTP_POST, la funzione restituisce ESP_FAIL.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * @retval ESP_OK Se il metodo della richiesta è HTTP_POST.
 * @retval ESP_FAIL Se il metodo della richiesta non è HTTP_POST.
 */
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

    /* campo source opzionale: 'qr', 'card', 'cash' */
    char source[16] = "qr";
    cJSON *jsource = cJSON_GetObjectItem(root, "source");
    if (cJSON_IsString(jsource) && jsource->valuestring) {
        strncpy(source, jsource->valuestring, sizeof(source)-1);
        source[sizeof(source)-1] = '\0';
    }
    cJSON_Delete(root);

    {
        /* testo evento: '<source>_coin_emulator' per tracciabilità */
        char ev_text[FSM_EVENT_TEXT_MAX_LEN];
        snprintf(ev_text, sizeof(ev_text), "%s_coin_emulator", source);

        /* tipo evento in base alla sorgente del pagamento */
        fsm_input_event_type_t ev_type =
            (strcmp(source, "card") == 0) ? FSM_INPUT_EVENT_CARD_CREDIT :
            (strcmp(source, "qr")   == 0) ? FSM_INPUT_EVENT_QR_CREDIT   :
                                             FSM_INPUT_EVENT_TOKEN;

        fsm_input_event_t ev = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PAYMENT_ACCEPTED,
            .type = ev_type,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = value,
            .value_u32 = 0,
            .aux_u32 = 0,
            .text = {0},
        };
        strncpy(ev.text, ev_text, sizeof(ev.text)-1);
        if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
            ESP_LOGW(TAG, "Queue FSM piena/non disponibile per coin=%d", value);
            httpd_resp_set_status(req, "503 Service Unavailable");
            return httpd_resp_send(req, "Coda FSM piena", -1);
        }
    }

    char response[128];
    snprintf(response, sizeof(response), "{\"status\":\"ok\",\"coin\":%d,\"source\":\"%s\"}", value, source);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, response, -1);
}


/**
 * @brief Ottiene i messaggi dell'automa Finite State Machine (FSM) tramite una richiesta HTTP GET.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * @note Questa funzione gestisce solo le richieste HTTP GET. Qualsiasi altro metodo restituirà un errore.
 */
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
    cJSON_AddNumberToObject(root, "ecd_coins", has_snapshot ? (double)snapshot.ecd_coins : 0.0);
    cJSON_AddNumberToObject(root, "vcd_coins", has_snapshot ? (double)snapshot.vcd_coins : 0.0);
    cJSON_AddStringToObject(root, "state", has_snapshot ? fsm_state_to_string(snapshot.state) : "unknown");
    cJSON_AddNumberToObject(root, "running_elapsed_ms", has_snapshot ? (double)snapshot.running_elapsed_ms : 0.0);
    cJSON_AddNumberToObject(root, "running_target_ms",  has_snapshot ? (double)snapshot.running_target_ms  : 0.0);
    cJSON_AddNumberToObject(root, "running_price_units", has_snapshot ? (double)snapshot.running_price_units : 0.0);
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


/**
 * Avvia il programma emulatore.
 *
 * @param req Puntatore alla richiesta HTTP.
 * @return Errore generato dalla funzione.
 * @note La funzione controlla se il metodo della richiesta è HTTP_POST.
 */
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

    if (snapshot.state == FSM_STATE_RUNNING || snapshot.state == FSM_STATE_PAUSED) {
        /* cambio programma a macchina accesa: scala il tempo residuo, non fermare */
        fsm_input_event_t sw_ev = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PROGRAM_SELECTED,
            .type = FSM_INPUT_EVENT_PROGRAM_SWITCH,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = (int32_t)entry->price_units,
            .value_u32 = (uint32_t)entry->pause_max_suspend_sec * 1000U,
            .aux_u32   = (uint32_t)entry->duration_sec * 1000U,
            .text = {0},
        };
        snprintf(sw_ev.text, sizeof(sw_ev.text), "%s", entry->name);
        if (!fsm_event_publish(&sw_ev, pdMS_TO_TICKS(20))) {
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
        }
    } else {
        fsm_input_event_t event = {
            .from = AGN_ID_WEB_UI,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PROGRAM_SELECTED,
            .type = FSM_INPUT_EVENT_PROGRAM_SELECTED,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = (int32_t)entry->price_units,
            .value_u32 = (uint32_t)entry->pause_max_suspend_sec * 1000U,
            .aux_u32   = (uint32_t)entry->duration_sec * 1000U,
            .text = {0},
        };
        snprintf(event.text, sizeof(event.text), "%s", entry->name);
        if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
            httpd_resp_set_status(req, "503 Service Unavailable");
            httpd_resp_set_type(req, "application/json");
            return httpd_resp_send(req, "{\"error\":\"Coda FSM piena\"}", -1);
        }
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


/**
 * @brief Arresta il programma emulatore.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * @retval ESP_OK Operazione riuscita.
 * @retval ESP_FAIL Operazione non riuscita.
 */
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


/**
 * @brief Gestisce la pausa e ripresa dell'emulatore.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 * 
 * @note Questa funzione gestisce la pausa e ripresa dell'emulatore in risposta a una richiesta HTTP POST.
 * Se il metodo della richiesta non è HTTP_POST, la funzione restituisce ESP_FAIL.
 */
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
