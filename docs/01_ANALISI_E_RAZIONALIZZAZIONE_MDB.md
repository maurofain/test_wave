# Rapporto — Analisi e Piano di Razionalizzazione Protocollo MDB

**Data:** 2026-05-07 → aggiornato 2026-05-06  
**Progetto:** MicroHard fw_esp32p4  
**File target:** `main/tasks.c`, `components/mdb/mdb.c`  
**Riferimento processo:** Analogia con razionalizzazione Scanner QR (USB CDC) e CCTalk  

---

## 1. Analisi dello Stato Attuale del Modulo MDB

### 1.1 Meccanismo di Controllo Corrente
Il modulo MDB gestisce i dispositivi NFC/ChipCard su bus I2C mediante il meccanismo di **Reset Logico** (comando hardware `MDB_RESET_DEVICE`), anziché l'uso di comandi ON/OFF tipici degli altri periferici.

#### Variabili e Flag Attuali
| Variabile | Tipo | Funzione Corrente |
|-----------|------|-------------------|
| `mdb_forced_reset_for_program` | `bool` locale in `fsm_task` | Bandiera temporale per tracciare che un reset è stato pianificato a inizio programma. |
| `cfg->sensors.mdb_enabled` | `bool` | Configurazione abilitazione/disabilitazione driver MDB globale. |
| `cfg->mdb.cashless_en` | `bool` | Configurazione specifica per il modulo cashless. |
| `keep_cashless_active` | `bool` | Flag locale che indica se mantenere la sessione attiva al termine del ciclo programma. |
| `hardware_session_open` / `device[0].session_open` | `bool` | Stato reale della sessione corrente sul bus MDB. |
| `s_mdb_inhibit_pending` | `static bool` | Nuovo: reset differito pianificato. |
| `s_mdb_inhibit_until_tick` | `static TickType_t` | Nuovo: tick target del reset differito. |
| `s_mdb_recovery_next_tick` | `static TickType_t` | Nuovo: prossima verifica anti-blocco. |

---

## 2. Cause Radice dei Token Rifiutati (Analisi Effettiva del Codice)

### 2.1 Doppio Reset in un Unico Tick di FSM (Bug principale)
Quando un programma terminava, in un unico tick di `fsm_task()` si verificavano **due** reset consecutivi:

1. **Ramo "riabilitato"** di `mdb_stop_needed`: `mdb_cashless_reset_device()` → dispositivo in `MDB_STATE_INACTIVE`
2. **Blocco `left_program_state`**: `mdb_cashless_reset_device()` di nuovo → secondo ciclo INACTIVE→INIT

Risultato: il dispositivo impiegava >500ms per tornare in `IDLE_POLLING`, e qualsiasi token presentato in questo intervallo veniva ignorato o rifiutato con `OUT_OF_SEQUENCE`.

### 2.2 Reset Immediato a Inizio Programma
Al primo tick FSM con `active_program_session=true` (programma non-card), il reset partiva istantaneamente, senza attendere la chiusura di eventuali transazioni residue.

### 2.3 Nessun Recovery da MDB_STATE_ERROR
Il `case MDB_STATE_ERROR` nel motore MDB (`mdb.c`) conteneva solo `break`. Una volta entrato in ERROR state, il lettore rimaneva bloccato fino al riavvio del firmware.

---

## 3. Implementazione Effettuata

### 3.1 Nuove Costanti e Variabili Statiche (`main/tasks.c`)

```c
#define MDB_INHIBIT_DEFER_MS      350U    // ritardo reset post-inizio programma
#define MDB_RECOVERY_INTERVAL_MS  30000U  // frequenza verifica anti-blocco

static bool      s_mdb_inhibit_pending    = false;
static TickType_t s_mdb_inhibit_until_tick = 0;
static TickType_t s_mdb_recovery_next_tick = 0;
```

