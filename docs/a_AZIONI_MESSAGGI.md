# 📋 Messaggi Eventi - Digital I/O e Programmi

## 🆕 Allineamento stato attuale (2026-03-26)

- I nomi dei programmi usati negli eventi non arrivano più da placeholder i18n, ma da `programs.json` (runtime: `/spiffs/programs.json`, seed: `data/programs.json`).
- Nel flusso OUT_OF_SERVICE, `ACTION_ID_SYSTEM_ERROR` e `ACTION_ID_SYSTEM_RUN` sono gestiti come eventi **diretti** verso `fsm_handle_input_event(...)` nel task FSM (non transitano da `fsm_event_publish/fsm_event_receive`).
- In uscita da OOS, il task FSM prova la riabilitazione runtime di gettoniera CCTALK e scanner USB CDC.

## 🔄 Messaggi generati quando premi la partenza di un programma dal touch

### Flusso Eventi Touch → Programma

#### 1. Pressione Touch (LVGL)
```c
// lvgl_page_programmi.c - funzione callback del pulsante
static void prog_btn_cb(lv_event_t *e)
{
    uint8_t pid = (uint8_t)(uintptr_t)lv_obj_get_user_data(e->target);
    esp_err_t err = tasks_publish_program_button_action(pid, AGN_ID_LVGL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[C] Azione programma pid=%u non pubblicata: %s", (unsigned)pid, tasks_err_to_name(err));
    } else {
        s_active_prog = pid;  // ✅ SUCCESSO: programma attivo localmente
    }
}
```

**Messaggio generato (in caso di successo):**
- **Nessun log** (solo update stato UI locale)

#### 2. Pubblicazione Evento (tasks.c)
```c
esp_err_t tasks_publish_program_button_action(uint8_t program_id, agn_id_t sender)
{
    // ...validazioni...
    fsm_input_event_t event = {
        .from = AGN_ID_LVGL,           // ✅ SOURCE: Touch panel
        .to = {AGN_ID_FSM},           // ✅ TARGET: FSM task
        .action = ACTION_ID_PROGRAM_SELECTED,
        .type = FSM_INPUT_EVENT_PROGRAM_SELECTED,
        .text = "NomeProgramma",      // ✅ Nome programma risolto dalla tabella programmi (programs.json)
        // ...altri dati...
    };
    
    if (!fsm_event_publish(&event, pdMS_TO_TICKS(20))) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
```

**Messaggi generati (con LOG_QUEUE abilitato):**
```
[FSM] ENQUEUE[0] type=program_selected action=0 from=3 to_mask=0x00000002
```

#### 3. Ricezione Evento (fsm_task)
```c
// tasks.c - loop principale del task FSM
if (fsm_event_receive(&event, AGN_ID_FSM, param->period_ticks)) {
    event_received = true;
    if (tasks_handle_digital_io_agent_event(&event)) {
        changed = false;
    } else {
        changed = fsm_handle_input_event(&fsm, &event);  // ✅ PROCESSING
    }
}
```

**Messaggi generati (con LOG_QUEUE abilitato):**
```
[FSM] DEQUEUE[0] type=program_selected action=0 from=3 receiver=2
```

#### 4. Processamento Logica FSM
```c
// fsm.c - gestione evento
case FSM_INPUT_EVENT_PROGRAM_SELECTED:
    if (ctx->state != FSM_STATE_CREDIT) {
        return false;  // ❌ ERRORE: stato non valido
    }
    
    if (ctx->credit_cents <= 0) {
        fsm_append_message("Credito a zero: selezione programma non consentita");
        return false;  // ❌ ERRORE: credito insufficiente
    }
    
    // ✅ SUCCESSO: setup programma
    ctx->running_price_units = event->value_i32;
    snprintf(ctx->running_program_name, sizeof(ctx->running_program_name), "%s", event->text);
    
    return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_SELECTED);
```

**Messaggi generati (in caso di errori):**
```
[FSM] Credito a zero: selezione programma non consentita
[FSM] Credito insufficiente per avvio programma
```

#### 5. Cambio Stato FSM
```c
// fsm.c - transizione stato
case FSM_STATE_CREDIT:
    if (event == FSM_EVENT_PROGRAM_SELECTED && ctx->credit_cents > 0) {
        ctx->state = FSM_STATE_RUNNING;           // ✅ RUNNING!
        ctx->program_running = true;
        ctx->running_elapsed_ms = 0;
        // ...setup timer...
    }
```

#### 6. Applicazione Output (tasks.c)
```c
// tasks.c - dopo transizione riuscita
if (event_received &&
    ((event.type == FSM_INPUT_EVENT_PROGRAM_SELECTED &&
      state_before == FSM_STATE_CREDIT &&
      fsm.state == FSM_STATE_RUNNING) ||
     (event.type == FSM_INPUT_EVENT_PROGRAM_SWITCH &&
      (state_before == FSM_STATE_RUNNING || state_before == FSM_STATE_PAUSED) &&
      fsm.state == FSM_STATE_RUNNING))) {
    tasks_apply_running_program_outputs(&fsm);  // ✅ OUTPUT RELAYS
}
```

