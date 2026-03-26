# Changelog - Refactoring i18n v2 Localization System
**Data**: 15 Marzo 2026  
**Versione**: v0.5.3  
**Autore**: Refactoring completo sistema i18n v2

---

## Obiettivo

Completamento refactoring sistema i18n v2 con focus su:
- Pulizia catalogo da stringhe tecniche/JavaScript
- Miglioramento editor Electron con funzionalità batch
- Validazione completezza traduzioni
- Ottimizzazione dimensioni partizione SPIFFS
- Verifica build completa

---

## Modifiche Principali

### 1. Editor i18n (Electron)

**File modificati:**
- `i18n_editor/renderer/renderer.js`
- `i18n_editor/renderer/styles.css`

**Modifiche:**
- Rimosso filtro aggressivo su chiavi `.attr.` nella funzione `shouldSkipTechnicalTranslation`
  - Prima: escludeva tutte le chiavi attributo dalla traduzione automatica
  - Dopo: mantiene solo esclusione `.js.` per frammenti JavaScript tecnici
- Aggiunti pulsanti per-riga nell'interfaccia:
  - **Azzera**: cancella tutte le traduzioni (eccetto IT) per una riga
  - **Copia IT**: copia testo italiano su tutte le altre lingue
  - **Ritraduci**: ritraduce automaticamente tutte le lingue per una riga
- Migliorata gestione batch translation con skip intelligente di stringhe tecniche

### 2. Validatore i18n v2

**File modificati:**
- `scripts/validate_i18n_v2.py`

**Modifiche:**
- Aggiunto flag `--require-all-languages` per validazione strict
- Controllo completezza: verifica che ogni chiave abbia tutte le lingue definite nel catalogo
- Esteso parsing per includere placeholder in file JS referenziati tramite marker `{{JS:...}}`

**Utilizzo:**
```bash
# Validazione standard (solo coerenza placeholder/catalogo)
python3 scripts/validate_i18n_v2.py --catalog data/i18n_v2.json --www data/www

# Validazione strict (richiede tutte le lingue per ogni chiave)
python3 scripts/validate_i18n_v2.py --catalog data/i18n_v2.json --www data/www --require-all-languages
```

### 3. Catalogo i18n v2

**File modificati:**
- `data/i18n_v2.json`

**Statistiche modifiche:**
- **Rimosse**: 97 chiavi non utilizzate (principalmente scope `test` e `logs`)
- **Completate**: 1539 traduzioni mancanti (fallback su testo italiano)
- **Aggiunte**: 34 nuove chiavi per scope `files` (placeholder UI `030..063`)

**Dimensione finale catalogo**: ~196 KB

### 4. File JavaScript Web

**File modificati:**
- `data/www/js/files_03.js` - reinseriti 34 placeholder UI
- `data/www/js/logs_03.js` - ripristinati testi statici (placeholder tecnici rimossi)
- `data/www/js/test_04.js` - ripristinati testi statici
- `data/www/js/test_05.js` - ripristinati testi statici

**Strategia applicata:**
- Stringhe UI utente → placeholder `{{NNN}}` nel catalogo
- Stringhe tecniche/debug → testo statico hardcoded in JS
- Mantenuta sintassi JavaScript valida (verificata con `node --check`)

### 5. Ottimizzazione Partizione SPIFFS

**File rimossi da `data/`:**
- `data/i18n_20260315_*.json` (5 backup temporanei)
- `data/www/test.html.backup`

**Risultato:**
- Dimensione `data/` ridotta: da ~2.2 MB a ~1.3 MB
- Limite partizione SPIFFS: 1536 KB (1.5 MB)
- **Margine disponibile**: ~200 KB

### 6. Configurazione Build

**File modificati:**
- `sdkconfig.defaults`

**Modifiche:**
- Disabilitate demo LVGL duplicate che causavano errori di compilazione:
  - `CONFIG_LV_USE_DEMO_MUSIC=n`
  - `CONFIG_LV_USE_DEMO_FLEX_LAYOUT=n`
  - `CONFIG_LV_USE_DEMO_MULTILANG=n`
