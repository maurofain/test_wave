# SEQUENZA_BOOT

> **Due sezioni distinte:**
> 1. [Implementazione reale attuale](#sequenza-reale-attuale) — cosa accade oggi, con i tempi misurati e i problemi noti.
> 2. [Architettura target](#architettura-target) — la sequenza ideale verso cui tendere.

---

## Sequenza reale attuale

### Tempi misurati (log monitor)
| T (ms) | Evento |
|--------|--------|
| ~0     | Avvio bootloader (verifica SHA256 app ~1 s) |
| ~1029  | `esp_core_dump_flash: Init core dump to flash` — primo log IDF, prima di `app_main` |
| ~1051  | `esp_core_dump_flash: Found core dump N bytes in flash` (se presente) |
| ~1051–5763 | **`app_main()` + `init_run_factory()`** (dettaglio sotto) |
| ~5763  | `TASKS: Avvio task ws2812` — `tasks_start_all()` completato |

### Sequenza dettagliata `app_main()`

**1. Policy log**
- `apply_boot_log_policy()` — livello globale `INFO` (APP) o `ERROR` (FACTORY); USB/CDC forzati a `ERROR`

**2. Storage errori early**
- `error_log_init()` → `sd_card_mount()` — SD montata (o mock); file di log aperto

**3. Banner versione**
- Log: mode, running app, versione, data compilazione

**4. Hardware minimo per decisione boot**
- `init_i2c_and_io_expander()`:
  - `bsp_i2c_init()` (se `CONFIG_BSP_I2C_NUM`)
  - `io_expander_init()`
  - loop GPIO3: `while (io_get_pin(3) == 0) vTaskDelay(1000ms)` — attende rilascio pulsante Service

**5. `init_run_factory()`**
```
a. init_nvs()
b. update_boot_reboot_guard()        — incrementa NVS consecutive_reboots se reset non pulito
c. update_crash_pending_record()     — scrive NVS crash_pending=1 se reset non pulito
d. init_spiffs()                     — mount SPIFFS (~500 ms)
e. log_partitions()
f. init_event_loop()
g. device_config_init()              — deserializza config JSON da SPIFFS
h. fsm_event_queue_init(0)
i. try_send_pending_crash_record()   — ⚠️ PROBLEMA NOTO (vedi sotto)
j. [check s_error_lock_active → blocco boot se >= REBOOT_LIMIT]
k. aux_gpio_init()
l. init_display_lvgl_minimal()       — solo se cfg->display.enabled
m. start_ethernet()                  — solo se cfg->eth.enabled
n. serial_test_init()
o. web_ui_init()
p. device_activity_init()            — solo COMPILE_APP
q. remote_logging_init()
r. io_expander_init() + loop GPIO3   — percorso legacy (senza BSP_I2C_NUM), se io_expander_enabled
s. led_init()                        — se led_enabled
t. rs232_init() + cctalk_driver_init() — se rs232_enabled
u. rs485_init()                      — se rs485_enabled
v. mdb_init() + mdb_start_engine()   — se mdb_enabled
w. pwm_init()                        — se pwm1/pwm2 enabled
x. sht40_init()                      — se temperature_enabled
y. sd_card_init_monitor()            — sempre (hot-plug detection)
z. sd_card_mount() + transfer_coredump_flash_to_sd() — se sd_card_enabled
```

**6. Post-init**
- `maybe_force_crash_at_boot()` — no-op (`DEBUG_FORCE_CRASH_AT_BOOT=0`)
- `apply_post_boot_log_policy()` — globale `INFO`, USB/CDC a `WARN`
- `tasks_load_config("/spiffs/tasks.csv")`
- `tasks_start_all()` ← **T ~5763 ms**

**7. Finestra di stabilità (`BOOT_COUNTER_RESET_DELAYED=1`)**
- loop 30 s in step da 5 s con log heap residuo
- `init_mark_boot_completed()` — azzera NVS `consecutive_reboots`

**8. Main loop**
- `vTaskDelay(5000)` + log "Device in funzione ✓" ogni 5 s

---

### ⚠️ Problema noto: `try_send_pending_crash_record` prima di Ethernet

**Passo `i`** chiama `esp_http_client_perform` con `timeout_ms=3000` **prima** che `start_ethernet()` (passo `m`) abbia inizializzato la rete. Se `crash_pending=1` (ovvero nel boot successivo a qualsiasi crash/panic), la funzione attende il timeout completo di **~3 s** prima di proseguire, causando il ritardo osservato tra T=1051 ms e T=5763 ms.

**Fix previsto:** spostare `try_send_pending_crash_record()` dopo `start_ethernet()`, oppure verificare che almeno una netif abbia un IP prima di tentare il POST.

---

## Architettura target

## Obiettivo
Definire una sequenza di avvio deterministica, senza race in inizializzazione, coerente con la scelta architetturale della coda eventi multi-destinatario (mailbox condivisa).

## Principi
- Prima decisione di boot: verifica modalità `Service` (già implementata).
- Il canale esterno per notifica crash usa solo `http_services` (`/api/deviceactivity`) ed è separato dalla sequenza standard, attivando solo il minimo necessario.
- In modalità normale, l'`event bus` viene inizializzato prima di ogni servizio che pubblica/consuma eventi.
- In caso di errore su servizi core (`event bus`, `FSM`), applicare fail-fast (non proseguire con init dipendenti).
- Gli init devono essere idempotenti dove possibile.

## Pre-boot crash channel (separato dalla init standard)
### Obiettivo
Permettere, al riavvio dopo crash, una comunicazione esterna rapida e robusta della `reboot_reason` senza dipendere dall'avvio completo dell'applicazione.

### Regole
- Eseguire questo ramo **prima** della sequenza standard (`init_run_factory`, task, periferiche non critiche).
- Attivare solo i moduli minimi: storage motivo reboot + minima rete necessaria all'invio + sender HTTP via `http_services`.
- Terminato l'invio (o timeout), rientrare nella sequenza di boot normale.

### Moduli minimi consigliati
1. Lettura causa reset (`esp_reset_reason`) e record crash precedente.
2. Persistenza record pre-boot (NVS/RTC/flash marker) con flag `pending_send`.
3. Init rete minima necessaria al canale esterno (es. solo Ethernet o interfaccia configurata).
4. Invio notifica crash via `http_services` (`/api/deviceactivity`) con payload ridotto: `device_id`, `boot_count`, `reset_reason`, `last_timestamp`.
5. Aggiornamento stato record (`sent_ok` / `sent_failed`) e ritorno al boot standard.

### Nota payload `deviceactivity`
- Usare la stringa generica del payload per descrivere il crash critico.
- Verificare il limite massimo di lunghezza del campo stringa; se necessario applicare troncamento controllato con suffisso `...`.

### Vincoli
- Timeout stretto (es. 3-10 s) per non bloccare indefinitamente il boot.
- Retry limitati nel pre-boot; eventuali retry estesi delegati al runtime standard.
- Nessuna inizializzazione di periferiche non necessarie in questo ramo.

## Politica reboot consecutivi
### Contatore
- Mantenere un contatore persistente `consecutive_reboots` incrementato a ogni boot non pulito.
- Azzerare il contatore solo dopo finestra di stabilità (es. uptime > soglia o shutdown/boot considerato sano).

### Soglia e stato ERROR
- Se `consecutive_reboots >= REBOOT_LIMIT`, entrare in stato `ERROR_LOCK`.
- In `ERROR_LOCK`:
   - segnalazione visiva dedicata (es. striscia LED rossa lampeggiante),
   - input utente inibiti,
   - device collegati portati in standby/safe state,
   - consentire solo canali di diagnosi/recovery (Service, API diagnostiche minime, log).

### Uscita da ERROR_LOCK
- Definire successivamente la policy (es. intervento Service esplicito, comando remoto autenticato, o boot stabili consecutivi).

## Sequenza di bootstrap (vincolante)
1. **Policy log + pre-init app**
   - `apply_boot_log_policy()`
   - `error_log_init()`
   - Motivazione: riduce rumore al boot e rende subito disponibile il canale errori.

2. **Init minimo hardware per decisione di boot**
   - `init_i2c_and_io_expander()`
   - verifica stato Service su `GPIO3` expander (`io_get_pin(3)`) già implementata.
   - Motivazione: la decisione Service/Normal deve avvenire prima di inizializzare i servizi applicativi.

3. **Bootstrap core piattaforma (`init_run_factory`)**
   - `init_nvs()`
   - `init_spiffs()`
   - `init_event_loop()`
   - `init_i2c_bus()` / `eeprom_24lc16_init()` (se percorso legacy I2C attivo)
   - `device_config_init()`
   - `aux_gpio_init()`
   - Motivazione: disponibilità di config e storage prima dei moduli che dipendono da parametri runtime.

4. **Init UI/rete di base (condizionale da config)**
   - `init_display_lvgl_minimal()` (se display abilitato)
   - `start_ethernet()` (se abilitato)
   - `serial_test_init()`
   - `web_ui_init()`
   - Motivazione: endpoint/UI devono partire solo dopo config valida e stack base pronto.

5. **Init telemetria e servizi comuni**
   - `device_activity_init()` (main)
   - `remote_logging_init()`
   - `sd_card_init_monitor()` (+ `sd_card_mount()` se abilitata)
   - Motivazione: log e monitoraggio devono essere disponibili prima dei task operativi.

6. **Init periferiche operative (condizionali da config)**
   - `io_expander_init()` (secondo check GPIO3 in percorso run)
   - `led_init()`
   - `rs232_init()` + `cctalk_driver_init()`
   - `rs485_init()`
   - `mdb_init()` + `mdb_start_engine()`
   - `pwm_init()`
   - `sht40_init()`
   - Motivazione: periferiche opzionali avviate solo quando configurate e con fallback runtime se errore.

7. **Event Bus/FSM (scelta architetturale)**
   - `event_bus_init()` (nuovo, mailbox condivisa multi-destinatario)
   - `fsm_init()` + `fsm_event_queue_init()` compatibile durante migrazione
   - Motivazione: separa chiaramente producer/consumer e previene race su pubblicazione eventi.

8. **Avvio task applicativi**
   - `tasks_load_config("/spiffs/tasks.csv")`
   - `tasks_start_all()`
   - `apply_post_boot_log_policy()`
   - Motivazione: i task partono solo dopo init completo dei moduli dipendenti.

9. **Health check post-boot**
   - verifica stato moduli core e logging bootstrap.
   - Motivazione: fail-fast e diagnosi rapida in campo.

## Sequenza alternativa: ramo Service
1. `apply_boot_log_policy()`
2. `error_log_init()`
3. `init_i2c_and_io_expander()` + check Service
4. Avvio sequenza completa di init hardware (allineata al ramo App): periferiche, bus, sensori, attuatori, logging, task di test.
5. Differenziazione principalmente nel backend Web: endpoint disponibili in Factory dedicati a recovery/test/manutenzione.

Motivazione: in modalità Factory l'hardware deve essere completamente inizializzato per consentire test funzionali completi dei componenti.

### Nota importante (Factory vs App)
- L'avvio in Factory **non** si differenzia in modo sostanziale dall'avvio in App sul piano hardware.
- La differenza principale è il set di endpoint/feature esposti dal backend Web.
- Tutti i moduli hardware necessari ai test devono essere portati a stato operativo.

## Commento sulla scelta dell'ordine
- **Prima Service, poi Event Bus**: evita inizializzazioni inutili quando il sistema deve restare in modalità Service.
- **Prima config/storage, poi servizi applicativi**: evita boot parziali con moduli avviati senza parametri validi.
- **Periferiche dopo web/log base**: garantisce osservabilità immediata durante eventuali fault periferici.
- **Task solo a init completato**: elimina race di startup tra producer/consumer e riduce false segnalazioni di errore.

## Ragionamento su logging ed `event_loop`
- Il logging va gestito in **due fasi**:
   1. **Logging locale early** (pre-`event_loop`): `error_log_init()` può partire subito perché aggancia il canale locale (`vprintf`) e non dipende dallo stack rete/eventi IDF.
   2. **Logging remoto/event-consumer di rete** (post-rete, post-`event_loop`): `remote_logging_init()` va avviato solo dopo init base (`esp_netif` / rete) per garantire invio affidabile.
- Un event-consumer interno al nuovo `event_bus` (mailbox/queue locale) può partire anche prima di `event_loop`, purché non usi API che richiedono stack rete o loop eventi di sistema.
- Regola pratica: **sink locale prima, sink remoto dopo**.

## Tabella rapida dipendenze (`event_loop`)
| Modulo / init | Dipende da `event_loop` | Note operative |
| --- | --- | --- |
| `error_log_init()` | No | Può partire in early boot. |
| `event_bus_init()` (mailbox locale) | No | Ammesso pre-`event_loop` se usa solo queue/mutex locali. |
| `fsm_init()` / `fsm_event_queue_init()` | No (diretta) | Dipende dall'event bus applicativo, non dal loop IDF. |
| `device_config_init()` | No | Richiede NVS/EEPROM già disponibili. |
| `remote_logging_init()` | Indiretta sì (via rete) | Avviare dopo stack rete (`esp_netif`/socket) pronto. |
| `web_ui_init()` | Tipicamente sì | Meglio dopo `init_event_loop()` e init rete di base. |
| `start_ethernet()` | Sì | Richiede infrastruttura eventi/rete IDF. |
| `init_sntp()` / `init_sync_ntp()` | Sì | Da avviare dopo rete attiva. |

## Regole operative
- Nessun componente deve inviare eventi prima del completamento di `event_bus_init()`.
- `event_bus_init()` deve essere protetta contro doppia inizializzazione concorrente.
- Se `event_bus_init()` fallisce:
  - log errore critico,
  - non avviare FSM,
  - non avviare task publisher/consumer dipendenti.
- La modalità Service deve essere risolta prima dell'avvio del bus eventi applicativo.

## Pseudoflusso
```text
boot_start
  -> crash_preboot_check
   -> if crash_pending:
      init_min_net_for_alert
      send_crash_alert_deviceactivity
      update_crash_record
  -> init_hw_min
  -> check_service_button
     -> if service_mode:
          init_service_stack
          start_service_tasks
          done
     -> else:
      if consecutive_reboots >= REBOOT_LIMIT:
         enter_error_lock
         done
          event_bus_init
          fsm_init
          init_publishers_consumers
          start_app_tasks
          health_check
          done
```

## Note di integrazione
- Il documento è coerente con i punti critici in `docs/TODO.md` (sezione analisi coda eventi FSM).
- Le API legacy di publish/receive restano supportate tramite wrapper durante la migrazione.
- La tabella agenti (`agn_id`) e la tabella azioni (`action_id`) devono essere introdotte prima della migrazione completa dei moduli.

## Checklist rapida
- [ ] Check Service eseguito prima di init event bus
- [ ] Event bus inizializzato una sola volta
- [ ] FSM inizializzata solo dopo event bus OK
- [ ] Publisher eventi avviati solo dopo FSM/event bus
- [ ] Branch Service senza init non necessari
- [ ] Log bootstrap completo e verificabile
