# Rapporto — Punti di attivazione/disattivazione sistemi di pagamento

**Data:** 2026-05-06  
**File analizzato:** `main/tasks.c`  
**Sistemi coperti:** Scanner QR (USB CDC), CCTalk, MDB Cashless

---

## Legenda stati FSM

| Stato FSM            | Descrizione                                        |
|---------------------|----------------------------------------------------|
| `CREDIT`            | Credito inserito, nessun programma avviato         |
| `RUNNING`           | Programma in esecuzione                            |
| `PAUSED`            | Programma in pausa                                 |
| `ADS`               | Schermata pubblicitaria (idle con ads attivi)      |
| `IDLE`              | Stato inattivo                                     |
| `OUT_OF_SERVICE`    | Fuori servizio                                     |

---

## 1. Scanner QR (USB CDC / Newland)

### 1.1 Flag di controllo
| Flag                         | Tipo   | Significato                                          |
|-----------------------------|--------|------------------------------------------------------|
| `scanner_forced_off_for_program` | `bool` | Spento dalla FSM per programma attivo             |
| `s_scanner_cooldown_active`  | `bool` | In cooldown post-lettura (attivo dopo ogni scan)    |
| `s_scanner_reenable_pending` | `bool` | In attesa di riabilitazione al termine del cooldown |

### 1.2 Punti di controllo

#### OFF — Spegnimento scanner

| Riga | Contesto / Trigger | Note |
|------|--------------------|------|
| 2454 | `fsm_task()` — loop principale FSM: `active_program_session && !scanner_forced_off_for_program` | Disattivato all'avvio di ogni ciclo (`RUNNING`/`PAUSED`). Imposta `scanner_forced_off_for_program = true`. |
| 2623 | `fsm_task()` — transizione verso `OUT_OF_SERVICE` (`state_before != fsm.state`) | Disattivato immediatamente all'ingresso in OOS. Condizione: `cfg->scanner.enabled`. |
| 2878 | `scanner_cooldown_tick()` — se `tasks_is_out_of_service_state()` | Forza OFF durante OOS anche dal task cooldown. Azzera i flag di reenable. |
| 3090 | `scanner_on_barcode_cb()` — dopo ogni lettura barcode valida | Spento per la durata del cooldown (`scanner_cooldown_active_ms`). |

#### ON — Accensione scanner

| Riga | Contesto / Trigger | Note |
|------|--------------------|------|
| 2463–2464 | `fsm_task()` — loop principale: `!active_program_session && scanner_forced_off_for_program` + stato CREDIT/ADS/IDLE | Riacceso al rientro in stato non-attivo. Setup + on. |
| 2562–2563 | `fsm_task()` — blocco `left_program_state` (transizione uscita RUNNING/PAUSED) | Riacceso al termine del programma (handler immediato). |
| 2310–2311 | `fsm_task()` — uscita da `OUT_OF_SERVICE` con recovery completato | Setup + on dopo uscita OOS. Condizione: `cfg->scanner.enabled`. |
| 2894 | `scanner_cooldown_tick()` — termine cooldown (`cooldown_until` superato) | Riacceso via `send_on_command()` con retry. |
| 2911–2913 | `scanner_cooldown_tick()` — fallback dopo 5 tentativi falliti | Esegue `setup + on` come fallback. |

### 1.3 Cooldown post-lettura

Funziona **indipendentemente** dalla FSM, tramite `scanner_cooldown_task()` (periodo 100 ms):
1. Dopo ogni barcode valido: scanner spento, `s_scanner_cooldown_until` impostato.
2. `scanner_cooldown_tick()` verifica ad ogni tick se il tempo è scaduto.
3. Al termine: `send_on_command()` con retry ogni 500 ms, fallback `setup+on` a 5 tentativi.
4. Se OOS diventa attivo durante il cooldown: cooldown azzerato, scanner resta spento.

### 1.4 Condizioni di abilitazione complessive (AND logico)

```
scanner attivo ⟺  cfg->scanner.enabled
               AND NOT out_of_service
               AND NOT active_program_session
               AND NOT scanner_cooldown_active
```

---

## 2. CCTalk (gettoniera)

