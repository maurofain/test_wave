Ricavo il contenuto del file MDB.Cashless e analizzo il flusso di pagamento NFC e reset transazione.

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless)

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless)

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless)

Searched for regex `session_open|present|begin_session|end_session|vend|credit|revalue|request_program_vend|confirm_vend_success|cashless|tag|detect` (`/home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless/*.{c,h}`), no results

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless/mdb.h)

Sto cercando le parti rilevanti del codice cashless in mdb.c e `cashless.c` con grep.

Ran terminal command:  cd /home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless && grep -nE 'CashLess|cashless|Vend|Session|present|credit|begin_session|end_session|revalue|request' *.c *.h | cat

Read [](file:///home/mauro/.config/Code/User/workspaceStorage/180108c32333b1ed283f579f9c2f1fdc/GitHub.copilot-chat/chat-session-resources/763bb8fb-4c93-41b7-b286-724ffb117640/call_YdsHJrPoZzMz3dyooN1wYBcU__vscode-1776240729196/content.txt#1-1), lines 1 to 200

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless/cashless.c)

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless/cashless.c)

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless/cashless.c)

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless/cashless.c)

Ran terminal command:  grep -n 'MDBCashLessVend\|MDBCashLessPoll\|MDBCashLessRevalue\|MDBCashLessEnable' cashless.c | cat

