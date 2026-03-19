# 260319_LANGUAGE_ISSUES.md

## 📋 ANALISI PROBLEMA LINGUE - 26/03/2026

### **🎯 Problema Principale**
Il sistema ha due gestioni lingue completamente separate ma **LVGL non sta caricando il dizionario in PSRAM**, causando fallback italiano hardcoded invece delle traduzioni corrette.

---

## 🏗️ ARCHITETTURA LINGUE ATUALE

### **1. LVGL (Pannello Touch)**
- **Configurazione**: `ui.user_language` (default: "it")
- **Caricamento**: Dizionario in PSRAM all'avvio via `lvgl_panel_load_i18n_dictionary()`
- **Cambio**: Su richiesta utente via `lvgl_panel_refresh_texts()`
- **Sorgente**: `/spiffs/i18n_v2.json` → sezione `lvgl.*`

### **2. Web UI (Backend/Manutentore)**
- **Configurazione**: `ui.backend_language` (default: "it")
- **Template**: Non localizzati in sorgente (`data/www/*.html`)
- **Caricamento**: Boot: lettura template + moduli JS → sostituzione placeholder i18n
- **Cache**: Pagine localizzate mantenute in PSRAM e servite direttamente
- **Cambio**: Su richiesta manutentore → `web_ui_i18n_cache_invalidate()` → rigenerazione cache
- **Sorgente**: `/spiffs/i18n_v2.json` → sezione `web.*`

---

## 🔍 PROBLEMA IDENTIFICATO

### **❌ Sintomi**
1. **LVGL mostra sempre italiano** anche se `user_language='en'`
2. **Nessun log di caricamento dizionario PSRAM**
3. **Fallback hardcoded** usato invece di traduzioni da i18n_v2.json

### **🔍 Analisi Log Boot**
```
I (16649) lvgl_panel: [C] Lingua aggiornata (senza bandiera): it
I (16669) lvgl_panel: [C] Lingua sincronizzata da device_config: it
```

**Problema**: `user_language='it'` ma non ci sono log di:
```
[C] LVGL show - user_language='it', backend_language='en'
[C] i18n_load_full_dictionary_psram: language='it'
Loaded XXXX i18n records into PSRAM for language it
```

### **🎯 Causa Radice**
`lvgl_panel_show()` viene chiamata ma **i log non appaiono** → oppure **il caricamento dizionario fallisce silenziosamente**.

---

## 📊 FLUSSO ATTUALE LVGL

### **Boot Sequence**
1. `lvgl_panel_show()` → `device_config_get_ui_user_language()` → "it"
2. `lvgl_panel_load_i18n_dictionary("it")` → chiama `web_ui_i18n_load_language_psram("it")`
3. `i18n_load_full_dictionary_psram("it")` → chiama `device_config_get_ui_texts_records_json("it")`
4. **❌ PROBLEMA**: Qualcosa fallisce nel passo 3-4

### **Lookup Testi**
1. `device_config_get_ui_text_scoped("lvgl", "credit_label", "Credito", ...)`
2. `i18n_concat_from_psram(scope_id, key_id)` → **NULL** (dizionario non caricato)
3. Fallback a "Credito" (italiano hardcoded)

---

## 🔧 PIANO DI CORREZIONE

### **FASE 1: DIAGNOSTICA COMPLETA**
1. **Verificare log lvgl_panel_show()**
   - ✅ Già aggiunto log: `ESP_LOGI(TAG, "[C] LVGL show - user_language='%s', backend_language='%s'")`
   - ❌ **PROBLEMA**: Log non appaiono nel boot → verificare perché la funzione non li emette
   - Azione: Aggiungere log immediato all'inizio di `lvgl_panel_show()` prima di qualsiasi altra operazione

2. **Verificare catena caricamento PSRAM**
   - ✅ Già aggiunto log in tutte le funzioni:
     - `lvgl_panel_load_i18n_dictionary()` → log successo/fallimento
     - `web_ui_i18n_load_language_psram()` → log record caricati
     - `i18n_load_full_dictionary_psram()` → log dettagliati parsing
     - `device_config_get_ui_text_scoped()` → log lookup
   - ❌ **PROBLEMA**: Nessuno di questi log appare nel boot
   - Azione: Verificare livello log e punto esatto di chiamata

