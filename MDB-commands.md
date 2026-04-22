# MDB Cashless - Sequenza comandi e FSM

## 1. Indirizzo device cashless
- `MDB_CASHLESS_DEVICE_ADDR_1 = 0x10`
- Il comando trasmesso sul bus è `address | command`.
- La prima parte del byte è sempre l'indirizzo del dispositivo, la seconda è il tipo di comando.

## 2. Struttura del messaggio MDB cashless
- Byte 0: indirizzo + comando.
- Byte 1..N: payload specifico del comando.
- Per i comandi che includono un payload, il primo byte del payload è spesso un subcomando.
- I comandi con risposta di 1 byte ricevono semplicemente ACK/NAK; gli altri usano CRC check e parsing del payload.

## 3. Fasi principali della connessione cashless
I valori reali usati nell'implementazione sono quelli dell'enum `mdb_device_state_t`:
- `MDB_STATE_INACTIVE = 0`
- `MDB_STATE_INIT_RESET = 1`
- `MDB_STATE_INIT_SETUP = 2`
- `MDB_STATE_INIT_EXPANSION = 3`
- `MDB_STATE_INIT_ENABLE = 4`
- `MDB_STATE_IDLE_POLLING = 5`
- `MDB_STATE_ERROR = 6`

## 4. Sequenza prevista dalla FSM cashless

### 4.1 Inizializzazione reset
- Stato iniziale: `MDB_STATE_INACTIVE = 0`
- Transizione verso: `MDB_STATE_INIT_RESET = 1`
- Azione:
  - invio RESET PK4
  - comando: `0x10` (indirizzo `0x10` + RESET `0x00`)
- Dopo il reset, la FSM invia un POLL di verifica:
  - comando POLL: `0x12` (indirizzo `0x10` + POLL `0x02`)
- Se riceve risposta valida o ACK, passa a `MDB_STATE_INIT_SETUP`.

### 4.2 Setup cashless
- Stato: `MDB_STATE_INIT_SETUP = 2`
- Scopo: ottenere feature level, abilitare capacità di vendita e configurare range prezzi.

#### 4.2.1 Primo SETUP: capacità VMC
- comando: `0x11` (indirizzo `0x10` + SETUP `0x01`)
- payload: `00 02 00 00 00`
  - byte 0: subcommand `0x00` = richiesta standard PK4 setup
  - byte 1: `0x02` = tipo di richiesta VMC capabilities
  - byte 2: `0x00` valore aggiuntivo/riservato
  - byte 3: `0x00` valore aggiuntivo/riservato
  - byte 4: `0x00` valore aggiuntivo/riservato
- Risposta attesa:
  - byte 0 = feature_level
  - byte 7 bit 3 = `cash_sale_support`
  - altre informazioni VMC sono nel payload di risposta.

#### 4.2.2 Secondo SETUP: max/min price
- comando: `0x11` (same)
- payload: `01 FF FF 00 00`
  - byte 0: subcommand `0x01` = richiesta max/min price
  - byte 1-2: max price (`0xFFFF` indica limite massimo o non definito)
  - byte 3-4: min price (`0x0000` = prezzo minimo zero)
- Risposta attesa: ACK singolo (`0x00`), conferma che il lettore ha accettato il range.
- Se ACK ricevuto, il dispositivo imposta `vmc_setup_done = true`.

### 4.3 Expansion / REQUEST_ID
- Stato: `MDB_STATE_INIT_EXPANSION = 3`
- Scopo: ottenere informazioni dettagliate sul dispositivo cashless.
- Azione:
  - invio comando: `0x17` (indirizzo `0x10` + EXPANSION `0x07`)
  - payload: `s_pk4_request_id_payload`

