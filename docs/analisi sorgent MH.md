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
- ad ogni autoripetizione del programma si ripete dal punti (1)
- all'interruzione del ciclo (fermando l'autorepeat) o alla fine del credito o in caso di rimozione del TAG il programma si arresta e il TAG viene disaccoppiato, e MDB emette il segnale di rimozione del tag (operazione loggata)
- alla rimozione del tag si logga il resoconto dell'operazione e di resetta il device MDB per essere nuovamente pronto per un TAG (anche lo stesso ripresentato)
- NOTE : nell'esempio in /home/mauro/1P/MicroHard/test_mdb è stato ottenuto il decremento di 1 cent come prova , va utilizato quel metodo anche in test_wave

