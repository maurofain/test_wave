# Documento: come splittare `components/web_ui/web_ui.c` in moduli più piccoli

## Obiettivi dello split
- Ridurre la dimensione/complessità di `web_ui.c` separando **pagine HTML**, **API REST**, **utility comuni**, **log store**, **test endpoints**.
- Migliorare manutenibilità (ogni area funzionale in un file dedicato).
- Minimizzare dipendenze incrociate: ogni modulo registra i propri URI sul server HTTP.

---

## Stato attuale (macro-aree presenti in `web_ui.c`)
In `components/web_ui/web_ui.c` convivono:
- **Core server**: `web_ui_init()`, `web_ui_stop()`, `s_server`, registrazione URI.
- **HTML/templating**: `send_head()`, `HTML_NAV`, `HTML_STYLE_NAV`, grandi stringhe HTML+JS delle pagine.
- **Pagine**: `/`, `/config`, `/stats`, `/tasks`, `/test`, `/logs`, `/ota`, `/logo.jpg`.
- **API**:
  - Config: `/api/config`, `/api/config/save`, `/api/config/reset`, `/api/config/backup`
  - NTP: `/api/ntp/sync`
  - Tasks: `/api/tasks`, `/api/tasks/save`, `/api/tasks/apply`
  - Logs: `/api/logs`, `/api/logs/receive`, `OPTIONS /api/logs/*`
  - Debug USB: `/api/debug/usb/enumerate`, `/api/debug/usb/restart`
  - Test multiplexer: `POST /api/test/*` (LED/PWM/IO/RS232/RS485/MDB/SD/SHT40/GPIO/serial monitor…)
- **Stato condiviso**: ring buffer log (`stored_logs`, `log_count`, `log_index`) e `web_ui_add_log()`.

---

## Struttura proposta (file + responsabilità)

### 1) Core server + route registration
**File:** `components/web_ui/web_ui_server.c`  
**Contiene:**
- `static httpd_handle_t s_server`
- `esp_err_t web_ui_init(void)`
- `void web_ui_stop(void)`
- chiamate a funzioni “register” dei moduli (es. `web_ui_register_pages(server)`)

**Idea chiave:** `web_ui_init()` non conosce i dettagli di ogni handler, ma delega.

---

### 2) Utility comuni (header HTML, IP helpers, OTA helper, asset logo)
**File:** `components/web_ui/web_ui_common.c`  
**Contiene:**
- `send_head(req, title, extra_style, show_nav)` (rinominala ad es. `web_ui_send_head` se vuoi)
- `ip_to_str(...)`
- `perform_ota(url)` (helper usato da `ota_post_handler`)
- `logo_get_handler` (opzionale: può stare qui o in “pages/assets”)

**Nota:** `send_head()` è usata da quasi tutte le pagine: è un ottimo candidato per diventare “utility comune”.

---

### 3) Log store + API logs + pagina logs
**File:** `components/web_ui/web_ui_logs.c`  
**Contiene:**
- struttura `stored_log_t`, array circolare + funzioni di append/iterate
- `void web_ui_add_log(...)` (API pubblica già in `web_ui.h`)
- `api_logs_get`, `api_logs_receive`, `api_logs_options`
- `logs_page_handler`

**Vantaggio:** il ring buffer log diventa “proprietà” del modulo logs; gli altri moduli chiamano solo `web_ui_add_log()`.

---

### 4) Pagine HTML (split per pagina o per gruppo)
Puoi scegliere tra:
- **A)** 1 file “pages” unico (medio): `web_ui_pages.c`
- **B)** 1 file per pagina (più pulito):  
  - `web_ui_page_root.c`
  - `web_ui_page_config.c`
  - `web_ui_page_stats.c`
  - `web_ui_page_tasks.c`
  - `web_ui_page_test.c`
  - `web_ui_page_ota.c`

**Consiglio pratico:** visto che `/config` e `/test` contengono enormi stringhe JS/HTML, lo split per pagina dà benefici immediati.

Ogni file contiene:
- handler `*_page_handler`
- eventuali helper locali `static` (se servono solo lì)

---

### 5) API config (+ NTP sync)
**File:** `components/web_ui/web_ui_api_config.c`  
**Contiene:**
- `api_config_get`
- `api_config_save`
- `api_config_reset`
- `api_config_backup`
- `api_ntp_sync`

**Nota importante:** `api_config_save` oggi gestisce sia config completa sia update parziali (es. salvataggio luminosità). Valuta se rendere esplicito il comportamento nel doc/commenti, ma non è necessario per lo split.

