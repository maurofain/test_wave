# USB CDC Barcode Scanner — Guida di Implementazione

## Obiettivo
Interfacciare uno scanner barcode USB che si presenta come dispositivo **CDC-ACM** (seriale virtuale).

## Requisiti
- ESP-IDF con supporto **USB Host**.
- Driver **CDC-ACM Host** abilitato.
- Alimentazione USB Host stabile per lo scanner.

## Passi di implementazione

### 1) Abilitare USB Host nel progetto
- Abilita USB Host in `sdkconfig`.
- Abilita il driver CDC-ACM Host.

### 2) Inizializzare lo stack USB Host
- Avvia lo stack USB Host.
- Registra le callback per eventi di collegamento/scollegamento.

### 3) Enumerazione e apertura CDC-ACM
- All’arrivo del device, verifica class/subclass/protocol di tipo CDC-ACM.
- Apri l’interfaccia e imposta parametri seriali (baud, parità, stop).

### 4) Lettura dati
- Leggi il flusso di byte dal CDC-ACM.
- Gestisci i delimitatori tipici (CR/LF) o un timeout di fine-codice.

### 5) Parsing del barcode
- Accumula i byte in un buffer.
- Alla ricezione del terminatore, valida e consegna il codice.

### 6) Gestione errori
- Gestisci disconnessione e riconnessione.
- Reinizializza il dispositivo al reconnect.

## Abilitare e testare il driver CDC-ACM Host (guida rapida)
Di seguito le istruzioni pratiche per abilitare il supporto reale via CDC‑ACM e per testarlo passo‑passo.

1) Abilitare le opzioni Kconfig
- Apri il menuconfig:
  - `idf.py menuconfig`
- In `Component config → USB CDC Scanner`:
  - `Enable USB CDC Scanner` = y
  - `Use CDC-ACM Host driver` = y (CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST)
  - Imposta `Vendor ID` / `Product ID` (CONFIG_USB_CDC_SCANNER_VID / _PID / _DUAL_PID) se necessario
- In `Component config → USB`: abilita `Enable USB OTG support` (CONFIG_USB_OTG_SUPPORTED = y)

> In alternativa puoi impostare valori di base in `sdkconfig.defaults`:
>
> ```text
> CONFIG_USB_OTG_SUPPORTED=y
> CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST=y
> CONFIG_USB_CDC_SCANNER_VID=0x303A
> CONFIG_USB_CDC_SCANNER_PID=0x4001
> ```

2) Aggiungere il componente CDC‑ACM Host al progetto
- Opzione A (consigliata): aggiungi il managed component dal registry (se disponibile):
  - `idf.py add-dependency "espressif/usb_host_cdc_acm@^2.*"`
- Opzione B: copia/incolla il componente `usb_host_cdc_acm` dentro `components/` o `managed_components/` del progetto.
- Aggiorna `components/usb_cdc_scanner/CMakeLists.txt` per richiedere il componente:

```cmake
idf_component_register(
  SRCS "usb_cdc_scanner.c"
  INCLUDE_DIRS "include"
  REQUIRES usb usb_host_cdc_acm
)
```

3) Ricostruire e flashare
- `idf.py build`
- `idf.py -p /dev/TTY flash` (sostituisci /dev/TTY con la porta corretta)
- `idf.py monitor` per vedere i log

4) Cosa cercare nei log (verifica runtime)
- Log d’inizializzazione USB: `USB Host installato` o simile
- Log del driver CDC: `CDC-ACM host enabled ...` (se presente) e `Trying to open CDC device XXXX:XXXX`
- Quando il device viene aperto: `CDC device opened`
- Quando viene ricevuto un barcode: `Barcode letto: <codice>` (stampato da `web_ui`)

5) Debug & note pratiche
- Se il dispositivo non viene trovato, abilita log più verbosi per `usb` e `cdc` in menuconfig per ottenere informazioni sull’enumerazione e sugli errori.
- Alcuni scanner si comportano come HID invece che CDC: in quel caso è necessario il driver HID (es. `usb_host_hid`).
- Verifica l’alimentazione 5V e la corrente disponibile sulla porta USB Host.

Con queste operazioni il driver cercherà automaticamente il device con i VID/PID configurati, aprirà il reporting e inoltrerà i barcode alla callback `on_barcode`.


## Architettura consigliata
- `usb_host_manager.c` → gestione stack e eventi
- `cdc_scanner.c` → gestione CDC-ACM e parser

## Note pratiche
- Alcuni scanner possono presentarsi come HID: in quel caso serve un driver diverso.
- Verifica l’alimentazione 5V e la corrente disponibile.

## Checklist
- [ ] USB Host attivo
- [ ] CDC-ACM driver attivo
- [ ] Enumerazione OK
- [ ] Lettura dati OK
- [ ] Parsing OK
- [ ] Reconnect OK

---

## Appendice: Menuconfig, Kconfig, CMake e aggiunta del componente

### A) Menuconfig (passo‑passo)
- `idf.py menuconfig`
- Vai in `Component config` → `USB CDC Scanner`:
  - `Enable USB CDC Scanner` (CONFIG_USB_CDC_SCANNER_ENABLE) = y
  - `Use CDC-ACM Host driver` (CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST) = y
  - Imposta `Vendor ID` / `Product ID` (CONFIG_USB_CDC_SCANNER_VID / _PID / _DUAL_PID)
- Vai in `Component config` → `USB`:
  - `Enable USB OTG support` (CONFIG_USB_OTG_SUPPORTED) = y

> Opzione rapida: aggiungi in `sdkconfig.defaults`:
> ```text
> CONFIG_USB_OTG_SUPPORTED=y
> CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST=y
> CONFIG_USB_CDC_SCANNER_VID=0x303A
> CONFIG_USB_CDC_SCANNER_PID=0x4001
> ```

### B) Kconfig (file completo per il componente)
Percorso: `components/usb_cdc_scanner/Kconfig`
```text
menu "USB CDC Scanner"

config USB_CDC_SCANNER_ENABLE
    bool "Enable USB CDC Scanner component"
    default y

config USB_CDC_SCANNER_USE_CDC_ACM_HOST
    bool "Use CDC-ACM Host driver for real device access"
    default n
    depends on USB_OTG_SUPPORTED

config USB_CDC_SCANNER_VID
    hex "Vendor ID (VID) of the scanner"
    default 0x303A

config USB_CDC_SCANNER_PID
    hex "Product ID (PID) of the scanner"
    default 0x4001

config USB_CDC_SCANNER_DUAL_PID
    hex "Optional dual PID variant"
    default 0x4002

endmenu
```

### C) CMake: richiedere il componente CDC-ACM host
Se abiliti `CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST`, assicurati che `usb_host_cdc_acm` sia richiesto:
```cmake
idf_component_register(
  SRCS "usb_cdc_scanner.c"
  INCLUDE_DIRS "include"
  REQUIRES usb usb_host_cdc_acm
)
```

### D) Aggiungere il managed component `usb_host_cdc_acm`
- Opzione A (raccomandata, se disponibile):
  - `idf.py add-dependency "espressif/usb_host_cdc_acm@^2.*"`
  - Poi `idf.py build` e verifica che il componente sia presente in `managed_components` o `build`.
- Opzione B (manuale):
  - Scarica il codice del componente e copialo in `components/` o `managed_components/` nella root del progetto.
  - Esegui `idf.py build`.

> Nota: se non abiliti `CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST`, il driver utilizza la modalità simulata (`getchar()`), quindi il componente non è necessario.
