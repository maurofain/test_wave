# Midpoint Check 01 — Analisi FSM + Message Queue

Data: 2026-02-24

---

## BUG CRITICI

---

### BUG 1 — `fsm_event_receive()`: avanzamento head errato (bug latente)
**File**: `main/fsm.c` ~L414  
**Severità**: 🔴 ALTO

```c
if (s_mailbox[i].to_mask == 0) {
    s_mb_head = (s_mb_head + 1) % FSM_MAILBOX_SIZE;  // BUG: solo se i == s_mb_head
}
```

Il loop scansiona da `s_mb_head` in avanti. Se lo slot trovato è alla posizione `i > s_mb_head`
(perché gli slot precedenti sono destinati ad altri agent privi di receiver),
svuotare la maschera e avanzare `s_mb_head` di 1 libera lo slot **sbagliato**.

In pratica oggi non si manifesta perché quasi tutti i messaggi vanno ad `AGN_ID_FSM`
e si trovano sempre alla testa — ma è una bomba a orologeria non appena si aggiungono
altri subscriber.

**Fix**: aggiungere il controllo `if (s_mailbox[i].to_mask == 0 && i == s_mb_head)`.

---

### BUG 2 — Credito esatto scalato prima del check: programma non parte
**File**: `main/fsm.c` ~L250  
**Severità**: 🔴 ALTO

```c
// in fsm_handle_input_event():
ctx->credit_cents -= ctx->running_price_units;         // scalato qui
return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_SELECTED);

// in fsm_handle_event():
if (event == FSM_EVENT_PROGRAM_SELECTED && ctx->credit_cents > 0) {  // check qui
```

Se il programma costa esattamente tutto il credito disponibile, dopo la sottrazione
`credit_cents == 0`. Il check `> 0` fallisce e la FSM **non** transita in `RUNNING`.
Il credito è già stato sottratto, il programma non parte. **Il cliente paga ma la macchina non si avvia.**

**Fix**: cambiare il check in `fsm_handle_event` in `>= 0`, oppure spostare
il check (e la sottrazione) interamente dentro `fsm_handle_event`, eliminando
la doppia responsabilità.

---

### BUG 3 — `fsm_event_publish_from_isr()`: semaforo bloccante chiamato da ISR
**File**: `main/fsm.c` ~L377  
**Severità**: 🔴 ALTO

```c
xSemaphoreGiveFromISR(s_mb_mutex, &higher);
fsm_pending_push(event);   // <-- chiama xSemaphoreTake(..., pdMS_TO_TICKS(10)) da ISR!
```

`fsm_pending_push()` usa `xSemaphoreTake(s_fsm_pending_lock, pdMS_TO_TICKS(10))`.
Chiamare un semaforo bloccante da contesto ISR è **undefined behavior** per FreeRTOS/ESP-IDF.
Può causare crash, assert di sicurezza o deadlock.

**Fix**: rimuovere la chiamata a `fsm_pending_push()` dalla ISR, oppure usare una
variante ISR-safe separata che accede alla pending list tramite `xSemaphoreTakeFromISR`.

---

### BUG 4 — `task_woken` scritto prima di `xSemaphoreGiveFromISR`
**File**: `main/fsm.c` ~L373  
**Severità**: 🟠 MEDIO-ALTO

```c
if (task_woken) *task_woken = higher;           // scritto con il risultato del Take
xSemaphoreGiveFromISR(s_mb_mutex, &higher);     // Give può cambiare 'higher'
```

L'output `task_woken` dovrebbe essere `pdTRUE` se il *Give* sblocca un task
ad alta priorità, non se il *Take* lo ha fatto. Il valore corretto va letto
**dopo** il Give.

**Fix**: spostare l'assegnazione a `task_woken` dopo `xSemaphoreGiveFromISR`.

---

## ISSUE MEDIE

---

### ISSUE 5 — `pause_limit_reached`: flag impostato ma nessuna transizione automatica
**File**: `main/fsm.c` ~L302  
**Severità**: 🟠 MEDIO

