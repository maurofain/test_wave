# TODO — MicroHard test_wave

---

## 📋 DA FARE

### 0. Caricamento remoto artefatti

Valutazione per il caricamento da remoto su chiamata di: immagini, tabelle testi e firmware. I contenuti possono essere salvati sia in SPIFFS che in SD.

- 

### 2. Piano test endpoint e funzioni

Strutturare i test in 4 livelli:
- **Smoke**: endpoint raggiungibile, status code atteso, JSON valido
- **Contract**: campi obbligatori e tipi minimi della risposta
- **Flow**: sequenze operative (es. `sd_init → sd_list`, `serial_send → monitor → clear`, `config/save → config/get`)
- **Hardware-aware**: aspettative diverse in base a periferica abilitata/disabilitata

Organizzazione:
- `tests/smoke`, `tests/contract`, `tests/flow`, `tests/hw`
- `tests/endpoints.yaml` con `method`, `path`, `payload`, `expected_status`, `required_keys`
- `conftest.py`: `base_url`, timeout, retry, helper JSON

Regole pratiche:
- Per endpoint mutativi: sempre rollback/ripristino stato
- Separare test rapidi da test hardware (`pytest -m hw` o `-m slow`)
- Loggare richiesta/risposta/tempo per diagnosi

Primo MVP:
- Smoke completo di tutte le route `/api/test/*` e `/api/config/*`
- 3 flow critici: SD, seriale unificato, backup config su SD
- Report `junit.xml` + riepilogo markdown

---

## ⏸️ RITARDATI

1. Abilitazione WiFi
2. Display minimale 1.44"
3. Connessione HTTPS

---

## ✅ FATTI

1. Doxygen per la documentazione automatica
2. Gestione doppia partizione per factory reset/upload/testing e per produzione
3. Gestione Ethernet
4. Gestione Pannello Web Factory
5. Driver I/O Expander
6. Driver LED
7. Driver MDB e protocollo 9 bit
8. Classe configurazione dispositivo con parametri: nome, luminosità, Ethernet, WiFi, rete, JSON aggiuntivi; pagina LVGL+HTML con tastiera virtuale e funzioni Salva/Annulla/Backup/Restore
9. Funzioni init periferiche (HARDWARE_SETUP), task per periferica, utility di test via Web
10. Driver EEProm 24LC16BT-I/OT con I²C e gestione CRC
11. EEProm per memorizzare config:
    - CRC sui dati; verifica all'avvio
    - Fallback su NVS se EEProm non valida; fallback su default se NVS assente
    - Salvataggio su EEProm e flag "modificato" al tasto Save
    - Salvataggio in NVS al riavvio successivo (se valido)
12. Driver PWM
13. Unificazione codice APP/FACTORY con flag `COMPILE_APP` (eliminata duplicazione cartelle)
14. Driver RS485 e ricezione dati (timeout hardware 10 ms)
15. Test EEProm: tasto "Leggi JSON" e visualizzazione CRC/Updated nell'interfaccia web
16. Scheda SD implementata
17. Settaggio manuale Frequenza/Duty per test PWM
18. Controllo colore e luminosità per tutti i LED nel test LED
19. Driver SD con log dettagliati, power cycle hardware e monitor hot-plug (GPIO 0)
20. Driver sensore temperatura SHT40-BDIB-R2 con lettura background e visualizzazione Web UI
21. GPIO 32 e 33 configurabili in Config e Test (IN/OUT, Pull, State)
22. Data e ora da NTP (`2.it.pool.ntp.org`, `2.europe.pool.ntp.org`): acquisizione condizionata a connettività; timestamp nei log e nel titolo pagine web
23. Log remoto via UDP con pagina HTML dedicata
24. Rotazione schermo 90° CCW e controllo luminosità in /config
25. Avvio app: init I²C e attivazione I/O Expander; controllo GPIO3 dell'expander 1 (blocco se = 0)
26. Web /config: abilitazione e configurazione scanner; /test: letture e tasti Scanner ON/OFF; /tasks: verifica task
27. Esclusione display MIPI e LVGL per uso senza interfaccia grafica (abilitazione in /config)
28. Filtraggio log remoto per stringa (textbox + tasto "Filtra")
29. CCTalk: seriale attivata + task di background (UART = RS232, baud 4800, TX=GPIO20, RX=GPIO21); integrato in /config e /test
30. Pagina /httpservices: layout riquadri come /config; textbox Richiesta/Risposta in cima
31. Flash OTA via VS Code (`Flash OTA (upload bin via /ota/upload)`, `Flash OTA (trigger URL via /ota)`)
32. Controllo luminosità schermo in /config (live update + persistenza)
33. Ripristino schermo verticale (portrait)
34. Clear monitor seriale: pulizia aree testo + formato output (TEXT compatto, HEX con prefisso '.')
35. Emulatore: pagina /emulator con layout pannello utente (tasti programmi, credito, messaggi, gauge, coin virtuali, indicatori relay) e predisposizione comandi hardware; tasti Crediti QR, Crediti Tessera, Monete 1/2 coin
36. Tabella testi UI in NVS/EEPROM + funzione multilingua interfaccia utente
37. Tabella Programmi: campo numerico "tempo massimo pausa in un ciclo"
38. Log errori su SD:
    - `app.log` giornaliero circolare 30 gg con invio via `api/deviceactivity` (se switch "Invia Log" attivo)
    - `ERROR.log` per crash con stack trace (nome file = timestamp)
    - In assenza SD: solo invio al server remoto
    - `activity.json` in SPIFFS, caricato in PSRAM al boot; API `device_activity_find()`
