# Sistema di localizzazione (i18n)

Guida aggiornata al funzionamento reale del progetto `test_wave`, con distinzione tra lingua **Pannello Utente (LVGL)** e lingua **Backend/Web UI**.

## 1) Modello dati e sorgenti

- Catalogo traduzioni: unico file `/spiffs/i18n_v2.json` generato da `data/i18n_v2.json` (niente più file per lingua o map separate).
- Config UI: `ui.user_language` e `ui.backend_language`.
- Compatibilità: `ui_language` è mantenuto e mappato a `ui.user_language`.
- API centrale lookup testo: `device_config_get_ui_text_scoped(scope, key, fallback, out, out_len)`.

## 2) Flusso runtime per composizione interfacce

### 2.1 Web UI (backend language)

1. `send_head()` (in `web_ui_common.c`) costruisce header/nav e richiama `send_i18n_runtime_script(req)`.
2. `send_i18n_runtime_script(req)`:
   - legge la lingua con `device_config_get_ui_backend_language()`;
   - calcola lo scope pagina con `i18n_scope_for_uri()`;
   - carica i record costruiti on-the-fly da `device_config_get_ui_texts_records_json(lang)` partendo dal catalogo v2;
   - filtra per scope con `filter_i18n_records_json_for_scope()`;
   - crea tabella runtime con fallback italiano tramite `build_i18n_table_json()`;
   - usa cache tramite `i18n_cache_get()` / `i18n_cache_put()`.
3. Lo script JS runtime applica le traduzioni nel DOM (`window.uiI18n.apply`).

### 2.2 Pannello utente LVGL (user language)

1. All'avvio LVGL, la lingua runtime del pannello viene inizializzata da `ui.user_language` (default persistito).
2. La scelta lingua da bandierina sul pannello aggiorna solo la lingua runtime (`lvgl_panel_set_runtime_language(...)`), senza `device_config_save(...)`.
3. Le stringhe LVGL sono già in memoria (`lvgl_i18n`) e lo switch lingua cambia solo il selettore di rendering.
4. La combo web "Lingua Pannello Utente (LCD, default)" salva solo il default di partenza (`ui.user_language`) e non forza il cambio immediato sul display corrente.

### 2.3 Programmi (pagina emulator/programs)

- I nomi programma non sono più risolti da chiavi `__i18n__`.
- La sorgente è `programs.json` (persistenza runtime: `/spiffs/programs.json`, seed progetto: `data/programs.json`).
- Ogni programma contiene i campi per lingua (`name_it`, `name_en`, `name_fr`, `name_de`, `name_es`).
- In runtime il nome mostrato viene selezionato in base alla lingua attiva pannello/utente con fallback su `name_it`.

## 3) Funzioni usate in fase di composizione interfacce

Di seguito l’elenco completo delle funzioni coinvolte direttamente nella composizione/rendering testi:

### 3.1 Device config / lookup

- `device_config_get_ui_text_scoped(...)`
- `device_config_get_ui_texts_records_json(const char *language)` (builder dal catalogo v2)
- `device_config_get_ui_user_language(void)`
- `device_config_get_ui_backend_language(void)`
- `device_config_get_ui_language(void)` *(getter compatibilità legacy)*

### 3.2 Web UI i18n pipeline

- `send_head(...)`
- `send_i18n_runtime_script(httpd_req_t *req)`
- `i18n_scope_for_uri(const char *uri)`
- `i18n_scope_allowed(const char *scope, const char *page_scope)`
- `filter_i18n_records_json_for_scope(const char *records_json, const char *page_scope)`
- `build_i18n_table_json(const char *records_json, const char *base_records_json)`
- `i18n_cache_get(const char *lang, const char *page_scope)`
- `i18n_cache_put(const char *lang, const char *page_scope, const char *table_json)`
- `web_ui_i18n_cache_invalidate(void)`

### 3.3 Web UI PSRAM dictionary

- `web_ui_i18n_load_language_psram(const char *language)`
- `i18n_load_full_dictionary_psram(const char *language, size_t *out_count)`
- `i18n_concat_from_psram(uint8_t scope_id, uint16_t key_id)`