```c
if (ctx->pause_elapsed_ms >= ctx->pause_max_ms) {
    ctx->pause_limit_reached = true;   // solo il flag, nessuna azione
}
```

Il flag viene impostato ma la FSM rimane in `PAUSED` indefinitamente.
Il comportamento atteso (tornare in `CREDIT` o fermare il programma) non avviene mai
in modo automatico. Il flag esiste ma nessuno lo controlla.

**Fix**: aggiungere dopo l'impostazione del flag:
```c
return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_STOP);
```

---

### ISSUE 6 — `elapsed_ms` calcolato prima del blocco receive
**File**: `main/tasks.c` ~L147  
**Severità**: 🟠 MEDIO

```c
TickType_t now_tick = xTaskGetTickCount();
uint32_t elapsed_ms = (uint32_t)pdTICKS_TO_MS(now_tick - prev_tick);
prev_tick = now_tick;

if (fsm_event_receive(&event, param->period_ticks)) { ... }  // blocca fino a 100ms
changed = fsm_tick(&fsm, elapsed_ms) || changed;             // usa elapsed calcolato PRIMA del blocco
```

`elapsed_ms` si calcola **prima** di `fsm_event_receive()`, quindi include solo il tempo
dall'iterazione precedente fino all'inizio dell'iterazione corrente; i 100ms di attesa
non sono conteggiati. Per una macchina a stati con timer (pausa, inattività, timeout splash)
questo introduce un errore sistematico nel computo del tempo.

**Fix**: calcolare `elapsed_ms` **dopo** `fsm_event_receive()`:
```c
if (fsm_event_receive(&event, param->period_ticks)) { changed = ...; }
TickType_t now_tick = xTaskGetTickCount();
uint32_t elapsed_ms = (uint32_t)pdTICKS_TO_MS(now_tick - prev_tick);
prev_tick = now_tick;
changed = fsm_tick(&fsm, elapsed_ms) || changed;
```

---

### ISSUE 7 — `fsm_event_receive()`: polling ogni 1ms (busy-wait)
**File**: `main/fsm.c` ~L395  
**Severità**: 🟠 MEDIO

```c
while (true) {
    if (xSemaphoreTake(s_mb_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        // scansiona... non trovato?
    }
    vTaskDelay(pdMS_TO_TICKS(1));  // aspetta 1ms, poi riprova
}
```

Consuma CPU inutilmente ogni millisecondo girando sul mutex.
La versione di publish è già stata migliorata; la receive ha ancora il loop manuale.

**Fix consigliato**: aggiungere un `SemaphoreHandle_t s_mb_signal` (counting semaphore)
dato ogni volta che si pubblica un messaggio. Il receiver aspetta su `s_mb_signal`
invece di girare, e poi entra nella sezione critica solo quando c'è qualcosa di nuovo.

---

## ISSUE MINORI / MIGLIORAMENTI

---

### ISSUE 8 — Solo `AGN_ID_FSM` può ricevere eventi (receive hardcoded)
**File**: `main/fsm.c` ~L392  
**Severità**: 🟡 BASSO

```c
uint32_t mybit = (1u << AGN_ID_FSM);
```

Il sistema mailbox supporta destinatari multipli tramite `to_mask`, ma `fsm_event_receive()`
è hardcoded all'ID della FSM. Nessun altro agent (RS232, MDB, USB scanner, ecc.)
può ricevere messaggi dalla mailbox. La metà ricevente del bus multi-agent non è implementata.

---

### ISSUE 9 — `xSemaphoreCreate*` chiamato dentro `portENTER_CRITICAL`
**File**: `main/fsm.c` ~L328  
**Severità**: 🟡 BASSO

L'allocatore heap di FreeRTOS/ESP-IDF usa internamente `taskENTER_CRITICAL`.
Annidare una sezione critica dentro un'altra (`portENTER_CRITICAL`) su RISC-V multi-core
può causare un assert o deadlock se il core tenta di riacquisire lo stesso spinlock.

**Fix**: creare i mutex fuori dalla sezione critica, poi verificare/assegnare dentro.

---