39. FSM per la gestione del ciclo operativo della macchina (vedi `FSM.md`)
40. Schermata "Fuori servizio" (font 48px) attivata su `ERROR_LOCK` (reboot consecutivi ≥ 3)
41. Coda eventi FSM — estensione struttura: campi `from`, `to[10]`, `action_id` aggiunti a `fsm_input_event_t`; tabella agenti e azioni compilata; mailbox condivisa con lock; API `publish_ex`/`claim_for_agent`/`ack_for_agent`
42. Analisi criticità coda eventi FSM: modello mailbox, campi strutturali, tabella agenti/azioni, init centralizzato, metriche saturazione, log standardizzati, unificazione code valutata
43. File manager SD/SPIFFS: upload, download e cancellazione file su root; accessibile da tasto Home in App e Factory
44. Scanner USB integrato nella coda messaggi; lettura QR → login automatico + `api_payment_post` via `http_services`
45. Chiamate server remoto: header `Date` runtime, relogin automatico su 401/403, HTTPS con validazione certificato, coda offline persistente con retry/backoff, task periodico keepalive, metriche esposte via API/UI

---

## 🖥️ TESTATI

1. LED Strip
2. PWM1 e PWM2
3. RS232 (TX/RX)
4. RS485 (TX)
5. MDB (TX)
6. Scanner USB

---

## 🔧 REFACTORING

### Migrazione Web UI statica su SD card

I file HTML/JS/CSS sono attualmente inclusi nel codice C (`web_ui.c`) → occupano spazio nella partizione app del firmware.

**Obiettivo**: spostare il contenuto web statico su file separati nella SD card (montata via FATFS).

**Stima risparmio firmware**: ~50 KB – 500 KB a seconda del volume attuale delle pagine embedded.

**Approccio**:
- Montare SD con `esp_vfs_fat_sdcard_create()`
- Configurare httpd per servire risorse da directory SD
- Cache opzionale in PSRAM per asset ad alto traffico

**Vantaggi**:
- Riduzione firmware: meno dati nel binary
- Aggiornamenti senza ricompilare
- RAM ottimizzata: nessun caricamento completo in memoria volatile
- Scalabilità: facile aggiunta di asset (immagini, font)

**Considerazioni**:
- Latenza lettura SD superiore a flash → usare buffering adeguato
- Fallback su pagina statica minimale hardcoded in caso di SD assente/danneggiata
- Monitorare uso PSRAM per evitare saturazione

**Passi**:
- [ ] Identificare file HTML/JS da migrare
- [ ] Calcolare dimensione attuale del contenuto embedded (via `objcopy` o `elf2hex`)
- [ ] Implementare FATFS su SD e configurare httpd per asset statici
- [ ] Aggiornare configurazione build ESP-IDF per risorse esterne vs inline

---

## 🌐 INTERNAZIONALIZZAZIONE

### A. Riorganizzazione struttura tabelle i18n