Read [](file:///home/mauro/Progetti/0.Clienti/MicroHard/docs/cashless/MDB.Cashless/cashless.c)

## Report analisi `docs/cashless/MDB.Cashless`

### File analizzati
- cashless.c
- mdb.h

---

## 1. Come viene gestito il rilevamento del tag NFC / inizio sessione cashless

Nel codice la “detect” dell’inserimento del tag NFC / cashless viene gestita in `MDBCashLessEngine()` via polling MDB:

- `MDBCashLessEngine()` esegue `MDBCashLessPoll(device)`.
- Se arriva un pacchetto valido, viene elaborato il primo byte di risposta (`abRX[0]`) nel `switch`.
- L’inizio della sessione è riconosciuto da:
  - `case RESP_BEGIN_SESSION:`
- Azioni eseguite allora:
  - `CashLess[device].SessionOpen = TRUE;`
  - `CashLess[device].Credit = (abRX[1]<<8) + abRX[2];`
  - `CashLess[device].KeyNum` e `KeyType` vengono impostati in base a `FeatureLevel` e `Payment Type`
  - `CashLess[device].TimerSessionBegin = RITTicksGet();`

  Quindi:
- `SessionOpen` è il principale flag di sessione aperta.
- `Credit` contiene il saldo disponibile iniziale del tag/NFC.
- La presenza fisica del dispositivo e del tag viene implicitamente rilevata da questa risposta.

---

## 2. Come viene gestita la transazione e la riduzione del credito

La logica di transazione è divisa in due parti:

### 2.1 Preparazione della vendita
In `MDBCashLessEngine()`:

- Se `CashLess[device].SessionOpen && CashLess[device].VendStatus == VEND_PENDING`:
  - viene inviato `MDBCashLessVend(device, VEND_REQUEST_CODE)`
  - il prezzo richiesto è preso da `CashLess[device].RequestPrice`
  - dopo invio si imposta `VendStatus = VEND_WORKING`

### 2.2 Risposta del lettore
Le risposte cashless nel `switch` gestiscono:

- `RESP_VEND_APPROVED`
  - imposta `CashLess[device].VendStatus = VEND_APPROVED`
  - legge `ApprovedPrice`
  - chiama `MDBCashLessVend(device, VEND_SUCCESS_CODE)`
  - quindi manda `VEND_SESSION_COMPLETE_CODE` per chiudere sessione

- `RESP_VEND_DENIED` o `RESP_SESSION_CANCEL`
  - imposta `CashLess[device].VendStatus = VEND_DENIED`
  - invia `VEND_SESSION_COMPLETE_CODE`
  - conclude la sessione senza vendere

- `RESP_END_SESSION`
  - imposta `CashLess[device].SessionOpen = FALSE`
  - segna la fine della sessione

### 2.3 Stato del credito
Importante:
- `CashLess[device].Credit` viene impostato solo in `RESP_BEGIN_SESSION`.
- Non c’è codice che decrementa esplicitamente `CashLess[device].Credit` dopo `RESP_VEND_APPROVED` o `VEND_SUCCESS_CODE`.
- Quindi la “riduzione del credito” NON è gestita localmente in questo modulo come aggiornamento del campo `Credit`.
- Il modulo registra invece `ApprovedPrice` e chiude la sessione.

Questo significa che:
- la logica di saldo residuo sembra delegata al device MDB stesso,
- oppure il codice manca di un aggiornamento esplicito del campo `Credit` dopo la transazione.

---

## 3. Completamento della sessione e chiusura

La fine della sessione cashless è gestita in più modi:

- `RESP_END_SESSION`
  - sessione conclusa normalmente
  - `SessionOpen = FALSE`

- `RESP_SESSION_CANCEL`
  - sessione annullata
  - viene inviato `VEND_SESSION_COMPLETE_CODE` per chiudere

- `RESP_VEND_DENIED`
  - transazione rifiutata
  - chiusura sessione con `VEND_SESSION_COMPLETE_CODE`

- Timeout da lato codice:
  - se `CashLess[device].SessionOpen` e la sessione dura più di 180.000 ms:
    - invia `VEND_SESSION_COMPLETE_CODE`

- Completamento forzato da VMC:
  - se `s_SessionComplete` = TRUE
  - invia `VEND_SESSION_COMPLETE_CODE`
  - poi resetta `s_SessionComplete`

---

## 4. Predisposizione a una nuova transazione

Dopo che la sessione è chiusa:

- `CashLess[device].SessionOpen = FALSE`
- l’engine continua a pollare il dispositivo
- se il device è presente ma non completamente configurato:
  - vengono effettuate richieste di expansion/configuration:
    - `MDBCashLessExpansion(device, EXP_REQUEST_ID)`
    - `MDBCashLessExpansion(device, EXP_REQUEST_FEATURE_ENABLE)`
- se il device è presente e abilitato:
  - viene gestita la flag `CashLess[device].Enable` tramite `MDBCashLessEnable(device)`
- questo serve a tenere il dispositivo “pronto” per la prossima sessione/NFC tap

Quindi la predisposizione avviene tramite:
- polling continuo
- ri-setup/expansion quando necessario
- enable/disable solo quando non c’è sessione aperta

---

## 5. Conclusioni chiave

- `RESP_BEGIN_SESSION` è il punto in cui il codice riconosce il tag NFC/inizio sessione.
- `CashLess[device].Credit` viene inizializzato all’inizio della sessione, ma non viene decrementato localmente dopo vendite.
- Il vero decremento del credito sembra basarsi sul protocollo MDB esterno e sulla chiusura della sessione, non su un aggiornamento interno a cashless.c.
- La chiusura e il reset della sessione avvengono attraverso:
  - `RESP_END_SESSION`
  - `RESP_SESSION_CANCEL`
  - `RESP_VEND_DENIED`
  - timeout sessione
  - `s_SessionComplete`
- La gestione della successiva transazione è affidata al polling continuo e alla riconfigurazione/enable del dispositivo.

---

## Note di interesse

- `MDBCashLessVendSuccess()` è implementata in modo molto semplice:
  - imposta `CashLess[CashlessId].VendSold = ApprovedPrice;`
  - commento `TODO: Forzato per avere la vendita OK.`
- Il modulo usa molto `CashLess[device].SessionOpen` come indice di stato:
  - finché è vero, la sessione è attiva e possono essere inviati comandi vend/failure/abort.
- Non è presente un handler esplicito “tag inserito/rimosso” a livello di evento; tutto è gestito dallo stato `SessionOpen` e dai codici di risposta MDB.

Se vuoi, posso ora comparare questo comportamento con l’implementazione attuale in tasks.c/`main/fsm.c` e evidenziare eventuali discrepanze.


# DESCRIZIONE FUNZIONAMENTO MDB
- Al boot viene inizializzato il device MDB per la getione dei TAG NFC
- in condizioni di standby il device resta in attesa di identificare un TAG inserto
- al rilevamento del tag lo legge e determiona il credito (se tag non valido credito =0)
- contabilizza il credito VCF nel sistema
- (1) all'avvio del programma logga l'evento e sottrae il credito dal TAG , loggando l'operazione e il credito residuo
- se il credito risulta sottratto (pagato) allora si esegue il programma di lavaggio
- durante l'esecuzione del programma il lavaggio può essere interrotto dal touch o da un tasto apposito (i due sistemi devono generare lo stesso evento)
- ad ogni autoripetizione del programma si ripete dal punti (1)
- all'interruzione del ciclo (fermando l'autorepeat) o alla fine del credito o in caso di rimozione del TAG il programma si arresta e il TAG viene disaccoppiato, e MDB emette il segnale di rimozione del tag (operazione loggata)
- alla rimozione del tag si logga il resoconto dell'operazione e di resetta il device MDB per essere nuovamente pronto per un TAG (anche lo stesso ripresentato)
- NOTE : nell'esempio in /home/mauro/1P/MicroHard/test_mdb è stato ottenuto il decremento di 1 cent come prova , va utilizato quel metodo anche in test_wave