#### 4.3.1 Payload REQUEST_ID PK4
- `0x00` = subcommand `REQUEST_ID`
- `4D 48 49` = "MHI" (manufacturer code)
- `30 30 30 30 30 30 30 30 30 30 30 31` = "0000000001" (model number)
- `45 53 50 33 32 2D 50 34 2D 56 4D 43` = "ESP32-P4-VMC" (product/version string)
- `00 01` = versione del payload / codice secondario
- Messaggio completo in esadecimale:
  - `00 4D 48 49 30 30 30 30 30 30 30 30 31 45 53 50 33 32 2D 50 34 2D 56 4D 43 00 01`
- Il lettore risponde con `REQUEST_ID` e dettagli del proprio hardware/software.
- Quando `expansion_read == true`, la FSM passa a `MDB_STATE_INIT_ENABLE`.

### 4.4 Enable
- Stato: `MDB_STATE_INIT_ENABLE = 4`
- Scopo: attivare il dispositivo cashless per la vendita.
- Azione:
  - invio comando: `0x14` (indirizzo `0x10` + ENABLE `0x04`)
  - payload: `01`
    - byte 0: `0x01` indica richiesta di abilitazione standard.
- Risposta attesa: `ACK` o un payload valido.
- Se OK, il dispositivo imposta `enabled_status = true` e la FSM passa a `MDB_STATE_IDLE_POLLING`.

### 4.5 Polling operativo
- Stato: `MDB_STATE_IDLE_POLLING = 5`
- Il ciclo di polling verifica prima i comandi pendenti:
  - Vend request (`MDB_CASHLESS_VEND_REQUEST`)
  - Vend success (`MDB_CASHLESS_VEND_SUCCESS`)
  - Session complete (`MDB_CASHLESS_VEND_SESSION_COMPLETE`)
  - Revalue request / revalue limit (`MDB_CASHLESS_CMD_REVALUE`)
- Se non ci sono richieste, invia un normale `POLL`.
- Risposta `MDB_ACK` (0x00) significa nessun evento nuovo.
- Risposta diversa indica un evento cashless da gestire.

### 4.6 Error recovery
- Se la risposta è `MDB_CASHLESS_RESP_JUST_RESET` -> la FSM ridiscende a `MDB_STATE_INIT_SETUP = 2`.
- Se la risposta è `MDB_CASHLESS_RESP_OUT_OF_SEQUENCE` -> torna a `MDB_STATE_INIT_RESET = 1`.
- Se setup fallisce ripetutamente, la FSM entra in `MDB_STATE_ERROR = 6`.

## 5. Comandi cashless usati in polling operativo

### 5.1 VEND_REQUEST
- comando: `0x13` (indirizzo `0x10` + VEND `0x03`)
- payload: `00 <amount_hi> <amount_lo> <item_hi> <item_lo>`
  - byte 0: subcommand `0x00` = vend request
  - byte 1-2: prezzo richiesto in centesimi (`amount_cents`)
  - byte 3-4: item number in word16
- Significato:
  - `amount_hi/amount_lo` = importo da addebitare
  - `item_hi/item_lo` = codice del prodotto o `0xFFFF` se non specificato
- Esempio: `13 00 01 F4 00 05` = richiesta vend da 500 centesimi per item 0x0005.

### 5.2 VEND_SUCCESS
- comando: `0x13`
- payload: `02 FF FF`
  - byte 0: subcommand `0x02` = vend success
  - byte 1-2: sempre `0xFF FF` per indicare "conferma di pagamento" senza un valore specifico trasmesso dal master
- Questo comando viene inviato dopo che il sistema ha autorizzato il prodotto.

### 5.3 VEND_SESSION_COMPLETE
- comando: `0x13`
- payload: `04`
  - byte 0: subcommand `0x04` = session complete
- Usato per chiudere la sessione cashless quando la transazione è terminata.

### 5.4 REVALUE_REQUEST
- comando: `0x15` (indirizzo `0x10` + REVALUE `0x05`)
- payload: `00 <amount_hi> <amount_lo>`
  - byte 0: subcommand `0x00` = revalue request
  - byte 1-2: importo da ricaricare in centesimi