3. **Verificare file i18n_v2.json**
   - ✅ Confermato esistenza in SPIFFS (215590 bytes)
   - 🔍 **Da verificare**: Contenuto sezione `lvgl.*` per lingua "it" è completa e valida
   - Azione: Test parsing JSON per verificare che non sia corrotto

4. **Verificare configurazione lingue attuale**
   - ✅ Dai log: `user_language='it'`, `backend_language='en'`
   - 🔍 **Da verificare**: Perché LVGL non mostra log se `user_language='it'`
   - Azione: Test forzato con lingua diversa per vedere se cambia comportamento

### **FASE 1.1: AZIONI IMMEDIATE**
1. **Flash e catturare log completi** attivando DEBUG per tutti i componenti i18n
2. **Verificare livello log LVGL_PANEL** - potrebbe essere disabilitato
3. **Test con user_language='en'** per vedere se appaiono log diversi
4. **Verificare che lvgl_panel_show() venga chiamata** al boot o in un momento diverso

### **FASE 2: CORREZIONE CARICAMENTO**
1. **Fix caricamento dizionario LVGL**
   - Assicurarsi che `device_config_get_ui_texts_records_json()` funzioni
   - Verificare parsing JSON e creazione array PSRAM
   - Gestire errori con log chiari

2. **Test caricamento lingue multiple**
   - Verificare funzionamento con "it", "en", "fr", "de", "es"
   - Test fallback lingua mancante

### **FASE 3: VALIDAZIONE FLUSSI COMPLETI**
1. **LVGL - Boot**
   - Caricamento dizionario `user_language` in PSRAM
   - Traduzioni corrette mostrate all'avvio

2. **LVGL - Cambio Utente**
   - `lvgl_panel_refresh_texts()` ricarica dizionario nuova lingua
   - Aggiornamento immediato UI

3. **Web UI - Boot**
   - Caricamento singola lingua `backend_language`
   - Traduzioni corrette nelle pagine web

4. **Web UI - Cambio Manutentore**
   - `web_ui_i18n_cache_invalidate()` + rilettura JSON
   - Aggiornamento pagine web

---

## 🎯 OBIETTIVI FINALI

### **✅ Comportamento Atteso**
1. **LVGL**: Mostra testi nella lingua configurata in `ui.user_language`
2. **Web UI**: Mostra testi nella lingua configurata in `ui.backend_language`
3. **Indipendenza**: Le due lingue possono essere diverse
4. **Dinamicità**: Entrambe possono essere cambiate runtime

### **📝 Test Cases da Validare**
1. **Boot con user_language='it'** → LVGL in italiano
2. **Boot con user_language='en'** → LVGL in inglese
3. **Boot con backend_language='en'** → Web UI in inglese
4. **Cambio LVGL runtime** → Aggiornamento immediato
5. **Cambio Web UI runtime** → Aggiornamento pagine

---

## 🔍 PROSSIMI PASSI IMMEDIATI

1. **Flash con log dettagliati** e catturare output completo
2. **Analizzare punto esatto del fallimento** nella catena LVGL
3. **Verificare contenuto i18n_v2.json** per lingua italiana
4. **Implementare correzione mirata** basata su diagnosi
5. **Test completo flussi LVGL e Web UI**

---

## 📝 NOTE TECNICHE

### **File Chiave**
- `components/lvgl_panel/lvgl_panel.c` → `lvgl_panel_show()`, `lvgl_panel_load_i18n_dictionary()`
- `components/web_ui/web_ui_common.c` → `i18n_load_full_dictionary_psram()`
- `components/device_config/device_config.c` → `device_config_get_ui_texts_records_json()`
- `data/i18n_v2.json` → Dizionario traduzioni

### **Funzioni Chiave**
- `lvgl_panel_show()` → Avvio LVGL e caricamento lingua
- `lvgl_panel_refresh_texts()` → Cambio lingua LVGL runtime
- `web_ui_i18n_load_language_psram()` → Caricamento dizionario in PSRAM
- `web_ui_i18n_cache_invalidate()` → Invalidazione cache Web UI

---

## ⚠️ RISCHI E CONSIDERAZIONI

1. **Memory PSRAM**: Verificare spazio sufficiente per dizionario completo
2. **Performance**: Caricamento dizionario non deve bloccare UI
3. **Fallback**: Gestione graceful se lingua richiesta non disponibile
4. **Consistenza**: Assicurarsi che tutte le chiavi i18n siano presenti

---

**Status**: 🔄 In analisi - prossima fase: diagnosi dettagliata con log completi
