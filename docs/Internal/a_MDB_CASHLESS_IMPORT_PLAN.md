# Piano Integrazione MDB Cashless NFC

## Obiettivo

Integrare in `test_wave` il supporto MDB cashless per token e card NFC precaricati usando due fonti distinte:

- `/home/mauro/1P/MicroHard/test_mdb`, utile per ricostruire la sequenza corretta dei comandi MDB cashless
- `/home/mauro/1P/MicroHard/docs/cashless/MDB.Cashless`, utile per individuare funzioni mature gia usate nello storico MicroHard

L'integrazione va fatta senza copiare il flusso demo di `test_mdb` dentro `main/main.c`, ma innestandola nell'architettura gia operativa di `test_wave`:

- task manager centralizzato
- FSM applicativa con mailbox eventi
- gestione crediti ECD/VCD
- UI LVGL/Web gia collegate allo stato runtime
- configurazione persistente in `device_config`

L'obiettivo finale non e simulare una vending machine classica, ma supportare questo flusso reale:

- il dispositivo NFC espone un credito precaricato
- il terminale legge questo credito e lo rende disponibile come VCD nella FSM
- quando l'utente avvia un programma, il valore usato viene scalato dal credito NFC
- il backend deve avere una utility per ricaricare il gettone/card NFC tramite funzioni di revalue
- l'evento economico inviato a `http_services` usa `paymenttype=CASHL`
- verso il cloud non vanno inviati identificativi NFC: si comunica solo l'importo della transazione

## Sorgenti analizzati

### Progetto sorgente `test_mdb`

- `/home/mauro/1P/MicroHard/test_mdb/main/main.c`
- `/home/mauro/1P/MicroHard/test_mdb/components/mdb/include/mdb.h`
- `/home/mauro/1P/MicroHard/test_mdb/components/mdb/mdb.c`
- `/home/mauro/1P/MicroHard/test_mdb/docs/PK4_MDB_TEST.md`

### Sorgente storico `docs/cashless`

- `/home/mauro/1P/MicroHard/docs/cashless/MDB.Cashless/mdb.h`
- `/home/mauro/1P/MicroHard/docs/cashless/MDB.Cashless/mdb.c`
- `/home/mauro/1P/MicroHard/docs/cashless/MDB.Cashless/cashless.c`
- `/home/mauro/1P/MicroHard/docs/cashless/InitMDBCashless.pdf`
- `/home/mauro/1P/MicroHard/docs/cashless/MDB.V.4.3.pdf`

### Progetto target `test_wave`

- `/home/mauro/1P/MicroHard/test_wave/main/fsm.h`
- `/home/mauro/1P/MicroHard/test_wave/main/fsm.c`
- `/home/mauro/1P/MicroHard/test_wave/main/tasks.c`
- `/home/mauro/1P/MicroHard/test_wave/main/init.c`
- `/home/mauro/1P/MicroHard/test_wave/components/mdb/include/mdb.h`
- `/home/mauro/1P/MicroHard/test_wave/components/mdb/mdb.c`
- `/home/mauro/1P/MicroHard/test_wave/components/device_config/include/device_config.h`
- `/home/mauro/1P/MicroHard/test_wave/docs/a_FSM.md`

## Vincoli funzionali aggiornati

Le regole da rispettare sono queste:

- `test_mdb` vale come riferimento per la sequenza dei comandi MDB cashless, non per il flusso operativo applicativo
- il lettore NFC e una sorgente di credito precaricato, non una semplice autorizzazione one-shot di pagamento carta
- il credito letto dal supporto NFC deve essere convertito in VCD disponibile nella FSM di `test_wave`
- l'utilizzo dei programmi deve ridurre il valore memorizzato sul supporto NFC tramite flusso cashless coerente col protocollo
- il backend deve esporre una utility per ricaricare il supporto NFC
- per il server cloud, la transazione va pubblicata come `CASHL`
- nel payload cloud non vanno passati `key number`, seriali carta o altri identificativi NFC; solo l'importo economico della transazione

