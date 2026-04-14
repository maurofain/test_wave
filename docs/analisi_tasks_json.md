# Analisi del settaggio di `data/tasks.json`

Data: 14 aprile 2026

## Scopo

Questa nota salva la valutazione del file `data/tasks.json` confrontandolo con il comportamento reale del firmware definito in `main/tasks.c` e con i task creati fuori dal task manager centrale.

## Sintesi

Il file `data/tasks.json` e utilizzabile, ma non e una fonte di verita pulita.
Alcuni valori vengono realmente applicati a runtime dal loader JSON, altri vengono forzati dal firmware, altri ancora sono presenti nel JSON ma non governano nessun task reale.

Il problema principale non e che tutti i valori siano sbagliati, ma che nello stesso file convivono tre categorie diverse:

1. task realmente configurabili dal loader
2. task il cui stato viene forzato dal firmware
3. task presenti nel JSON ma non gestiti dal task manager centrale

## Risultati principali

### 1. Incoerenza critica sul task `fsm`

Nel file `data/tasks.json` il task `fsm` ha `k = 0`, quindi stack in SPIRAM.

Nel codice, invece, il default del task `fsm` e definito con stack in RAM interna:

- `main/tasks.c`: `.name = "fsm"`
- `main/tasks.c`: `.stack_caps = MALLOC_CAP_INTERNAL`

Poiche il loader JSON applica davvero il campo `k`, il JSON sta sovrascrivendo una scelta intenzionale del firmware.

Valutazione: critica.

Motivo: `fsm` e un task centrale e sensibile; spostarne lo stack in SPIRAM senza motivo forte non e una buona base di default.

### 2. Priorita `lvgl` troppo aggressiva nel JSON

Nel file `data/tasks.json` il task `lvgl` ha `p = 10`.

Nel codice il default e:

- `main/tasks.c`: `.name = "lvgl"`
- `main/tasks.c`: `.priority = 5`

Questo significa che il JSON sta alzando la priorita di LVGL rispetto alla scelta del firmware.

Valutazione: medio-alta.

Motivo: con periodo a 16 ms, una priorita 10 rischia di rubare tempo a task funzionali come `fsm`, `cctalk_task`, `mdb_engine` e `sd_monitor` senza che ci sia evidenza, in questa analisi, di una necessita reale.

### 3. Presenza di task nel JSON che non sono gestiti dal task manager centrale

Nel JSON compaiono almeno queste voci:

- `keepalive_task`
- `log_sender`

Questi task non risultano presenti nella tabella `s_tasks[]` di `main/tasks.c`, quindi non sono governati da `tasks_load_config()`.

In pratica:

- `keepalive_task` viene creato direttamente in `components/http_services/keepalive_task.c`
- `log_sender` viene creato direttamente in `components/remote_logging/remote_logging.c`

Valutazione: media.

Motivo: il file `data/tasks.json` lascia intendere che questi task siano configurabili da li, ma a runtime non e cosi. Questo rende il file fuorviante.

### 4. Ambiguita tra `mdb` e `mdb_engine`

Nel JSON esistono sia:

- `mdb`
- `mdb_engine`

Nel codice esiste una mappatura legacy che traduce `mdb` in `mdb_engine` durante la ricerca del task.

Questo non sembra rompere il runtime in modo diretto, ma crea ambiguita:

- chi modifica il JSON puo pensare che le due entry siano distinte
- l'ordine delle entry puo influenzare quale configurazione prevale
- la presenza doppia rende il file meno affidabile come configurazione operativa

Inoltre lo stato di `mdb_engine` viene comunque forzato dal firmware in base alla device config:

- `sensors.mdb_enabled`
- `mdb.coin_acceptor_en`
- `mdb.cashless_en`

Valutazione: media.

### 5. Alcuni campi `m` del JSON sono di fatto decorativi

