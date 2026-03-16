# Piano tecnico implementazione flusso utente

## Obiettivo

Implementare il **FLUSSO OPERATIVO CLIENTE** descritto in `docs/FLUSSI.md` allineando:

- FSM applicativa
- sorgenti evento pagamento/input
- orchestrazione schermate LVGL
- policy di sessione
- auto-rinnovo programma finché c’è credito
- gestione i18n delle nuove stringhe utente eventualmente introdotte

Questo documento descrive **come implementare** il flusso nel codice esistente, con ordine di intervento, impatti file-by-file, invarianti e punti ancora da chiarire.

---

# 1. Requisiti funzionali da rispettare

## 1.1 Bootstrap cliente

Alla fine dell’inizializzazione devono essere abilitati:

- gettoniera
- QRCode
- MDB / chip card

## 1.2 Ingresso UX iniziale

- se `ads` sono abilitate, il display mostra la schermata ads con pubblicità e pulsante `Scegli programma`
- se `ads` non sono abilitate, il sistema entra direttamente nella schermata scelta programmi
- l’abilitazione ads deve essere configurabile tramite switch **`Attiva ADS`** in `/config -> Identità`

## 1.3 Eventi che sbloccano la scelta programmi

Devono essere riconosciuti come eventi validi di ingresso/sessione:

- touch sul display
- pressione pulsante fisico
- lettura barcode / QR
- inserimento moneta
- lettura chip card

## 1.4 Regole di sessione all’ingresso in scelta programmi

Quando si entra nella scelta programmi:

- i programmi con costo `<= credito disponibile` devono risultare selezionabili
- il sistema deve continuare ad accettare ulteriore credito **solo** se l’ingresso è avvenuto tramite:
  - touch
  - pulsante fisico
  - moneta
- se l’ingresso è avvenuto tramite:
  - QRCode
  - chip card
  allora **non devono essere accettati ulteriori crediti**

## 1.5 Esecuzione programma

Una volta avviato un programma:

- vengono attivati i relay previsti dalla tabella programmi
- il tempo di esecuzione deriva dalla tabella programmi
- il credito viene scalato **prima dal credito effettivo** e **poi dal credito virtuale**
- il programma deve potersi eseguire **per più cicli consecutivi finché c’è credito disponibile**

## 1.6 Cambio programma

Durante l’esecuzione:

- l’utente può selezionare un altro programma
- il sistema deve ricalcolare il tempo residuo in base al rapporto tra:
  - credito residuo
  - costo del nuovo programma
  - durata del nuovo programma

## 1.7 Sospensione

Durante l’esecuzione:

- premendo lo stesso programma attivo si sospende
- premendo un programma qualsiasi si riattiva
- la sospensione è consentita fino al massimo definito nella tabella programmi

## 1.8 Stop

Premendo `STOP`:

- si ferma il programma corrente
- si azzera il tempo rimanente
- se rimane credito effettivo:
  - il sistema resta in scelta programmi per il timeout configurato
  - allo scadere azzera il credito effettivo e torna ad ads o scelta programmi pulita
- se il credito rimanente è solo virtuale:
  - la sessione si considera conclusa
  - il sistema torna ad ads o scelta programmi con credito azzerato

## 1.9 Lingua

In ogni momento deve essere possibile cambiare lingua dalla bandierina in alto a destra.

---

# 2. Stato attuale del codice

## 2.1 Componenti principali già presenti

Il codice attuale dispone già di:

- FSM con stati base in `main/fsm.h` e `main/fsm.c`
- task orchestratore in `main/tasks.c`
- schermata ads in `components/lvgl_panel/lvgl_page_ads.c`
- schermata programmi in `components/lvgl_panel/lvgl_page_programmi.c`
- pagina lingua / selezione lingua in LVGL
- pipeline QR:
  - scanner USB
  - evento verso `HTTP_SERVICES`
  - lookup customer
  - pubblicazione credito verso FSM