## Stato attuale delle sorgenti

### Cosa esiste gia in `test_mdb`

`test_mdb` contiene una demo funzionale orientata al cashless MDB con queste parti utili:

- sessione cashless locale in `main/main.c`
- parser delle risposte cashless `BEGIN_SESSION`, `SESSION_CANCEL`, `VEND_APPROVED`, `VEND_DENIED`, `END_SESSION`
- helper per i comandi VEND (`REQUEST`, `SUCCESS`, `SESSION_COMPLETE`)
- diagnostica dettagliata delle risposte cashless
- sequenza pratica di test con il lettore PK4

In pratica `test_mdb` dimostra che il dialogo con il lettore esiste davvero, ma la logica applicativa non va presa come modello finale:

- loop unico in `app_main()`
- nessuna FSM applicativa separata
- nessuna integrazione con crediti/programma/UI di `test_wave`
- selezione articolo simulata tramite pulsante BOOT
- gestione pensata per test di protocollo, non per il ciclo operativo di `test_wave`

### Cosa aggiunge il sorgente storico `docs/cashless`

Il sorgente storico contiene una base piu aderente al dominio MicroHard rispetto a `test_mdb`.

Le informazioni piu importanti sono:

- gestione di due device cashless (`0x10` e `0x60`)
- polling engine cashless separato dal resto del bus MDB
- classificazione `CreditCard`, `LoyaltyCard`, `UserCard`, `FreeVendCard`
- lettura del credito disponibile da `BEGIN_SESSION`
- gestione `VEND_REQUEST`, `VEND_SUCCESS`, `VEND_FAILURE`, `VEND_SESSION_COMPLETE`
- supporto `REVALUE_REQUEST` e `REVALUE_LIMIT_REQUEST`, utile per la ricarica NFC
- supporto `CASH_SALE`, utile solo come riferimento protocollo, non necessariamente come flusso principale
- getter dedicati per credito, tipo chiave, stato vend, revalue e presenza device

Questo sorgente conferma che, nello storico MicroHard, il cashless era gia usato anche come credito utente e ricarica, non solo come POS bancario.

### Cosa esiste gia in `test_wave`

`test_wave` ha gia l'infrastruttura corretta per un'integrazione pulita:

- FSM applicativa completa in `main/fsm.c`
- mailbox eventi con sorgente `AGN_ID_MDB`
- supporto a `FSM_INPUT_EVENT_CARD_CREDIT`
- API `tasks_publish_card_credit_event(...)` che accredita VCD verso la FSM
- configurazione persistente `cfg->mdb.cashless_en`
- init MDB in `main/init.c` e task wrapper `mdb_engine_wrapper()` in `main/tasks.c`

Il driver MDB di `test_wave` pero oggi usa solo la gettoniera:

- la macchina a stati attiva e `mdb_coin_sm()`
- `mdb_engine_run()` polla solo coin changer
- lo stato pubblico `mdb_status_t` non espone il cashless
- nessun evento cashless viene inoltrato alla FSM

## Gap reale da colmare

Il lavoro da importare non e il driver UART MDB, che esiste gia, ma il layer di sessione cashless e la sua integrazione con il credito NFC.

### 1. Manca il layer di sessione cashless

In `test_mdb` la sessione e modellata da:

- `SESSION_IDLE`
- `SESSION_OPEN`
- `SESSION_ARTICLE_SELECTED`
- `SESSION_VEND_REQUESTED`
- `SESSION_VEND_APPROVED`
- `SESSION_ENDING`

In `test_wave` non esiste un modulo equivalente. Ci sono tipi storici in `components/mdb/include/mdb.h` come `ST_MDB_CASHLESS`, `MDB_VEND_STATUS` e `MDB_REVALUE_STATUS`, ma non sono collegati a un flusso runtime completo.

### 2. Manca il polling cashless in parallelo alla gettoniera