## 📊 Riepilogo Messaggi per Partenza Programma

| Fase | Componente | Log Level | Messaggio |
|------|-------------|-----------|-----------|
| **Touch Press** | LVGL | - | (nessuno) |
| **Event Publish** | FSM | DEBUG¹ | `ENQUEUE[0] type=program_selected action=0 from=3 to_mask=0x00000002` |
| **Event Receive** | FSM Task | DEBUG¹ | `DEQUEUE[0] type=program_selected action=0 from=3 receiver=2` |
| **Credit Insufficient** | FSM | INFO | `Credito insufficiente per avvio programma` |
| **Zero Credit** | FSM | INFO | `Credito a zero: selezione programma non consentita` |
| **Publish Error** | Tasks | WARNING | `[C] Azione programma pid=X non pubblicata: ...` |

¹ *Solo se `LOG_QUEUE` è definito in `fsm.c`*

## 🎯 Messaggi Chiave da Monitorare

### In caso di successo:
- Nessun messaggio visibile (silent operation)
- Solo log interni di debug se abilitati

### In caso di errori:
- `Credito insufficiente per avvio programma`
- `Credito a zero: selezione programma non consentita`  
- `[C] Azione programma pid=X non pubblicata: ...`

**Il sistema è progettato per essere silenzioso in caso di funzionamento normale!**

---

## 📡 Altri Eventi Digital I/O

### Eventi OOS / ripristino runtime (gestione diretta FSM)

| Evento | Source | Target | Messaggio | Note percorso |
|--------|--------|--------|----------|---------------|
| **Ingresso OUT_OF_SERVICE** | FSM task (`tasks_fsm_task`) | FSM core | `[M] ... OUT_OF_SERVICE ...` | `ACTION_ID_SYSTEM_ERROR` iniettato direttamente in `fsm_handle_input_event(...)` |
| **Retry risolto OUT_OF_SERVICE** | FSM task (`tasks_fsm_task`) | FSM core | `[M] OUT_OF_SERVICE risolto ... ritorno in RUN` | `ACTION_ID_SYSTEM_RUN` iniettato direttamente in `fsm_handle_input_event(...)` |
| **Riabilitazione gettoniera** | FSM task (`tasks_fsm_task`) | CCTALK task | `[M] Gettoniera CCTALK riabilitata ...` | Dopo uscita OOS viene pubblicato `ACTION_ID_CCTALK_START` |
| **Riabilitazione scanner** | FSM task (`tasks_fsm_task`) | Scanner USB CDC | `[M] Scanner riabilitato ...` oppure warning | Dopo uscita OOS: `usb_cdc_scanner_send_setup_command()` + `usb_cdc_scanner_send_on_command()` |

### Eventi da Ingressi Digitali

| Evento | Source | Target | Messaggio | Condizioni |
|--------|--------|--------|----------|-----------|
| **Fronte Salita** | Digital I/O Task | FSM | `[M] IN%02u -> programma %u` | Ingresso mappato a programma |
| **Snapshot Error** | Digital I/O Task | - | `[M] Snapshot digital_io attesa config (non critico)` | Config non pronto |
| **Local IO Disabled** | Digital I/O Task | - | `[M] I/O locali disabilitati (non critico)` | I/O expander off |
| **Modbus Runtime Blocked** | Digital I/O Task | - | `[M] Modbus bloccato runtime (OOS/hard inhibit)` | Blocco operativo durante OOS/hard inhibit |

### Codici Errore Specifici Digital I/O

| Codice Errore | Valore | Descrizione | Log Level |
|---------------|--------|-------------|-----------|
| `DIGITAL_IO_ERR_CONFIG_NOT_READY` | 0x7601 | Device config non disponibile | DEBUG |
| `DIGITAL_IO_ERR_LOCAL_IO_DISABLED` | 0x7602 | I/O expander disabilitato | DEBUG |
| `DIGITAL_IO_ERR_MODBUS_DISABLED` | 0x7603 | Modbus non disponibile | DEBUG |
| `DIGITAL_IO_ERR_INVALID_CHANNEL` | 0x7604 | ID canale non valido | WARNING |

---

## 🔍 Debug Tips

### Per abilitare log dettagliati:
1. Definire `LOG_QUEUE` in `fsm.c` per vedere enqueue/dequeue
2. Impostare log level a `DEBUG` per vedere messaggi digital I/O
3. Monitorare `[M]` e `[FSM]` tags nei log

### Flusso completo touch → programma:
```
Touch → LVGL → tasks_publish_program_button_action() → fsm_event_publish() 
→ fsm_event_receive() → fsm_handle_input_event() → FSM_EVENT_PROGRAM_SELECTED 
→ cambio stato RUNNING → tasks_apply_running_program_outputs()
```

### Errori comuni:
- **Credito zero**: Controllare caricamento crediti
- **Config non pronto**: Verificare inizializzazione device_config  
- **I/O disabilitati**: Controllare configurazione sensors.io_expander_enabled
- **Publish timeout**: Verificare coda eventi FSM