### 2.1 Flag di controllo
| Flag                          | Tipo   | Significato                                           |
|------------------------------|--------|-------------------------------------------------------|
| `cctalk_forced_stop_for_vcd`     | `bool` | Fermata per sessione VCD bloccata                  |
| `cctalk_forced_stop_for_program` | `bool` | Fermata per programma attivo (solo VCD bloccata)   |

**Nota importante:** la gettoniera CCTalk rimane **attiva durante RUNNING/PAUSED** in condizioni normali.
Viene fermata **solo** durante una sessione VCD con `session_mode == FSM_SESSION_MODE_VIRTUAL_LOCKED`
e `allow_additional_payments == false`.

### 2.2 Punti di controllo

#### STOP — Fermata CCTalk

| Riga | Contesto / Trigger | Note |
|------|--------------------|------|
| 2426 | `fsm_task()` — loop principale: `vcd_locked_session == true` AND nessuno stop già attivo | Pubblicato `ACTION_ID_CCTALK_STOP`. Flag impostati in base a `active_program_session`. |
| 2623 | `fsm_task()` — transizione verso `OUT_OF_SERVICE` | Pubblicato `ACTION_ID_CCTALK_STOP` immediatamente. Condizione: `cfg->sensors.cctalk_enabled`. |

#### START — Riavvio CCTalk

| Riga | Contesto / Trigger | Note |
|------|--------------------|------|
| 2439 | `fsm_task()` — loop principale: `!vcd_locked_session` AND (stop era attivo) AND stato CREDIT/ADS/IDLE | Pubblicato `ACTION_ID_CCTALK_START`. Azzera entrambi i flag. |
| 2552 | `fsm_task()` — blocco `left_program_state` (uscita RUNNING/PAUSED) | Riavviato immediatamente se uno dei flag è attivo. |
| 2302 | `fsm_task()` — uscita da `OUT_OF_SERVICE` con recovery completato | Riavviato se `cfg->sensors.cctalk_enabled`. |

### 2.3 Meccanismo di controllo (via mailbox FSM)

Il controllo CCTalk avviene tramite `tasks_publish_cctalk_control_event()` che
pubblica un evento verso `AGN_ID_CCTALK` con `ACTION_ID_CCTALK_START/STOP`.
Il driver CCTalk (`cctalk_task_run`) consuma l'evento e abilita/disabilita
l'accettatore internamente.

### 2.4 Condizioni di fermata

```
cctalk fermata ⟺  vcd_locked_session == true
                  (= session_mode==VIRTUAL_LOCKED AND source==QR|CARD AND !allow_additional_payments)
          OPPURE  fsm.state == OUT_OF_SERVICE
```

---

## 3. MDB Cashless (lettore chip card / NFC)

### 3.1 Flag di controllo
| Flag                        | Tipo   | Significato                                                    |
|----------------------------|--------|----------------------------------------------------------------|
| `mdb_forced_reset_for_program` | `bool` | Reset eseguito a inizio programma (senza sessione aperta) |

### 3.2 Meccanismo

Il controllo MDB avviene via `mdb_cashless_reset_device(i)` (reset hardware del
dispositivo sul bus MDB), **non** tramite un flag software on/off.
Il reset forza il lettore a reinizializzare il protocollo MDB.

**Eccezione:** se al momento dello stop-programma il lettore ha già una sessione
aperta (`session_open == true`), il cashless NON viene resettato, per non
interrompere la sessione in corso (es. tag NFC ancora posato).

### 3.3 Punti di controllo

#### RESET (disabilitazione funzionale)

| Riga | Contesto / Trigger | Note |
|------|--------------------|------|
| 2491–2495 | `fsm_task()` — loop principale: `active_program_session && !mdb_session_open` | Reset al primo tick di RUNNING/PAUSED senza sessione aperta. Imposta `mdb_forced_reset_for_program`. |
| 2500 | `fsm_task()` — loop principale: fine programma + `mdb_forced_reset_for_program` + stato CREDIT/ADS/IDLE | Reset di "reattivazione" (ri-inizializza il lettore). |
| 2593 | `fsm_task()` — blocco `left_program_state`: condizione `!keep_cashless_active` | Reset al termine del programma se nessuna sessione card è ancora aperta. |

#### MANTENUTO ATTIVO (nessun reset)

