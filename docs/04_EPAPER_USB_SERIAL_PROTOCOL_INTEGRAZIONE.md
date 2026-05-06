# EPAPER_USB — Implementazione protocollo seriale (USB CDC) al posto di LVGL

## Scopo

Quando `cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_USB` il firmware **non** deve usare LVGL per la UI cliente.  
La UI viene demandata al **modulo ePaper** collegato via **USB CDC** (si presenta come CDC ACM, es. ESP32‑S2 `VID=303A PID=0002`).

In questa modalità:
- il firmware invia al modulo ePaper **pacchetti di comando/testo/LED** secondo il protocollo descritto in `fw_epaper/docs/Serial_protocol.md`;
- il modulo ePaper invia al firmware **codici scanner** (barcode/QR) tramite la stessa seriale CDC;
- il firmware sostituisce le pagine LVGL (`ADS`, `CREDIT`, `RUNNING`, `OOS`) con invii seriali equivalenti.

## Formato pacchetti (riassunto)

Il primo byte determina il tipo:
- `0x00` = **comando**
- `0x01` = **stringa da visualizzare**
- `0x02` = **controllo LED WS2812B** (4 LED, 12 byte RGB)

### `0x00` — comandi

Secondo byte:
- `0xAA` init handshake (finché non arriva, ignorare tutto)
- `0xEE` disconnect (finché non arriva un nuovo `0xAA`, ignorare tutto)
- `0x00` full refresh
- `0x01` attiva scanner + full refresh
- `0x02` scanner ON
- `0x03` scanner OFF
- `0x04` tema normale (bianco/nero)
- `0x05` tema invertito (nero/bianco)
- `0xBB` reboot modulo ePaper
- `0xFF` mostra logo

### `0x01` — testo

**Formato base**
- `0x01`
- `LEN` (max 127)
- ASCII (`CR` ammesso per newline)

**Formato esteso**
- `0x01`
- `0xFF`
- `font#` (1..15)
- `pos_x`
- `pos_y`
- ASCII
- terminatore `0x00`

Modificatori a inizio testo (non stampati / influenzano layout):
- `§` refresh parziale (clear area testo)
- `ç` bold (se disponibile)
- `£` font family Montserrat (se supportata)

### `0x02` — LED

- `0x02`
- 12 byte: `l1r,l1g,l1b,l2r,l2g,l2b,l3r,l3g,l3b,l4r,l4g,l4b`

## Architettura target nel firmware

### 1) Driver “porta EPAPER_USB” sopra `usb_cdc_scanner`

Creare (o estendere) un layer che esponga:
- `epaper_usb_send_cmd(uint8_t cmd)`
- `epaper_usb_send_text_auto(const char *text)` → invio pacchetto `0x01 LEN ...`
- `epaper_usb_send_text_ext(uint8_t font, uint8_t x, uint8_t y, const char *text)` → `0x01 0xFF ... 0x00`
- `epaper_usb_send_led(const uint8_t rgb12[12])`

Trasporto:
- usare `cdc_acm_host_data_tx_blocking()` sul device CDC aperto (stesso handle usato per RX).
- in modalità `EPAPER_USB` **non inviare** comandi Newland (`SCNMOD/SCNENA/...`) sul CDC.

### 2) RX: codici scanner dal modulo ePaper

Il modulo ePaper invia stringhe scanner sulla seriale CDC.  
Nel firmware:
- riusare la pipeline di parse già esistente (es. callback `scanner_on_barcode_cb()`),
- ma con un filtro: in `EPAPER_USB` **non applicare** logiche di “setup Newland” e non interpretare echi comandi come barcode.

### 3) Mapping UI LVGL → comandi ePaper

L’obiettivo è sostituire le chiamate a:
- `lvgl_panel_show_ads_page()`
- `lvgl_panel_show_main_page()`
- `lvgl_panel_show_out_of_service*()`
- box log crediti

con invii al modulo ePaper in base allo stato FSM.

#### Stati FSM suggeriti → display

- **Boot**
  - `0x00 0xAA` (solo se il protocollo lo richiede lato modulo; altrimenti aspettare che il modulo sia pronto)
  - `0x00 0xFF` (logo) opzionale
  - testo “Benvenuto”

- **ADS (credito = 0)**
  - testo “Benvenuto / Presenta moneta / QR / NFC”

- **CREDIT (credito > 0)**
  - mostra credito con formato richiesto in `docs/TODO.md`:
    - numero grande: `0x01 0xFF font=GoogleSansBold 100pt` (font 14) centrato
    - label “Credito” sotto: `0x01 0xFF font=GoogleSansBold 70pt` (font 13) o equivalente

- **RUNNING/PAUSED**
  - mostra countdown / stato (nel documento TODO ci sono vincoli su come inviare testi; il countdown “speciale” verrà gestito a parte)

- **OUT_OF_SERVICE**
  - tema invertito (`0x00 0x05`) e testo di errore, oppure testo normale con prefisso `§` per clear.

### 4) Regole pagamento durante RUN

Da `docs/TODO.md` punto 13:
- durante RUN/PAUSE disabilitare sistemi acquisizione credito (CCTalk, scanner, MDB) fino a fine esecuzione.

Per `EPAPER_USB`:
- lo “scanner” è parte del modulo ePaper → gestire con `0x00 0x02` (scanner ON) / `0x00 0x03` (scanner OFF) sul protocollo ePaper.

## Checklist implementazione

- [ ] Aggiungere switch `EPAPER_USB` come modalità UI primaria (no LVGL) senza crash.
- [ ] Aggiungere TX pacchetti `0x00/0x01/0x02` sul CDC aperto.
- [ ] Aggiungere mapping FSM→display (welcome/credito/running/oos).
- [ ] Aggiungere gestione scanner ON/OFF in RUN via comandi `0x00 0x02/0x03`.
- [ ] Validare RX barcode dal modulo ePaper e pubblicare evento `FSM_INPUT_EVENT_QR_SCANNED`.