`mdb_engine_run()` esegue solo:

- `mdb_coin_sm()`
- eventuali placeholder futuri per bill validator

Non esiste ancora:

- `mdb_cashless_sm()`
- handshake reset/setup/expansion/enable per il lettore cashless
- poll del device cashless con parsing risposta

### 3. Manca il bridge tra MDB cashless e FSM applicativa

La parte importante in `test_wave` esiste gia:

- `tasks_publish_card_credit_event(...)`
- `FSM_INPUT_EVENT_CARD_CREDIT`
- gestione `FSM_SESSION_SOURCE_CARD`
- bucket VCD e sessione `VIRTUAL_LOCKED`

Quello che manca e collegare il lettore cashless MDB a questo ponte nel modo corretto per il credito prepagato.

### 4. Manca il flusso reale NFC prepagato

`test_wave` non deve copiare il modello demo di `test_mdb` basato su articoli e pulsante BOOT.

Il flusso reale richiesto e invece:

- identificazione del credito disponibile sul token/card NFC
- accredito del valore come VCD nel runtime FSM
- decremento del saldo NFC quando il programma viene realmente consumato
- supporto ricarica via utility backend
- pubblicazione contabile verso `http_services` come `CASHL`

### 5. Stato e diagnostica MDB incompleti lato UI

Oggi `test_wave` mostra sostanzialmente lo stato MDB della gettoniera. Per l'integrazione cashless serve esporre anche:

- presenza lettore cashless
- stato sessione cashless
- ultimo evento ricevuto
- ultimo errore protocollo
- importo disponibile/approvato
- ultimo revalue richiesto/approvato

## Architettura consigliata

La scelta corretta non e copiare `main/main.c` di `test_mdb`, ma combinare:

- la sequenza comandi confermata da `test_mdb`
- le API storiche gia orientate a credito e ricarica in `docs/cashless/MDB.Cashless`

Il tutto in un modulo dedicato che viva sotto il componente `mdb` o in un componente ausiliario dedicato.

### Opzione consigliata

Creare un nuovo modulo dedicato al cashless, per esempio:

- `components/mdb/include/mdb_cashless.h`
- `components/mdb/mdb_cashless.c`

Motivi:

- evita di gonfiare `mdb.c` con logica di sessione applicativa
- separa driver bus e business flow cashless
- rende piu semplice testare il parser delle risposte
- permette di mantenere leggibile la macchina a stati coin gia esistente

### Responsabilita dei moduli

`mdb.c`:

- init UART MDB
- invio/ricezione pacchetti MDB
- polling engine
- discovery/stato periferiche MDB
- dispatch grezzo delle risposte cashless

`mdb_cashless.c`:

- stato sessione cashless
- parser delle risposte cashless
- costruzione dei comandi VEND
- costruzione dei comandi REVALUE
- timeout di sessione
- callback o eventi verso `tasks.c`
- API di servizio per lettura saldo, decremento saldo e ricarica

`tasks.c`:

- bridge verso mailbox FSM
- pubblicazione `CARD_CREDIT`
- API di servizio per la utility backend di ricarica
- eventuale inoltro di stato/errore verso UI o log runtime

`fsm.c`:

- nessuna riscrittura architetturale
- solo consumo dell'evento `CARD_CREDIT` gia esistente
- eventuale estensione, se necessario, per distinguere meglio saldo disponibile, saldo consumato e saldo confermato dal lettore

## Integrazione con la FSM di test_wave

La FSM di `test_wave` e gia pronta a ricevere credito da card:

- `FSM_INPUT_EVENT_CARD_CREDIT`
- `fsm_prepare_virtual_locked_session(ctx, FSM_SESSION_SOURCE_CARD)`
- `fsm_add_credit_from_cents(..., false, "card_vcd")`
- transizione successiva su `FSM_EVENT_PAYMENT_ACCEPTED`

Quindi la vera integrazione non richiede di importare la FSM di `test_mdb`, ma di tradurre la sessione cashless demo in eventi compatibili con la FSM gia esistente.

