## Analisi implementazione `TODO.md` punto 5 — Operazioni su EPAPER

### Obiettivo (punto 5)
Implementare su variante **EPAPER_USB** il flusso operativo:

- **attesa credito**
- **visualizzazione credito**
- **scelta programma**
- **esecuzione + autorepeat**
- **saluti**

con le seguenti regole specifiche:

- In attesa credito alternare ogni **2s** i testi: `SCAN CODE` → `INSERT COIN` → `USE NFC` (loop).
- Quando arrivano crediti mostrare in alto `CREDITO` e il **numero credito** in carattere ~**100px**.
- Il numero credito deve aggiornarsi a ogni incremento credito.
- Alla scelta programma mostrare:
  - in alto **nome programma** (carattere ~**50px**, *su una sola riga*, troncato)
  - sotto i **secondi rimanenti**
- Passare a **tema invertito** in `preFineCiclo` e ripristinare tema normale a inizio autorepeat o fine ciclo.
- Cambio programma durante RUN come sulle versioni LCD: aggiornare nome e tempo.
- A fine programma mostrare `Grazie!` per **3s** poi tornare:
  - a attesa credito/scelta se credito finito
  - oppure a visualizzazione credito se credito ancora presente.

### Vincoli di variante
- La variante target è **`DEVICE_DISPLAY_TYPE_EPAPER_USB`**.
- In questa variante la porta CDC è usata come **trasporto display ePaper** (protocollo `Serial_protocol.md`).
- Lo “scanner Newland” USB **non è presente** e deve rimanere disattivato in EPAPER.
- I flussi UI devono dipendere da:
  - **condizioni runtime** (`cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_USB`)
  - non da “presenza fisica” dello scanner.

### Protocollo display (riferimento)
Il protocollo è descritto in `fw_epaper/docs/Serial_protocol.md`.

- **INIT**: `0x00 0xAA`
- **KEEPALIVE**: `0x00 0xAF` (obbligatorio: resetta timer OOS)
- **FULL REFRESH**: `0x00 0x00`
- **Tema**: normale `0x00 0x04`, invertito `0x00 0x05`
- **Testo base (auto layout)**: `0x01 LEN <ASCII...>`
- **Testo esteso**: `0x01 0xFF font# x y <ASCII...> 0x00`
- **Modificatori a inizio testo**: `§` (clear area/refresh parziale), `ç` (bold), `£` (Montserrat)

### Font disponibili (tabella)
Da `Serial_protocol.md`:

- **Font 1–3 (Montserrat)**: 14pt, 28pt, 48pt  
- **Font 4–11 (GoogleSans)**: 10pt, 15pt, 20pt, 28pt, 40pt, 60pt, 100pt, 140pt  
- **Font 12–15 (GoogleSansBold)**: 40pt, 60pt, 100pt, 140pt  

Implicazioni per il `TODO.md`:
- **Credito 100px bold**: usare **font#14** (GoogleSansBold 100pt).
- **Label “CREDITO” 70 bold**: **70pt non esiste**; scelta consigliata:
  - **font#13** (GoogleSansBold 60pt) oppure
  - font#12 (40pt) se serve più spazio/layout.
- **Nome programma ~50px**: **50pt non esiste**; scelta consigliata:
  - font#3 (Montserrat 48pt) per “simil-50”, oppure
  - font#8 (GoogleSans 40pt) se il nome è spesso lungo.

### Architettura proposta (minimo impatto, separata dal trasporto)
Oggi il trasporto EPAPER è in `components/usb_cdc_epaper/` e viene agganciato quando la CDC viene aperta (in `usb_cdc_scanner`).

Per il punto 5 serve un layer “UI EPAPER” separato dal trasporto, ad esempio:

- **Nuovo componente**: `components/epaper_ui/`
  - dipende da `usb_cdc_epaper`
  - espone API tipo:
    - `epaper_ui_on_fsm_snapshot(const fsm_snapshot_t*)` oppure `epaper_ui_tick(...)`
    - `epaper_ui_set_state_*()` per ADS/CREDIT/RUNNING/PAUSED/OOS
  - mantiene lo stato “last rendered” per evitare spam di comandi.

Motivo: `usb_cdc_epaper` deve restare solo “porta seriale + keepalive + send_raw”, mentre la logica del flusso (punto 5) è applicativa.