---

## Verifica corrispondenza con implementazione attuale in test_wave

Di seguito il confronto tra la specifica attesa e il codice attuale del progetto `test_wave`.

### Esito sintetico

- La corrispondenza e' **parziale**.
- Il flusso base MDB/NFC e' presente.
- Restano pero' alcuni scostamenti funzionali importanti rispetto alla specifica desiderata.

### Verifica punto per punto

#### 1. Al boot viene inizializzato il device MDB per la gestione dei TAG NFC

- **Conforme**.
- Il bootstrap MDB viene avviato in `main/init.c` tramite `mdb_init()`.
- L'inizializzazione dello stato cashless avviene in `components/mdb/mdb.c` con `mdb_cashless_init_state()`.

#### 2. In condizioni di standby il device resta in attesa di identificare un TAG inserito

- **Conforme**.
- Dopo reset, setup, expansion ed enable, il device entra nello stato di polling idle (`MDB_STATE_IDLE_POLLING`).
- Questo corrisponde al comportamento atteso di attesa continua del tag.

#### 3. Al rilevamento del tag lo legge e determina il credito (se tag non valido credito = 0)

- **Parzialmente conforme**.
- Il rilevamento del tag avviene con `RESP_BEGIN_SESSION` nel parser cashless.
- Il credito viene letto e assegnato a `device->credit_cents`.
- Viene anche loggato l'evento di tag inserito con il credito letto.
- **Gap:** se il credito letto e' `0`, il codice attuale non propaga un vero evento funzionale verso la FSM, perche' la sincronizzazione del credito viene marcata solo per valori maggiori di zero.
- Quindi il caso “tag non valido / credito zero” e' gestito a livello dati interni, ma non completamente esposto al runtime applicativo.

#### 4. Contabilizza il credito VCF nel sistema

- **Conforme con limite**.
- Il credito letto dal lettore viene sincronizzato verso la FSM come evento `CARD_CREDIT`.
- La FSM lo contabilizza come credito virtuale card.
- **Limite:** questo vale solo per credito maggiore di zero.

#### 5. All'avvio del programma logga l'evento e sottrae il credito dal TAG, loggando operazione e credito residuo

- **Parzialmente conforme**.
- All'avvio del programma il codice:
  - invia `VEND_REQUEST` al lettore MDB,
  - riceve `VEND_APPROVED`,
  - invia `VEND_SUCCESS`,
  - aggiorna il credito locale FSM,
  - logga avvio programma e credito residuo lato FSM.
- **Gap principale:** il residuo del **TAG** non viene aggiornato in modo esplicito nel driver cashless locale, ne' viene loggato come saldo residuo reale del supporto NFC.
- In pratica oggi viene scalato e loggato il credito della FSM, non il saldo residuo del device/tag come nell'esempio `test_mdb`.

