# i18n v2 - Nuova architettura localizzazione (Web + LVGL)

## Obiettivo

Ristrutturare la localizzazione per migliorare prestazioni runtime, ridurre overhead di parsing, e mantenere una pipeline di sviluppo semplice.

Principi guida:
- lingua di riferimento: `it`
- separazione netta tra localizzazione `web` e `lvgl`
- sorgente unico mantenibile (JSON)
- ottimizzazione runtime tramite preload in PSRAM

---

## Stato attuale (sintesi)

- Le pagine web esistono in doppia forma:
  - embedded in C (`webpages_embedded.c`)
  - esterne (`/spiffs/www` o `/sdcard/www`)
- Le traduzioni sono in file `i18n_<lang>.json` su SPIFFS.
- Oggi il web usa una localizzazione runtime (script i18n + mapping per scope), mentre LVGL usa lookup puntuali.

Questa versione v2 introduce un modello più deterministico:
- pagine web localizzate una volta al boot (o al cambio lingua)
- pubblicazione diretta da cache in PSRAM
- niente parsing JSON durante la singola richiesta HTTP

---

## Decisione pipeline web (aggiornata)

Decisione approvata:
- eliminare il passaggio Python di conversione `embedded C -> html` nel flusso principale
- usare direttamente file HTML in `SPIFFS` come sorgente web (`HTML-first`)
- mantenere opzionalmente una pagina embedded minima solo come fallback di emergenza

Motivazioni:
- riduzione complessità toolchain
- eliminazione doppio mantenimento (`.c` + `.html`)
- migliore allineamento con preload/localizzazione in PSRAM

---

## Scelta architetturale approvata

### 1) Web

- I template web restano non localizzati in sorgente (`data/www/*.html`).
- Al boot si leggono template + moduli JS e si sostituiscono i placeholder i18n in base alla lingua backend corrente.
- Le pagine già localizzate vengono mantenute in PSRAM e servite direttamente.
- Al cambio lingua backend, la cache viene invalidata e rigenerata.

### 2) LVGL

- Numero ridotto di stringhe.
- Lookup al momento della pubblicazione schermata (o refresh) senza cache pesante di pagine.
- Manteniamo fallback su italiano per chiavi mancanti.

### 3) JavaScript

- Gli script vengono separati in file dedicati (`data/www/js/*.js`).
- I template possono includerli via `script src` oppure via include placeholder in fase build/preload.
- Gli script non devono più dipendere da localizzazione runtime DOM-based.

---

## Convenzione formale i18n v2

## 1) Regole generali

- Lingua base obbligatoria: `it`.
- Runtime web usa solo chiavi numeriche placeholder.
- Campo alfanumerico `label` è solo descrittivo/editoriale (mai usato dal parser runtime).
- Fallback obbligatorio: `lang richiesta -> it -> stringa vuota`.

## 2) Formato placeholder web

- Pattern ammesso: `{{NNN}}` dove `NNN` è numerico a 3 cifre.
- Regex valida: `^\{\{[0-9]{3}\}\}$`
- Range: `001..999` per ogni pagina/scope.
- Unicità: ogni key è unica all’interno della singola pagina.

## 3) Convenzione `label` alfanumerica

- Pattern: `<page>.<section>.<name>`
- Charset: `[a-z0-9_.]` (solo minuscole, numeri, `_`, `.`)
- Regex valida: `^[a-z0-9]+(\.[a-z0-9_]+){2,}$`
- Esempi validi:
  - `config.ui.backend_language.title`
  - `index.nav.programs.button`
  - `test.ioexpander.out8.label`

## 4) Schema JSON web (normativo)

```json
{
  "version": 2,
  "base_language": "it",
  "languages": ["it", "en", "fr", "de", "es"],
  "web": {
    "config": {
      "001": {
        "label": "config.ui.backend_language.title",
        "text": {
          "it": "Lingua Backend",
          "en": "Backend Language"
        }
      }
    }
  },
  "lvgl": {
    "credit_label": {
      "text": {
        "it": "Credito",
        "en": "Credit"
      }
    }
  }
}
```

## 5) Runtime contract

- Il builder pagina:
  1. trova `{{NNN}}`,
  2. cerca `web.<page>.<NNN>.text[lang]`,
  3. fallback su `text.it`,
  4. sostituisce inline nella pagina in PSRAM.
