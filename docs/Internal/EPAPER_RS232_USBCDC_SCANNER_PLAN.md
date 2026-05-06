# Piano di implementazione: supporto ingressi seriali STRL_PROTOCOL per `DEVICE_DISPLAY_TYPE_EPAPER_USB`

## Obiettivo
Supportare due possibili sorgenti di input/output seriale per il protocollo `STRL_PROTOCOL` quando il display è impostato su `DEVICE_DISPLAY_TYPE_EPAPER_USB`:
- porta RS232 fisica su GPIO36/RX e GPIO46/TX
- porta USB CDC sulla porta USB

Questa scelta deve essere configurabile e gestita correttamente a runtime, mantenendo l'attuale logica `EPAPER_USB` per l'output display via USB CDC.

## Contesto attuale
- `DEVICE_DISPLAY_TYPE_EPAPER_USB` è già definito in `components/device_config/include/device_config.h`.
- Il display e-paper viene inizializzato da `main/init.c` usando `rs232_epaper_init()` e `rs232_epaper_display_welcome()`.
- La logica di avvio task deve garantire `usb_scanner` RUN quando `cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_USB`.
- Il componente `components/usb_cdc_scanner` gestisce l'interfaccia scanner USB CDC.
- Il componente `components/rs232/rs232_epaper.c` gestisce la scrittura verso il display e-paper via RS232.

## Requisiti funzionali
1. Quando `display.type == DEVICE_DISPLAY_TYPE_EPAPER_USB`, il sistema deve poter scegliere tra due canali dati:
   - `SERIAL_SOURCE_RS232` → usare la porta RS232 fisica su GPIO36/46
   - `SERIAL_SOURCE_USBCDC` → usare la porta USB CDC per ricevere i dati scanner
2. L'output verso l'e-paper rimane su RS232 fisico in ogni caso, perché il display e-paper è fisicamente collegato a RS232.
3. Il canale scanner `STRL_PROTOCOL` deve poter ricevere input dal canale selezionato e consegnarlo al flusso parser/scanner esistente.
4. In `EPAPER_USB`, il task `usb_scanner` deve restare abilitato e fornire sia display che barcode via USB CDC.
5. Quando è selezionata `RS232`, la porta RS232 fisica deve essere condivisa con l'e-paper secondo le regole HW e SW previste, e `usb_scanner` deve restare `IDLE` se non necessario.
6. Deve essere possibile cambiare l'impostazione tramite configurazione persistente (`device_config`) e in fase di avvio/runtime al boot.

## Proposta di architettura
### 1) Nuova configurazione di origine seriale
Aggiungere in `device_display_config_t` o nella configurazione scanner un nuovo campo:
- `device_serial_source_t` enum:
  - `DEVICE_SERIAL_SOURCE_RS232`
  - `DEVICE_SERIAL_SOURCE_USBCDC`
- campo di config: `serial_source` o `scanner_input_source`

### 2) Comportamento dei task
Modificare `main/tasks.c`:
- `usb_scanner`: deve rimanere RUN in `EPAPER_USB`.
- `rs232`: non è usato per il modulo ePaper in `EPAPER_USB`.
- aggiungere una condizione runtime che selezioni il canale scanner corretto.

### 3) Inizializzazione display e parser
- Mantenere `rs232_epaper_init()` e l'invio verso il display su RS232 fisico.
- Implementare un wrapper di input scanner per `STRL_PROTOCOL` che legge:
  - dalla stessa UART RS232 quando `serial_source == RS232`
  - dalla USB CDC quando `serial_source == USBCDC`
- Eventualmente creare un adapter `strl_input_source_t` con funzioni:
  - `strl_input_init()`
  - `strl_input_read()`
  - `strl_input_deinit()`

### 4) Compatibilità HW
- Assicurare che il driver RS232 fisico sia inizializzato solo una volta e che il canale sia condiviso con l'e-paper se necessario.
- Verificare se il display E-Paper e lo scanner RS232 utilizzano la stessa UART fisica: se sì, occorre una gestione multiplexing o una modalità TIME-SHARING.
- Se il display e-paper e lo scanner RS232 usano UART diverse, la soluzione è molto più semplice.

## Passi di implementazione
### Fase 1: Configurazione
1. Aggiornare `components/device_config/include/device_config.h` con l'enum `device_serial_source_t` e il campo di config.
2. Aggiornare parser e serializzazione config in `components/device_config/device_config.c` e `components/web_ui/web_ui.c` se necessario.
3. Aggiornare `data/i18n_v2.json` per le nuove etichette UI e eventuale descrizione.

### Fase 2: Runtime task management
1. Modificare `main/tasks.c` per rendere `EPAPER_USB` coerente (usb_scanner sempre RUN).
2. Aggiornare documentazione e naming (EPAPER_RS232 -> EPAPER_USB).

### Fase 3: Integrazione input STRL_PROTOCOL
1. Identificare il parser/gestore `STRL_PROTOCOL` esistente.
2. Creare un layer di astrazione `strl_input_source.c` o simile.
3. Collegare l'adapter al task scanner e al task RS232, in base alla selezione.

### Fase 4: Test e validazione
1. Verificare boot con `display.type == EPAPER_USB`:
   - display e-paper inizializza correttamente
   - `usb_scanner` resta IDLE
   - i dati scanner arrivano dalla RS232 fisica se supportato
2. Verificare lettura barcode e comandi display su USB CDC in `EPAPER_USB`:
   - display e-paper inizializza su RS232
   - `usb_scanner` si avvia
   - i dati scanner arrivano via USB CDC
3. Testare edge case:
   - `serial_source == USBCDC` ma USB non connesso
   - `serial_source == RS232` ma RS232 disabilitato
   - cambio runtime se previsto

## Documento consigliato
Creare una pagina interna di design e specifica come questa:
- `docs/Internal/EPAPER_RS232_USBCDC_SCANNER_PLAN.md` (da rinominare in follow-up)

## Note aggiuntive
- Il display E-Paper non deve essere mandato su USB CDC: l'output display resta sempre su RS232 fisico.
- Il nuovo campo di configurazione deve essere chiaro ai tecnici: "Scanner input" o "Serial input source".
- Eventuale supporto per `STRL_PROTOCOL` su USB CDC è un behavior di scanner, non un comportamento del display.
