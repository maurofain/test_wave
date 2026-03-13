# ESP-IDF starter for ESP32-P4 Waveshare module

This project targets the Waveshare ESP32-P4 Module Dev Kit and an expansion board wired as described below. It initializes the basic peripherals (I2C bus, WS2812 strip, RS232/RS485/MDB UARTs, PWM outputs, BOOT button) and supports both **APP** and **FACTORY** modes from a single codebase (`main/`) using the compile-time flag `COMPILE_APP`.

| [Specifiche](docs/specifiche.md) |[PinOut](docs/PINOUT.md) |
| [RULES](docs/RULES.md) | [TODO](docs/TODO.md) |

## Hardware summary (per HARDWARE_SETUP)
- I2C bus (SDA GPIO27, SCL GPIO26) for 26LC16 EEPROM, two FXL6408 IO expanders, SHT40 sensor.
- WS2812 RGB strip on GPIO5.
- RS232: TX GPIO36, RX GPIO46.
- RS485: TX GPIO4, RX GPIO19, DE GPIO21.
- PWM outputs: OUT1 GPIO47, OUT2 GPIO48.
- MDB: RX GPIO23, TX GPIO22.
- BOOT button: GPIO35.

## Partitions
The provided custom table in [partitions.csv](partitions.csv) is used by default:
- `factory` (4.75 MB): recovery/test image when compiled with `COMPILE_APP=0`.
- `ota_0` (4.75 MB): application slot A.
- `ota_1` (4.75 MB): application slot B.
- `nvs`, `phy_init`, `otadata`, `storage` (SPIFFS) as support partitions.

## Networking (Factory mode)
- Wi-Fi AP always on: SSID `factory-setup`, password `factory123` (open if empty), channel configurable.
- Wi-Fi STA optional (enable and set credentials in `idf.py menuconfig`).
- Ethernet optional (RMII PHY pins configurable; disabled by default).
- HTTP server on port 80 (configurable).

HTTP endpoints:
- `GET /status` → JSON with running/boot partition and IPs (AP/STA/Ethernet).
- `POST /ota?url=<http(s)://...>` → triggers OTA to the inactive app slot (`ota_0` / `ota_1`) and reboots on success. If no query is provided, uses `APP_OTA_DEFAULT_URL` when set.

### Test API (legacy + REST)

`/api/test/*` supporta due formati equivalenti:
- Legacy token: `/api/test/<token>` (es. `/api/test/led_start`)
- REST path: `/api/test/<group>/<action>` (es. `/api/test/led/start`)

Catalogo endpoint disponibili:
- `GET /api/test/endpoints`

Esempi rapidi:

| Method | Endpoint | Body (JSON) | Risposta attesa (esempio) |
|---|---|---|---|
| GET | `/api/test/endpoints` | - | `{"tokens":[...],"legacy_base":"/api/test/<token>","rest_base":"/api/test/<group>/<action>"}` |
| POST | `/api/test/led/start` | `{}` | `{"message":"Test LED avviato"}` |
| POST | `/api/test/pwm1/start` | `{}` | `{"message":"Test PWM1 avviato"}` |
| POST | `/api/test/ioexp/start` | `{}` | `{"message":"Test I/O Expander avviato (1Hz)"}` |
| POST | `/api/test/serial/send` | `{"port":"rs232","data":"A5 C0 6D","mode":"HEX"}` | `{"status":"ok","message":"Inviato su rs232"}` |
| POST | `/api/test/serial/monitor` | `{}` | `{"rs232":"...","rs485":"...","mdb":"...","cctalk":"..."}` |
| POST | `/api/test/serial/clear` | `{"port":"rs232"}` | `{"status":"ok"}` |

### Modbus RTU su RS485 (ESP-IDF `freemodbus`)

Endpoint di test disponibili:

| Method | Endpoint | Body (JSON) | Risposta attesa (esempio) |
|---|---|---|---|
| POST | `/api/test/modbus/status` | `{}` | `{"status":"ok","running":true,"coils":[...],"inputs":[...]}` |
| POST | `/api/test/modbus/read/di` | `{"slave_id":1,"start":0,"count":8}` | `{"status":"ok","type":"discrete_inputs","values":[0,1,...]}` |
| POST | `/api/test/modbus/read/coils` | `{"slave_id":1,"start":0,"count":8}` | `{"status":"ok","type":"coils","values":[1,0,...]}` |
| POST | `/api/test/modbus/write/coil` | `{"slave_id":1,"coil":0,"state":1}` | `{"status":"ok","message":"Coil aggiornata",...}` |
| POST | `/api/test/modbus/write/coils` | `{"slave_id":1,"start":0,"states":[1,1,0,0]}` | `{"status":"ok","message":"Coils aggiornate",...}` |

Nota: quando il controller Modbus è attivo, i test raw RS485 (`/api/test/rs485/start` e `serial/send` su `port=rs485`) vengono bloccati per evitare conflitti sulla UART.

## Build
1. Install ESP-IDF (v5.1+ recommended for ESP32-P4) and set `IDF_PATH`/`idf.py` in PATH.
2. Configure (adjust pins if needed):
   ```
   idf.py menuconfig
   ```
3. Build and flash:
   ```
   idf.py set-target esp32p4
   idf.py build
   idf.py -p <PORT> flash monitor
   ```

## Rigenerazione pagine HTML (Web UI)

Se modifichi i template embedded o il generatore, rigenera i file statici in `data/www` con:

```bash
python3 scripts/export_embedded_pages.py --output data/www
```

Esempio (solo home):

```bash
python3 scripts/export_embedded_pages.py --output data/www --pages index.html
```

Per aggiornare sul device solo la partizione SPIFFS (senza ricompilare il firmware):

```bash
bash scripts/flash_spiffs.sh
```

Nota: in modalità pagine esterne (`WEB_UI_PAGE_SOURCE=1` su SPIFFS), questi file sono quelli serviti dalla Web UI.

### Codice unificato APP/FACTORY
- Il progetto usa una singola base sorgente in `main/`.
- La modalità di compilazione è selezionata con `COMPILE_APP` in `main/app_version.h` (file sorgente unico per la scelta modalità):
   - `COMPILE_APP=1` → APP
   - `COMPILE_APP=0` → FACTORY
- Le differenze operative sono condizionate da macro, evitando drift tra cartelle duplicate.

## Flash OTA da VS Code

Sono disponibili task VS Code per eseguire il flash via rete senza porta seriale.

### Prerequisiti
- Device raggiungibile via HTTP (es. AP `factory-setup`, IP `192.168.4.1`).
- Firmware già compilato (`build/test_wave.bin` presente).
- Endpoint OTA attivi nel firmware (`POST /ota/upload` e `POST /ota?url=...`).

### Metodo 1: Upload diretto binario
1. In VS Code apri `Run Task`.
2. Esegui `Flash OTA (upload bin via /ota/upload)`.
3. Inserisci `otaDeviceHost` (default `192.168.4.1`).

Il task invia direttamente `build/test_wave.bin` al device tramite multipart upload.

### Metodo 2: Trigger OTA da URL
1. Pubblica `test_wave.bin` su un server HTTP raggiungibile dal device.
2. In VS Code esegui `Flash OTA (trigger URL via /ota)`.
3. Inserisci:
   - `otaDeviceHost` (host del device)
   - `otaUrl` (URL completo del firmware, es. `http://192.168.4.2:8000/test_wave.bin`)

Il task chiama `POST /ota?url=<firmware_url>` e il device scarica il firmware in autonomia.

### Task utili collegati
- `Build (idfc -b)` per generare il binario prima dell'OTA.
- `Monitor (idf.py monitor)` se vuoi verificare log e riavvio via seriale.

## Valutazione: update firmware triggerato dal server