### Regola architetturale da rispettare

La FSM demo di `test_mdb` non deve sostituire la FSM di `test_wave`.

Va invece trattata come sotto-FSM periferica del lettore cashless MDB:

- scope limitato al dialogo con il lettore
- nessuna decisione su stati macchina globali
- nessuna logica UI di alto livello
- nessuna selezione programma autonoma

### Mappatura consigliata degli eventi

`BEGIN_SESSION`:

- aggiorna stato cashless interno
- memorizza credito disponibile e tipo supporto
- puo leggere localmente `key number` e `key type` per classificare il supporto, ma questi identificativi non devono uscire verso `http_services`
- rende disponibile il credito come VCD alla FSM o a un buffer dedicato di sessione, secondo il punto esatto in cui si decide di sincronizzare il saldo

`VEND_REQUEST`:

- deve essere generato quando `test_wave` deve scalare dal supporto NFC il costo di un programma reale
- non va simulato con tasto BOOT

`VEND_APPROVED`:

- conferma il decremento del saldo sul supporto NFC
- deve aggiornare il saldo disponibile lato runtime
- deve produrre la registrazione economica verso `http_services` con `paymenttype=CASHL`
- il payload cloud deve contenere solo l'importo e non identificativi NFC

`VEND_DENIED`:

- nessun accredito ulteriore FSM
- log errore
- eventuale messaggio utente
- chiusura o rollback sessione cashless

`REVALUE_REQUEST` / `REVALUE_APPROVED`:

- sono il perno della utility backend di ricarica NFC
- non devono passare dalla FSM programmi
- devono essere esposti come operazione amministrativa separata

`SESSION_CANCEL` / `END_SESSION`:

- pulizia stato cashless locale
- nessuna modifica a crediti gia consumati dalla FSM

## Funzioni mancanti da implementare

### Nel layer cashless

- `mdb_cashless_init_state()`
- `mdb_cashless_reset_session()`
- `mdb_cashless_handle_poll_response(const uint8_t *data, size_t len)`
- `mdb_cashless_sync_credit_to_runtime()`
- `mdb_cashless_send_vend_request(uint16_t amount_cents, uint16_t item_id)`
- `mdb_cashless_send_vend_success(uint16_t approved_cents)`
- `mdb_cashless_send_session_complete()`
- `mdb_cashless_request_revalue_limit()`
- `mdb_cashless_send_revalue(uint16_t amount_cents)`
- `mdb_cashless_handle_begin_session(...)`
- `mdb_cashless_handle_vend_approved(...)`
- `mdb_cashless_handle_vend_denied(...)`
- `mdb_cashless_handle_revalue_approved(...)`
- `mdb_cashless_handle_revalue_denied(...)`
- `mdb_cashless_handle_session_cancel(...)`
- `mdb_cashless_handle_end_session()`
- `mdb_cashless_tick(uint32_t now_ms)`

### Nel polling engine MDB

- `mdb_cashless_sm()`
- setup/enable cashless parallelo a `mdb_coin_sm()`
- routing dei frame cashless al parser
- aggiornamento stato online/offline del lettore cashless

### Nel bridge runtime

- callback da cashless a `tasks_publish_card_credit_event(...)` quando il credito letto dal supporto va reso disponibile come VCD
- callback errore/sessione annullata
- API per richiedere un `vend request` dal runtime applicativo
- API backend per richiedere `revalue` di un gettone/card NFC

### In stato/configurazione

- estensione di `mdb_status_t` con sezione `cashless`
- diagnostica leggibile per UI/web
- coerenza con `cfg->mdb.cashless_en`
- tracciamento separato di `credito_nfc_disponibile`, `saldo_vcd_runtime`, `ultimo_debito`, `ultima_ricarica`

## Funzioni riusabili individuate in `docs/cashless/MDB.Cashless`

