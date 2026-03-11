- il progetto gestisce 2 modalità di esecuzione: APP e FACTORY, compilate da base codice unica (`main/`) tramite flag `COMPILE_APP` in `main/app_version.h`. Le partizioni restano separate per OTA sicuro.
- La app è multilingua e carica la tabella stringhe dai file in SPIFFS i18n_XX.json dove XX è il cofice lingua. Ogni tsto che appare nell'interfaccia web o nel pannello touch va tabellata
- per il sourcing di ESP-IDF si può usare get_idf 
- per la compilazione usare `idfc -b`, flash `idfc -f`, per flash solo factory `idfc -f -o f`, per monitor `idfc -m`
- per analizzare il funzionamento di idfc chiamare 'idfc -h'
- va gestito un blocco per la generazione del codice che deve inibire la modifica di funzioni marcate con //DO_NOT_MODIFY o di sezioni di codice racchiuse tra //DO_NOT_MODIFY and //DO_NOT_MODIFY_END : io indicherò l'operazione con `fai DNM a <file>` o `fai DNME a <file>`
- Il markup/JS generato per le pagine ` /config ` e ` / ` (home) deve essere sempre racchiuso tra i marcatori `/* DO_NOT_MODIFY_START: <page> */` e `/* DO_NOT_MODIFY_END: <page> */` per impedire modifiche automatiche.
- l'esecuzione degli script deve avvenire col path `/home/mauro/Progetti/0.Clienti/MicroHard/scripts`
- i commenti vanno tradotti in italiano, i nomi di funzioni, costanti e variabili in inglese
- il comando .rl significa 'leggi il file docs/RULES.md'
- il comando .el significa 'Controlla il file exec.log'
- il comando .lt sugnifica 'leggi il file docs/TODO.md'
- il comando .cp sugnifica 'fai il commit e il push'
- il comando .fs esegue scripts/flash-spiffs.sh
- il comando .ff esegue `scripts/flash_factory.sh`
- il comando .fm esegue scripts/flash-main.sh
- il comando .bf esegue build_flash_no_spiffs.sh
- il codice che si trova in  ~/Progetti/0.Clienti/MicroHard/WT99P4C5-S1 è un esempio per la stessa configurazione da cui prelevare codice di esempio : lo chiamerò PDR
- il codice che si trova in  ~/Progetti/0.Clienti/MicroHard/test è una versione precedente di prova  per la stessa configurazione da cui prelevare porzioni di codice specifiche cha vanno riadattate al progetto attuale  : lo chiamerò PSI
- i comandi git vanno fatti solo su mia richiesta e proposti solo dopo un test di compilazione positivo
- nei messaggi di commit inserire sempre la versione del sorgente (es. v1.2.1)
- salvo esplicita richiesta i build flash e monitor vengono eseguiti in un terminale a parte
- i file di documentazione vanno salvati in docs
- quando un #define o un CONFIG_* che influisce sulla compilazione viene aggiunto/modificato/rimosso, aggiornare sempre docs/COMPILE_FLAGS.md nella stessa modifica
- quando indico che uno step previsto in TODO.md è completato spuntiamo la relativa voce
- nei log ESP_LOGI/ESP_LOGW/ESP_LOGE va aggiunto un prefisso tra parentesi quadre all'inizio del messaggio per
- identificare il contesto: [M] per main app, [F] per factory app, [C] per common/components
- in caso di modifiche alle dimesioni degli stack delle funzioni va aggiornata la voce corrispondente in tasks.json
- per simulare l'hardware senza alterare il codice "vero" utilizzare la tecnica dei
  mockup:
  1. definire un simbolo preprocessoriale (es. `DNA_SD_CARD`) in un header o
     a livello di compilazione (menuconfig, CMake, ecc.);
  2. lasciare intatte le funzioni originali e appendere **dopo di esse** una
     sezione condizionale `#if defined(DNA_SD_CARD) && (DNA_SD_CARD==1)` che
     contiene i dettagli del comportamento fittizio;
  3. all'interno del blocco descrivere le operazioni previste e restituire
     sempre valori che rappresentano un'operazione andata a buon fine (ESP_OK,
     dimensioni non nulle, elenchi di file fittizi, ecc.); l'hardware reale non
     viene mai toccato perché il codice reale è escluso dalla compilazione in
     presenza del flag.
  4. i mock devono essere auto‑confinati e non modificare lo stato del codice
     reale.
  5. Scrivere sempre le funzioni doppie: la versione reale e la versione mockup
  Questo approccio permette di mantenere la logica originale intonsa e di
  attivare/disattivare rapidamente il comportamento simulato cambiando il
  valore del simbolo.


- le modifiche alle pagine HTML vanno eseguite nel file sorgente `components/web_ui/webpages_embedded.c` e poi propagate ai file HTML eseguendo lo script `scripts/export_embedded_pages.py`; non modificare mai i file HTML in `data/www/` o `build/exported_www_subset/` direttamente.