1. Rimuovere il campo `lang` dai JSON (ridondante) e aggiornare i file
2. Impostare il campo `scope` come `uint8_t` ID e aggiornare i file
3. Impostare il campo `key` come `uint16_t` ID univoco per lingua e aggiornare i file
4. Aggiungere campo `section` (`uint8_t`) per concatenazione automatica di stringhe > 32 byte (record con stessa key ordinati per section vengono concatenati)
5. Chiave unificata a 32 bit: `ssssssss kkkkkkkk kkkkkkkk ssssssss`
6. Campo `text` con lunghezza massima 32 byte incluso `\0`
7. Mantenere i vecchi JSON e aggiornare i record con un campo `id` generato dalla conversione
8. Creare nuove funzioni per ricerca, concatenazione e restituzione stringhe
9. Aggiornare tutto il codice: sostituire la ricerca per `scope+key` alfanumerico con quella numerica; usare i vecchi file come tabella di conversione
10. Script Python di confronto: partendo dai file `.bak`, ricostruisce le stringhe tramite la tabella sync e genera un file `.md` con i confronti per tutte le lingue
11. Header `i18n_map_defs.h` come documento di origine delle tabelle JSON:
    - Un `#define` per ogni stringa (`I18N_SCOPE_NAV`, `I18N_KEY_NAV`)
    - `scope_id`, `key_id`, nomi letterali (oggi nei `*.map.json`), testi in tutte le lingue (it, en, de, fr, es + estendibile)
    - Funzioni di verifica e rigenerazione dei file JSON
    - Esempio entry:
      ```c
      #define LANGUAGES        "[\"it\",\"en\",\"de\",\"fr\",\"es\"]"
      #define I18N_SCOPE_NAV   3
      #define I18N_KEY_HOME    5
      #define I18N_SCOPE_TEXT  "nav"
      #define I18N_KEY_TEXT    "home"
      #define I18N_VALUES      "{\"it\":\"Casa\",\"en\":\"Home\",\"de\":\"Haus\"}"
      ```
12. Gestione 2 ambienti lingua distinti: **pannello LVGL** e **backend Web UI**
    - All'avvio: caricare tutte le lingue in PSRAM per cambio rapido sul pannello
    - Web UI: usa lingua separata nella composizione delle pagine
    - In /config sezione Device: combo "Lingua Pannello Utente" e combo "Lingua Backend"
    - Verificare che ciascuna interfaccia usi la propria lingua

---

### B. Eliminazione file `i18n_<lang>.map.json` dallo SPIFFS

#### Contesto

I file `/spiffs/i18n_<lang>.map.json` (it, en, de, fr, es) contengono la mappatura ID numerico → nome simbolico per **scope** (9 entries) e **key** (476 entries). Tutti e 5 i file sono **byte-per-byte identici** (il map non è localizzato). Vengono letti a runtime da SPIFFS per risolvere ID numerici in nomi testuali durante la costruzione delle tabelle i18n.

**Spazio SPIFFS liberato**: 5 × 17.775 bytes = **86.8 KB**

#### Problema attuale

- I/O su SPIFFS a runtime (lento, soggetto a errori di montaggio)
- Allocazione heap per il cJSON parse tree (cache volatile)
- 5 file identici replicati senza motivo
- Rischio desync tra file su SPIFFS e costanti nel codice C

#### Implementazione

**Step 1 — Creare `components/device_config/include/i18n_map_defs.h`**

```c
// Scope IDs (1-based)
#define I18N_SCOPE_HEADER     1
#define I18N_SCOPE_LVGL       2
#define I18N_SCOPE_NAV        3
#define I18N_SCOPE_P_CONFIG   4
#define I18N_SCOPE_P_EMULATOR 5
#define I18N_SCOPE_P_LOGS     6
#define I18N_SCOPE_P_PROGRAMS 7
#define I18N_SCOPE_P_RUNTIME  8
#define I18N_SCOPE_P_TEST     9
#define I18N_SCOPE_COUNT      9

// Key IDs (1-based, 476 entries)
#define I18N_KEY_TIME_NOT_SET   1
#define I18N_KEY_TIME_NOT_AVAIL 2
// ... tutti i 476 #define ...
#define I18N_KEY_COUNT        476

// Lookup O(1) da array in flash (rodata)
static inline const char *i18n_scope_name(int id);  // NULL se fuori range
static inline const char *i18n_key_name(int id);    // NULL se fuori range

// Lookup inverso nome→ID (O(n) su array in flash, senza heap)
int i18n_scope_id(const char *name);  // 0 se non trovato
int i18n_key_id(const char *name);    // 0 se non trovato
```