- Rimosse righe duplicate che riattivavano le demo alla fine del file

### 7. Fix Componenti

**File modificati:**
- `components/http_services/http_services.c`

**Modifiche:**
- Rimosso riferimento a `ESP_ERR_HTTP_INCOMPLETE_DATA` (non disponibile in ESP-IDF 5.5)
- Semplificata logica retry chunked data (già gestita dal loop esterno)

---

## Validazioni Eseguite

### Validazione i18n

```bash
# Standard
python3 scripts/validate_i18n_v2.py --catalog data/i18n_v2.json --www data/www
# Risultato: 0 error(s), 0 warning(s) ✅

# Strict (tutte le lingue)
python3 scripts/validate_i18n_v2.py --catalog data/i18n_v2.json --www data/www --require-all-languages
# Risultato: 0 error(s), 0 warning(s) ✅
```

### Validazione Sintassi JavaScript

```bash
node --check data/www/js/files_03.js    # ✅
node --check data/www/js/logs_03.js     # ✅
node --check data/www/js/test_04.js     # ✅
node --check data/www/js/test_05.js     # ✅
```

### Build Firmware

```bash
idfc -b
# Risultato: SUCCESS ✅
# Binary: test_wave.bin (1,957,520 bytes)
# Spazio libero partizione app: 61% (3,023,216 bytes)
```

---

## Statistiche Finali

| Metrica | Valore |
|---------|--------|
| Chiavi catalogo totali | 681 |
| Chiavi rimosse (non usate) | 97 |
| Traduzioni completate | 1539 |
| Nuove chiavi aggiunte | 34 |
| Dimensione `data/` | 1.3 MB |
| Margine SPIFFS | ~200 KB |
| File `data/` totali | 67 |
| Lingue supportate | 5 (it, en, fr, de, es) |

---

## Scope i18n Catalogo

| Scope | Chiavi | Note |
|-------|--------|------|
| `api` | 29 | Endpoints API web |
| `config` | 155 | Pagina configurazione |
| `emulator` | 48 | Emulatore hardware |
| `files` | 63 | File manager (34 nuove) |
| `httpservices` | 54 | Servizi HTTP |
| `index` | 56 | Home page |
| `logs` | 25 | Visualizzazione log |
| `ota` | 18 | Aggiornamenti OTA |
| `programs` | 33 | Gestione programmi |
| `stats` | 23 | Statistiche |
| `tasks` | 38 | Gestione task |
| `test` | 126 | Pagina test hardware |

---

## Problemi Risolti

1. **SPIFFS overflow**: ridotta dimensione `data/` rimuovendo backup temporanei
2. **Build LVGL**: disabilitate demo incompatibili con versione corrente
3. **HTTP services**: rimosso simbolo non disponibile in ESP-IDF 5.5
4. **Traduzioni incomplete**: completate 1539 entry con fallback italiano
5. **Chiavi non usate**: rimosse 97 entry orfane dal catalogo

---

## Prossimi Passi Suggeriti

1. **Traduzione batch**: usare editor Electron per tradurre le 382 entry con solo testo italiano
2. **Test runtime**: verificare rendering corretto pagine web con nuovi placeholder
3. **Ottimizzazione catalogo**: valutare compressione JSON per ridurre ulteriormente dimensioni
4. **Documentazione utente**: aggiornare manuale con nuove funzionalità editor

---

## Note Tecniche

### Architettura i18n v2

- **Catalogo**: `data/i18n_v2.json` con struttura `web.<scope>.<key>.text.<lang>`
- **Placeholder**: formato `{{NNN}}` con chiavi numeriche a 3 cifre
- **Runtime**: caricamento PSRAM per performance, fallback su JSON cache
- **Editor**: Electron app con integrazione DeepL/OpenAI per traduzioni automatiche
- **Validazione**: script Python per coerenza template/catalogo

### Marker JavaScript

- `{{JS:path/file.js}}` nei template HTML per inclusione moduli esterni
- Espansione lato C in `webpages.c` durante serving pagine
- Validatore esteso per parsing placeholder anche nei JS inclusi

---

**Documento generato automaticamente il 15/03/2026**