## 2.2 Gap principali rispetto alla specifica

I principali gap da colmare sono:

- distinzione non abbastanza esplicita tra `ads`, `scelta programmi`, `running`, `paused`
- mancanza di una **policy di sessione** che distingua:
  - sessione aperta a nuovi pagamenti
  - sessione chiusa a nuovi pagamenti (QR/card)
- auto-rinnovo programma non formalizzato in FSM
- ritorno post-stop non coerente al 100% con la specifica
- produttore evento per pulsante fisico non ancora definito
- semantica di QR e card da allineare meglio al modello `credito effettivo / virtuale`

---

# 3. Modello tecnico proposto

## 3.1 Estensioni al contesto FSM

File principale:

- `main/fsm.h`

Aggiungere nuovi enum e campi al contesto `fsm_ctx_t`.

## 3.2 Nuovi enum proposti

### Stato UX cliente

Aggiungere stati espliciti:

- `FSM_STATE_ADS`
- `FSM_STATE_PROGRAM_SELECT`
- `FSM_STATE_RUNNING`
- `FSM_STATE_PAUSED`

Valutare se mantenere `FSM_STATE_IDLE` solo come stato tecnico di bootstrap.

### Origine sessione

Aggiungere enum tipo:

- `FSM_SESSION_SOURCE_NONE`
- `FSM_SESSION_SOURCE_TOUCH`
- `FSM_SESSION_SOURCE_KEY`
- `FSM_SESSION_SOURCE_COIN`
- `FSM_SESSION_SOURCE_QR`
- `FSM_SESSION_SOURCE_CARD`

### Modalità sessione

Aggiungere enum tipo:

- `FSM_SESSION_MODE_NONE`
- `FSM_SESSION_MODE_OPEN_PAYMENTS`
- `FSM_SESSION_MODE_VIRTUAL_LOCKED`

### Destinazione di ritorno idle

Aggiungere enum tipo:

- `FSM_IDLE_RETURN_ADS`
- `FSM_IDLE_RETURN_PROGRAMS`

## 3.3 Nuovi campi consigliati in `fsm_ctx_t`

Aggiungere campi simili a:

- `fsm_session_mode_t session_mode`
- `fsm_session_source_t session_source`
- `fsm_idle_return_t idle_return`
- `bool ads_enabled`
- `bool pending_credit_clear_on_timeout`
- `bool stop_requested`
- `bool allow_additional_payments`
- `uint32_t select_timeout_ms`
- `uint32_t ads_timeout_ms`
- `uint32_t stop_grace_timeout_ms`

Questi timeout devono essere caricati dalla configurazione applicativa usando come fonte ufficiale:

- `/config -> Timeouts`

## 3.4 Invarianti applicative

Queste regole devono essere vere in qualunque momento:

- **il FSM è la source of truth** del flusso utente, non la pagina LVGL attiva
- se `session_mode == FSM_SESSION_MODE_VIRTUAL_LOCKED`, ulteriori pagamenti devono essere rifiutati o ignorati
- il credito va scalato **prima da effettivo**, poi da virtuale
- lo stop non deve lasciare relay attivi
- la pagina lingua deve essere accessibile in overlay o da chrome, senza alterare in modo improprio la sessione cliente

---

# 4. Strategia di implementazione per file

## 4.1 `main/fsm.h`

### Modifiche

- aggiungere enum nuovi per stati UX, sessione e ritorno idle
- estendere `fsm_ctx_t`
- aggiungere eventuali helper pubblici di snapshot se servono alla UI

### Output atteso

Il contesto FSM deve poter rappresentare esplicitamente:

- pagina/flow corrente
- origine sessione
- policy pagamenti
- politica di ritorno ad ads o program select

---

## 4.2 `main/fsm.c`

Questo è il file principale del refactor.

### 4.2.1 Inizializzazione

Aggiornare `fsm_init()` per inizializzare:

- stato iniziale coerente con il bootstrap tecnico
- campi di sessione
- policy pagamenti
- timeout di default

