# Piano di Lavoro — ePaper: migrazione CTRL-PROTOCOL su USB CDC

**Data:** 2026-05-06  
**Stato:** BOZZA  
**Riferimento enum condizionante:** `DEVICE_DISPLAY_TYPE_EPAPER_RS232`

---

## 1. Obiettivo

Migrare la comunicazione tra il firmware e la scheda display ePaper dalla porta RS232
alla porta USB CDC, riutilizzando lo stesso driver fisico (`usb_host_cdc_acm`) già
impiegato dallo scanner Newland.

| Configurazione display       | Porta USB CDC   | RS232               |
|-----------------------------|-----------------|---------------------|
| `EPAPER_RS232`               | ePaper TX/RX    | Libera (non usata)  |
| `DSI7_TOUCH` / `DSI101_TOUCH` | Scanner Newland | Non usata           |
| `NONE` / headless            | Nessuno         | —                   |

La condizione che abilita tutta la logica è:

```c
cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232
```

---

## 2. Architettura attuale (AS-IS)

### Flusso ePaper → FW (barcode in ingresso)
```
[ePaper board] --RS232--> rs232_task() --> rs232_process_scanner_line()
                                               --> scanner_on_barcode_cb()
```

### Flusso FW → ePaper (comandi di controllo / CTRL-PROTOCOL)
```
rs232_epaper_display_*() --> rs232_epaper_send_packet() --> rs232_send()
                                                               --> [RS232 HW]
```

### Flusso scanner Newland (USB CDC)
```
[Newland scanner] --USB CDC--> cdc_data_cb() --> usb_cdc_scanner_task()
                                                    --> scanner_on_barcode_cb()
```

---

## 3. Architettura obiettivo (TO-BE)

### Quando `DEVICE_DISPLAY_TYPE_EPAPER_RS232`

#### Flusso ePaper ← FW (CTRL-PROTOCOL in uscita)
```
rs232_epaper_display_*() --> rs232_epaper_send_packet()
                               --> [if EPAPER_RS232] usb_cdc_epaper_send(data, len)
                               --> [else]            rs232_send(data, len)
```

#### Flusso ePaper → FW (barcode in ingresso via USB CDC)
```
[ePaper board] --USB CDC--> cdc_data_cb() --> usb_cdc_scanner_task()
                                                --> scanner_on_barcode_cb()
```
*(stessa callback già usata per Newland; il driver non invia comandi Newland in modalità ePaper)*

#### RS232: non usata (task rs232 in IDLE)

### Quando `DSI7_TOUCH` / `DSI101_TOUCH`
Comportamento invariato: USB CDC = scanner Newland, RS232 non usata.

---

## 4. Modifiche per file

### 4.1 `components/usb_cdc_scanner/usb_cdc_scanner.c` + `.h`

**Obiettivo:** aggiungere modalità ePaper e funzione di TX.

#### 4.1.1 Aggiungere funzione pubblica per TX ePaper
```c
// In usb_cdc_scanner.h
esp_err_t usb_cdc_scanner_epaper_send(const uint8_t *data, size_t len);
```

Internamente chiama `cdc_acm_host_data_tx_blocking()` (già disponibile
nel driver CDC-ACM) o equivalente sul canale USB CDC aperto.

#### 4.1.2 Rilevare la modalità ePaper all'init
Aggiungere una funzione helper `static bool s_is_epaper_mode` che viene
impostata in `usb_cdc_scanner_init()` in base a:
```c
s_is_epaper_mode = (device_config_get()->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232);
```

#### 4.1.3 Sopprimere comandi Newland in modalità ePaper
Le funzioni `usb_cdc_scanner_send_setup_command()`, `send_on_command()`,
`send_off_command()` devono fare early-return con `ESP_OK` (no-op silenzioso)
quando `s_is_epaper_mode == true`.

Aggiungere funzione pubblica:
```c
bool usb_cdc_scanner_is_epaper_mode(void);
```

#### 4.1.4 CMakeLists.txt: nessuna modifica necessaria
`usb_cdc_scanner` dipende già da `device_config`.

---