- `label` è ignorata a runtime.

---

## Piano di implementazione

## Fase 1 - Censimento stringhe localizzabili web

Obiettivo: estrarre in modo sistematico tutte le stringhe UI dai template web.

Attività:
- analizzare `data/www/*.html` e stringhe utente in `data/www/js/*.js`
- distinguere stringhe localizzabili da stringhe tecniche
- individuare testi in:
  - nodi HTML
  - attributi (`placeholder`, `title`, `aria-label`, `value`)
  - porzioni JS che mostrano messaggi utente

Output:
- lista normalizzata delle stringhe localizzabili per pagina/scope

---

## Fase 2 - Definizione schema chiavi compatte

Obiettivo: minimizzare footprint memoria e payload placeholder.

Scelte:
- chiavi numeriche a 3 cifre per pagina (`001`, `002`, ...)
- scope pagina esplicito (`index`, `config`, `test`, `logs`, `ota`, ...)
- placeholder nei template: `{{NNN}}`
- campo `label` alfanumerico per editazione/manutenzione

Regole:
- una key identifica una sola stringa nel contesto pagina
- riuso key obbligatorio quando la stringa è semanticamente identica nella stessa pagina
- `label` stabile nel tempo (non usata a runtime)

---

## Fase 3 - JSON sorgente i18n

Obiettivo: creare il formato sorgente unico da cui derivano tutte le localizzazioni.

Note:
- il JSON rimane il single source of truth
- fallback automatico a `it` per chiavi mancanti
- mantenere coerenza tra template e catalogo tramite validatore

---

## Fase 4 - Refactor template con placeholder

Obiettivo: sostituire tutte le stringhe localizzabili nei template con placeholder compatti.

Esempio:
- prima: `"<span>Configurazione</span>"`
- dopo: `"<span>{{001}}</span>"`

Ambito:
- body HTML
- attributi localizzabili
- messaggi utente in JS

Vincolo:
- nessuna regressione funzionale lato JS/UI

---

## Fase 5 - Separazione script JS in moduli

Obiettivo: pulire la struttura pagine e rendere gli script riusabili.

Attività:
- estrarre script inline in `data/www/js/*.js`
- definire include order (`core -> page -> init`)
- prevedere include via `script src` o placeholder include lato builder

---

## Fase 6 - Builder pagine localizzate in PSRAM

Obiettivo: costruire le pagine localizzate in RAM una sola volta.

Flusso:
1. boot: carica lingua backend corrente
2. carica template pagina da SPIFFS
3. carica eventuali moduli JS richiesti
4. sostituisce `{{NNN}}` usando catalogo i18n pagina
5. salva risultato in cache PSRAM (`page -> html_localizzato`)
6. runtime HTTP: serve il buffer già pronto

Cambio lingua:
1. invalidazione cache pagine
2. rebuild cache con nuova lingua
3. swap atomico dei puntatori

---

## Fase 7 - Invalidazione cache e coerenza runtime

Trigger invalidazione:
- modifica `backend_language`
- aggiornamento file i18n
- aggiornamento template pagina

Comportamento:
- se rebuild fallisce: fallback a italiano
- nessuna pagina deve restare in stato parziale
- logging dettagliato su tempi di rebuild e consumo memoria

---

## Fase 8 - Revisione `i18n_editor` v2

L’editor deve supportare:
- schema chiavi numeriche + `label`
- editing multilingua con obbligo `it`
- aggiunta nuove chiavi da UI

Funzionalità obbligatorie:
1. selezione scope/pagina (`index`, `config`, `test`, ...)
2. tabella con colonne: `key`, `label`, lingue
3. inserimento nuova key:
   - assegnazione automatica prossima key libera (`001..999`) per pagina
   - possibilità di override manuale (se non in conflitto)
4. validazione `label` con regex:
   - `^[a-z0-9]+(\.[a-z0-9_]+){2,}$`
5. obbligo compilazione `text.it`
6. controllo duplicati (`key` e `label`) prima del salvataggio

---

## Fase 9 - Qualità, validazioni e benchmark

Validazioni build-time:
- key duplicate nella stessa pagina
- placeholder presenti nei template ma assenti nel JSON
- placeholder non conformi a `{{NNN}}`
- `label` non conforme o mancante
- `text.it` mancante