- Esempio: `15 00 00 C8` = richiesta di ricarica 200 cent.

### 5.5 REVALUE_LIMIT_REQUEST
- comando: `0x15`
- payload: `01`
  - byte 0: subcommand `0x01` = richiesta limite di ricarica
- Chiede al lettore il limite massimo di revalue disponibile.

### 5.6 POLL operativo
- comando: `0x12` (indirizzo `0x10` + POLL `0x02`)
- payload: nessuno
- Significato: verifica se è presente un nuovo evento sulla linea cashless.

## 6. Comandi di supporto e payload esatti

### 6.1 RESET PK4
- `0x10`
  - byte 0: `0x10` = address `0x10` + RESET `0x00`
  - nessun payload

### 6.2 SETUP PK4 VMC capabilities
- `0x11 00 02 00 00 00`
  - byte 0: `0x11` = address `0x10` + SETUP `0x01`
  - byte 1: `0x00` = subcommand request setup
  - byte 2: `0x02` = richiesta capacità VMC
  - byte 3-4-5: riservati/zero

### 6.3 SETUP PK4 max/min price
- `0x11 01 FF FF 00 00`
  - byte 0: `0x11` = address + SETUP
  - byte 1: `0x01` = subcommand max/min price
  - byte 2-3: `0xFF FF` = max price
  - byte 4-5: `0x00 00` = min price

### 6.4 REQUEST_ID PK4
- `0x17 00 4D 48 49 30 30 30 30 30 30 30 30 31 45 53 50 33 32 2D 50 34 2D 56 4D 43 00 01`
  - byte 0: `0x17` = address `0x10` + EXPANSION `0x07`
  - byte 1: `0x00` = REQUEST_ID subcommand
  - byte 2-4: manufacturer code `MHI`
  - byte 5-16: model number `0000000001`
  - byte 17-28: product string `ESP32-P4-VMC`
  - byte 29-30: payload version `0x00 0x01`

### 6.5 ENABLE
- `0x14 01`
  - byte 0: `0x14` = address `0x10` + ENABLE `0x04`
  - byte 1: `0x01` = abilitazione standard

### 6.6 VEND_REQUEST
- `0x13 00 <amount_hi> <amount_lo> <item_hi> <item_lo>`

### 6.7 VEND_SUCCESS
- `0x13 02 FF FF`

### 6.8 VEND_SESSION_COMPLETE
- `0x13 04`

### 6.9 REVALUE_REQUEST
- `0x15 00 <amount_hi> <amount_lo>`

### 6.10 REVALUE_LIMIT_REQUEST
- `0x15 01`

## 7. Risposte cashless gestite dalla FSM
- `0x00` = `MDB_CASHLESS_RESP_JUST_RESET`
- `0x01` = `MDB_CASHLESS_RESP_READ_CONFIG`
- `0x02` = `MDB_CASHLESS_RESP_DISPLAY_REQUEST`
- `0x03` = `MDB_CASHLESS_RESP_BEGIN_SESSION`
- `0x04` = `MDB_CASHLESS_RESP_SESSION_CANCEL`
- `0x05` = `MDB_CASHLESS_RESP_VEND_APPROVED`
- `0x06` = `MDB_CASHLESS_RESP_VEND_DENIED`
- `0x07` = `MDB_CASHLESS_RESP_END_SESSION`
- `0x09` = `MDB_CASHLESS_RESP_REQUEST_ID`
- `0x0A` = `MDB_CASHLESS_RESP_MALFUNCTION`
- `0x0B` = `MDB_CASHLESS_RESP_OUT_OF_SEQUENCE`
- `0x0D` = `MDB_CASHLESS_RESP_REVALUE_APPROVED`
- `0x0E` = `MDB_CASHLESS_RESP_REVALUE_DENIED`
- `0x0F` = `MDB_CASHLESS_RESP_REVALUE_LIMIT`

## 8. Riepilogo finale: sequenze byte e risposte attese