### Mappatura FSM → schermate EPAPER
Riferimenti runtime:
- stati FSM: `FSM_STATE_IDLE`, `FSM_STATE_ADS`, `FSM_STATE_CREDIT`, `FSM_STATE_RUNNING`, `FSM_STATE_PAUSED`, `FSM_STATE_OUT_OF_SERVICE` (in `main/fsm.h`)
- evento “prefine ciclo” già previsto come `ACTION_ID_PROGRAM_PREFINE_CYCLO` (in `main/fsm.h`)

#### 1) Attesa credito (IDLE/ADS, credito = 0)
Comportamento:
- Loop testi ogni 2s: `SCAN CODE`, `INSERT COIN`, `USE NFC`

Implementazione:
- Timer/contatore interno in `epaper_ui` (tick ogni 100–200ms, cambio frame ogni 2000ms).
- Invio testo con formato esteso (font#9 = 60pt) o base “auto” con modificatore `§` per clear:
  - preferibile **esteso** per posizionare e controllare dimensione.
- Rate-limit: inviare solo quando cambia il frame, non a ogni tick.

Nota: lo “SCAN CODE” iniziale oggi è demo (`usb_cdc_epaper_demo_send_scan_code()`); il punto 5 richiede 3 messaggi, non 2.

#### 2) Visualizzazione credito (CREDIT, credito > 0)
Layout:
- in alto: `CREDITO` (bold, font#13 consigliato) centrato o in alto a sinistra
- numero credito grande: font#14 (100pt bold)

Trigger update:
- ad ogni variazione `credit_cents`/credit “coin” nel runtime snapshot della FSM.
- quando si torna da RUN/PAUSE a CREDIT e il credito residuo è > 0.

Regole:
- full refresh non necessario ad ogni update; usare `§` per clear area testo prima del nuovo numero (comportamento già descritto dal protocollo).

#### 3) Scelta programma (transizione CREDIT→RUNNING via PROGRAM_SELECTED)
All’ingresso RUN:
- testo in alto: **nome programma** su una riga (truncate hard, senza wrap)
- sotto: **secondi rimanenti**

Aggiornamento durante RUN:
- decremento secondi: aggiornare solo la riga del tempo (idealmente testo separato e con `§` per clear area della riga).
- cambio programma (evento `FSM_INPUT_EVENT_PROGRAM_SWITCH`): aggiornare nome + tempo.

#### 4) PreFineCiclo (tema invertito)
Quando entra la condizione `preFineCiclo`:
- inviare `0x00 0x05` (invertito)

Quando esce (inizio autorepeat o fine ciclo):
- inviare `0x00 0x04` (normale)

Nota: per evitare flicker/spam inviare il comando tema solo su cambio booleano.

#### 5) Saluti fine programma
Alla fine di ogni programma:
- mostrare `Grazie!` per 3 secondi
- poi:
  - se credito == 0 → torna al loop “attesa credito”
  - se credito > 0 → torna alla schermata credito

Implementazione:
- stato interno `EPAPER_UI_STATE_THANKS` con deadline `now+3000ms`.
- durante i 3s sospendere gli update di countdown/credito.

### Scheduling / anti-spam (fondamentale su EPAPER)
Regole consigliate nel layer `epaper_ui`:
- **debounce**: non inviare due volte lo stesso pacchetto (memcmp su payload oppure hash).
- **rate limit**: per countdown max 1Hz; per alternanza testo 0.5Hz (2s).
- **sequenza init**:
  - dopo open CDC: `INIT` → breve delay → `FULL_REFRESH` → avvio UI
  - keepalive 1Hz sempre attivo (già gestito da `usb_cdc_epaper`).

### Test plan (diagnostica)
- Log `INIT: #### EPAPER_USB TX: KEEPALIVE ...` deve essere continuo (1Hz).
- Log in `epaper_ui`: per ogni “screen change” loggare `#### EPAPER_UI state=...`.
- Verifica manuale:
  - senza credito: alternanza 3 testi ogni 2s
  - inserimento credito: aggiornamento numero immediato
  - start programma: nome+timer
  - prefine: tema invertito
  - fine programma: “Grazie!” 3s e ritorno corretto