Il sorgente storico contiene gia funzioni che conviene mappare o riutilizzare concettualmente nell'implementazione nuova.

### Inizializzazione e motore

- `MDBCashLessInit()`
- `MDBCashLessReset(int device)`
- `MDBCashLessEngine(int device)`

### Lettura stato supporto NFC e sessione

- `MDBCashLessGetPresent(int device)`
- `MDBCashLessGetCredit(int device)`
- `MDBCashLessGetKeyType(int device)`
- `MDBCashLessGetKeyNum(int device)`
- `MDBCashLessGetKeyPriceGroup(int device)`
- `MDBCashLessDeviceType(int device)`
- `MDBCashLessGetFeatureLevel(int device)`
- `MDBCashLessGetEnableStatus(int device)`
- `MDBCashLessGetVendStatus(int device)`
- `MDBCashLessGetVendApprovedPrice(int device)`

### Metadati dispositivo

- `MDBCashLessManuFactCode(int device, char *s)`
- `MDBCashLessModelNumber(int device, char *s)`
- `MDBCashLessManuFactVer(int device)`
- `MDBCashLessTotalErgVer(int device, char *s)`
- `MDBCashLessIngCB2Ver(int device, char *s)`

### Comandi di vendita e decremento saldo

- `MDBCashLessSetPrice(int device, int value_vend)`
- `MDBCashLessSetVend(int device, int price)`
- `MDBCashLessVendSuccess(int cashless_id, int value_sold)`
- `MDBCashLessVendAbort(int device)`
- `MDBCashLessVendFailure(int device)`
- `MDBCashLessSetSessionComplete()`

### Comandi di ricarica

- `MDBCashLessSetRevalue(int device, int revalue)`
- `MDBCashLessSetRevalueLimit(int device)`
- `MDBCashLessGetRevalueStatus(int device)`
- `MDBCashLessGetRevalueLimit(int device)`
- `MDBCashLessGetRevalueApprovedPrice(int device)`

### Funzioni ausiliarie

- `MDBCashLessEnableDevice(int device, int enable)`
- `MDBCashLessGetCashSaleSupport(int device)`
- `MDBCashLessCashSaleItemPrice(WORD item_price)`
- `MDBCashLessCashSaleItemNumber(WORD item_number)`
- `MDBCashLessSetDisplayFunction(void (*fun)(const char *))`

### Nota di progetto

Le funzioni che espongono `KeyNum` o dati identificativi del media NFC vanno considerate solo per logica locale o debug bus. Non devono essere propagate nei payload server o nelle API backend esterne se il requisito resta "solo importo transazione".

## Piano operativo consigliato

### Fase 1. Estrarre la sequenza protocollare corretta

Portare da `test_mdb/main/main.c` solo queste parti:

- enum stato sessione
- struct sessione
- parser risposta cashless
- helper `send_vend_command(...)`
- handler `BEGIN_SESSION`, `VEND_APPROVED`, `VEND_DENIED`, `SESSION_CANCEL`, `END_SESSION`

Usare invece `docs/cashless/MDB.Cashless` per guidare:

- naming delle API cashless
- distinzione tra lettura credito, vendita e revalue
- gestione device multipli `0x10` e `0x60`
- comandi di ricarica backend

Da non importare cosi come sono:

- loop demo in `app_main()`
- selezione articolo da pulsante BOOT
- logica di test manuale integrata nel main

### Fase 2. Completare il componente MDB di test_wave

In `components/mdb` aggiungere il supporto cashless vero:

- handshake di inizializzazione cashless
- polling cashless
- stato online/offline cashless
- dispatch eventi verso il parser sessione

Questa fase deve chiudere il vuoto attuale per cui l'MDB di `test_wave` e presente ma usa solo la gettoniera.

### Fase 3. Collegare il cashless alla FSM e al saldo NFC

Usare l'hook gia pronto:

- `tasks_publish_card_credit_event(...)`

Il flusso da ottenere e questo:

