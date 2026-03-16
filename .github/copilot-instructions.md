# Istruzioni Personalizzate per GitHub Copilot - Progetto MicroHard

Segui rigorosamente queste regole durante la generazione del codice, l'analisi e l'assistenza nel progetto.

## 1. Architettura e Compilazione
- **Modalità di Esecuzione:** Il progetto gestisce due modalità (`APP` e `FACTORY`) da un'unica base codice in `main/`.
- **Flag di Compilazione:** Utilizza il flag `COMPILE_APP` definito in `main/app_version.h`.
- **Cambiamenti Configurazione:** Ogni volta che aggiungi, modifichi o rimuovi un `#define` o un `CONFIG_*`, aggiorna immediatamente il file `docs/COMPILE_FLAGS.md`.

## 2. Standard di Codifica e Commenti
- **Lingua:** - Scrivi i **commenti in italiano**.
    - Usa l'**inglese** per nomi di funzioni, costanti e variabili.
- **Logging (ESP-IDF):** Aggiungi sempre un prefisso al messaggio nei log:
    - `[M]` per Main App.
    - `[F]` per Factory App.
    - `[C]` per Common/Components.
    - Esempio: `ESP_LOGI(TAG, "[M] Inizializzazione completata");`
- **Gestione Stack:** Se modifichi le dimensioni dello stack di una funzione, aggiorna la voce corrispondente in `tasks.json`.

## 3. Internazionalizzazione (i18n) e Stringhe
- **Catalogo Unico:** Tutte le stringhe (Web UI e Touch Panel LVGL) devono risiedere in `/spiffs/i18n_v2.json`.
- **Nuovi Testi:** Non inserire stringhe hardcoded. Ogni nuovo testo deve essere aggiunto a `data/i18n_v2.json` con `scope`, `key` e `legacyId`, e referenziato tramite codice alfanumerico.

## 4. Sviluppo Web (Web UI)
- **Sorgente Unico:** Modifica solo `components/web_ui/webpages_embedded.c`. Non toccare mai i file in `data/www/` o `build/`.
- **Marcatori Obbligatori:** Il markup/JS per le pagine `/` e `/config` deve essere racchiuso tra:
    `/* DO_NOT_MODIFY_START: <page> */` e `/* DO_NOT_MODIFY_END: <page> */`.

## 5. Protezione del Codice (DNM / DNME)
- Rispetta i blocchi di codice protetti. Non modificare mai:
    - Funzioni marcate con `//DO_NOT_MODIFY`.
    - Sezioni tra `//DO_NOT_MODIFY` e `//DO_NOT_MODIFY_END`.

## 6. Tecnica di Mockup (Simulazione Hardware)
Per simulare hardware (es. SD Card) senza alterare il codice reale:
1. Usa un simbolo preprocessoriale (es. `DNA_SD_CARD`).
2. Mantieni la funzione originale.
3. Appendi **dopo** la funzione reale un blocco `#if defined(SYMBOL) && (SYMBOL==1)`.
4. Implementa il comportamento fittizio nel blocco, restituendo valori di successo (es. `ESP_OK`).

## 7. Workflow e Comandi Rapidi
Interpreta i seguenti comandi o riferimenti quando citati:
- **.rl**: Leggi `docs/RULES.md`.
- **.lt**: Leggi `docs/TODO.md`.
- **.cp**: Esegui commit e push (solo su richiesta esplicita).
- **PDR**: Codice di esempio in `~/Progetti/0.Clienti/MicroHard/WT99P4C5-S1`.
- **PSI**: Vecchia versione di prova in `~/Progetti/0.Clienti/MicroHard/test`.
- **Script Path:** Esegui gli script sempre dal path `/home/mauro/Progetti/0.Clienti/MicroHard/scripts`.

## 8. Gestione Compilazione (idfc)
Usa il tool `idfc` con i seguenti flag:
- `idfc -b`: Build di verifica (es. `get_idf && idfc -b`).
- `idfc -f`: Flash completo.
- `idfc -ff`: Flash solo factory.
- `idfc -fs`: Flash solo partizione SPIFFS.
- `idfc -m`: Monitor.

## 9. Versionamento e Documentazione
- **Commit:** Inizia sempre il messaggio di commit con la versione (es. `v1.2.1`). Proponi il commit solo dopo una build con successo.
- **TODO:** Quando uno step è completato, sposta la voce da `TODO.md` a `TODO_DONE.md` e smarcatela.
