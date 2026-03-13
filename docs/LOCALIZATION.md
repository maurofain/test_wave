# LOCALIZATION (i18n) - schema operativo

Questo documento descrive come viene applicata la localizzazione alle pagine web nel firmware.

## Flusso end-to-end

1. Richiesta pagina (`GET /config`, `/stats`, `/files`, ecc.)
2. Rendering header con `send_head(...)`
3. Traduzione server-side di alcune stringhe comuni (`nav`, `header`) via `device_config_get_ui_text_scoped(...)`
4. Iniezione script i18n runtime con `send_i18n_runtime_script(req)`
5. Il browser esegue lo script i18n e applica traduzioni al DOM
6. Un `MutationObserver` applica automaticamente la traduzione anche ai nodi inseriti dopo il load

---

## Backend (C)

### 1) Scelta lingua
- Default: `cfg->ui.backend_language`
- Override possibile via query `?lang=xx` su endpoint i18n
- Endpoint: `GET /api/ui/texts`

Riferimenti:
- `components/web_ui/web_ui.c` (`api_ui_texts_get`)

### 2) Scope pagina
Per ridurre payload e applicare solo testi pertinenti, lo scope viene derivato da URI (es. runtime, logs, config, ...).

Riferimenti:
- `components/web_ui/web_ui_common.c` (`i18n_scope_for_uri`)

### 3) Costruzione tabella traduzioni
- Lettura record lingua da storage (`device_config_get_ui_texts_records_json(language)`)
- Filtro per scope
- Fallback su italiano (`it`) per chiavi mancanti
- Serializzazione tabella finale JSON

Riferimenti:
- `components/web_ui/web_ui_common.c`
  - `filter_i18n_records_json_for_scope(...)`
  - `build_i18n_table_json(...)`

### 4) Cache i18n
La tabella filtrata viene cachata per coppia `lang + scope` per evitare parsing ripetuto.

Riferimenti:
- `components/web_ui/web_ui_common.c`
  - `i18n_cache_get(...)`
  - `i18n_cache_put(...)`
  - `web_ui_i18n_cache_invalidate(...)`

### 5) Iniezione nello HTML
`send_head()` invia header e subito dopo aggiunge lo script i18n runtime (`WEBPAGE_COMMON_I18N_SCRIPT_FMT`) con lingua + tabella.

Riferimenti:
- `components/web_ui/web_ui_common.c` (`send_head`, `send_i18n_runtime_script`)
- `components/web_ui/webpages_embedded.c` (`WEBPAGE_COMMON_I18N_SCRIPT_FMT`)

---

## Frontend (JS runtime)

Lo script runtime crea `window.uiI18n` con:
- `language`
- `table`
- `translate(text)`
- `apply(root)`

### Cosa traduce
- Testo dei nodi
- Attributo `data-i18n`
- Attributi: `placeholder`, `title`, `aria-label`, `value`

### Quando traduce
- su `DOMContentLoaded`
- su nuovi nodi via `MutationObserver`

Riferimento:
- `components/web_ui/webpages_embedded.c` (`WEBPAGE_COMMON_I18N_SCRIPT_FMT`)

---

## Cambio lingua a runtime

Dopo salvataggio config (`/api/config/save`):
- se cambia backend language: invalidazione cache i18n e reload tabella backend
- se cambia user language: invalidazione cache e refresh testi pannello LVGL

Riferimenti:
- `components/web_ui/web_ui.c` (blocco post-save lingua)

---

## Schema sintetico

`HTTP GET pagina`  
`-> send_head()`  
`-> send_i18n_runtime_script(req)`  
`-> [lang + scope] -> cache/filter/fallback`  
`-> script JS con tabella`  
`-> apply(document.body)`  
`-> MutationObserver(apply nuovi nodi)`