Per task come `cctalk_task` e `mdb_engine`, il wrapper in `main/tasks.c` entra direttamente nei loop interni del componente:

- `cctalk_task_run(NULL)`
- `mdb_engine_run(NULL)`

Questo significa che il campo `m` nel JSON non governa realmente il ritmo del task come avviene per altri task che usano `param->period_ticks`.

Valutazione: media-bassa.

Motivo: il file suggerisce un grado di controllo che in questi casi non esiste davvero.

### 6. Documentazione disallineata dal codice reale

La tabella in `docs/a_TASKS.md` non e pienamente allineata con `main/tasks.c`.

Un caso evidente emerso durante l'analisi e `fsm`, descritto nella documentazione come se usasse SPIRAM, mentre nel codice usa RAM interna.

Valutazione: bassa come rischio runtime, ma alta come rischio di manutenzione.

Motivo: chi consulta la documentazione puo prendere decisioni su una base non aggiornata.

## Valutazione dei settaggi attuali

### Settaggi sostanzialmente sensati

I valori di stack e priorita per questi task appaiono, in prima battuta, coerenti come ordine di grandezza:

- `audio`
- `http_services`
- `ntp`
- `usb_scanner`
- `cctalk_task`
- `mdb_engine`

Questi valori non risultano immediatamente sospetti nella sola analisi statica.

### Settaggi da correggere o chiarire

- `fsm`: da riallineare su stack interno
- `lvgl`: da riportare a una priorita coerente con il firmware, salvo misure che giustifichino altro
- `keepalive_task`: da rimuovere dal JSON oppure da dichiarare esplicitamente non gestito da questo file
- `log_sender`: stessa considerazione di `keepalive_task`
- `mdb` e `mdb_engine`: da unificare o almeno da chiarire meglio

## Comportamento runtime da tenere presente

Anche quando `data/tasks.json` contiene uno stato iniziale (`s`), in diversi casi il firmware forza comunque il valore finale a boot o durante `tasks_apply_n_run()`.

Tra i casi piu importanti:

- `usb_scanner` viene forzato in base a `cfg->scanner.enabled`
- `cctalk_task` viene forzato in base a `cfg->sensors.cctalk_enabled`
- `mdb_engine` viene forzato in base a `cfg->sensors.mdb_enabled` e alle sottosezioni MDB abilitate
- `lvgl` e `touchscreen` vengono bloccati se `display.enabled` e falso
- `io_expander` viene bloccato se `sensors.io_expander_enabled` e falso
- `fsm` viene saltato in `tasks_start_all()` per avvio differito post-bootstrap UI

Questo conferma che `data/tasks.json` oggi non rappresenta da solo il comportamento finale del sistema.

## Conclusione

La configurazione attuale di `data/tasks.json` e discreta come base, ma non abbastanza coerente da poter essere considerata affidabile senza conoscere il codice.

I problemi reali emersi non sono tanto di dimensionamento generale, quanto di governance della configurazione:

1. alcuni valori nel JSON sovrascrivono scelte intenzionali del firmware
2. alcuni task presenti nel JSON non sono veramente configurabili da quel file
3. alcune voci sono duplicate o legacy e rendono ambiguo il significato operativo
4. la documentazione non e completamente allineata al comportamento reale

## Interventi consigliati

Ordine suggerito:

1. riallineare `fsm` in `data/tasks.json` su stack interno
2. riportare `lvgl` a una priorita coerente col default del firmware, salvo misura contraria
3. rimuovere o marcare come non gestite le entry `keepalive_task` e `log_sender`
4. eliminare la duplicazione tra `mdb` e `mdb_engine`, mantenendo una sola voce canonica
5. aggiornare `docs/a_TASKS.md` in modo che rifletta il codice reale

## Riferimenti analizzati

- `data/tasks.json`
- `main/tasks.c`
- `docs/a_TASKS.md`
- `components/http_services/keepalive_task.c`
- `components/remote_logging/remote_logging.c`