#### 6. Se il credito risulta sottratto (pagato) allora si esegue il programma di lavaggio

- **Conforme**.
- Il programma parte solo dopo approvazione MDB e dopo il buon esito della sequenza di addebito locale.

#### 7. Ad ogni autoripetizione del programma si ripete dal punto (1)

- **Parzialmente conforme**.
- In base al comportamento osservato a runtime, l'autoripetizione risulta ora funzionante: i cicli successivi vengono eseguiti e il VCD viene decrementato correttamente sul display.
- **Gap principale:** i cicli successivi non vengono loggati in modo coerente come il primo avvio programma, anche se a schermo risultano effettivamente eseguiti.
- Di conseguenza il comportamento macchina appare corretto lato utente, ma la tracciabilita' diagnostica e il logging del rinnovo automatico non sono ancora allineati alla specifica.

#### 8. All'interruzione del ciclo, alla fine del credito o in caso di rimozione del TAG il programma si arresta e il TAG viene disaccoppiato, e MDB emette il segnale di rimozione del tag

- **Conforme in larga parte**.
- Il programma viene arrestato su fine credito / stop / rimozione tag.
- La rimozione del tag viene rilevata attraverso la chiusura della sessione cashless (`session_open == false`).
- Il lettore MDB produce e logga i segnali `SESSION_CANCEL` o `END_SESSION`, con log di rimozione tag.

#### 9. Alla rimozione del tag si logga il resoconto e si resetta il device MDB per essere pronto per un nuovo TAG

- **Attualmente non conforme alla lettera della specifica**.
- Il log di rimozione e' presente.
- Pero' il reset forzato immediato del device cashless dopo rimozione tag e' stato rimosso, per evitare loop di log ripetitivi e bip alla reinserzione.
- Oggi il driver lascia il device in chiusura/idle gestita dal polling MDB invece di forzare un reset hardware/logico immediato.
- Questo migliora la stabilita' runtime osservata, ma non coincide con il requisito scritto di reset esplicito ad ogni rimozione.

#### 10. La nota su `test_mdb` con decremento di 1 cent deve essere usata anche in `test_wave`

- **Non conforme**.
- Nell'esempio esterno `test_mdb` esiste una prova esplicita di prelievo di `1` centesimo tramite `VEND_REQUEST` con importo `1` e log del credito residuo calcolato.
- In `test_wave` questa logica di prova non e' stata portata come metodo esplicito di addebito/residuo della card.
- `test_wave` usa la sequenza MDB standard `VEND_REQUEST -> VEND_APPROVED -> VEND_SUCCESS -> SESSION_COMPLETE`, ma non replica il modello di test con prelievo minimo e log del residuo card come nell'esempio.

### Scostamenti principali da chiudere

1. L'autoripetizione risulta funzionante a runtime, ma i cicli successivi non vengono loggati correttamente come il primo ciclo.
2. Il saldo residuo del TAG non viene mantenuto e loggato come nell'esempio `test_mdb`; viene gestito soprattutto il credito locale FSM.
3. Il caso tag con credito zero non viene propagato completamente alla logica applicativa.
4. Il reset esplicito del device MDB alla rimozione del tag non e' piu' eseguito nel punto di rimozione, per evitare bip e log ripetitivi.
5. Il metodo di prova con decremento di 1 cent presente in `test_mdb` non e' stato ancora portato in `test_wave`.

### Conclusione finale

- Il codice attuale `test_wave` implementa correttamente il **bootstrap MDB**, il **polling idle**, il **rilevamento del tag**, la **lettura del credito**, la **sequenza base di vendita MDB** e la **chiusura sessione**.
- Tuttavia non e' ancora pienamente aderente alla specifica funzionale desiderata per quanto riguarda:
  - logging completo dell'autoripetizione con card,
  - decremento/residuo reale del TAG,
  - gestione esplicita del credito zero,
  - reset del device alla rimozione,
  - porting del metodo `1 cent` da `test_mdb`.

