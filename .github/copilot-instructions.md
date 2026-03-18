# Istruzioni Personalizzate per GitHub Copilot - Progetto MicroHard

Segui rigorosamente queste regole durante la generazione del codice, l'analisi e l'assistenza nel progetto.

## 1. Architettura, Modalità e OTA
- **Modalità di Esecuzione:** Il progetto gestisce due modalità (`APP` e `FACTORY`) da un'unica base codice in `main/`.
- **Flag di Compilazione:** Utilizza il flag `COMPILE_APP` definito in `main/app_version.h`.
- **Partizioni OTA:** Mantieni la separazione delle partizioni per garantire un aggiornamento OTA sicuro.

## 2. Sourcing ESP-IDF e Compilazione (`idfc`)
- **Sourcing:** Per inizializzare ESP-IDF puoi usare `get_idf`.
- **Build Verifica:** Usa `get_idf && idfc -b`.
- **Flash Completo:** Usa `idfc -f`.
- **Flash Factory:** Usa `idfc -ff`.
- **Flash SPIFFS:** Usa `idfc -fs`.
- **Monitor:** Usa `idfc -m`.
- **Flag Concatenabili:** I flag di `idfc` possono essere concatenati.
- **Help Tooling:** Per analizzare il funzionamento di `idfc` usa `idfc -h`.

## 3. Configurazione che Impatta la Compilazione
- **Cambiamenti Configurazione:** Ogni volta che aggiungi, modifichi o rimuovi un `#define` o un `CONFIG_*`, aggiorna immediatamente `docs/COMPILE_FLAGS.md` nella stessa modifica.

## 4. Standard di Codifica, Commenti e Logging
- **Lingua:** - Scrivi i **commenti in italiano**.
    - Usa l'**inglese** per nomi di funzioni, costanti e variabili.
- **Logging (ESP-IDF):** Aggiungi sempre un prefisso al messaggio nei log:
    - `[M]` per Main App.
    - `[F]` per Factory App.
    - `[C]` per Common/Components.
    - Esempio: `ESP_LOGI(TAG, "[M] Inizializzazione completata");`
- **Gestione Stack:** Se modifichi le dimensioni dello stack di una funzione, aggiorna la voce corrispondente in `tasks.json`.

## 5. Internazionalizzazione (i18n) e Stringhe
- **Catalogo Unico:** Tutte le stringhe (Web UI e Touch Panel LVGL) devono risiedere in `/spiffs/i18n_v2.json`.
- **Nuovi Testi:** Non inserire stringhe hardcoded. Ogni nuovo testo deve essere aggiunto a `data/i18n_v2.json` con `scope`, `key` e `legacyId`, referenziato tramite codice a 3 cifre o alfanumerico, e tradotto in tutte le lingue supportate.

## 6. Sviluppo Web (Web UI)
- **Sorgente Web UI:** Modifica HTML/CSS/JS direttamente nei file in `data/www/`.
- **JavaScript Separato:** Salva il codice JS in file separati in `data/www/js/`.
- **Pipeline Legacy Disabilitata:** Non usare `components/web_ui/webpages_embedded.c` né lo script `export_embedded_pages.py`.
- **Deploy Rapido Web:** Dopo modifiche Web UI esegui solo il flash SPIFFS con `idfc -fs`, senza ricompilare il firmware.
- **Marcatori Obbligatori:** Il markup/JS per le pagine `/` e `/config` deve essere racchiuso tra:
    `/* DO_NOT_MODIFY_START: <page> */` e `/* DO_NOT_MODIFY_END: <page> */`.

## 7. Protezione del Codice (DNM / DNME)
- Rispetta i blocchi di codice protetti. Non modificare mai:
    - Funzioni marcate con `//DO_NOT_MODIFY`.
    - Sezioni tra `//DO_NOT_MODIFY` e `//DO_NOT_MODIFY_END`.
- Interpretazione comandi utente:
    - `fai DNM a <file>`
    - `fai DNME a <file>`

## 8. Tecnica di Mockup (Simulazione Hardware)
Per simulare hardware (es. SD Card) senza alterare il codice reale:
1. Usa un simbolo preprocessoriale (es. `DNA_SD_CARD`).
2. Mantieni intatte le funzioni originali.
3. Appendi **dopo** la funzione reale un blocco `#if defined(DNA_SD_CARD) && (DNA_SD_CARD==1)`.
4. Implementa il comportamento fittizio nel blocco restituendo sempre valori di successo (es. `ESP_OK`, dimensioni non nulle, elenchi fittizi).
5. I mock devono essere auto-confinati e non alterare lo stato del codice reale.
6. Scrivi sempre entrambe le versioni: funzione reale e funzione mockup.

## 9. Workflow, Comandi Rapidi e Script
Interpreta i seguenti comandi o riferimenti quando citati:
- **.rl**: Leggi `docs/RULES.md`.
- **.el**: Controlla `exec.log`.
- **.lt**: Leggi `docs/TODO.md`.
- **.cp**: Esegui commit e push (solo su richiesta esplicita).
- **.fs**: Esegue `scripts/flash-spiffs.sh`.
- **.ff**: Esegue `scripts/flash_factory.sh`.
- **.fm**: Esegue `scripts/flash-main.sh`.
- **.bf**: Esegue `build_flash_no_spiffs.sh`.
- **PDR**: Codice di esempio in `~/Progetti/0.Clienti/MicroHard/WT99P4C5-S1`.
- **PSI**: Vecchia versione di prova in `~/Progetti/0.Clienti/MicroHard/test`.
- **Script Path:** Esegui gli script sempre dal path `/home/mauro/Progetti/0.Clienti/MicroHard/scripts`.
- **Comandi Git:** Esegui comandi Git solo su richiesta esplicita dell'utente.
- **Terminale Device:** Salvo richiesta esplicita, build/flash/monitor per test su device vanno eseguiti in un terminale separato.
- **Documentazione:** Salva i file di documentazione nella cartella `docs/`.

## 10. Versionamento e TODO
- **Commit:** Inizia sempre il messaggio di commit con la versione (es. `v1.2.1`). Proponi il commit solo dopo una build con successo.
- **TODO:** Quando uno step è completato, sposta la voce da `TODO.md` a `TODO_DOBNE.md` e spuntala.
