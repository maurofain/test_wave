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