Aggiungere anche un helper di caricamento timeout/config, ad esempio:

- `fsm_apply_runtime_config(fsm_ctx_t *ctx, const device_config_t *cfg)`

Responsabilità:

- leggere da `/config -> Timeouts` i timeout usati dal flusso cliente
- leggere da config il flag `Attiva ADS`
- aggiornare `ctx->ads_enabled`
- aggiornare i timeout runtime usati da:
  - ads
  - scelta programmi
  - ritorno lingua
  - stop/grace handling se previsto

### 4.2.2 Gestione eventi utente iniziali

In `fsm_handle_input_event()`:

- `TOUCH`
- `KEY`
- `COIN`
- `QR`
- `CARD`

non devono essere trattati tutti allo stesso modo.

Serve una funzione interna tipo:

- `fsm_start_or_update_customer_session()`

che:

- decide lo stato target (`ADS` -> `PROGRAM_SELECT` oppure bootstrap diretto)
- imposta `session_source`
- imposta `session_mode`
- imposta `allow_additional_payments`

### 4.2.3 Policy pagamenti

Aggiungere helper interni tipo:

- `fsm_can_accept_coin(const fsm_ctx_t *ctx)`
- `fsm_can_accept_token(const fsm_ctx_t *ctx)`
- `fsm_can_accept_qr(const fsm_ctx_t *ctx)`
- `fsm_can_accept_card(const fsm_ctx_t *ctx)`

Comportamento:

- in `OPEN_PAYMENTS` i pagamenti sono accettati
- in `VIRTUAL_LOCKED` i nuovi pagamenti devono essere ignorati con log diagnostico

### 4.2.4 Avvio programma

Quando arriva `FSM_INPUT_EVENT_PROGRAM_SELECTED`:

- validare che il programma sia selezionabile con il credito attuale
- impostare `running_program_name`
- impostare `running_price_units`
- impostare `running_target_ms`
- impostare `pause_max_ms`
- scalare credito da `ecd` poi `vcd`
- passare a `RUNNING`

### 4.2.5 Cambio programma

Quando arriva `FSM_INPUT_EVENT_PROGRAM_SWITCH`:

- consentirlo in `RUNNING` e `PAUSED`
- ricalcolare il residuo in base al nuovo rateo
- aggiornare:
  - `running_program_name`
  - `running_price_units`
  - `running_target_ms`
  - `pause_max_ms`

La logica base è già presente nel file, ma va resa parte del flusso standard e usata realmente dalla UI.

### 4.2.6 Pause/resume

Quando arriva `FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE`:

- se `RUNNING` -> `PAUSED`
- se `PAUSED` -> `RUNNING`
- applicare il limite massimo di pausa
- se la pausa eccede `pause_max_ms`, decidere comportamento finale:
  - stop forzato
  - o credito terminato / sessione chiusa

### 4.2.7 Stop

Quando arriva `FSM_INPUT_EVENT_PROGRAM_STOP`:

- fermare esecuzione
- azzerare tempo residuo del ciclo corrente
- decidere il prossimo stato in base al credito residuo:
  - credito effettivo residuo > 0 -> `PROGRAM_SELECT` con timeout
  - solo credito virtuale residuo -> chiusura sessione e ritorno a schermata iniziale

### 4.2.8 Auto-rinnovo programma

Va implementato in `fsm_tick()` o in helper richiamato da `fsm_tick()`.

#### Regola richiesta

Il programma corrente deve continuare automaticamente per ulteriori cicli finché c’è credito disponibile.

#### Logica consigliata

Quando il ciclo corrente termina:

1. verificare se `credit_cents >= running_price_units`
2. se sì:
   - scalare nuovamente credito da `ecd` poi `vcd`
   - riavviare il ciclo dello stesso programma
   - mantenere stato `RUNNING`