È possibile gestire l'aggiornamento firmware in operatività normale facendo restituire al server (in risposta a una chiamata app) un parametro/flag di update, quindi scaricando il `.bin` da rete e applicando OTA sulla partizione inattiva.

- **Fattibilità tecnica:** alta su ESP-IDF (flusso già coerente con il meccanismo OTA standard).
- **FTP:** utilizzabile ma non ideale in produzione (credenziali in chiaro, minori garanzie di integrità/autenticità).
- **Raccomandazione produzione:** preferire HTTPS oppure firma firmware + verifica hash SHA256.
- **Robustezza minima richiesta:** download a chunk con retry/timeout, verifica dimensione e integrità, controllo versione/anti-rollback, switch della boot partition solo dopo `esp_ota_end()` riuscito, rollback in caso di boot fallito.
- **Gestione del file `.bin` (stato attuale):** il firmware non salva l'immagine completa né in RAM né su SD; usa streaming a chunk con buffer ridotto in RAM e scrittura diretta sulla partizione OTA inattiva.
- **Impatto operativo:** medio; soluzione consigliata se accompagnata da controlli di sicurezza e recovery.

## Nota di fattibilità: unificazione code FSM

Stato attuale:
- La **mailbox eventi FSM** trasporta gli eventi strutturati (`fsm_input_event_t`) per la logica runtime.
- La coda **pending messages** (testuale) è usata per diagnostica/UI (`fsm_append_message`, endpoint `/api/emulator/fsm/messages`).

Fattibilità modifica (tenere solo la mailbox eventi):
- **Fattibile**, senza impatto sul core stati/transizioni della FSM.
- Richiede però di sostituire il canale testuale dell’emulatore con una vista derivata dagli eventi/snapshot, oppure di rinunciare allo storico messaggi “umano”.

Decisione corrente:
- Per ora si mantiene l’architettura attuale (mailbox + pending messages) perché utile per debug rapido e feedback UI lato emulatore.

## Notes
- WS2812 uses the RMT-based led_strip helper and blinks the first LED to indicate the app is alive.
- RS485 and MDB UARTs are only initialized if the target exposes enough UART controllers; warnings are logged otherwise.
- Replace the SHT40 stub with the actual driver when available.
- Ethernet driver uses the internal EMAC + RMII (LAN8720 by default); adjust pins/PHY type as needed or disable via Kconfig if not present.
- OTA over plain HTTP is allowed by default for lab convenience; change to HTTPS in production and provide the server certificate in Kconfig.

## Regola testi UI (multilingua)
- Tutti i testi mostrati all'utente (Web UI e LVGL) devono essere generati tramite tabella i18n su SPIFFS, con file per lingua: `/spiffs/i18n_<lang>.json`.
- Formato record obbligatorio: `{ "lang": "it", "scope": "nav", "key": "home", "text": "Home" }`.
- `key` deve essere univoca all'interno dello stesso `scope`.
- Evitare nuove stringhe hardcoded nelle pagine: usare `scope+key` con fallback minimo.
- Ogni nuova stringa UI aggiunta nel codice deve essere contestualmente tabellata nel set IT in `data/i18n_it.json`.
- Lingua corrente: `device_config.ui.language` (solo selezione lingua; i testi non sono salvati in NVS/EEPROM).

## Documentazione Hardware
Per note specifiche su limitazioni hardware e pinout, vedi [docs/NOTES_HW.md](docs/NOTES_HW.md).

## Documentazione Tecnica

Il progetto utilizza **Doxygen** con **Graphviz** per la generazione automatica della documentazione tecnica e dei grafi delle chiamate (Call Graphs).

### Generazione
Per aggiornare la documentazione:
```bash
doxygen Doxyfile
```

La documentazione generata è disponibile qui: [Doxygen Index](docs/doxygen/html/index.html).

### Requisiti
- Doxygen (v1.9+)
- Graphviz (per i grafi)