### 3.2 Funzione `tasks_mdb_inhibit_tick()` (`main/tasks.c`)
Esegue il reset differito quando:
- il timer scade
- nessuna sessione hardware è aperta (se c'è una sessione, posticipa di 500ms)

### 3.3 Funzione `tasks_mdb_recovery_tick()` (`main/tasks.c`)
Ogni `MDB_RECOVERY_INTERVAL_MS = 30s`, verifica se un dispositivo è in `MDB_STATE_ERROR`
senza sessione aperta → forza reset di recupero. Complementa il recovery nel driver.

### 3.4 Blocco `mdb_stop_needed` in `fsm_task()` — Modificato

**Prima:**
```c
// Reset immediato ad inizio programma
mdb_cashless_reset_device(i);
// ...
// Secondo reset alla fine programma (nel ramo "riabilitato")
mdb_cashless_reset_device(i);
```

**Dopo:**
```c
// Pianifica reset differito: dà 350ms alle transazioni residue
s_mdb_inhibit_pending = true;
s_mdb_inhibit_until_tick = now_tick + pdMS_TO_TICKS(MDB_INHIBIT_DEFER_MS);
// Alla fine del programma: libera flag, nessun secondo reset
mdb_forced_reset_for_program = false;
s_mdb_inhibit_pending = false;
```

### 3.5 Blocco `left_program_state` — Secondo Reset Rimosso

**Prima:**
```c
for (size_t i = 0; i < mdb_count; ++i) {
    mdb_cashless_reset_device(i);  // secondo reset ridondante
}
```

**Dopo:**
```c
// Nessun reset: il dispositivo è in reinizializzazione naturale dal reset pianificato
ESP_LOGI(TAG, "[M] MDB cashless: fine programma non-card, lettore in reinizializzazione naturale");
```

### 3.6 Recovery Automatico da `MDB_STATE_ERROR` (`components/mdb/mdb.c`)

Aggiunto `s_cashless_error_since_tick[]` (timestamp tick di ingresso in ERROR).

**Prima:**
```c
case MDB_STATE_ERROR:
    break;  // bloccato per sempre
```

**Dopo:**
```c
case MDB_STATE_ERROR:
    if (non_sessione_aperta && in_error_da > MDB_ERROR_RECOVERY_MS) {
        mdb_cashless_reset_device(device_index);
        device_rw->poll_state = MDB_STATE_INIT_RESET;
    }
    break;
```
Con `MDB_ERROR_RECOVERY_MS = 20000U` (20 secondi).

---

## 4. Tabella Comparativa — Comportamento Pre/Post

| Scenario | Prima | Dopo |
|---|---|---|
| **Inizio programma (non-card)** | Reset immediato al primo tick | Reset differito di 350ms |
| **Fine programma (non-card)** | 2 reset nello stesso tick | 0 reset aggiuntivi (lettore già in reinit) |
| **Fine programma (card)** | Reset se `!keep_cashless_active` | Stessa logica, ma inibizione cancellata se session viva |
| **MDB_STATE_ERROR** | Bloccato per sempre | Recovery automatico dopo 20s |
| **Token presentato <500ms dopo programma** | Rifiutato (lettore in INIT) | Accettato (lettore già in IDLE_POLLING) |

---

## 5. Verifica Robustezza del Flusso MDB

### 5.1 Scenario: Programma rapido (coin) + token subito dopo
- **Prima**: lettore resettato 2×, init ~600ms, token rifiutato
- **Dopo**: un solo reset differito di 350ms, init completata durante il programma, token accettato

### 5.2 Scenario: Programma via card (VCD/NFC)
- `mdb_session_open=true` → `mdb_stop_needed=false` → nessuna inibizione pianificata  
- A fine programma: `keep_cashless_active` valutato correttamente  
- Se sessione ancora aperta: inibizione cancellata

### 5.3 Scenario: Lettore fisico scollegato / errore UART
- Il motore raggiunge `MDB_STATE_ERROR` dopo `MDB_CASHLESS_SETUP_MAX_RETRIES=5` tentativi  
- Recovery automatico dopo 20s (mdb.c) + watchdog ogni 30s (tasks.c)  
- Al ricollegamento il lettore risponde con `JUST_RESET` → init cycle riparte normalmente

### 5.4 Scenario: `OUT_OF_SEQUENCE` durante sessione
- Già gestito in `mdb_cashless_sm()`: `poll_state = MDB_STATE_INIT_RESET`  
- Nessuna modifica necessaria

---

## 6. File Modificati

| File | Modifica |
|---|---|
| `main/tasks.c` | Costanti e variabili statiche MDB; `tasks_mdb_inhibit_tick()`; `tasks_mdb_recovery_tick()`; blocco `mdb_stop_needed`; blocco `left_program_state` |
| `components/mdb/mdb.c` | `s_cashless_error_since_tick[]`; `MDB_ERROR_RECOVERY_MS`; `case MDB_STATE_ERROR` con recovery; log migliorato su ERROR entry |

---

*Documento aggiornato post-implementazione.*

**Data:** 2026-05-07  
**Progetto:** MicroHard fw_esp32p4  
**File target:** `main/tasks.c`  
**Riferimento processo:** Analogia con razionalizzazione Scanner QR (USB CDC) e CCTalk  

---

## 1. Analisi dello Stato Attuale del Modulo MDB

### 1.1 Meccanismo di Controllo Corrente
Il modulo MDB gestisce i dispositivi NFC/ChipCard su bus I2C mediante il meccanismo di **Reset Logico** (comando hardware `MDB_RESET_DEVICE`), anziché l'uso di comandi ON/OFF tipici degli altri periferici.

#### Variabili e Flag Attuali
| Variabile | Tipo | Funzione Corrente |
|-----------|------|-------------------|
| `mdb_forced_reset_for_program` | `bool` | Bandiera temporale per forzare un reset del dispositivo a inizio programma se non c'è una sessione attiva. |
| `cfg->sensors.mdb_enabled` | `bool` | Configurazione abilitazione/disabilitazione driver MDB globale. |
| `cfg->mdb.cashless_en` | `bool` | Configurazione specifica per il modulo cashless. |
| `keep_cashless_active` | `bool` | Flag di derivazione (logico) che indica se mantenere la sessione attiva al termine del ciclo programma. |
| `hardware_session_open` / `device[0].session_open` | `bool` | Stato reale della sessione corrente sul bus MDB. |

### 1.2 Logica di Controllo Attuale (Summary)
La FSM in `main/tasks.c` gestisce il controllo MDB all'interno di `fsm_task()`.

| Trigger | Condizione Attuale | Azione Eseguita | Note |
|---------|--------------------|-----------------|------------------------|
| **Inizio Programma** (`RUNNING`/`PAUSED`) | `active_program_session && !mdb_session_open` | Reset Device (disabilitazione funzionale) | Aziona comando di reset hardware. Imposta `mdb_forced_reset_for_program`. |
| **Fine Programma** (`IDLE`) | `left_program_state && mdb_forced_reset_for_program && stato CREDIT/ADS/IDLE` | Reset "Riattivazione" | Tenta di ri-inizializzare il lettore se necessario. |
| **Exit Program (Condizione)** | `!keep_cashless_active` | Reset immediato | Se la condizione non è soddisfatta, reset eseguito. |
| **Sessione Attiva** | `hardware_session_open == true` | Nessun Reset | Evita di interrompere transazioni utente in corso. |

### 1.3 Funzione `keep_cashless_active`
La logica attuale per decidere se mantenere il dispositivo attivo è:
```c
bool keep_cashless_active = 
    cfg->sensors.mdb_enabled && cfg->mdb.cashless_en && (
        fsm.state == FSM_STATE_CREDIT && fsm.session_source == FSM_SESSION_SOURCE_CARD 
        OR hardware_session_open  // device[0].session_open == true
    );
```

---

## 2. Piano di Razionalizzazione MDB

L'obiettivo è armonizzare la gestione del modulo MDB con quella dello Scanner QR e CCTalk, introducendo una gestione degli stati più chiara, separando le responsabilità hardware/software e garantendo robustezza nel controllo delle transizioni.

### 2.1 Obiettivi della Razionalizzazione

1.  **Definizione Esplicita dei Stati di Sessione:** Introducere uno stato esplicito (simile a `MDB_RUNNING` o `MDB_LOCKED`) nella logica FSM, analogamente a come lo scanner gestisce i flag di cooldown.
2.  **Separazione tra Reset Logico e Reset Hardware:** Distinguere chiaramente tra il comando di reset fisico inviato al bus e la gestione logica delle sessioni in corso.
3.  **Introduzione del Timeout di Reset MDB (`MDB_RESET_TIMEOUT_MS`):** Simile a `SCANNER_COOLDOWN_ACTIVE`, evitare reset immediati se c'è un margine temporale per completare transazioni residue.
4.  **Miglioramento della Condizione di Transizione:** Riformulare la logica `keep_cashless_active` in modo da essere più esplicita e leggibile, separando le condizioni di sessione attiva da quelle di configurazione.
5.  **Sicurezza nei Reset a Fine Programma:** Garantire che un reset non venga inviato se esiste ancora una sessione "virtuale" o temporanea aperta nel codice (es. logica VCD).

### 2.2 Nuove Variabili e Flag Proposti

Aggiungere i seguenti flag strutturali alla definizione `static` in `main/tasks.c`:

| Variabile | Tipo | Scopo |
|-----------|------|-------|
| `mdb_session_valid` | `bool` | Indica che il dispositivo è in una sessione di pagamento attiva e non deve essere resettato. (Analogia a `scanner_cooldown_active`). |
| `mdb_reset_pending` | `bool` | Bandiera temporale per l'invio del reset al termine del ciclo, gestito tramite un timeout. (Simile alla logica di riattivazione dello scanner). |
| `mdb_last_reset_time_ms` | `uint32_t` | Timestamp dell'ultimo reset effettuato, utile per loggare e debuggarne il timing. |

### 2.3 Gestione della Transizione RUNNING <-> IDLE
Attualmente la FSM esegue un "Reset" sia all'avvio che al termine del programma. Questo può essere ridondante.

#### Cambiamento Proposto:
- **Inizio Programma:** Impostare `mdb_session_valid = false` e invio immediato di reset se non sessione aperta, ma solo dopo un piccolo ritardo (simulando un cooldown hardware necessario al MDB per resettarsi).  
- **Fine Programma:** 
  - Se `keep_cashless_active == true`: Nessun reset.
  - Altrimenti: Verificare `mdb_session_valid`. Se è falso, impostare `mdb_reset_pending = true` e attendere il `MDB_RESET_TIMEOUT_MS` prima di inviare comando di reset.  
- **Gestione Reset Timeout:** Implementare un semplice timer per gestire il timeout di reset (se non sessione aperta) simile a come lo scanner gestisce il cooldown post-lettura.

### 2.4 Gestione della Sessione Aperta (`Session Lock`) 

#### Proposta:
Introdurre una transizione esplicita `MDB_LOCKED_SESSION`:

| Stato FSM Corrente | Trigger | Azione Proposta |
|-------------|--------|---------
| `CREDIT` | Evento di rilevamento NFC card valida (confermata autenticazione) | Passaggio a `MDB_RUNNING` (non resetta). Imposta `mdb_session_valid = true`. |
| `RUNNING/PAUSED` | Rilevamento card continua (`session_open`) | Mantiene stato `RUNNING`, nessuna azione di reset. |
| `IDLE` | Fine sessione NFC / Timeout sessione | Passaggio a `CREDIT`. Aziona gestione timeout per il reset (`mdb_reset_pending`). |

### 2.5 Logica di Reset Ottimizzata
Riformulare la condizione di reset in modo esplicito:
```c
// Nuovamente definita in modo più leggibile
bool should_reset_mdb = (
    fsm.state == FSM_STATE_CREDIT && fsm.session_source != FSM_SESSION_SOURCE_CARD  // No sessione card aperta
) || (!fsm.session_open && !hardware_session_open); // Nessuna sessione hardware attiva
```

### 2.6 Gestione della Configurazione (Configuration)
Aggiungere commenti e documentazione nella struttura `cfg->mdb` per chiarire:
- **`cashless_en`**: Abilita il modulo cashless.
- **`session_timeout_ms`** (Proposta): Timeout per considerare chiusa una sessione se non ci sono transazioni attive.  

---

## 3. Dettagli delle Modifiche al Codice (`main/tasks.c`)

### 3.1 Aggiunta Nuove Variabili Statiche

Inserire nella sezione `static` all'inizio di `main/tasks.c`:
```c
// --- MDB Session Control Flags ---
static bool mdb_session_valid = false;
static bool mdb_reset_pending = false;
static uint32_t mdb_last_reset_time_ms = 0;
static const uint32_t MDB_RESET_TIMEOUT_MS = 1500; // Timeout per il reset di sicurezza al termine del ciclo
```

### 3.2 Modifica Logica `fsm_task()` - Blocco Controllo MDB

#### Inizializzazione e Reset a Inizio Programma (Running/Paused)
```c
if (active_program_session && !mdb_session_valid) {
    // Evitare reset immediato se sessione NFC è in corso (es. transazione residua)
    if (!device[0].session_open) {
        mdb_reset_pending = true;
        // Invio comando di reset hardware
        // mdb_cashless_reset_device(0);
        //mdb_last_reset_time_ms = time_us_32();
    }
}
```

#### Gestione Timeout Reset a Fine Programma (Exit Program)
```c
if (left_program_state && mdb_reset_pending) {
    if (!device[0].session_open && !fsm.session_open) {
        // Timeout superato? Se sì, invia reset.
        // mdb_cashless_reset_device(0);
        //mdb_last_reset_time_ms = time_us_32();
        //mdb_reset_pending = false;  // Reset completato
    }
}
```

#### Gestione Sessione Attiva (Session Lock)
```c
if (device[0].session_open || hardware_session_open) {
    mdb_session_valid = true;
    mdb_reset_pending = false; // Reset non necessario se sessione attiva
}
```

### 3.3 Gestione Timeout del Reset MDB 

Creare un timer periodico (simile a `scanner_cooldown_tick`) per monitorare il timeout di reset:
```c
// In una task periodica o FSM tick
if (mdb_reset_pending && (time_ms() - mdb_last_reset_time_ms) >= MDB_RESET_TIMEOUT_MS) {
    if (!device[0].session_open && !fsm.session_open) {
        // Invio comando di reset hardware
        // mdb_cashless_reset_device(0);
        //mdb_reset_pending = false;
    }
}
```

---

## 4. Tabella Comparativa — Comportamento Pre/Post Razionalizzazione

| Condizione | Comportamento Attuale | Comportamento Post-Razionalizzazione |
|---------|---------------|-------|
| **Inizio Programma** | Reset immediato se `!session_open` | Reset posticipato di ~150ms (simulando cooldown hardware). Imposta `mdb_reset_pending`. |
| **Fine Programma** | Reset immediato se `!keep_cashless_active` | Verifica `mdb_session_valid` e timeout. Reset sicuro solo dopo verifica sessione chiuda. |
| **Sessione NFC in Corso** | Nessun reset (eccezione) | Nessuno reset; stato esplicito `mdb_session_valid = true`. |
| **Timeout Sessione NFC** | Nessuna gestione esplicita (possibile comportamento errato se sessione "virtuale" residua) | Gestione timeout con variabile `session_timeout_ms` (proposto). Se sessione virtuale non gestita, reset sicuro dopo 1.5s. |

---

## 5. Implementazione Dettagliata dei File Coinvolti

### 5.1 `main/tasks.c`
- **Aggiungere:** Nuove variabili `static` (come descritto in 3.1).
- **Modificare:** Logica FSM nel blocco `fsm_task()` per gestire il reset pending e il lock sessione.
- **Creare:** Timer periodico per monitorare timeout del reset (opzionale: usare task dedicata o timer nella stessa FSM).

### 5.2 `components/mdb/mdb.c` (Driver MDB)
Aggiornare commenti e documentazioni per chiarire:
```c
/**
 * @brief Invia comando di reset hardware al dispositivo MDB.
 *
 * @param device_idx Indice del dispositivo su bus I2C.
 *
 * Note:
 * - Questo comando forza il dispositivo a reiniziarsi (comando `MDB_RESET_DEVICE`).  
 * - Se una sessione è in corso (`device[idx].session_open == true`), il reset è evitato
 *   per non interrompere la sessione di pagamento attiva.
 */
void mdb_cashless_reset_device(uint8_t device_idx) {
    // Implementazione esistente...
}
```

### 5.3 `data/tasks.json`
Nessuna modifica necessaria, a meno che non si voglia aggiungere nuove configurazioni per il timeout MDB (`session_timeout_ms`).

---

## 6. Verifica della Robustezza del Flusso MDB (Post-Implementazione)

Dopo l'implementazione, eseguire i seguenti test:

1. **Test di Transizione Programmi Rapid Successivi:** 
   - Avviare un programma e stopparlo immediatamente.
   - Avviare un secondo programma a distanza breve.
   - Verificare che il reset del primo programma non interferisca con l'avvio del secondo (nessun "reset stuck").  

2. **Test di Sessione NFC Residua:** 
   - Inserire un tag NFC in una sessione attiva e terminare il programma.
   - Verificare che il sistema non invii reset immediati se la sessione è ancora aperta logicamente (`fsm.session_open`).

3. **Test Timeout ResetMDB:**
   - Simulare chiusura di sessione NFC a fine programma.
   - Misurare tempo tra stop-programma e comando di reset. Verificare che rispetta `MDB_RESET_TIMEOUT_MS`.  

4. **Test Interruzione Hardware del Bus I2C:**
   - Scollegare cavo I2C MDB in corso di programma.
   - Riavviare sistema o terminare programma.
   - Verificare gestione errore e ripristino (reset corretto al rientro OOS o IDLE).

---

## 7. Conclusioni e Raccomandazioni

La razionalizzazione proposta per il modulo MDB segue le stesse best practice applicate allo Scanner QR:
- **Chiarezza negli Stati:** Definizione esplicita di stati di sessione (`mdb_session_valid`).
- **Gestione Timeout di Reset:** Introducendo un periodo di sicurezza tra stop-programma e reset, simile al cooldown dello scanner.
- **Sicurezza nei Reset:** Evitare reset immediati se c'è una sessione attiva (logica o hardware).
- **Documentazione Dettagliata:** Aggiunta di commenti nel codice e nel `tasks.json` per chiarezza futura.

L'implementazione migliorata garantirà:
- Maggiore affidabilità delle transazioni NFC durante i cicli di programma.
- Riduzione di anomalie dovute a reset non necessari o troppo rapidi.
- Maggiore chiarezza nella gestione degli stati nel codice.

---

**Prossimi Step:**
1. Autorizzazione dell'utente per procedere con l'applicazione delle modifiche in `main/tasks.c`.
2. Implementazione del timer di timeout reset (`MDB_RESET_TIMEOUT_MS`).
3. Aggiornamento della documentazione tecnica dopo la revisione del codice (se necessario).

---

*Documento generato automaticamente dall'AI assistant per supporto al team MicroHard.*