**Step 2 — Modifiche a `device_config.c`**

Rimuovere (~80 righe): `s_i18n_map_cache`, `s_i18n_map_lang`, `_i18n_map_cache_clear()`, `_build_i18n_map_path()`, `_i18n_map_cache_build_for_lang()`, `_lookup_map_text_from_id()`

In `_build_i18n_table_for_lang()` (righe 308–345):
```c
// Prima:
if (_i18n_map_cache_build_for_lang(lang) == ESP_OK && s_i18n_map_cache) { ... }
const char *scope_text = _lookup_map_text_from_id(map_scopes, scope_id);
const char *key_text   = _lookup_map_text_from_id(map_keys, key_id);

// Dopo:
const char *scope_text = i18n_scope_name(scope_id);
const char *key_text   = i18n_key_name(key_id);
```

In `device_config_get_ui_text_scoped()` (righe 1738–1760):
```c
// Prima: doppio cJSON_ArrayForEach per risolvere nome→ID
if (_i18n_map_cache_build_for_lang(NULL) == ESP_OK && s_i18n_map_cache) { ... }

// Dopo:
uint8_t  scope_id = (uint8_t)i18n_scope_id(scope);
uint16_t key_id   = (uint16_t)i18n_key_id(key);
if (scope_id != 0 && key_id != 0) {
    char *concat = i18n_concat_from_psram(scope_id, key_id);
    // ...
}
```

**Step 3 — Modifiche a `web_ui_common.c`**

Rimuovere (~70 righe): `load_i18n_map_json()`, `lookup_map_name()`

In `build_i18n_table_json()` (righe 722–754):
```c
// Prima:
cJSON *map_root = load_i18n_map_json(language);
const char *scope_text = lookup_map_name(map_root, "scopes", scope_id);
const char *key_text   = lookup_map_name(map_root, "keys", key_id);
if (map_root) cJSON_Delete(map_root);

// Dopo:
const char *scope_text = i18n_scope_name(scope_id);
const char *key_text   = i18n_key_name(key_id);
```

Nel filtro scope (riga 957):
```c
// Prima:
cJSON *map_root = load_i18n_map_json(language);
scope_name = lookup_map_name(map_root, "scopes", scope->valueint);

// Dopo:
scope_name = i18n_scope_name(scope->valueint);
```

**Step 4 — Rimuovere i file da SPIFFS**

- Eliminare da `data/`: `i18n_it.map.json`, `i18n_en.map.json`, `i18n_de.map.json`, `i18n_fr.map.json`, `i18n_es.map.json`
- Aggiornare `CMakeLists.txt` / definizione partizione SPIFFS

#### Riepilogo

| Cosa | Azione | File |
|---|---|---|
| 5 file `*.map.json` | eliminare | `data/` |
| ~80 righe infrastruttura map | rimuovere | `device_config.c` |
| ~70 righe infrastruttura map | rimuovere | `web_ui_common.c` |
| 7 call site lookup | sostituire con array in flash | `device_config.c` (4), `web_ui_common.c` (3) |
| Nuovo header | creare | `i18n_map_defs.h` |

| Risorsa | Prima | Dopo |
|---|---|---|
| SPIFFS | 86.8 KB | 0 KB |
| Heap runtime (cJSON cache) | allocato per ogni cambio lingua | 0 KB |
| I/O SPIFFS al runtime | fopen + fread + parse | nessuno |
| Ridondanza file | 5 file identici | 1 header |

---

## 🐛 ERRORI NOTI

### Task Watchdog — IDLE0 CPU0

**Sintomo**: `E task_wdt: Task watchdog got triggered — IDLE0 (CPU 0)`
**Causa**: crash nel driver MIPI DSI (`mipi_dsi_hal_host_gen_read_short_packet`) durante init display.
**Contesto**: `ESP_ERROR_CHECK` fallisce su `bsp_touch_new()` → `GT911 init failed` (`ESP_ERR_INVALID_STATE 0x103`).
**Conseguenza**: abort + core dump salvato in flash → reboot.

**Stack rilevante**:
```
esp32_p4_nano.c:777 — bsp_display_indev_init → bsp_touch_new
i2c_common.c:462    — i2c_common_deinit_pins
```

**Azione**: verificare connessione hardware touch GT911 (I²C), controllare alimentazione e init I²C prima dell'init display.

---

## 📡 Riferimenti

### API Server Base