### 8.1 Inizializzazione cashless
1. RESET PK4
   - invio: `10`
   - atteso: ACK `00` oppure risposta con payload di reset
2. POLL di verifica
   - invio: `12`
   - atteso: `00` = ACK se il lettore è in attesa, oppure risposta evento
3. SETUP VMC capabilities
   - invio: `11 00 02 00 00 00`
   - atteso: risposta payload `feature_level ...` con byte 0 = feature_level, byte 7 bit 3 = cash_sale_support
4. SETUP max/min price
   - invio: `11 01 FF FF 00 00`
   - atteso: ACK `00`
5. REQUEST_ID PK4
   - invio: `17 00 4D 48 49 30 30 30 30 30 30 30 30 31 45 53 50 33 32 2D 50 34 2D 56 4D 43 00 01`
   - atteso: `09 ...` (risposta `REQUEST_ID`) con manufacturer/model/version
6. ENABLE
   - invio: `14 01`
   - atteso: ACK `00` o risposta valida di abilitazione

### 8.2 Polling operativo
- comando ciclico: `12`
  - invio: `12`
  - risposta tipica: `00` = ACK, nessun evento
  - risposta evento reale: qualsiasi byte diverso da `00`, ad esempio
    * `03 ...` = `BEGIN_SESSION`
    * `04` = `SESSION_CANCEL`
    * `05 XX YY` = `VEND_APPROVED` con importo approvato
    * `06` = `VEND_DENIED`
    * `07` = `END_SESSION`
    * `09 ...` = `REQUEST_ID`
    * `0A` = malfunction
    * `0B` = out_of_sequence
    * `0D` = revalue approved
    * `0E` = revalue denied
    * `0F XX YY` = revalue limit
- esempio realistico di response per credito card inserita:
  - ricezione: `03 01 F4 00 00` (BEGIN_SESSION con 500 centesimi)
  - atteso successivo: `13 02 FF FF` per confermare vend success

### 8.3 Sequenza VEND_REQUEST
1. richiesta vend
   - invio: `13 00 <amount_hi> <amount_lo> <item_hi> <item_lo>`
   - esempio: `13 00 01 F4 00 05` = 500 cent per item 0x0005
2. attesa risposta lettore
   - tipicamente: `03 ...` oppure `05 ...` / `06`
3. conferma approvazione (se necessario)
   - invio: `13 02 FF FF`
4. chiusura sessione
   - invio: `13 04`
   - atteso: eventuale `07` = END_SESSION

### 8.4 Sequenza REVALUE
1. richiesta limite di revalue
   - invio: `15 01`
   - atteso: `0F <limit_hi> <limit_lo>`
   - esempio: `0F 03 E8` = limite 1000 centesimi
2. richiesta revalue effettiva
   - invio: `15 00 <amount_hi> <amount_lo>`
   - esempio: `15 00 00 C8` = ricarica 200 centesimi
   - atteso: `0D` = revalue approved oppure `0E` = revalue denied
3. se approvato, lo stato interno passa a `MDB_REVALUE_IN_PROGRESS`

### 8.5 Error handling durante il polling
- `0B` = `OUT_OF_SEQUENCE`
  - effetto: torna a `INIT_RESET = 1`
- `00` in risposta a molti comandi significa ACK e nessun payload speciale
- `0A` = malfunction, registra errore e mantiene la sessione

## 9. Note sul flusso operativo
- Il motore `mdb_engine_run()` viene eseguito ciclicamente (~20 ms).
- `mdb_cashless_sm()` controlla fase, invia comandi e legge risposte.
- Il polling operativo invia `POLL` se non ci sono richieste pendenti.
- In caso di `JUST_RESET` o `OUT_OF_SEQUENCE`, la FSM torna a `INIT_SETUP` o `INIT_RESET`.
- Lo stato `MDB_STATE_ERROR` indica che la procedura di setup ha fallito più volte.