3. se no:
   - terminare il programma
   - se rimane credito residuo insufficiente per quel programma:
     - passare a `PROGRAM_SELECT`
   - se non rimane credito:
     - chiudere la sessione secondo policy

#### Nota importante

L’auto-rinnovo deve essere **automatico**, senza richiedere una nuova pressione utente.

### 4.2.9 Gestione timeout scelta programmi

In `fsm_tick()` gestire timeout espliciti per `PROGRAM_SELECT`:

- se c’è credito effettivo residuo e timeout scade:
  - azzerare il credito effettivo
  - chiudere sessione
- se sessione virtuale è conclusa:
  - chiudere sessione
- al termine della chiusura sessione:
  - tornare ad ads se ads abilitate
  - altrimenti a program select pulita o stato iniziale definito

Fonte dei timeout:

- i timeout del flusso devono essere letti da `/config -> Timeouts`
- non introdurre costanti duplicate se esiste già un valore configurabile coerente

---

## 4.3 `main/tasks.c`

Questo file resta il principale orchestratore runtime.

### 4.3.1 Bootstrap UI coerente

Rivedere il comportamento iniziale di `fsm_task()` e del bootstrap complessivo.

Serve che il runtime mostri:

- `ADS` se ads abilitate
- `PROGRAM_SELECT` se ads disabilitate

La decisione deve dipendere dal flag configurabile:

- `/config -> Identità -> Attiva ADS`

### 4.3.2 Bridge stato FSM -> schermata LVGL

Nel task FSM introdurre una funzione dedicata, ad esempio:

- `fsm_apply_ui_transition(prev_state, new_state, ctx)`

Responsabilità:

- `ADS` -> mostra ads page
- `PROGRAM_SELECT` -> mostra programs page
- `RUNNING` / `PAUSED` -> refresh pagina programmi, pulsanti e timer
- chiusura sessione -> ritorno alla pagina prevista

### 4.3.3 Timeout lingua

Oggi il task contiene logica di ritorno lingua. Va verificato che:

- il cambio lingua resti sempre disponibile
- il ritorno automatico alla lingua non interferisca col flusso cliente principale

Se necessario, separare:

- timeout di lingua
- timeout di inattività sessione cliente

Entrambi devono essere letti da `/config -> Timeouts`, evitando logiche hardcoded fuori dalla configurazione.

### 4.3.4 Sorgenti evento hardware

Mappare correttamente verso FSM:

- touchscreen -> `FSM_INPUT_EVENT_TOUCH`
- QR scanner -> `FSM_INPUT_EVENT_QR_SCANNED` e/o credito QR
- card/MDB -> `FSM_INPUT_EVENT_CARD_CREDIT`
- coin acceptor -> `FSM_INPUT_EVENT_COIN`
- pulsante fisico -> `FSM_INPUT_EVENT_KEY`

### 4.3.5 Pulsante fisico

Definire un hook reale o un adapter tipo:

- `publish_customer_key_event(button_id)`

che traduca il pulsante fisico in evento FSM.

---

## 4.4 `main/main.c`

### Modifiche

Rivedere il bootstrap attuale che mostra direttamente ads page.

L’obiettivo è spostare la decisione finale il più possibile sul runtime/FSM, così `main.c` resta limitato a:

- init componenti
- enable canali pagamento
- avvio task

### Responsabilità consigliata di `main.c`

- inizializzare cctalk / scanner / mdb se abilitati
- non diventare la source of truth della pagina iniziale oltre il bootstrap minimo

Il bootstrap non deve più assumere implicitamente che ads siano sempre attive: deve rispettare il flag configurabile `Attiva ADS`.

---

## 4.5 `components/web_ui/webpages_embedded.c` e pagine config web

### Modifiche

Va previsto l’inserimento dello switch:

- **`Attiva ADS`**

nel percorso UI:

- `/config -> Identità`

### Impatti attesi

- aggiornare il form/config page web della sezione `Identità`
- leggere/salvare il valore nel payload configurazione
- riflettere il valore nel backend/config model già esistente
- predisporre eventuali nuove stringhe i18n web se il testo non esiste già