### Raccomandazione tecnica

Per allineare `test_wave` alla specifica conviene procedere in quest'ordine:

1. Portare in `test_wave` il metodo di addebito esplicito usato in `test_mdb`, incluso il tracciamento del residuo card.
2. Allineare il logging dei cicli di autoripetizione facendo comparire nei renew successivi gli stessi log significativi del primo avvio programma e dell'addebito credito.
3. Decidere se il reset alla rimozione deve restare **hard reset** come da specifica oppure diventare un **riarmo soft** senza bip, aggiornando in tal caso anche la specifica documentale.

### Roadmap per la correzione del codice

#### Fase 1. Rendere osservabile il comportamento reale

1. Aggiungere log espliciti sul percorso di autoripetizione per distinguere:
  - inizio nuovo ciclo,
  - importo richiesto,
  - credito prima/dopo il rinnovo,
  - esito del rinnovo.
2. Uniformare i log dei cicli successivi a quelli gia' presenti sul primo avvio programma.
3. Verificare che ogni autorenew generi anche il corrispondente evento di pagamento e il relativo log HTTP/applicativo.

#### Fase 2. Allineare il credito locale con il credito reale del TAG

1. Portare da `test_mdb` il metodo di addebito esplicito del lettore, incluso il caso di prova con prelievo minimo.
2. Introdurre il tracciamento del residuo card/tag dopo ogni `VEND_SUCCESS` o completamento transazione.
3. Loggare in modo distinto:
  - credito locale FSM,
  - credito residuo reale del TAG, se disponibile dal protocollo o da calcolo coerente con la sessione.

#### Fase 3. Gestire i casi limite della sessione NFC

1. Esporre correttamente alla FSM anche il caso di tag letto con credito `0`.
2. Distinguere nei log i casi:
  - tag valido con credito,
  - tag valido senza credito,
  - tag rimosso,
  - sessione annullata dal lettore.
3. Evitare duplicazioni di log ripetitivi su `SESSION_CANCEL` e `END_SESSION`, mantenendo pero' un solo evento diagnostico significativo.

#### Fase 4. Stabilizzare la chiusura sessione e il riarmo del lettore

1. Definire se il requisito corretto sia:
  - reset hard del device ad ogni rimozione,
  - oppure riarmo soft senza reset completo per evitare bip e reinit indesiderate.
2. Se resta richiesto il reset esplicito, implementarlo in modo controllato per non generare loop di setup o segnali acustici anomali.
3. Se si adotta il riarmo soft, aggiornare la documentazione funzionale per riflettere il comportamento effettivo desiderato.

#### Fase 5. Validazione finale su macchina

1. Testare un ciclo completo con:
  - inserimento tag,
  - avvio primo programma,
  - uno o piu' rinnovi automatici,
  - stop manuale,
  - rimozione tag,
  - reinserimento stesso tag.
2. Verificare che i log raccontino correttamente tutti i passaggi visibili a display.
3. Confermare assenza di bip anomali, reset ripetitivi o mancati log nei cicli successivi.

### Checklist tecnica con file e funzioni da modificare

#### Checklist Fase 1. Logging e osservabilita' dei renew

1. `main/fsm.c`
  - `fsm_try_autorenew_running_program()`
  - `fsm_publish_autorenew_payment_event()`
  - Obiettivo: aggiungere log espliciti su inizio renew, costo ciclo, credito prima/dopo e motivo di eventuale stop del renew.
2. `main/tasks.c`
  - blocco che logga `AVVIO PROGRAMMA`
  - `publish_program_payment_event()`
  - Obiettivo: emettere anche per i cicli successivi lo stesso set di log del primo ciclo e rendere visibile il pagamento associato al renew.
3. `components/http_services/http_services.c`
  - solo verifica del tracciamento lato payment se necessario
  - Obiettivo: confermare che ogni renew visibile a display abbia anche un evento di pagamento/log coerente.

#### Checklist Fase 2. Addebito reale MDB e residuo TAG