### 4.2 `components/rs232/rs232_epaper.c` + `.h`

**Obiettivo:** instradare il CTRL-PROTOCOL su USB CDC invece di RS232.

#### 4.2.1 Modificare `rs232_epaper_send_packet()`
```c
// Attuale
int written = rs232_send(data, len);

// Nuovo
if (device_config_get()->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232) {
    return usb_cdc_scanner_epaper_send(data, len);
}
int written = rs232_send(data, len);
```

Include necessario: aggiungere `#include "usb_cdc_scanner.h"`.

#### 4.2.2 Aggiornare `rs232_epaper_is_enabled()`
Attuale:
```c
return cfg && cfg->display.enabled
           && cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232
           && cfg->sensors.rs232_enabled;  // <-- questo vincolo cade
```
Nuovo:
```c
return cfg && cfg->display.enabled
           && cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232;
```
La condizione `rs232_enabled` non serve più perché il TX avviene via CDC.

#### 4.2.3 CMakeLists.txt: aggiungere dipendenza
```cmake
idf_component_register(SRCS "rs232.c" "rs232_epaper.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver device_config usb_cdc_scanner)
```

---

### 4.3 `main/tasks.c`

**Obiettivo:** escludere il codice scanner Newland quando display è ePaper.

#### 4.3.1 `usb_cdc_scanner_runtime_allowed()` (weak overriding function)
Aggiungere esclusione per modalità ePaper:
```c
bool usb_cdc_scanner_runtime_allowed(void)
{
    const device_config_t *cfg = device_config_get();
    if (cfg && cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232) {
        return false;  // porta CDC riservata a ePaper
    }
    return !tasks_is_out_of_service_state() && !tasks_is_program_active();
}
```

#### 4.3.2 `tasks_health_check()` — blocco scanner/cctalk
Nella verifica combinata scanner+cctalk:
```c
if ((focus_agent == AGN_ID_NONE || focus_agent == AGN_ID_CCTALK
     || focus_agent == AGN_ID_USB_CDC_SCANNER)) {
    bool cctalk_ok = ...;
    bool scanner_ok = cfg->scanner.enabled && usb_cdc_scanner_is_connected();

    // NUOVO: in modalità ePaper lo scanner USB non è atteso → mai in OOS per mancanza scanner
    bool epaper_cdc_mode = (cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_RS232);
    if (epaper_cdc_mode) scanner_ok = true;  // considera scanner OK per non triggerare OOS

    if (!cctalk_ok && !scanner_ok) { ... }
    ...
    if (!cfg->scanner.enabled || epaper_cdc_mode) {
        init_agent_status_set(AGN_ID_USB_CDC_SCANNER, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
    }
```

#### 4.3.3 Loop FSM — gestione scanner ON/OFF in modalità ePaper

**Vincolo hardware:** lo scanner integrato nel modulo ePaper **non è controllabile
dal firmware**. Non risponde a comandi di tipo `SCNENA0/1` né a sequenze di setup
Newland. Il canale di scan resta sempre attivo (il modulo emette barcode in
autonomia quando rileva un codice).

Ne consegue:

- Tutti i blocchi FSM che invocano `send_setup_command()`, `send_on_command()`,
  `send_off_command()` devono essere disabilitati quando `usb_cdc_scanner_is_epaper_mode() == true`.
  Aggiungere la guard:
  ```c
  if (cfg->scanner.enabled && !usb_cdc_scanner_is_epaper_mode()) {
      usb_cdc_scanner_send_setup_command();
      usb_cdc_scanner_send_on_command();
  }
  ```
  *(Le funzioni `send_*_command()` sono già no-op in modalità ePaper per la
  modifica 4.1.3, ma la guard esplicita documenta l'intenzione e risparmia
  il path USB.)*

- La FSM **continua a ricevere** gli eventi `FSM_INPUT_EVENT_QR_SCANNED` provenienti
  dal modulo ePaper via `cdc_data_cb()` → `scanner_on_barcode_cb()`.