### Obiettivo

Rendere il comportamento iniziale `ads on/off` completamente configurabile da interfaccia.

---

## 4.6 `components/lvgl_panel/lvgl_panel.c`

### Modifiche

Assicurarsi di avere entry point chiari per:

- `lvgl_panel_show_ads_page()`
- `lvgl_panel_show_main_page()` o equivalente programma select
- eventuale `lvgl_panel_refresh_program_runtime()`

### Obiettivo

Separare bene:

- navigazione schermate
- aggiornamento dati runtime

---

## 4.7 `components/lvgl_panel/lvgl_page_ads.c`

### Modifiche

La pagina ads deve:

- mostrare il pulsante `Scegli programma`
- pubblicare evento FSM coerente alla pressione touch/pulsante
- eventualmente considerare il touch generico come evento di uscita da ads

### Punto chiave

L’uscita da ads non deve contenere logica business complessa: deve solo pubblicare l’evento corretto.

---

## 4.8 `components/lvgl_panel/lvgl_page_programmi.c`

Questo file richiede interventi importanti.

### 4.8.1 Selezione programma da fermo

Quando il sistema è in `PROGRAM_SELECT`:

- la pressione su un programma deve pubblicare `FSM_INPUT_EVENT_PROGRAM_SELECTED`

### 4.8.2 Cambio programma a macchina attiva

Quando il sistema è in `RUNNING` o `PAUSED`:

- se l’utente preme **lo stesso programma attivo**:
  - toggle pausa
- se l’utente preme **un programma diverso**:
  - pubblicare `FSM_INPUT_EVENT_PROGRAM_SWITCH`

### 4.8.3 Stop

Il pulsante stop deve:

- pubblicare `FSM_INPUT_EVENT_PROGRAM_STOP`
- aggiornare correttamente la label locale
- non prendere decisioni business sul credito residuo: la decisione deve restare nel FSM

### 4.8.4 Abilitazione/disabilitazione programmi

La UI deve ricevere o derivare dallo snapshot FSM:

- credito disponibile
- programma attivo
- pausa attiva
- sessione bloccata a nuovi pagamenti

In base a questo:

- abilitare i programmi acquistabili
- disabilitare i non acquistabili
- mostrare chiaramente lo stato del programma attivo

### 4.8.5 Auto-rinnovo visuale

Durante auto-rinnovo:

- il timer deve ripartire correttamente
- la UI non deve simulare uno stop/riavvio completo se il programma è lo stesso
- eventuali label devono restare coerenti

---

# 5. Gestione credito

## 5.1 Modello da mantenere

Continuare a mantenere separati:

- `ecd_coins` = credito effettivo
- `vcd_coins` = credito virtuale

## 5.2 Ordine di scalatura

Ogni addebito programma deve:

1. usare prima `ecd_coins`
2. usare poi `vcd_coins`

## 5.3 Regole di sessione

### Sessione open payments

Ingresso da:

- touch
- key
- moneta

Comportamento:

- ulteriori pagamenti accettati

### Sessione virtual locked

Ingresso da:

- QR
- card

Comportamento:

- ulteriori pagamenti rifiutati/ignorati
- il residuo virtuale non deve mantenere aperta una sessione indefinita dopo stop/timeout

---

# 6. Auto-rinnovo programma

## 6.1 Obiettivo

Implementare esplicitamente il requisito:

- **auto-rinnovo del programma finché c’è credito**

## 6.2 Comportamento tecnico

Alla scadenza del ciclo corrente:

- se il credito residuo consente un nuovo ciclo dello stesso programma:
  - scalare un nuovo costo
  - riavviare il timer del programma
  - mantenere relay coerenti
- se il credito residuo non basta:
  - terminare il ciclo corrente
  - tornare a `PROGRAM_SELECT` o chiudere sessione secondo policy

## 6.3 Funzione consigliata