### ISSUE 10 — Leak parziale di risorse in `fsm_event_queue_init()` su fallimento
**File**: `main/fsm.c` ~L340  
**Severità**: 🟡 BASSO

Se `s_fsm_pending_lock` viene creato ma poi `s_fsm_runtime_lock` fallisce,
il primo semaforo non viene deallocato e `inited` rimane `false`.
Al prossimo tentativo di init si tenta di ricreare tutto,
ma il puntatore al semaforo precedente è sovrascritto (doppio leak).

---

### ISSUE 11 — `inactivity_ms` overflow in stato IDLE (49 giorni)
**File**: `main/fsm.c` ~L281  
**Severità**: 🟡 BASSO

```c
ctx->inactivity_ms += elapsed_ms;  // sempre, in ogni stato
```

Il campo viene resettato nelle transizioni di stato ma in `FSM_STATE_IDLE`
la macchina può restare per ore/giorni. `uint32_t` va in overflow dopo ~49 giorni.
Non causa crash (overflow well-defined in C unsigned), ma potrebbe interferire
con future logiche che leggono il campo.

---

### ISSUE 12 — Tre sistemi enum paralleli non allineati
**File**: `main/fsm.h`  
**Severità**: ⚪ INFO

Coesistono:
- `fsm_event_t` — eventi interni FSM
- `fsm_input_event_type_t` — vecchi eventi legacy
- `action_id_t` — nuovo sistema (non ancora usato da `fsm_handle_input_event`)

Il campo `.action` in `fsm_input_event_t` non è mai letto da `fsm_handle_input_event()`;
solo `.type` viene processato. La migrazione verso `action_id_t` è incompiuta
e i tre sistemi creano confusione per chi aggiunge nuovi componenti.

---

## Riepilogo priorità

| # | Severità | Problema | File | Stato |
|---|---------|----------|------|-------|
| 1 | 🔴 ALTO | Avanzamento head errato in `receive` | `fsm.c` ~L414 | ✅ risolto |
| 2 | 🔴 ALTO | Bug credito `== 0` blocca avvio programma | `fsm.c` ~L250 | ✅ risolto |
| 3 | 🔴 ALTO | Semaforo bloccante chiamato da ISR | `fsm.c` ~L377 | ✅ risolto |
| 4 | 🟠 MEDIO | `task_woken` scritto prima di Give | `fsm.c` ~L373 | ✅ risolto |
| 5 | 🟠 MEDIO | Pausa scaduta: nessuna transizione automatica | `fsm.c` ~L302 | ✅ risolto (resume auto) |
| 6 | 🟠 MEDIO | `elapsed_ms` calcolato prima del blocco receive | `tasks.c` ~L147 | ✅ risolto |
| 7 | 🟠 MEDIO | Receive polling ogni 1ms (busy-wait) | `fsm.c` ~L395 | ✅ risolto (signal sema) |
| 8 | 🟡 BASSO | Solo FSM può ricevere (receive hardcoded) | `fsm.c` ~L392 | ✅ risolto (param receiver_id) |
| 9 | 🟡 BASSO | `xSemaphoreCreate` dentro critical section | `fsm.c` ~L328 | ✅ risolto (pre-alloc fuori) |
| 10 | 🟡 BASSO | Leak parziale su init failure | `fsm.c` ~L340 | ✅ risolto |
| 11 | 🟡 BASSO | `inactivity_ms` overflow in IDLE (49gg) | `fsm.c` ~L281 | ✅ risolto (cap a splash+5s) |
| 12 | ⚪ INFO | Tre sistemi enum non allineati (`fsm_event_t`, `fsm_input_event_type_t`, `action_id_t`) | `fsm.h` | ✅ bridge action→type in `fsm_handle_input_event` |
| EM | 🔴 ALTO | `/emulator` 404: `max_uri_handlers` 64 superato (~68 route) | `web_ui_server.c` | ✅ risolto (→100) |
| TODO4 | ⚪ INFO | Emulator: gauge → tempo rimanente in secondi, credito in coin interi | `web_ui_auth_emulator.c` | ✅ risolto |