| Riga | Contesto / Trigger | Note |
|------|--------------------|------|
| 2588–2590 | `left_program_state`: `keep_cashless_active == true` | Sessione CARD ancora valida o `hardware_session_open == true`. Nessun reset. |

### 3.4 Logica di `keep_cashless_active`

```c
keep_cashless_active =
    cfg->sensors.mdb_enabled && cfg->mdb.cashless_en &&
    (
        (fsm.state == FSM_STATE_CREDIT && fsm.session_source == FSM_SESSION_SOURCE_CARD)
        OR hardware_session_open  // device[0].session_open == true
    );
```

### 3.5 Condizioni di reset a fine programma

```
reset MDB ⟺  programma attivo (RUNNING|PAUSED)
         AND nessuna sessione MDB aperta
         AND NOT (sessione CARD ancora in CREDIT o hardware_session_open)
```

---

## 4. Tabella comparativa — comportamento per stato FSM

| Stato FSM       | Scanner QR        | CCTalk            | MDB Cashless           |
|-----------------|-------------------|-------------------|------------------------|
| `IDLE`          | ON                | ON                | ON                     |
| `ADS`           | ON                | ON                | ON                     |
| `CREDIT`        | ON                | ON                | ON (o reset se rientro da RUN) |
| `RUNNING`       | **OFF** (HW cmd)  | ON *(1)*          | **RESET** se no sessione aperta |
| `PAUSED`        | **OFF** (HW cmd)  | ON *(1)*          | **RESET** se no sessione aperta |
| `OUT_OF_SERVICE`| **OFF** (HW cmd)  | **OFF** (STOP)    | Invariato (reset al recovery) |

*(1) CCTalk è spento durante RUNNING/PAUSED solo in caso di `vcd_locked_session`.*

---

## 5. Interazioni reciproche e anomalie note

### 5.1 Scanner: doppio spegnimento durante programma + post-lettura
Se arriva un barcode appena prima dell'avvio del programma, la FSM tenta di
spegnere lo scanner (`scanner_forced_off_for_program`) mentre il cooldown è già
attivo. I due meccanismi sono indipendenti: il cooldown task tenterà il
`send_on_command()` alla scadenza, ma se `scanner_forced_off_for_program` è ancora
attivo il re-enable avviene ugualmente (HW on, ma la FSM all'iterazione successiva
potrebbe rispedirlo off). **Nessun workaround attuale**.

### 5.2 CCTalk: nessun stop durante RUNNING in sessione normale
La decisione di tenere la gettoniera attiva durante un programma attivo permette
l'inserimento di monete aggiuntive durante il ciclo (autorepeat / top-up).
È un comportamento **intenzionale**.

### 5.3 MDB: reset ≠ off
Il reset MDB non spegne il bus; reinizializza il dispositivo affinché torni
in stato `DISABLED` e poi `ENABLED`. Se il tag NFC è ancora posato durante il
reset, il lettore potrebbe emettere un bip o un'animazione di errore. Questo è
documentato come comportamento noto e tollerato.

### 5.4 Scanner in modalità ePaper (DEVICE_DISPLAY_TYPE_EPAPER_RS232)
Vedi `docs/PIANO_EPAPER_USBCDC.md` § 4.3.3. In questa modalità:
- I comandi `send_off/on/setup` diventano **no-op** (scanner non controllabile HW).
- Il silenziamento durante programma avviene via **filtro software** su `scanner_on_barcode_cb()`.
- `scanner_cooldown_tick()` invoca `send_on_command()` che è no-op: nessun effetto indesiderato.

---

## 6. File coinvolti nel controllo dei sistemi di pagamento

| File | Ruolo |
|------|-------|
| `main/tasks.c` | Logica di controllo FSM (tutti i punti elencati sopra) |
| `components/usb_cdc_scanner/usb_cdc_scanner.c` | Driver scanner; implementa `send_on/off/setup_command()` |
| `components/cctalk/` | Driver CCTalk; consuma `ACTION_ID_CCTALK_START/STOP` |
| `components/mdb/` | Driver MDB; implementa `mdb_cashless_reset_device()` |
| `data/tasks.json` | Abilita/disabilita i task a livello OS (stato RUN/IDLE) |