### 3.4 LVGL

- `lvgl_panel_refresh_texts(void)`

### 3.5 Utility elenco lingue disponibili

- `web_ui_extract_lang_from_filename(const char *name, char *out_lang, size_t out_len)`
- `web_ui_lang_label_from_code(const char *lang, char *label, size_t label_len)`
- `api_ui_languages_get(httpd_req_t *req)`

## 4) Cambio lingua da `/config`

Nel salvataggio configurazione (`api_config_save`):

- se cambia `ui.backend_language`:
  - `web_ui_i18n_cache_invalidate()`
  - `web_ui_i18n_load_language_psram(new_backend_lang)`
- se cambia `ui.user_language`:
  - viene aggiornato solo il valore persistito di default
  - nessun refresh immediato forzato del pannello LVGL

## 5) Elementi legacy rimossi

Durante il cleanup sono state rimosse le API legacy non usate:

- `device_config_get_ui_texts_json(void)`
- `device_config_get_ui_text(const char *key, const char *fallback, char *out, size_t out_len)`

Il percorso attivo usa esclusivamente:

- `device_config_get_ui_text_scoped(...)`
- `device_config_get_ui_texts_records_json(...)`

## 6) Regola operativa per nuove stringhe

Ogni testo mostrato in Web UI o LVGL deve essere censito in `data/i18n_v2.json` (sezione `web` o `lvgl`) con scope corretto (`nav`, `header`, `lvgl`, `p_config`, `p_runtime`, `p_emulator`, ecc.) e recuperato tramite API i18n, evitando stringhe hardcoded nei renderer.

Eccezione attuale: i **nomi dei programmi** sono gestiti nel dataset `programs.json` con campi multilingua dedicati, non nel catalogo `i18n_v2`.

## 7) Esempio reale: localizzazione stringa nella Home Web

Se la pagina `/` viene servita da `root_get_handler(...)` (file `web_ui_pages_runtime.c`), il percorso di localizzazione di una stringa è questo:

1. `root_get_handler(req)` invia l'header comune chiamando `send_head(req, home_title, extra_style, true)`.
2. `send_head(...)` richiama `send_i18n_runtime_script(req)` prima di completare la risposta.
3. `send_i18n_runtime_script(req)`:
  - legge la lingua backend con `device_config_get_ui_backend_language()`;
  - determina lo scope pagina con `i18n_scope_for_uri(req->uri)` (per `/` lo scope è `p_runtime`);
  - carica i record lingua con `device_config_get_ui_texts_records_json(lang)`;
  - filtra i record per scope con `filter_i18n_records_json_for_scope(...)`;
  - genera la tabella runtime con `build_i18n_table_json(...)` (con fallback base italiano);
  - pubblica in pagina `window.uiI18n = { language, table, apply, translate }`.
4. Quando il DOM è pronto, lo script esegue `apply(document.body)`:
  - traduce nodi testo;
  - traduce attributi (`placeholder`, `title`, `aria-label`, `value`);
  - traduce elementi con `data-i18n`.
5. Dopo il primo rendering, un `MutationObserver` intercetta nuovi nodi aggiunti dinamicamente e rilancia la traduzione.

In pratica, una stringa presente nell'HTML della Home viene sostituita con la versione localizzata quando trova una corrispondenza nella tabella runtime della lingua backend.

## 8) Workflow operativo obbligatorio (i18n)

Per mantenere il comportamento coerente tra Web UI e LVGL:

1. aggiungere/modificare sempre i testi in `data/i18n_v2.json` (mai stringhe hardcoded nei renderer);
2. valorizzare correttamente `scope`, `key` e `legacyId`;
3. tradurre tutte le lingue supportate prima del rilascio;
4. per modifiche sole Web UI/i18n eseguire deploy rapido con `idfc -fs` (flash SPIFFS), senza rebuild firmware completo.

## 9) Policy documentazione approvata

Per la documentazione che deve essere inclusa nei flussi approvati (sync/documentazione generata), usare file con prefisso `a_` nella cartella `docs/`.

Questa regola non cambia il runtime i18n, ma impatta la tracciabilità delle guide operative incluse nei processi di pubblicazione documentale.