Validazioni runtime:
- pagina servita correttamente con cache pronta
- fallback `it` su mancanza chiave lingua
- coerenza dopo cambio lingua

Metriche da misurare:
- tempo build cache al boot
- memoria PSRAM usata per cache pagine
- latenza media endpoint pagina (prima/dopo)

---

## Validatore automatico (build-time)

Obiettivo: verificare coerenza tra template web, catalogo i18n e regole di naming prima del deploy.

Input:
- template HTML/JS sorgente (`data/www/*.html`, `data/www/js/*.js`)
- catalogo i18n v2 JSON

Regole di validazione (ERROR):
1. placeholder non valido:
   - pattern diverso da `{{NNN}}`
2. placeholder presente nel template ma assente nel JSON della pagina
3. key duplicata nella stessa pagina JSON
4. `label` mancante o non conforme alla regex:
   - `^[a-z0-9]+(\.[a-z0-9_]+){2,}$`
5. `it` mancante per una key
6. tipo JSON non conforme (`text` non oggetto, lingua non stringa, ecc.)

Regole di validazione (WARNING):
1. key presente nel JSON ma non usata in nessun template
2. traduzione mancante in una lingua non-base (fallback su `it`)
3. `label` semanticamente ambigua

Output validatore:
- exit code `0`: nessun errore bloccante
- exit code `1`: presenti errori bloccanti
- report testuale con:
  - pagina
  - key
  - label
  - tipo problema
  - suggerimento correzione

Pseudocodice:

```text
for each page in WEB_PAGES:
    placeholders = extract_placeholders(page_template)   # es. {"001","002"}
    json_keys = load_json_keys_for_page(page)

    assert all placeholders match ^[0-9]{3}$
    assert placeholders subset of json_keys
    assert no duplicate json_keys

    for each key in json_keys:
        entry = json[page][key]
        assert label exists and matches regex
        assert text.it exists and is string

    warn if json_keys - placeholders not empty
    warn for missing non-base translations
```

---

## Funzioni legacy web deprecabili con v2

Con pagine già localizzate in PSRAM non sono più necessarie (o vanno fortemente ridotte) le funzioni di localizzazione DOM runtime:
- costruzione tabella i18n per scope/pagina lato server
- iniezione script i18n in `head` per traduzione DOM in browser
- sostituzione runtime text-node/attributi tramite JS su ogni pagina

Nota:
- `device_config_get_ui_text_scoped` resta utile per LVGL e testi server-side puntuali.

---

## Strategia di migrazione

- Step 1: introdurre formato v2 e tool senza rompere v1
- Step 2: migrare una pagina pilota (`index`)
- Step 3: migrare `config`, `test`, `programs`, ...
- Step 4: disattivare path runtime i18n legacy per il web
- Step 5: mantenere compatibilità LVGL e fallback IT

---

## Decisioni operative

- Nessun blob binario obbligatorio nella prima iterazione.
- JSON resta il formato sorgente.
- Ottimizzazione ottenuta tramite preload in PSRAM delle pagine già localizzate.
- Pipeline web primaria `HTML-first` su SPIFFS.
- Possibile evoluzione futura: compilazione offline JSON->artefatti compatti, senza cambiare il contratto placeholder.

---

## Checklist operativa (lista esatta interventi)

1. definire contratto placeholder `{{NNN}}` ✅
2. censire stringhe localizzabili da HTML/JS ✅
3. generare catalogo i18n v2 con `label` ✅
4. sostituire testi template con placeholder ✅
5. separare JS in moduli
6. implementare builder pagine in PSRAM ✅
7. adattare page handler a cache PSRAM
8. gestire invalidazione/rebuild su cambio lingua
9. deprecare runtime i18n web legacy
10. aggiornare `i18n_editor` con nuova key creation
11. aggiungere validatore build-time
12. eseguire benchmark e validazione finale

---

## Deliverable attesi

1. documento architetturale (questo file)
2. inventario stringhe localizzabili per pagina
3. JSON i18n v2 iniziale con chiavi compatte + `label`
4. template HTML/JS refactorizzati con placeholder
5. modulo cache pagine PSRAM + invalidazione/rebuild
6. aggiornamento `i18n_editor` con creazione nuove chiavi
7. benchmark prima/dopo