Aggiungere helper interno in `fsm.c`, ad esempio:

- `fsm_try_autorenew_running_program(fsm_ctx_t *ctx)`

Responsabilità:

- verificare sufficienza credito
- addebitare nuovo ciclo
- resettare contatori ciclo
- loggare l’operazione

## 6.4 Invarianti auto-rinnovo

- non deve addebitare due volte lo stesso ciclo
- non deve perdere il `running_program_name`
- non deve cambiare programma implicitamente
- non deve uscire da `RUNNING` se il rinnovo va a buon fine

---

# 7. i18n: stringhe nuove da prevedere

Qualsiasi nuova stringa UI o messaggio utente introdotto durante l’implementazione deve essere prevista in i18n.

## 7.1 LVGL

Per nuove stringhe pannello LVGL usare chiavi sotto `lvgl` con nomi testuali brevi e sensati.

### Possibili nuove stringhe LVGL

- `session_locked_payments`
  - IT: `Pagamento aggiuntivo non consentito`
- `program_autorenew`
  - IT: `Rinnovo automatico`
- `program_cycle_ended`
  - IT: `Ciclo completato`
- `program_credit_residual`
  - IT: `Credito residuo`
- `program_select_title`
  - IT: `Scegli programma`
- `pause_limit_reached`
  - IT: `Tempo di sospensione terminato`
- `virtual_credit_session_ended`
  - IT: `Sessione terminata`
- `effective_credit_timeout`
  - IT: `Credito scaduto`

## 7.2 Web

Se il flusso viene esposto anche via Web UI o emulator:

- inserire eventuali nuove stringhe in `web.<page_name>.<NNN>`
- usare placeholder `{{NNN}}`
- non alterare chiavi esistenti

### Possibili testi web

- `Pagamento aggiuntivo non consentito in questa sessione`
- `Rinnovo automatico programma`
- `Credito insufficiente per un nuovo ciclo`
- `Sessione conclusa`
- `Credito scaduto per inattività`

## 7.3 Regole operative i18n

- verificare sempre che il testo sostituito coincida esattamente con il record i18n
- riusare chiavi esistenti dove possibile
- aggiungere nuove chiavi solo se necessarie
- per le nuove traduzioni, inizialmente copiare il testo italiano nelle altre lingue se questo è il criterio di progetto corrente

---

# 8. Ordine di implementazione consigliato

## Milestone 1 — modello stati/sessione

### File

- `main/fsm.h`
- `main/fsm.c`

### Deliverable

- nuovi stati UX
- nuovi campi sessione
- policy pagamenti
- transizioni principali ads/program select/running/paused

## Milestone 2 — orchestrazione schermate

### File

- `main/tasks.c`
- `components/lvgl_panel/lvgl_panel.c`
- `components/lvgl_panel/lvgl_page_ads.c`

### Deliverable

- bootstrap corretto
- transizioni LVGL guidate dal runtime FSM
- uscita da ads coerente

## Milestone 3 — pagina programmi

### File

- `components/lvgl_panel/lvgl_page_programmi.c`

### Deliverable

- select programma da fermo
- switch programma da running/paused
- toggle pausa corretto
- stop delegato al FSM
- abilitazione/disabilitazione pulsanti coerente

## Milestone 4 — auto-rinnovo

### File

- `main/fsm.c`
- eventuale refresh UI in `lvgl_page_programmi.c`

### Deliverable

- rinnovo automatico stesso programma a fine ciclo
- addebito corretto
- mantenimento stato `RUNNING`

## Milestone 5 — eventi hardware mancanti

### File

- `main/tasks.c`
- eventuali driver GPIO / MDB / cctalk già presenti

### Deliverable

- hook pulsante fisico
- integrazione completa card/MDB/coin nel modello sessione

## Milestone 6 — i18n finale

### File

- `data/i18n_v2.json`
- eventuali file LVGL / web coinvolti

### Deliverable