---

### 6) API tasks
**File:** `components/web_ui/web_ui_api_tasks.c`  
**Contiene:**
- `api_tasks_get`
- `api_tasks_save`
- `api_tasks_apply`

---

### 7) API debug USB
**File:** `components/web_ui/web_ui_api_debug.c`  
**Contiene:**
- `api_debug_usb_enumerate`
- `api_debug_usb_restart`

---

### 8) API test multiplexer (`/api/test/*`)
**File:** `components/web_ui/web_ui_api_test.c`  
**Contiene:**
- `api_test_handler`
- eventuali task handle e task function (`uart_test_task`, `s_rs232_test_handle`, `s_rs485_test_handle`)

**Nota:** qui dentro c’è tanta logica “hardware test”; è coerente tenerla separata da UI/config/logs.

---

## Pattern consigliato: ogni modulo “registra” i propri endpoint
Per evitare dipendenze circolari, fai in modo che *ogni file* esponga una sola funzione:

```c
// web_ui_pages.h (o web_ui_internal.h)
esp_err_t web_ui_register_pages(httpd_handle_t server);
```

e dentro `web_ui_register_pages()` definisci e registri i `httpd_uri_t`.

Stesso per API:

```c
esp_err_t web_ui_register_api_config(httpd_handle_t server);
esp_err_t web_ui_register_api_tasks(httpd_handle_t server);
esp_err_t web_ui_register_api_logs(httpd_handle_t server);
esp_err_t web_ui_register_api_test(httpd_handle_t server);
esp_err_t web_ui_register_api_debug(httpd_handle_t server);
```

Poi `web_ui_init()` fa solo orchestration.

---

## Header: pubblico vs interno

### Header pubblico (già esistente)
`components/web_ui/include/web_ui.h` dovrebbe restare minimale:
- `web_ui_init()`
- `web_ui_stop()`
- `web_ui_add_log()`

### Header interno (nuovo, non in `include/`)
Aggiungi un header interno (es. `components/web_ui/web_ui_internal.h`) per condividere solo ciò che serve tra moduli, ad esempio:
- `esp_err_t web_ui_send_head(...)`
- eventuali helper comuni
- funzioni `web_ui_register_*`

---

## Aggiornamento `CMakeLists.txt`
Oggi hai solo:

```cmake
idf_component_register(
    SRCS "web_ui.c"
    INCLUDE_DIRS "include"
    ...
)
```

Dopo lo split, elenca i nuovi `.c`:

```cmake
idf_component_register(
    SRCS
        "web_ui_server.c"
        "web_ui_common.c"
        "web_ui_logs.c"
        "web_ui_api_config.c"
        "web_ui_api_tasks.c"
        "web_ui_api_test.c"
        "web_ui_api_debug.c"
        "web_ui_page_root.c"
        "web_ui_page_config.c"
        "web_ui_page_stats.c"
        "web_ui_page_tasks.c"
        "web_ui_page_test.c"
        "web_ui_page_ota.c"
    INCLUDE_DIRS "include"
    ...
)
```

---

## Sequenza di refactor (a basso rischio)
1. **Crea `web_ui_server.c`** e sposta lì `web_ui_init/web_ui_stop` + `s_server`.
2. **Crea `web_ui_common.c`** e sposta `send_head`, `HTML_NAV`, `HTML_STYLE_NAV`, `ip_to_str`, `perform_ota`, `logo_get_handler` (se vuoi).
3. **Crea `web_ui_logs.c`** e sposta ring buffer + `web_ui_add_log` + handlers logs.
4. **Sposta API config/tasks/debug/test** ognuna nel proprio file.
5. **Sposta le pagine**: inizia da quelle più grandi (`/config`, `/test`) perché sono quelle che “pesano” di più.
6. Aggiorna `components/web_ui/CMakeLists.txt`.
7. Build/test: avvia firmware e verifica che tutti gli endpoint registrati in `web_ui_init()` rispondano.

---

## Cose a cui fare attenzione (classiche nello split in C)
- Funzioni oggi `static` usate altrove: quando le sposti, o restano `static` nel file nuovo, oppure diventano **prototipi in `web_ui_internal.h`**.
- Stato globale condiviso (es. log buffer): idealmente “vive” in un solo modulo ed è manipolato tramite funzioni.
- Include: ogni file includa solo ciò che serve (riduce tempi di build e dipendenze).
- `config.max_uri_handlers`: se lo split aggiunge URI, assicurati che resti adeguato.
