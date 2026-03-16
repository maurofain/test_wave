# Multilingua UI: implementazione e utilizzo

## Obiettivo
Implementare una base multilingua con:
- catalogo strutturato (`scope`, `key`, `legacyId`, `text.<lang>`),
- storage esclusivo su SPIFFS tramite **unico file** `/spiffs/i18n_v2.json`,
- builder runtime che produce i record legacy per PSRAM/Web a partire dal catalogo,
- recupero centralizzato testi per `scope+key`.

## Architettura implementata
La funzionalità usa la configurazione esistente solo per la lingua corrente.
I testi non sono più salvati nel JSON config (NVS/EEPROM), ma in file dedicati su SPIFFS.

Componenti coinvolti:
- `components/device_config/include/device_config.h`
- `components/device_config/device_config.c`
- `components/web_ui/web_ui_internal.h`
- `components/web_ui/web_ui.c`
- `components/web_ui/web_ui_common.c`

## Modello dati
La sezione `ui` in `device_config_t` mantiene la lingua corrente:

- `ui.language` (es. `it`, `en`)

Le stringhe risiedono nel catalogo JSON `data/i18n_v2.json`, poi copiato su SPIFFS come `/spiffs/i18n_v2.json` durante il build.

### Estratto catalogo v2
```json
{
  "web": {
    "nav": {
      "001": {
        "label": "nav.home",
        "legacyId": { "scope": 1, "key": 1 },
        "text": {"it": "Home", "en": "Home"}
      }
    }
  },
  "lvgl": {
    "001": {
      "label": "lvgl.time_not_available",
      "legacyId": { "scope": 9, "key": 15 },
      "text": {"it": "Ora non disponibile", "en": "Time unavailable"}
    }
  }
}
```

## Persistenza
La tabella i18n è salvata solo su SPIFFS:
- catalogo unico `/spiffs/i18n_v2.json`
- nessuna persistenza testi in NVS/EEPROM

Regola manutentiva obbligatoria:
- ogni nuova stringa UI inserita nel codice deve essere aggiunta in `data/i18n_v2.json` (sezione corretta) prima del merge.

NVS/EEPROM continuano a gestire solo `ui.language` (lingua selezionata).

## API introdotte/estese

### API nuove
- `GET /api/ui/texts`
  - restituisce lingua e `records` (array record i18n).

### API estese
- `GET /api/config`
  - include `ui.language` (e metadato storage).
- `POST /api/config/save`
  - salva `ui.language`; opzionalmente salva `ui.records` nel file lingua SPIFFS.
- `POST /api/config/backup`
  - include solo metadati `ui` (non duplica la tabella).

## Funzioni di supporto aggiunte
In `device_config`:
- `device_config_get_ui_language()`
- `device_config_get_ui_texts_records_json(language)`
- `device_config_set_ui_texts_records_json(language, records_json)` *(solo per import legacy)*
- `device_config_get_ui_text_scoped(scope, key, fallback, out, out_len)`

`device_config_get_ui_text_scoped(...)`:
- cerca record per lingua corrente + `scope` + `key`,
- se trovata ritorna `ESP_OK`,
- se non trovata usa `fallback` e ritorna `ESP_ERR_NOT_FOUND`.

## Come viene usato nella UI
In `send_head(...)` (`web_ui_common.c`) i testi della navbar/header non sono più hardcoded:
- vengono letti a runtime tramite `device_config_get_ui_text_scoped(...)`.

Chiavi attualmente usate in header/navbar/LVGL:
- `header.time_not_set`
- `nav.home`, `nav.config`, `nav.stats`, `nav.tasks`, `nav.logs`, `nav.test`, `nav.ota`, `nav.emulator`
- `lvgl.time_not_available`

## Flusso operativo tipico
1. Client legge configurazione (`GET /api/config` oppure `GET /api/ui/texts`).
2. Client modifica `ui.language` e/o `ui.records`.
3. Client salva con `POST /api/config/save`.
4. Il backend ricostruisce i record dalla cache PSRAM/catalogo e aggiorna `i18n_v2.json` (se previsto dallo strumento) oppure solo la lingua selezionata.

## Note e limiti attuali
- Il modello record è pronto e persistente su SPIFFS per lingua.
- Le pagine con markup esteso richiedono progressiva migrazione a chiavi `scope+key` (o `data-i18n`).

## Estensioni consigliate
- Introdurre prefisso chiavi per pagina (es. `home.*`, `config.*`, `test.*`).
- Aggiungere endpoint di validazione chiavi mancanti.
- Portare progressivamente tutte le stringhe visibili in tabella multilingua.
- Aggiungere fallback lingua (`en` -> `it`) automatico per chiavi assenti.