1. `main/fsm.c`
  - ramo `FSM_INPUT_EVENT_PROGRAM_SELECTED` per sessione `CARD`
  - ramo `FSM_INPUT_EVENT_CARD_VEND_APPROVED`
  - Obiettivo: separare chiaramente l'addebito locale FSM dall'addebito confermato MDB e preparare il calcolo del residuo carta.
2. `main/tasks.c`
  - `tasks_request_card_vend()`
  - `tasks_request_card_vend_success()`
  - Obiettivo: uniformare richiesta/importo/item number alla logica portata da `test_mdb`.
3. `components/mdb/mdb.c`
  - `mdb_cashless_sm()`
  - Obiettivo: verificare la sequenza `VEND_REQUEST -> VEND_SUCCESS -> SESSION_COMPLETE` e inserire eventuali hook/log per il residuo transazione.
4. `components/mdb/mdb_cashless.c`
  - `mdb_cashless_parse_begin_session()`
  - `mdb_cashless_handle_poll_response()`
  - Obiettivo: mantenere il credito iniziale di sessione, il credito aggiornato e un eventuale residuo calcolato/loggato a fine transazione.
5. Riferimento esterno
  - `/home/mauro/1P/MicroHard/test_mdb/main/main.c`
  - Obiettivo: portare il metodo di prelievo esplicito e il modello di log del residuo usato nell'esempio.

#### Checklist Fase 3. Gestione tag con credito zero e casi limite

1. `components/mdb/mdb_cashless.c`
  - `mdb_cashless_parse_begin_session()`
  - `mdb_cashless_get_pending_credit_event()`
  - `mdb_cashless_ack_pending_credit_event()`
  - Obiettivo: propagare anche il caso credito `0` come stato significativo e non solo come assenza di credito sincronizzato.
2. `components/mdb/mdb.c`
  - `mdb_cashless_dispatch_pending_credit()`
  - Obiettivo: definire se il runtime debba ricevere un evento esplicito di tag presente ma saldo zero.
3. `main/tasks.c`
  - `tasks_publish_card_credit_event()`
  - Obiettivo: differenziare i casi `credito > 0`, `credito = 0`, `tag letto ma non pagabile`.

#### Checklist Fase 4. Rimozione tag, reset e riarmo lettore

1. `main/tasks.c`
  - `tasks_is_card_session_removed()`
  - blocco di gestione rimozione tag nel loop principale
  - Obiettivo: definire esattamente quando considerare chiusa la sessione e quando pubblicare `CREDIT_ENDED`.
2. `components/mdb/mdb_cashless.c`
  - `mdb_cashless_handle_poll_response()`
  - Obiettivo: consolidare i casi `SESSION_CANCEL` e `END_SESSION`, evitando log ripetuti ma mantenendo un solo evento diagnostico chiaro.
3. `components/mdb/mdb.c`
  - `mdb_cashless_sm()` negli stati `MDB_STATE_INIT_RESET`, `MDB_STATE_INIT_ENABLE`, `MDB_STATE_IDLE_POLLING`
  - Obiettivo: decidere se il riarmo dopo rimozione debba passare da reset completo o da ritorno soft a idle polling.
4. Decisione progettuale
  - Se si conferma il reset hard, implementarlo senza generare loop di setup o bip anomali.
  - Se si conferma il riarmo soft, aggiornare il documento funzionale e l'analisi tecnica.

#### Checklist Fase 5. Validazione e collaudo

1. `docs/analisi sorgent MH.md`
  - aggiornare esito finale a valle delle modifiche.
2. Verifica su macchina reale
  - tag con credito valido
  - tag con credito zero
  - primo ciclo
  - renew successivi
  - stop manuale
  - rimozione tag
  - reinserimento dello stesso tag
3. Criteri di accettazione
  - il display deve mostrare il decremento corretto del VCD
  - ogni ciclo eseguito deve avere log coerenti
  - il lettore non deve produrre bip anomali alla reinizializzazione o reinserzione
  - la chiusura sessione deve risultare stabile e non duplicata nei log