- Base URL: `http://195.231.69.227:5556/`
- Auth: `Authorization: Bearer <token>` (ottenuto da `/api/login`)

| Endpoint | Metodo |
|---|---|
| `/api/login` | POST |
| `/api/keepalive` | POST |
| `/api/payment` | POST |
| `/api/paymentoffline` | POST |
| `/api/serviceused` | POST |
| `/api/deviceactivity` | POST |
| `/api/getconfig` | POST |
| `/api/getimages` | POST |
| `/api/getfirmware` | POST |
| `/api/gettranslations` | POST |

Esempio login:
```http
POST /api/login HTTP/1.1
Host: 195.231.69.227:5556
Content-Type: application/json

{
    "serial": "AD-34-DFG-333",
    "password": "c1ef6429c5e0f753ff24a114de6ee7d4"
}
```

### Scanner QR — Comandi seriali

```
Setup:  ~\x01 0000#SCNMOD3;RRDENA1;CIDENA1;SCNENA0;RRDDUR3000; \x03
State:  ~\x01 0000#SCNENA*; \x03
On:     ~\x01 0000#SCNENA1; \x03
Off:    ~\x01 0000#SCNENA0; \x03
```

### App — Passi successivi

1. Aggiungere task App per funzioni operative della macchina controllata
2. Configurazione monitor/touch e integrazione LVGL
3. Creazione slideshow iniziale
4. Creazione pagina di test operazioni

# SPECIFICHE

### 1. Funzionamento macchina — logica credito e ciclo operativo

#### 1.1 Credito

Il credito è espresso in coin (1 coin = 1€). Le sorgenti sono:

- **Monete/gettoni**: contribuiscono a `ecd` (credito effettivo definitivo, non rimborsabile)
- **Tessera precaricata**: 1 `ecd` all'inserzione; credito rimanente come `vcd` (virtuale) se lasciata nel lettore
- **QR code precaricato**: contribuisce a `ecd` (1 coin per volta)

#### 1.2 Variabili di erogazione

| Variabile | Descrizione                                              |
| --------- | -------------------------------------------------------- |
| `vpp`     | Valore prezzo programma (crediti per ciclo)              |
| `vtp`     | Valore tempo programma (secondi)                         |
| `vtt`     | Ticks totali = `vpp × 60`                                |
| `rdu`     | Rateo utilizzo = `vtt / vtp` ticks/sec                   |
| `tts`     | Time-to-stop: tempo rimanente (aggiornato ogni secondo)  |
| `vtr`     | `tts / rdu` (aggiornato ogni secondo)                    |
| `ttp`     | Tempo pausa utilizzato                                   |
| `ecd`     | Credito effettivo prelevato = `vpp`                      |
| `vcd`     | Credito virtuale (solo con tessera lasciata nel lettore) |

#### 1.3 Flusso operativo (pay-before-use)

1. Cliente inserisce monete/tessera/QR → credito acquisito
2. Cliente sceglie programma → scala `vcd`, calcola `vpp/vtp/vtt/rdu/tts/vtr`
3. Macchina eroga; aggiorna valori ogni secondo
4. Cliente può sospendere: durata massima pausa configurabile
5. Cliente può cambiare programma durante l'erogazione → ricalcolo `rdu` e relay mask
6. Al termine del tempo: stop erogazione; se `vcd` residuo, attende nuova selezione
7. Credito residuo da monete: disponibile al cliente successivo; da tessera: richiede riapresentazione se tolta; da QR: richiede riapresentazione

#### 1.4 Visualizzazione

- Programma attivo: sfondo **rosso** in run, **giallo** in pausa
- Area centrale: mostra `ecd` prima della selezione, poi `vtp` fino all'avvio, poi `tts` durante run
- Sotto area centrale: `vtp` fino all'avvio, poi `vtt−tts` in run; in pausa: `ttp`
- Barra laterale (+ LED strip): percentuale consumo `(vtt−ticks_usati)×100/vtt`; verde 100%→30%, rosso 29%→0%
- All'avvio programma: credito scompare se era 1, altrimenti appare ridotto (30%); secondi disponibili in grande (100%)
- Credito iniziale mostrato nell'area centrale (carattere grande)

#### 1.5 Vincoli

- 1 solo ciclo selezionabile alla volta (no prenotazione multipla)
- Cambio programma in corso: relay mask ricalcolata e aggiornata immediatamente
