# CCTalk Initialization Sequence - v1.5.3

## Descrizione

Implementazione della sequenza di inizializzazione CCTalk (gettoniera) che viene eseguita:
1. **Al boot**: Dopo la finestra di stabilità, prima di mostrare la pagina principale
2. **Ad ogni cambio pagina**: Prima di ogni caricamento della pagina programmi (`lvgl_page_main_show()`)

## Sequenza di comandi

La funzione `main_cctalk_send_initialization_sequence()` invia 4 comandi di configurazione al dispositivo gettoniera (indirizzo 0x02) usando le API di alto livello del driver:

### Comando 1: Address Poll (Header 254 / 0xFE)
- **Funzione API**: `cctalk_address_poll(0x02, timeout_ms)`
- **Descrizione**: Verifica che la gettoniera sia presente e risponda
- **Risposta**: ACK

### Comando 2: Modify Inhibit Status (Header 231 / 0xE7)
- **Funzione API**: `cctalk_modify_inhibit_status(0x02, 0xFF, 0xFF, timeout_ms)`
- **Descrizione**: Abilita tutti i 16 canali di accettazione monete (mask 0xFF 0xFF)
- **Risposta**: ACK

### Comando 3: Modify Master Inhibit Std (Header 228 / 0xE4)
- **Funzione API**: `cctalk_modify_master_inhibit_std(0x02, true, timeout_ms)`
- **Descrizione**: Abilita accettazione globale delle monete
- **Risposta**: ACK

### Comando 4: Request Inhibit Status (Header 230 / 0xE6)
- **Funzione API**: `cctalk_request_inhibit_status(0x02, &mask_low, &mask_high, timeout_ms)`
- **Descrizione**: Legge e verifica lo stato corrente delle inibizioni
- **Risposta**: 2 byte con maschere canali attivi

## Verifica abilitazione

Prima di inviare, la funzione verifica che `device_config_t->sensors.cctalk_enabled` sia `true`. Se disabilitato, il log semplicemente registra lo skip.

## File modificati

### 1. `/main/main.c`
- Aggiunta funzione `main_cctalk_send_initialization_sequence()`
- Usa API CCTalk di alto livello anziché `cctalk_send()` grezzo
- Integrazione al boot (prima di `lvgl_page_main_show()`)

### 2. `/main/main.h` (creato)
- Prototipo della funzione `main_cctalk_send_initialization_sequence()`

### 3. `/components/lvgl_panel/lvgl_page_main.c`
- Aggiunto include `#include "main.h"`
- Richiamata `main_cctalk_send_initialization_sequence()` all'inizio di `lvgl_page_main_show()`

## Logging

Tutti i comandi generano log a livello:
- **LOGI**: Inizio/fine sequenza e comandi inviati con successo
- **LOGW**: Fallimento di un singolo comando
- Prefisso log: `[M]` per main app

Esempio output:
```
[M] Inizio sequenza di inizializzazione CCTalk...
[M] Cmd1 - Address Poll: OK
[M] Cmd2 - Modify Inhibit Status (all channels): OK
[M] Cmd3 - Modify Master Inhibit (accept enabled): OK
[M] Cmd4 - Request Inhibit Status: OK (mask=0xFFFF)
[M] Sequenza CCTalk completata
```

## Timeout

Tutti i comandi usano timeout di 1000ms configurabile. Questo consente al driver di:
- Attendere la risposta dal dispositivo
- Gestire ritardi della comunicazione seriale
- Fallire gracefully se il dispositivo non risponde

## Testing

Build completata correttamente con tutte le API. Pronto per il test su device.