- tutte le nuove stringhe utente inserite in i18n
- placeholder e lookup corretti

---

# 9. Checklist tecnica di validazione

## FSM

- [ ] da boot entra in ads o program select in base alla config
- [ ] touch da ads apre scelta programmi
- [ ] coin apre scelta programmi e incrementa credito
- [ ] QR apre scelta programmi con sessione bloccata a nuovi pagamenti
- [ ] card apre scelta programmi con sessione bloccata a nuovi pagamenti
- [ ] programma selezionabile solo con credito sufficiente
- [ ] stesso programma attivo -> pausa/resume
- [ ] programma diverso -> switch
- [ ] stop -> decisione corretta in base al tipo di credito residuo
- [ ] timeout program select -> chiusura sessione corretta

## Auto-rinnovo

- [ ] a fine ciclo, se credito basta, parte nuovo ciclo automatico
- [ ] il credito viene scalato una sola volta per ciclo
- [ ] i relay restano coerenti durante il rinnovo
- [ ] la UI mostra timer coerente

## i18n

- [ ] nessuna nuova stringa utente hardcoded se evitabile
- [ ] nuove stringhe LVGL sotto `lvgl`
- [ ] nuove stringhe web sotto sezione `web.<page>` corretta
- [ ] nessuna chiave esistente alterata

---

# 10. Indicazioni mancanti da chiarire

Per completare l’implementazione senza ambiguità mancano alcune decisioni funzionali.

## 10.1 QR: credito effettivo o virtuale?

Decisione presa:

- `QR = credito virtuale bloccante`
- `card = credito virtuale bloccante`
- in queste sessioni **non si accettano ulteriori pagamenti**

Impatto applicativo:

- sessione in `FSM_SESSION_MODE_VIRTUAL_LOCKED`
- scalatura credito dal ramo virtuale
- a fine sessione o stop il residuo virtuale non mantiene aperta la sessione

## 10.2 Ritorno finale senza ads

Decisione presa:

- se `ads` sono disabilitate, a fine sessione il sistema torna a **scelta programmi vuota**

## 10.3 Timeout esatti da usare

Decisione presa:

- i timeout del flusso utente devono essere letti da `/config -> Timeouts`

Applicazione prevista:

- timeout ads
- timeout scelta programmi con credito effettivo residuo
- timeout pausa massima
- timeout ritorno lingua

Regola implementativa:

- riusare i campi già presenti in configurazione quando semanticamente coerenti
- aggiungere nuovi campi in config solo se strettamente necessario

## 10.4 Card / MDB: origine credito

Va confermato come viene pubblicato il credito card nel sistema reale:

- evento già disponibile?
- importo in centesimi / unità?
- card sempre virtuale? **Sì, per decisione funzionale**

## 10.5 Hook pulsante fisico

Decisione presa:

- i pulsanti fisici sono la duplicazione dei pulsanti programmi touch
- hanno la **stessa numerazione** dei programmi mostrati su display
- operano **allo stesso modo** dei pulsanti touch

Impatto implementativo:

- il producer hardware dei tasti fisici dovrà pubblicare gli stessi eventi usati dalla pagina programmi
- se il programma corrisponde al programma attivo:
  - toggle pausa
- se il programma corrisponde a un programma diverso durante `RUNNING` / `PAUSED`:
  - `FSM_INPUT_EVENT_PROGRAM_SWITCH`
- se il sistema è in `PROGRAM_SELECT`:
  - `FSM_INPUT_EVENT_PROGRAM_SELECTED`

## 10.6 Program select senza credito

Decisione presa:

- la schermata programmi può essere mostrata anche con **credito zero**
- i programmi non acquistabili devono risultare **disabilitati**

## 10.7 Auto-rinnovo e switch concorrente

Decisione presa:

- in caso di concorrenza tra auto-rinnovo e pressione utente, **vince l’input utente**

Interpretazione tecnica:

- se nello stesso intervallo utile arriva un comando utente di selezione/switch, il rinnovo automatico non deve partire
- l’FSM deve privilegiare l’evento esplicito dell’utente rispetto al rinnovo implicito

---

# 11. Raccomandazione finale

Procedere in quest’ordine:

1. formalizzare stati/sessioni nel FSM
2. allineare bootstrap e transizioni schermate
3. correggere pagina programmi per switch/pause/stop
4. implementare auto-rinnovo
5. completare hook eventi hardware mancanti
6. chiudere i18n delle nuove stringhe introdotte

La parte più importante da chiarire prima di iniziare il codice resta:

- dettaglio tecnico della sorgente hardware card/MDB
- dettaglio tecnico della sorgente hardware dei pulsanti fisici

---

# 12. Sezione finale: contratti di integrazione hardware da completare

I dettagli hardware restano volutamente demandati a una fase successiva.
Per consentire però l’implementazione immediata del flusso software, vanno definiti da subito dei contratti stabili tra hardware, task e FSM.

## 12.1 Obiettivo

Consentire al software applicativo di procedere senza conoscere ancora:

- GPIO definitivi
- driver finali
- protocollo completo MDB/card
- mapping elettrico dei pulsanti fisici

## 12.2 Principio

Il codice business non deve dipendere dal dettaglio hardware.
I driver o adapter hardware devono tradurre gli ingressi reali in chiamate o eventi applicativi stabili.

## 12.3 Hook/API da prevedere

### Pulsanti fisici programma

Definire una chiamata del tipo:

```c
bool customer_input_publish_program_button(uint8_t program_id);
```

Semantica:

- `program_id` ha la stessa numerazione del pulsante touch programma
- il comportamento deve essere identico al corrispondente pulsante LVGL

Comportamento atteso:

- in `PROGRAM_SELECT`:
  - selezione programma
- in `RUNNING` / `PAUSED`:
  - stesso programma -> pausa/resume
  - programma diverso -> switch programma

### Inserimento moneta / gettone

Definire una chiamata del tipo:

```c
bool customer_payment_publish_coin_credit(int32_t amount_units, const char *source_tag);
```

Semantica:

- pubblica credito effettivo
- apre o aggiorna una sessione `OPEN_PAYMENTS`

### Credito card / MDB

Definire una chiamata del tipo:

```c
bool customer_payment_publish_card_credit(int32_t amount_units, const char *card_id);
```

Semantica:

- pubblica credito virtuale
- apre o aggiorna una sessione `VIRTUAL_LOCKED`
- blocca ulteriori pagamenti

### Lettura QR

Definire una chiamata del tipo:

```c
bool customer_payment_publish_qr_credit(int32_t amount_units, const char *qr_code);
```

Semantica:

- pubblica credito virtuale
- apre o aggiorna una sessione `VIRTUAL_LOCKED`
- blocca ulteriori pagamenti

### User activity generica

Definire una chiamata del tipo:

```c
bool customer_input_publish_user_activity(fsm_session_source_t source);
```

Semantica:

- consente uscita da ads
- consente ingresso in scelta programmi anche senza credito
- non implica da sola un pagamento

## 12.4 Adapter consigliati

Per non legare il flusso ai driver concreti, introdurre adapter intermedi, ad esempio:

- `customer_input_adapter.c`
- `customer_payment_adapter.c`

Responsabilità:

- normalizzare l’input hardware
- validare i parametri minimi
- pubblicare gli eventi FSM corretti

## 12.5 Vincoli

- queste API devono essere pensate come interfacce stabili
- i driver reali potranno cambiare senza modificare la logica del FSM
- LVGL e hardware fisico devono convergere sugli stessi eventi applicativi

## 12.6 Cosa resta da completare dopo

In una fase successiva andranno definiti:

- file sorgente esatti che producono gli eventi hardware
- GPIO o mapping pulsanti
- protocollo e parsing definitivi per MDB/card
- unità esatta del credito pubblicato dai driver