**Gestione del "scanner disabilitato" durante programma attivo:**  
Poiché lo scanner non può essere spento via software, non è possibile silenziarlo
fisicamente durante `FSM_STATE_RUNNING` / `FSM_STATE_PAUSED`. La soluzione è un
**filtro software in `scanner_on_barcode_cb()`**: se `usb_cdc_scanner_is_epaper_mode()`
è `true` e il flag interno `scanner_forced_off_for_program` è attivo, il messaggio
viene scartato senza essere pubblicato sulla mailbox FSM.

Aggiungere in `scanner_on_barcode_cb()`:
```c
static void scanner_on_barcode_cb(const char *barcode)
{
    if (tasks_is_out_of_service_state()) { ... return; }

    // NUOVO: in modalità ePaper lo scanner non è disattivabile hardware;
    // scartiamo i barcode quando la FSM ha richiesto la disabilitazione scanner.
    if (usb_cdc_scanner_is_epaper_mode() && scanner_forced_off_for_program) {
        ESP_LOGD("SCANNER", "[M] Barcode ePaper scartato: scanner sospeso per programma attivo");
        return;
    }

    // resto del codice invariato ...
}
```

Il flag `scanner_forced_off_for_program` (già presente in `fsm_task()`) viene
impostato a `true` all'avvio del programma e a `false` al termine: in modalità ePaper
non viene inviato il comando HW off, ma il flag da solo è sufficiente a sopprimere
i barcode in arrivo fino alla fine del ciclo.

#### 4.3.4 `rs232_task()` — evitare ricezione barcode da RS232 in modalità ePaper CDC
In modalità ePaper i barcode arrivano via USB CDC (non più via RS232).
Il task `rs232_task` deve fermarsi:
```c
static void rs232_task(void *arg)
{
    ...
    while (true) {
        if (rs232_epaper_is_enabled()) {
            // NUOVO: se il TX ePaper è su USB CDC, anche il RX è su CDC;
            // rs232 non riceve più barcode in questa configurazione
            if (usb_cdc_scanner_is_epaper_mode()) {
                vTaskDelayUntil(&last_wake, param->period_ticks);
                continue;
            }
            // else: vecchio comportamento RS232 RX (retrocompat)
            ...
        }
    }
}
```
> **Nota:** il task `rs232` viene già impostato a IDLE da `tasks.csv`/`tasks.json`
> quando display è EPAPER_RS232 (riga nel CSV con state=IDLE). Verificare che
> questa configurazione resti coerente.

#### 4.3.5 Blocco OOS → uscita OOS: `send_setup_command` / `send_on_command`
Stessa logica del punto 4.3.3: aggiungere guard `!usb_cdc_scanner_is_epaper_mode()`.

---

### 4.4 `main/init.c` (verifica)

Verificare dove viene chiamata l'init dello scanner USB e se ci sono condizioni da
aggiornare. In `init.c` alla riga circa 2857 c'è un blocco `DNA_USB_SCANNER`;
verificare che il parametro `cfg` che abilita lo scanner consideri la modalità ePaper.

Se `scanner.enabled` deve restare `true` anche in modalità ePaper (per consentire
il funzionamento USB CDC del display), nessuna modifica è necessaria qui.
Altrimenti aggiungere:
```c
bool scanner_hardware_active = cfg->scanner.enabled
                               && (cfg->display.type != DEVICE_DISPLAY_TYPE_EPAPER_RS232);
```

---

### 4.5 `data/tasks.json` (verifica)

Verificare che il task `rs232` sia impostato con `IDLE` per la configurazione ePaper
e che `usb_scanner` sia `RUN` (il driver CDC serve anche in modalità ePaper per
il canale TX/RX). Aggiornare il commento descrittivo della voce.

---

## 5. Considerazioni aggiuntive

### 5.1 Dipendenza circolare component
`rs232_epaper.c` includerà `usb_cdc_scanner.h`.  
`usb_cdc_scanner.c` NON include `rs232_epaper.h` → nessun ciclo.

### 5.2 Gestione connessione USB CDC in modalità ePaper
Il driver `usb_cdc_scanner_task()` apre il canale CDC sulla base di VID/PID
configurati. Verificare che VID/PID del modulo ePaper sia diverso dal Newland
e configurabile tramite `config.json` (campi `scanner.vid` / `scanner.pid`).
Se identici, il driver si connette senza modifiche; altrimenti aggiornare i default.