1. il lettore apre sessione e comunica il credito presente sul supporto NFC
2. il runtime rende quel valore disponibile come VCD
3. la FSM usa il VCD per l'avvio programma
4. al consumo reale del programma il layer cashless invia `VEND_REQUEST`
5. il lettore risponde `VEND_APPROVED` o `VEND_DENIED`
6. su `VEND_APPROVED` il saldo NFC viene considerato decrementato e viene registrato il pagamento come `CASHL`

### Fase 4. Confermare l'addebito programma tramite `VEND_REQUEST`

Questa fase sposta il punto di verita economico dal solo runtime FSM alla conferma del lettore MDB cashless.

Oggi il credito letto da `BEGIN_SESSION` viene gia pubblicato verso la FSM come `CARD_CREDIT` e trasformato in VCD disponibile per la sessione `FSM_SESSION_SOURCE_CARD`.

Il punto ancora da completare e che, al momento della selezione programma, la FSM scala subito il credito locale. Questo comportamento va corretto per le sessioni card: il decremento effettivo deve avvenire solo dopo risposta positiva del lettore cashless.

Flusso richiesto:

1. il lettore NFC apre la sessione e comunica il credito con `BEGIN_SESSION`
2. il bridge runtime pubblica `CARD_CREDIT` e la FSM rende il saldo disponibile come VCD
3. l'utente seleziona un programma mentre la sessione resta `FSM_SESSION_SOURCE_CARD`
4. il runtime non deve ancora scalare il VCD locale, ma deve memorizzare una richiesta pendente di avvio programma
5. il layer cashless invia `VEND_REQUEST` con l'importo del programma
6. il lettore risponde `VEND_APPROVED` o `VEND_DENIED`
7. solo su `VEND_APPROVED` il runtime scala il credito locale, avvia il programma e pubblica l'evento contabile verso `http_services`
8. su `VEND_DENIED` il programma non parte, il credito locale resta invariato e non viene inviato nessun pagamento cloud
9. a fine ciclo il layer cashless chiude correttamente la sessione con il comando di completamento previsto dal protocollo

Impatto architetturale concreto:

- `components/mdb/mdb_cashless.c` deve estendere il modello runtime con un evento pending per l'esito vend, analogo al pending gia introdotto per la sincronizzazione credito
- `components/mdb/mdb.c` deve esporre un bridge runtime per notificare `VEND_APPROVED` e `VEND_DENIED` al livello applicativo
- `main/fsm.c` non deve piu eseguire l'addebito immediato per le sessioni `FSM_SESSION_SOURCE_CARD`, ma deve attendere la conferma MDB prima di scalare `vcd_coins`, `vcd_used` e `credit_cents`
- `main/fsm.h` va esteso con lo stato minimo necessario a tracciare una richiesta vend pendente: programma richiesto, importo richiesto, device cashless attivo e stato autorizzazione
- `main/tasks.c` deve fare da ponte tra esito MDB e FSM, mantenendo invariata la contabilizzazione finale verso `http_services` che gia mappa `FSM_SESSION_SOURCE_CARD` in `paymenttype=CASHL`

Regole di comportamento:

- il credito locale non deve piu essere considerato autorevole da solo per l'avvio programma nelle sessioni card
- il programma non deve partire su sola disponibilita di VCD se manca la conferma del lettore MDB
- il pagamento cloud non deve essere pubblicato prima di `VEND_APPROVED`
- l'importo contabilizzato deve riflettere il prezzo effettivamente approvato dal lettore, quando disponibile

Obiettivo della fase:

- allineare saldo NFC, saldo VCD nel runtime e contabilizzazione `CASHL` in un unico flusso coerente
- evitare divergenze tra credito locale e supporto fisico
- usare il lettore cashless come autorita finale sull'addebito, non solo come sorgente iniziale del credito

Implementazione attuale in `test_wave`:

- il componente `mdb` espone ora un bridge runtime per richiedere `VEND_REQUEST`, `VEND_SUCCESS` e `SESSION_COMPLETE` sul device cashless attivo
- il componente `mdb_cashless` genera un evento pending quando riceve `VEND_APPROVED` o `VEND_DENIED`
- `tasks.c` traduce l'esito vend in eventi FSM dedicati, mantenendo il bridge separato dal polling MDB
- `fsm.c` non scala piu il credito locale al `PROGRAM_SELECTED` nelle sessioni `FSM_SESSION_SOURCE_CARD`, ma attende `CARD_VEND_APPROVED`
- il programma viene avviato solo dopo approvazione MDB e il pagamento continua a essere contabilizzato come `CASHL`
- il rinnovo automatico locale e disabilitato per le sessioni card per evitare addebiti fuori protocollo MDB
- alla fine del ciclo la FSM richiede `SESSION_COMPLETE` al layer cashless

### Fase 5. Aggiungere la utility backend di ricarica NFC

Il backend locale deve offrire una utility amministrativa per ricaricare il supporto NFC.

Scope minimo consigliato:

- endpoint o pagina di servizio dedicata
- visualizzazione presenza lettore e stato sessione
- richiesta limite di ricarica tramite `MDBCashLessSetRevalueLimit()`
- invio ricarica tramite `MDBCashLessSetRevalue()`
- lettura esito con `MDBCashLessGetRevalueStatus()` e `MDBCashLessGetRevalueApprovedPrice()`

Questo flusso non deve dipendere dalla FSM programmi: e una funzione di manutenzione e servizio.

Implementazione minima attuale in `test_wave`:

- stato cashless/revalue esposto in `/status` dentro il blocco `mdb`
- endpoint locale `GET /api/mdb/cashless/status` per leggere stato reader, sessione, credito, limite ricarica ed esito revalue
- endpoint locale `POST /api/mdb/cashless/revalue_limit` per richiedere il limite massimo di ricarica del supporto NFC attivo
- endpoint locale `POST /api/mdb/cashless/revalue` con body JSON `{ "amount_cents": <valore> }` per richiedere la ricarica
- il layer `mdb` instrada queste richieste verso il device cashless attivo usando i comandi `REVALUE_LIMIT_REQUEST` e `REVALUE_REQUEST`
- nessun identificativo NFC viene esposto o inoltrato al cloud in questo flusso

Nota operativa:

- questa prima utility backend e pensata per uso locale/amministrativo via HTTP o test tools
- la UI dedicata puo essere aggiunta in un passaggio successivo senza cambiare le API runtime gia esposte

## Ordine di implementazione consigliato

1. Esporre in `mdb_status_t` lo stato cashless e la presenza lettore.
2. Aggiungere `mdb_cashless_sm()` e il polling base del lettore cashless.
3. Portare il parser delle risposte da `test_mdb` nel nuovo modulo cashless.
4. Sincronizzare il credito letto dal supporto NFC con il VCD runtime.
5. Aggiungere una API runtime per richiedere un `VEND_REQUEST` con importo in centesimi al momento del consumo programma.
6. Collegare la registrazione contabile a `http_services` usando `paymenttype=CASHL` e senza identificativi NFC.
7. Aggiungere la utility backend di ricarica tramite `REVALUE`.
8. Esporre diagnostica cashless in UI/home/log.
9. Eseguire test hardware e prove di regressione con la gettoniera.

## Rischi tecnici da tenere presenti

### 1. Collisione tra demo cashless e FSM globale

Il rischio maggiore e importare in blocco la logica di `test_mdb` e introdurre una seconda FSM applicativa in conflitto con quella di `test_wave`.

Mitigazione:

- mantenere la FSM demo confinata al solo componente cashless
- usare solo eventi verso la FSM principale

### 2. Disallineamento tra saldo NFC e VCD runtime

Se il credito viene copiato in VCD ma il decremento su NFC fallisce o avviene in tempi diversi, runtime e supporto fisico divergono.

Mitigazione:

- definire un punto univoco di sincronizzazione iniziale del saldo
- definire un punto univoco di decremento confermato del saldo
- loggare ogni delta di saldo locale e dispositivo

### 3. Gestione timeout e sessioni appese

`test_mdb` usa timestamp e retry semplificati. In `test_wave` servono timeout robusti per:

- sessione aperta senza scelta importo
- vend request senza risposta
- denial/cancel con sessione non chiusa
- revalue avviato ma non concluso

Mitigazione:

- introdurre `mdb_cashless_tick()` e recovery esplicito

### 4. Regressione sulla gettoniera

Il polling cashless non deve rompere il polling coin changer.

Mitigazione:

- polling round-robin leggero
- tempi separati per coin e cashless
- test regressione dedicati

### 5. Violazione del requisito privacy lato cloud

Il sorgente storico usa `KeyNum` e altre informazioni del media; se questi campi finiscono in log, API o payload server si viola il requisito aggiornato.

Mitigazione:

- non propagare identificativi NFC oltre il componente MDB cashless
- usare verso `http_services` solo importo e tipo pagamento `CASHL`

### 6. Stato servizio non rappresentativo

Se la UI continua a leggere solo lo stato coin, il lettore cashless puo essere online ma risultare invisibile all'operatore.

Mitigazione:

- estendere stato e diagnostica separando coin e cashless

## File del progetto `test_wave` da modificare per l'implementazione

Modifiche quasi certe:

- `/home/mauro/1P/MicroHard/test_wave/components/mdb/include/mdb.h`
- `/home/mauro/1P/MicroHard/test_wave/components/mdb/mdb.c`
- `/home/mauro/1P/MicroHard/test_wave/main/tasks.c`
- `/home/mauro/1P/MicroHard/test_wave/components/http_services/http_services.c`

Nuovi file consigliati:

- `/home/mauro/1P/MicroHard/test_wave/components/mdb/include/mdb_cashless.h`
- `/home/mauro/1P/MicroHard/test_wave/components/mdb/mdb_cashless.c`

Modifiche probabili in una seconda fase:

- `/home/mauro/1P/MicroHard/test_wave/components/lvgl_panel/...` per diagnostica cashless
- `/home/mauro/1P/MicroHard/test_wave/components/web_ui/...` per stato, test e utility ricarica NFC
- `/home/mauro/1P/MicroHard/test_wave/docs/a_FSM.md` per documentare il flusso finale
- `/home/mauro/1P/MicroHard/test_wave/docs/a_TASKS.md` se cambia il runtime task

## Conclusione operativa

Il punto 21 aggiornato non richiede di importare un intero progetto, ma di fare quattro cose precise:

1. usare `test_mdb` solo per ricostruire la sequenza dei comandi cashless
2. usare `docs/cashless/MDB.Cashless` come riferimento principale per API credito, vendita e ricarica
3. innestare il layer cashless nel polling engine MDB di `test_wave`
4. sincronizzare credito NFC, VCD runtime, utility di ricarica e contabilizzazione `CASHL` verso `http_services`

Questa strada minimizza il rischio, riusa l'architettura gia buona di `test_wave` e consente un'implementazione rapida senza riscrivere la FSM principale e senza trascinare nel cloud dati identificativi del supporto NFC.

## Implementazione rapida consigliata

Se l'obiettivo immediato e arrivare presto a una prima versione funzionante, il primo sprint dovrebbe fermarsi a questo scope minimo:

- discovery cashless online/offline
- parse `BEGIN_SESSION`
- lettura credito NFC e pubblicazione come VCD
- invio `VEND_REQUEST` sul consumo effettivo programma
- gestione `VEND_APPROVED` e `VEND_DENIED`
- invio a `http_services` con `paymenttype=CASHL`
- chiusura sessione con `VEND_SESSION_COMPLETE`

Solo dopo questa milestone conviene aggiungere:

- revalue
- utility backend di ricarica completa
- UI di diagnostica completa
- recovery avanzato e telemetria