### 5.3 Assenza di comandi di setup specifici per ePaper
Lo scanner Newland richiede `SCN_CMD_SETUP` / `SCN_CMD_ON` alla connessione.
Il modulo ePaper non richiede (presumibilmente) tali comandi: le funzioni in 4.1.3
diventano no-op. Confermare con la specifica hardware del modulo ePaper.

### 5.4 Nome enum vs. trasporto
L'enum `DEVICE_DISPLAY_TYPE_EPAPER_RS232` mantiene il nome originale per
retrocompatibilità con configurazioni NVS già salvate. Il nome diventa in parte
impreciso ma il costo di un migration path NVS sarebbe sproporzionato.

### 5.5 Rimozione del vincolo `rs232_enabled` per ePaper
Dopo la modifica 4.2.2, impostare `sensors.rs232_enabled = false` in una
configurazione con `EPAPER_RS232` non bloccherà più il display.  
Aggiornare `docs/a_COMPILE_FLAGS.md` e la UI di configurazione web se espone
questo campo.

---

## 6. Ordine di implementazione

| Step | Descrizione                                                           | File                          |
|------|-----------------------------------------------------------------------|-------------------------------|
| 1    | Aggiungere `usb_cdc_scanner_epaper_send()` + `is_epaper_mode()`      | `usb_cdc_scanner.c/.h`        |
| 2    | Sopprimere comandi Newland in modalità ePaper                        | `usb_cdc_scanner.c`           |
| 3    | Aggiornare CMakeLists del componente rs232                            | `rs232/CMakeLists.txt`        |
| 4    | Modificare `rs232_epaper_send_packet()` → USB CDC                    | `rs232_epaper.c`              |
| 5    | Aggiornare `rs232_epaper_is_enabled()` (rimuovere vincolo rs232)     | `rs232_epaper.c`              |
| 6    | Aggiornare `usb_cdc_scanner_runtime_allowed()` in tasks.c            | `main/tasks.c`                |
| 7    | Aggiornare `tasks_health_check()` per modalità ePaper               | `main/tasks.c`                |
| 8    | Aggiornare blocchi FSM on/off scanner                                | `main/tasks.c`                |
| 9    | Aggiornare `rs232_task()` (skip RX in modalità ePaper CDC)           | `main/tasks.c`                |
| 10   | Verifica e aggiornamento `init.c`                                    | `main/init.c`                 |
| 11   | Verifica coerenza `tasks.json`                                        | `data/tasks.json`             |
| 12   | Build e test su device con config `EPAPER_RS232`                     | —                             |

---

## 7. Criteri di verifica

- [ ] Build senza errori e warning per entrambe le modalità (EPAPER_RS232 e DSI7_TOUCH)
- [ ] Con `EPAPER_RS232`: i pacchetti CTRL-PROTOCOL arrivano sul bus USB CDC (verificabile con analizzatore USB o log TX)
- [ ] Con `EPAPER_RS232`: nessun comando Newland (`SCNMOD`, `SCNENA`) inviato sul CDC
- [ ] Con `EPAPER_RS232`: barcode letti dal modulo ePaper via USB CDC → evento `QR_SCANNED` in FSM
- [ ] Con `DSI7_TOUCH` / `DSI101_TOUCH`: scanner Newland funziona normalmente via USB CDC
- [ ] Con `DSI7_TOUCH`: nessun codice ePaper attivato (rs232_epaper_is_enabled() = false)
- [ ] Nessuna regressione sui task RS485 / MDB / CCTalk
- [ ] Nessun OOS spurio in modalità EPAPER_RS232 dovuto a scanner USB "non connesso"

---

## 8. File da NON modificare

- `device_config.h` — l'enum `DEVICE_DISPLAY_TYPE_EPAPER_RS232` resta invariato
- `components/rs232/rs232.c` — il driver RS232 base non viene toccato
- `fsm.c` / `fsm.h` — nessuna logica FSM cambia
