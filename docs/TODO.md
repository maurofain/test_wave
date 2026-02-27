# ✅ COMPLETATI

- Factory

  1. Doxigen per la documentazione automatica

  2. Gestione doppia partizione per factory reset/upload/testing e per produzione

  3. Gestione Ethernet

  4. Gestione Pannello Web Factory

  5. Driver I/O Expander

  6. Driver led

  7. Driver MDB e protocollo 9 bit

  8. classe per la configurazione con i seguenti parametri

     - nome del dispositivo
     - luminosità schermo (default 100%)
     - abilitazione Ethernet (default true)
     - Configurazione rete (DHCP (default) / parametri manuali)
     - Abilitazione WiFi  (defult false)

     - Configurazione WiFI (DHCP (deaulut) / parametri manuali)

     - Json configurazioni aggiuntive (String) e funzioni per leggere/Scrivere i parametri dal codice della app

     - Crea una pagina lvgl e html  per editare i parametri inclusiva della tastiera virtuale e delle funzioni Salva / Annulla / Backup / Restore

  9. creare le funzioni per l'init delle periferiche descritte in HARDWARE_SETUP, un task per ogni periferica e una utility di test gestibile da interfaccia WEB

  10. Implementare la gestione della EEProm 24LC16BT-I/OT  , creiamo il codice per interfacciarsi in i2c e la gestione CRC
  11. ✅ Usare la EEProm per memorizzare i dati di config. Implementiamo la seguente logica :
       1. ✅ I dati di config devono avere un valore di CRC
       2. ✅ all'avvio verifichiamo se c'è il config in EEPROM e se è valido
          1. ✅ altrimenti lo leggiamo da NVS e settiamo il flag di modificato in EEProm
          2. ✅ se non c'è in NVS o non è validi (CRC) carichiamo i valori di default, scriviamo EEProm e NVS
          3. ✅ alla modifica dei parametri (tasto save in configurazione) salviamo i dati nella EEProm e sempre nella eeprom  settiamo flag di modificato 
          4. ✅ al successivo riavvio salviamo i dati (se validi) in NVS - se non validi li riprendiamo da NVS e settiamo il flag di modificato
12. Driver PWM
13. ✅ Unificazione codice APP/FACTORY con flag `COMPILE_APP` (eliminata duplicazione cartelle)
14. ✅ Driver RS485 e ricezione dati (timeout hardware 10ms)
15. ✅ Test EEprom: tasto "Leggi JSON" e visualizzazione CRC/Updated nell'interfaccia web
16. ✅ Implementata la scheda SD
17. ✅ Aggiunto settaggio manuale Frequenza/Duty per test PWM
18. ✅ Aggiunto controllo colore e luminosità per tutti i LED nel test LED
19. ✅ Rafforzato driver SD con log dettagliati, power cycle hardware e monitor hot-plug (GPIO 0)
21. ✅ Driver per il sensore temperatura SHT40-BDIB-R2 con lettura background e visualizzazione Web UI
22. ✅ Gestione GPIO 32 (ex BOOT) e 33 configurabili in Config e Test (IN/OUT, Pull, State)
23. ✅ Implementare la data e ora da ntp sui server `2.it.pool.ntp.org`e 2.europe.pool.ntp.org, questi server vanno aggiunti al config dopo il WiFI. L'ora va acquisita solo se c'è connessione di rete e accesso a Internet (testare con un ping su server google).     Una volta acquisita l'ora il timestamp va aggiunto nei log dopo il timing CPU e mostrata nel titolo delle pagine web
24. ✅ Implementare il log su ethernet da visualizzare su una apposita pagina HTML  : valutare se utilizzare UDP sia il protocollo validoper l'invio dei dati
25. ✅ RUOTA LO SCHERMO 90° ccw e gestisci il controllo di luminosità in /config INCONTRATO LIMITAZINI LVGL
26. ✅ all'avvio della app per prima cosa inizializzare I2C ed attivare l'I/O expander. poi controllare il GPIO3 del del expander 1 e loggar eil valore (0/1) e se è risulta a 0 fermare il programma fino al ritorno a 1  
27. ✅ interfaccia web: implementare in /config l'abilitazione e la configurazione dello scanner. Nella pagina /test aggiungere la visualizzazione delle letture e i tasti 'Scanner ON' e 'Scenner OFF'. nella pagina /tasks verificare che tutti i task siano rappresentati.
28. ✅ Implementare l'esclusione del display MIPI e di LVGL per uso senza interfaccia grafica. L'abilitazione va messaa nella pagina /config e se abilitato deve mostrare lo stato di attivazione di display e touch e se attivi la loro operatività
29. ✅ nella pagina di visulzzazione del LOG remoto inserici un filtraggio in base a una stringa che verrà compilata in un textbox e attivata trami un pbutto' Filtra'
30. ✅ CCtalk: seriale attivata + task di background (driver in `components/cctalk`, UART = CONFIG_APP_RS232_UART_PORT, baud 4800 — TX=GPIO20, RX=GP1.  ✅ CCtalk integrato in `/config` e `/test`
31. ✅ in /httpservices usa per le sezioni la stessa impostazione grafica usata in /config con sezioni riquadrate e sottolineate
32. ✅ riorganizza la pagina /httpservices : metti in cima 2 textbox : richiesta e pi risposta: Richiesta mostra il messaggio post inviato in un a delle sezioni e in Risposta la risposta della chiamata. Compila  questi campi azzerandoli ad ogni richiesta. All pressione del send cambia lo sfondo di risposta fino a che la rioposta non arriva. Togli la text Token e mostra il token nella risposta, togli 'test token' e l'area hs_status
33. ✅ Impostare vscode per eseguire il flash via OTA (task: `Flash OTA (upload bin via /ota/upload)`, `Flash OTA (trigger URL via /ota)`)
34. ✅ Controllo luminosità schermo implementato e collegato a /config (live update + persistenza)
35. ✅ Riportare lo schermo in verticale (video portrait)
36. ✅ Clear monitor seriale: pulizia aree testo + formato output (senza TX/RX testuale, TEXT compatto, HEX con prefisso '.')
37. ✅ Emulatore: pagina web /emulator con layout pannello utente (tasti programmi, credito, messaggi, gauge, coin virtuali e indicatori relay) e predisposizione comandi hardware
38. ✅ Creata tabella testi UI salvata in NVS/EEPROM + codice di recupero testi per funzione multilingua nell'interfaccia utente
39. ✅ Inserire in Config / tabella Programmi un campo di edit numerico per il tempo massimo di pausa in un ciclo
40. ✅ implementa il salvataggio dei log degli errori su SD. ci servono 2 log separati : 
     1. app.log con il log di ogni operazione (eventi legati al credito es azioni su tasti e touch) eseguita dal cliente e la relativa azione, su file giornaliero di tipo circolare con memorizzazione massima di 30 gg, e i dati andranno inviati al server tramite api/deviceactivity (se lo swich Invia Log nella sezione Server Remoto è attivo - lo switch e il parametroon in config va creato)
     2. ERROR.log che  deve comprendere i dati di crash con lo stack chiamate. Ogni errore avrà il suo file con nome = timestamp
     3. ✅ In caso di indisponibilità del SD (assente, guasta o piena) per ambedue i tipi di log si eseguirà solo l'invio al server remoto.
     4. ✅ nella chiamata api/deviceactivity: tabella JSON `activity.json` salvata in SPIFFS e caricata in PSRAM al boot; fornita API `device_activity_find()` con ID/descrizioni (id es. 1=Start,2=Stop,3=Pause,4=Resume).

# ⏸️ RITARDATI

1. abilitazione WiFi
2. display miimale 1.44"
3. connessione HTTPS

# 📋 DA FARE

- Factory 

 0. Fare valutazione per le funzioni di caricamento da remoto su chiamata degli artefatti immagini, tabelle testi e del firmware stesso. Considerare che questi contenuti possono essere salvarti sia in SPIFFS che in SD
 1. Creare una FSM per la gestione del ciclo operativo della macchina (vedi FSM.md)
 2. modifica della Coda di eventi da utilizzare nell FSM e nei moduli di controllo: 
    1. partendo dalla definizione attuale di fsm_input_event_t definiamo che ogni agente (task o funzione di origine o destinazione di un messaggio) deve possedere un suo id unico chiamato agn_id di tipo uint8_t, quindi aggiungiamo dei campi alla struttura:
       1. From : agente che ha generato il messaggio
       2. To : destinatari del messaggio rappresentati da un array di 10 agn_id;
       3. action: campo uint8_t action_id con la definizione delle azioni da effettuare con il messaggio
       4. (esistente) Timestamp da riportare nei log
       5. (esistente) value_i32 : valore signed 
       6. (esistente) value_u32 : valore unsigned 
       7. (esistente) aux_u32 : valore unsigned 
       8. (esistente) text[64] : testo libero
    2. ogni task analizza sotto mutex la lista dei messaggi per verificare se nei destinari ci sia il suo agn_id, se lo trova  lo toglie dalla lista , sblocca il mutex, e se somma di tutti i valori dell'array destinatari=0 elinima il messaggio dalla queue. Infine esegue quanto previsto dal messaggio 
    3. venno identificati tutti gli agenti, metterli in una tabella ed assegnare gli agn_id
    4. preparare una tabella con tutte le azioni ed assegnare gli action_id
    5. tutti i messaggi includono automaticamente l'agente per i log
 3. Crea un mini file manager per ispezionare il contenuto della SD e deiSPIFFS , con possibilità di caricare scaricare e cancellare file - solo sulla root senza folder. Si accede a questa funzione con un tasto nella Home. E' disponibile sia in App che in Factory.
 4. ✅ aggiungiamo altri tasti di pagamento all'emulatore : quelli attuali li definiamo come 'Crediti QR', mettiamo nella linea sotto una serie uguale con gli stessi valori come 'Crediti Tessera', poi sotto 2 tasti 'Monete' con valore 1 coin e 2 coins
 5. funzionamento macchina 
    1. credito: il credito è espresso in coin ed ha una corrispondenza economica di un dato valore (per ora assumiamo 1€ per coin). Il programma ha come unità il tempo (in secondi) e un prezzo indivisibile (per ora consideriamo 1 programma = 1 coin ma potrebbe essere un valore qualsiasi). 
       1. il credito può arrivare da 
          1. 1 o più gettoni/monete (non è previsto resto o restituzzione di monete inserite nella gettoniera)
          2. tessera precaricata in coin
          3. codice QR precaricato in coin
       2. Funzionamento generale pay before use
          1. il cliente inserisce monete o presenta la tessara o mostra il qrcode. 
          2. la macchina acquisisce il credito disponibile distinguendo due tipi: **ecd** (credito effettivo definitivo, non rimborsabile) e **vcd** (credito virtuale, scalato alla selezione del programma). QR (1 coin per volta) e monete contribuiscono all'ecd; la tessera contribuisce per 1 ecd all'inserzione e il credito rimanente al vcd. Sistemi misti sono ammessi. 
          3. variabili da considerare nella gestione dell'erogazione:
             1. vpp : valore prezzo programma indicato in tabella che indica i crediti utilizati alla pressione del tasto programma
             2. vtp : valore tempo programma indicato in tabella 
             3. vtt : valore di ticks totali = vpp * 60
             4. rdu : rateo di utilizzo : numero di ticks scalati ogni secondo ricavato dal vpp * 60 / vtp
             5. tts : time to stop : tempo rimanente alla fine del ciclo : si aggiorna ogni secondo 
             6. vtr : tts/rdu : si aggiorna ogni secondo 
             7. ttp : tempo pausa utilizzato
             8. ecd : credito effettivo prelevato = vpp
             9. vcd : credito virtuale disponibile usato solo con tessera se lasciata nel lettore
          4. il cliente sceglie un programma e la macchina scarica dai vcd e la macchina calcola ogni secondo il valore di 
             1. vpp
             2. vtp
             3. vtt
             4. rdu
             5. tts
             6. vtr
          5. la macchina visualizza :
             1. evidenzia il programma scelto con sfondo rosso quando in run e giallo in pausa
             2. cambia lo sfondo dell'area centrale in rosso  quando in run e giallo in pausa
             3. Mostra nell'area centrale il valore principale vdp di ecd fino alla pressione del programma, poi il valore di vtp fino all'attivazione del servizio (es. accensione aspiratore ) , poi tts durante il run
             4. sotto vdp mostra il tempo vtp fino all'attivazione del servizio , poi vtt-tts in run. Se in Pausa mostra ttp
             5. la barra laterale mostra tts*100/vtp in forma grafica con colore verde da 100% a 30%, rosso da 29% a 0%
          6. Il programma può essere selezionato per solo 1 ciclo alla volta (non si può premotare ad esempio 3 cicli tipo 1)
          7. il cliente usa la macchina per il tempo previsto dal programma che partirà dal momento in cui il cliente seleziona il programma
          8. durante l'utilizzo il cliente può sospendere il ciclo e la macchina interrompe l'erogazione del servizio (ad esempio ferma l'aspiratore). La macchina ferma il conteggio del tempo per una durata massima trascorso il quale il conteggio riprende.
          9.  durante l'utilizzo di un programma il cliente può selezionare un altro programma e la macchina adegua tutte laviabili al nuovo rdu.
             1. passaggio da P1(rdu=1) a P5(rdu=2) dopo 10 sec.: utilizzati 10 ticks , rimanenti 50 - nuovo tempo disponibile
             2. al cambio di programma l'output hardware/sensori viene ricalcolato e riaggiornato in base alla maschera **Ralaty mask**, assicurando che i valori visualizzati e i comandi inviati riflettano il nuovo profilo di erogazione. 
          10. al raggiungimento della durata programmata la macchina si ferma e se c'è ancora credito vcd attende l'avvio di un altro programma.
          11. se il cliente ha caricato tramite :
             1.  gettoni : il credito rimane disponibile e se il cliente se ne va verrà usato dal cliente successivo
             2.  tessera : se la tessera è ancora inserirtanel lettore sll'avvio del duccessivo programma scala una altro coin, se è stata tolata il cliente deve ripresentare la tessera al lettore
             3.  se ha usato il QR deve ripresentarlo
    2. Visualizzazione
       1. nella fase di carica del punto 5.1.1 il box centrale mostra il credito totale disponibile **ecd + vcd** (carattere grande 100%): monete/gettoni e QR (1 coin per volta) contribuiscono all'ecd; la tessera contribuisce al vcd (1 coin se tolta, totale se lasciata nel lettore). Il valore mostrato è sempre la somma ecd+vcd. Va mantenuto in memoria il valore delle singole sorgenti di ecd e vcd, e le quote utilizzate.
       2. all'avvio del programma il numero crediti scompare se era 1 oppure appare in basso con carattere grandezza 30% ed appare il numero dei secondi disponibili carattere 100%.
       3. la barra laterale (con replica sulla striscia LED) mostra la progressione del consumo del cliente in percentuale (ticks totali - ticks usati) * 100 7 ticks totali.
   
       4.  viene inserito un certo credito (es. 20€) - il valore viene mostrato nel riquadro centrale. Quando viene scelto un programma si calcola il tempo massimo di uso del programma (crediti / costo ciclo * tempo ciclo). L'indicatore del tempo disponibile si aggiorna in base a questo valore. La barra va al 100% e comincia a scendere (percentuale = (tempo totale - tempo trascorso)/tempo totale ). Il credito viene scalato di 1 unità (es. 1€) che varrà in base al 
 6. Analisi criticità (riferita al punto 2 - coda eventi FSM):
      1. Modello coda non compatibile con destinatari multipli: la FIFO con receive distruttivo non supporta la logica `To[10]` + rimozione destinatario + eliminazione messaggio a somma destinatari zero.
        - Azione (SCELTA DECISIVA): introdurre mailbox condivisa con lock e stato destinatari per messaggio.
      2. Mancano i campi strutturali del messaggio: `from_agn_id`, `to_agn_id[10]`, `action_id`.
        - Azione: estendere `fsm_input_event_t` con i nuovi campi e fornire wrapper di compatibilità per i publisher esistenti.
      3. Assenza tabella agenti e tabella azioni: non esiste mapping centralizzato e stabile per `agn_id` e `action_id`.
        - Azione: creare una tabella unificata agenti/azioni in header condiviso, con ID fissi e descrizioni per log/debug.
      4. Rischio race in inizializzazione coda/eventi: init richiamata da più contesti (task + HTTP) senza protezione one-time robusta.
        - Azione: centralizzare init al boot o proteggere `fsm_event_queue_init` con mutex/call-once atomico.
      5. Rischio saturazione/perdita eventi: publisher multipli su coda corta, con timeout ridotti, aumentano drop e timeout sotto burst.
        - Azione: aggiungere metriche (`drop_count`, `queue_high_watermark`), rate-limit per sorgente e policy priorità eventi.
      6. Tracciabilità log non completa: non sono sempre presenti agente sorgente/destinazione/azione.
        - Azione: standardizzare il formato log evento includendo sempre `from`, `to`, `action_id`, `timestamp` e risultato gestione.
      7. Duplicazione main/factory: rischio drift tra implementazioni FSM/eventi.
        - Azione: ✅ risolto con base codice unica e selezione modalità via `COMPILE_APP`.
      8. Strategia migrazione API: servono API di claim/ack per agente mantenendo compatibilità con publish/receive attuali.
        - Azione: introdurre nuove API (`publish_ex`, `claim_for_agent`, `ack_for_agent`) mantenendo le API correnti come wrapper.

 7. Piano test endpoint e funzioni (da riprendere)

    - Strutturare i test in 4 livelli:
      - Smoke: endpoint raggiungibile, status code atteso, JSON valido.
      - Contract: campi obbligatori e tipi minimi della risposta.
      - Flow: sequenze operative (es. `sd_init -> sd_list`, `serial_send -> serial_monitor -> serial_clear`, `config/save -> config/get`).
      - Hardware-aware: aspettative diverse in base a periferica abilitata/disabilitata o assente.

    - Organizzazione consigliata:
      - `tests/smoke`, `tests/contract`, `tests/flow`, `tests/hw`.
      - File dati `tests/endpoints.yaml` con `method`, `path`, `payload`, `expected_status`, `required_keys`.
      - Fixture comuni in `conftest.py`: `base_url`, timeout, retry, helper JSON.

    - Regole pratiche:
      - Per endpoint mutativi prevedere sempre rollback/ripristino stato.
      - Separare test rapidi (default) da test hardware/lenti (`pytest -m hw` o `-m slow`).
      - Loggare richiesta/risposta/tempo per facilitare diagnosi.

    - Primo MVP test:
      - Smoke completo di tutte le route `/api/test/*` e `/api/config/*` usate dalla UI.
      - 3 flow critici: SD, seriale unificato, backup config su SD.
      - Report `junit.xml` + riepilogo markdown.
 8. Chiamate server remoto: completare hardening/integrazione (gap analisi codice)

    - Autenticazione/token
      - Generare sempre header `Date` runtime (ora è hardcoded in `http_services.c`).
      - Gestire scadenza token + relogin automatico su `401/403` e retry della request originale.
      - Evitare esposizione token nei log verbosi/UI (mascheramento token in output).

    - Sicurezza trasporto
      - Completare supporto HTTPS con validazione certificato (ora presente `skip_cert_common_name_check = true`).
      - Aggiungere opzione pinning/cert bundle e policy per ambienti test/prod.

    - Allineamento API/spec
      - Uniformare endpoint attività: codice usa `/api/activity`, specifica indica `/api/deviceactivity`.
      - Verificare e allineare payload minimi richiesti per `keepalive`, `payment`, `serviceused`, `paymentoffline`.

    - Affidabilità operativa
      - Implementare coda persistente offline (SPIFFS/SD) per POST fallite con retry/backoff configurabile.
      - Distinguere policy errori `4xx`/`5xx`/timeout e gestione `Retry-After` (429).
      - Aggiungere task periodico per `keepalive` (non solo invocazione manuale/proxy).

    - Risorse remote (config/images/translations/firmware)
      - Implementare download file reale e salvataggio su SD/SPIFFS (oggi c’è proxy JSON della risposta).
      - Per firmware: validazione (checksum/versione) + integrazione con flusso OTA locale.

    - Osservabilità e test
      - Aggiungere metriche minime (`last_ok`, `last_err`, `queue_len`, `last_status_code`) esposte via API/UI.
      - Preparare test contract/flow dedicati alle route remote con mock server.
 9. lo scanner USB è gestito tramite la coda mesaggi?  
 10. la lettura di un codice QR deve passare a http_services che deve eseguire , se non già acquisito, il token tramite la chiamata login e quindi eseguire una chiamata a api_payment_post
 11. ✅ crea una funzione per visualizzare sullo schermo con il carattere a 48px la scritta 'Fuori servizio' ed eseguila nel caso di 'APP: [F] ERROR_LOCK attivo: avvio task inibito (reboot consecutivi=3)
'
      
 ```

> POST /api/login HTTP/1.1
> Host: 195.231.69.227:5556
> Content-Type: application/json
> Date: 2026-01-23T13:25:13.218763+01:00
> User-Agent: insomnia/12.1.0
> Accept: */*
> Content-Length: 83

| {
|     "serial":"AD-34-DFG-333",
|     "password":"c1ef6429c5e0f753ff24a114de6ee7d4"
| }
```
   
# ERRORI
E (122479) task_wdt: Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
E (122479) task_wdt:  - IDLE0 (CPU 0)
E (122479) task_wdt: Tasks currently running:
E (122479) task_wdt: CPU 0: main
E (122479) task_wdt: CPU 1: IDLE1
E (122479) task_wdt: Print CPU 0 (current core) registers
Core  0 register dump:
MEPC    : 0x4007f862  RA      : 0x4007f854  SP      : 0x4ff299a0  GP      : 0x4ff22300  
--- 0x4007f862: mipi_dsi_host_ll_gen_is_read_fifo_empty at /home/mauro/esp/v5.5.2/esp-idf/components/hal/esp32p4/include/hal/mipi_dsi_host_ll.h:690
--- (inlined by) mipi_dsi_hal_host_gen_read_short_packet at /home/mauro/esp/v5.5.2/esp-idf/components/hal/mipi_dsi_hal.c:212
--- 0x4007f854: mipi_dsi_hal_host_gen_read_short_packet at /home/mauro/esp/v5.5.2/esp-idf/components/hal/mipi_dsi_hal.c:210
TP      : 0x4ff29c40  T0      : 0x4fc0a9f8  T1      : 0x4ff3f004  T2      : 0x00003000  
S0/FP   : 0x00000001  S1      : 0x4ff3f008  A0      : 0x4ff3f008  A1      : 0x00000000  
A2      : 0x00000006  A3      : 0x00002005  A4      : 0x500a0000  A5      : 0x00000000  
A6      : 0x00000000  A7      : 0x00000008  S2      : 0x4ff299fb  S3      : 0x00000000  
S4      : 0x00000006  S5      : 0x00000000  S6      : 0x00000000  S7      : 0x00000000  
S8      : 0x00000000  S9      : 0x00000000  S10     : 0x00000000  S11     : 0x00000000  
T3      : 0x00000004  T4      : 0x00000003  T5      : 0x00000175  T6      : 0x00000033  
MSTATUS : 0x00001888  MTVEC   : 0x4ff00003  MCAUSE  : 0xdeadc0de  MTVAL   : 0xdeadc0de  
--- 0x4ff00003: _vector_table at ??:?
MHARTID : 0x00000000  
Please enable CONFIG_ESP_SYSTEM_USE_FRAME_POINTER option to have a full backtrace.

  # Scanner QR : 


  - ```
  Case Ciclo_Imager_Setup
      
  commandToSend = ChrW(126) & ChrW(1) & "0000#SCNMOD3;RRDENA1;CIDENA1;SCNENA0;RRDDUR3000;" & ChrW(3)
      
  wIsCommand = True
    
  Case Ciclo_Imager_State
    
    ' richiesta di stato
    
    commandToSend = ChrW(126) & ChrW(1) & "0000#SCNENA*;" & ChrW(3)
    
    wIsCommand = True
    
  Case Ciclo_Imager_On
    
    commandToSend = ChrW(126) & ChrW(1) & "0000#SCNENA1;" & ChrW(3)
    
    wIsCommand = True
    
  Case Ciclo_Imager_Off
    
    commandToSend = ChrW(126) & ChrW(1) & "0000#SCNENA0;" & ChrW(3)
    
    wIsCommand = True
    ```
  
   
  # API Server Base 📦

  - Server API base: `http://195.231.69.227:5556/`
  - Endpoints:
    - `POST http://195.231.69.227:5556/api/login`
    - `POST http://195.231.69.227:5556/api/keepalive`
    - `POST http://195.231.69.227:5556/api/payment`
    - `POST http://195.231.69.227:5556/api/paymentoffline`
    - `POST http://195.231.69.227:5556/api/serviceused`
    - `POST http://195.231.69.227:5556/api/deviceactivity`
    - `POST http://195.231.69.227:5556/api/getconfig`
    - `POST http://195.231.69.227:5556/api/getimages`
    - `POST http://195.231.69.227:5556/api/getfirmware`
    - `POST http://195.231.69.227:5556/api/gettranslations`
  - Auth: `Authorization: Bearer <token>` (ottenuto da `/api/login`)
    
  - Seriale Terminale 
    
    | Wash 3 | CF-55-DDD-222 |
    | ------ | ------------- |
  
     
    
    
    
  - Display 1.44 
   - Chiedere : 
    - problema alimentazione doppia (scheda e espansione) 
    - Porte USB sul p4?
    - Alimentazione display
    - tasto SERVICE su GPIO per boot + reset P4
    - Interrupt su cambio stato I/O Expander
    - dimensioni massime stringhe in activitydevice
- USB
  - letto lo scanner con il sorgente di test in /home/mauro/esp/esp-idf/examples/peripherals/usb/host/cdc/cdc_acm_host/main/usb_cdc_example_main.c

- App
  1. Aggiungere un task App per l'esecuzione delle funzioni operative della macchina controllata 
  2. Configurazione monitor/touch e integrazione LVGL 
  3. Creazione Slideshow iniziale
  4. Creazione Pagina di test delle operazioni

# 🖥️ TESTATI

1. Led Stripe
2. PWM1 e PWM2
3. RS232 (tx rx)
4. RS485 (tx)
5. MDB (tx)
6. Scanner

----------------------------------------------


i/mauro/code/Progetti/0.Clienti/MicroHard/test_wave/.venv/bin/activate
--- esp-idf-monitor 1.9.0 on /dev/ttyACM0 115200
--- Quit: Ctrl+] | Menu: Ctrl+T | Help: Ctrl+T followed by Ctrl+H
ESP-ROM:esp32p4-eco2-20240710
Build:Jul 10 2024
rst:0x1 (POWERON),boot:0x30f (SPI_FAST_FLASH_BOOT)
SPI mode:DIO, clock div:1
load:0x4ff33ce0,len:0x10b4
load:0x4ff29ed0,len:0xb5c
load:0x4ff2cbd0,len:0x32b8
entry 0x4ff29ed0
I (1079) esp_core_dump_flash: Init core dump to flash
I (1079) esp_core_dump_flash: Found partition 'coredump' @ e50000 188416 bytes
I (1094) esp_core_dump_flash: Core dump data checksum is correct
I (1094) esp_core_dump_flash: Found core dump 13988 bytes in flash @ 0xe50000
I (1096) INIT: Valore GPIO3: 1
I (1126) INIT: [M] Sezioni DNA mock attive: LED_STRIP RS232 RS485 PWM GPIO MDB
W (1136) INIT: Avviso lettura NVS: ESP_ERR_INVALID_ARG (verranno usate le defaults)
W (1136) INIT: [M] boot_guard: reset_reason=1 force_crash=0 consecutive_reboots=0 limit=3
I (1216) INIT: [M] SPIFFS montato: totale=1438481, usato=147588
I (1216) INIT: [M] === Elenco file SPIFFS ===
I (1226) INIT: [M]   [1] i18n_en.json (60915 bytes)
I (1226) INIT: [M]   [2] tasks.csv (1608 bytes)
I (1226) INIT: [M]   [3] i18n_it.json (65434 bytes)
I (1236) INIT: [M]   [4] logo.jpg (12183 bytes)
I (1236) INIT: [M]   [5] tasks.json (1757 bytes)
I (1246) INIT: [M]   [6] .gitkeep (77 bytes)
I (1246) INIT: [M]   [7] activity.json (170 bytes)
I (1246) INIT: [M]   [8] .comments/image01.jpg.xml (152 bytes)
I (1256) INIT: [M]   [9] programs.json (1068 bytes)
I (1296) INIT: [M] === Totale file: 9 ===
I (1346) INIT: [M] tasks.json presente: 1757 byte
I (1346) INIT: [M] Partizione in esecuzione: ota_1 (tipo 0, sottotipo 17)
I (1346) INIT: [M] Partizione boot      : ota_1 (tipo 0, sottotipo 17)
I (1356) INIT: [M] BSP I2C attivo: inizializzo EEPROM 24LC16 su i2c_master
I (2056) INIT: [M] remote_logging early init attivo (log pre-rete catturati)
W (2056) INIT: [M] [C] preboot crash send saltato: server non configurato
I (2056) INIT: Heap before display init:
I (2066) INIT:   INTERNAL free: 294843
I (2066) INIT:   DMA free: 255295
I (2066) INIT:   PSRAM free: 33242016
I (2076) INIT:   SPIRAM caps alloc free (8bit): 33242016
E (3506) lcd_panel.io.i2c: panel_io_i2c_rx_buffer(145): i2c transaction failed
E (3506) GT911: touch_gt911_read_cfg(410): GT911 read error!
E (3506) GT911: esp_lcd_touch_new_i2c_gt911(161): GT911 init failed
E (3516) GT911: Error (0x103)! Touch controller GT911 initialization failed!
ESP_ERROR_CHECK failed: esp_err_t 0x103 (ESP_ERR_INVALID_STATE) at 0x400378e2
file: "./components/waveshare__esp32_p4_nano/esp32_p4_nano.c" line 777
func: bsp_display_indev_init
expression: bsp_touch_new(((void *)0), &tp)

abort() was called at PC 0x4ff141a7 on core 0
Backtrace: 0x4ff141fe:0x4ff2ed20 0x4ff141ae:0x4ff2ed20 0x4ff1e38c:0x4ff2ed30 0x4ff141a8:0x4ff2eda0 0x400378e4:0x4ff2edb0 0x4003792c:0x4ff2ede0 0x40014a80:0x4ff2edf0 0x400160ec:0x4ff2ee40 0x40013c2c:0x4ff2ee60 0x40108e20:0x4ff2ee80 0x4ff15624:0x4ff2eea0
--- 0x40014a80: io_mux_enable_lp_io_clock at /home/mauro/esp/v5.5.2/esp-idf/components/esp_hw_support/port/esp32p4/io_mux.c:69
--- 0x400160ec: i2c_common_deinit_pins at /home/mauro/esp/v5.5.2/esp-idf/components/esp_driver_i2c/i2c_common.c:462
--- 0x40013c2c: spi_flash_hal_init at /home/mauro/esp/v5.5.2/esp-idf/components/hal/spi_flash_hal.c:118
Unsupported DWARF opcode 0: 0x00000007




ELF file SHA256: b81c04875
--- Warning: Checksum mismatch between flashed and built applications. Checksum of built application is 88357c1fa76465f0373a0e589c7d6b49848732493ca29c9a023ca489e09d909c

I (3610) esp_core_dump_flash: Save core dump to flash...
I (3616) esp_core_dump_common: Backing up stack @ 0x4ff2ebd0 and use core dump stack @ 0x4ff261a0
I (3625) esp_core_dump_flash: Erase flash 16384 bytes @ 0xe50000
I (3787) esp_core_dump_flash: Write end offset 0x36c4, check sum length 4
I (3787) esp_core_dump_common: Core dump used 980 bytes on stack. 904 bytes left free.
I (3790) esp_core_dump_common: Restoring stack @ 0x4ff2ebd0
I (3795) esp_core_dump_flash: Core dump has been saved to flash.
Rebooting...
ESP-ROM:esp32p4-eco2-20240710
Build:Jul 10 2024
rst:0xc (SW_CPU_RESET),boot:0x30f (SPI_FAST_FLASH_BOOT)
Core0 Saved PC:0x4ff0da2c
--- 0x4ff0da2c: spi_flash_hal_common_command at /home/mauro/esp/v5.5.2/esp-idf/components/hal/spi_flash_hal_common.inc:188
Core1 Saved PC:0x4ff0db74
--- 0x4ff0db74: spimem_flash_ll_get_buffer_data at /home/mauro/esp/v5.5.2/esp-idf/components/hal/esp32p4/include/hal/spimem_flash_ll.h:357
--- (inlined by) spi_flash_hal_common_command at /home/mauro/esp/v5.5.2/esp-idf/components/hal/spi_flash_hal_common.inc:212
SPI mode:DIO, clock div:1
load:0x4ff33ce0,len:0x10b4
load:0x4ff29ed0,len:0xb5c
load:0x4ff2cbd0,len:0x32b8
entry 0x4ff29ed0
I (1021) esp_core_dump_flash: Init core dump to flash
I (1021) esp_core_dump_flash: Found partition 'coredump' @ e50000 188416 bytes
I (1036) esp_core_dump_flash: Core dump data checksum is correct
I (1036) esp_core_dump_flash: Found core dump 14020 bytes in flash @ 0xe50000
I (1038) INIT: Valore GPIO3: 1
I (1068) INIT: [M] Sezioni DNA mock attive: LED_STRIP RS232 RS485 PWM GPIO MDB
W (1078) INIT: Avviso lettura NVS: ESP_ERR_INVALID_ARG (verranno usate le defaults)
W (1088) INIT: [M] boot_guard: reset_reason=4 force_crash=0 consecutive_reboots=1 limit=3
W (1088) INIT: [M] [C] crash pending registrato: reason=4
I (1168) INIT: [M] SPIFFS montato: totale=1438481, usato=147588
I (1168) INIT: [M] === Elenco file SPIFFS ===
I (1168) INIT: [M]   [1] i18n_en.json (60915 bytes)
I (1178) INIT: [M]   [2] tasks.csv (1608 bytes)
I (1178) INIT: [M]   [3] i18n_it.json (65434 bytes)
I (1188) INIT: [M]   [4] logo.jpg (12183 bytes)
I (1188) INIT: [M]   [5] tasks.json (1757 bytes)
I (1188) INIT: [M]   [6] .gitkeep (77 bytes)
I (1188) INIT: [M]   [7] activity.json (170 bytes)
I (1198) INIT: [M]   [8] .comments/image01.jpg.xml (152 bytes)
I (1208) INIT: [M]   [9] programs.json (1068 bytes)
I (1248) INIT: [M] === Totale file: 9 ===
I (1298) INIT: [M] tasks.json presente: 1757 byte
I (1298) INIT: [M] Partizione in esecuzione: ota_1 (tipo 0, sottotipo 17)
I (1298) INIT: [M] Partizione boot      : ota_1 (tipo 0, sottotipo 17)
I (1298) INIT: [M] BSP I2C attivo: inizializzo EEPROM 24LC16 su i2c_master
I (2008) INIT: [M] remote_logging early init attivo (log pre-rete catturati)
W (2008) INIT: [M] [C] preboot crash send saltato: server non configurato
I (2008) INIT: Heap before display init:
I (2008) INIT:   INTERNAL free: 294843
I (2018) INIT:   DMA free: 255295
I (2018) INIT:   PSRAM free: 33242016
I (2018) INIT:   SPIRAM caps alloc free (8bit): 33242016
E (3448) lcd_panel.io.i2c: panel_io_i2c_rx_buffer(145): i2c transaction failed
E (3458) GT911: touch_gt911_read_cfg(410): GT911 read error!
E (3458) GT911: esp_lcd_touch_new_i2c_gt911(161): GT911 init failed
E (3458) GT911: Error (0x103)! Touch controller GT911 initialization failed!
ESP_ERROR_CHECK failed: esp_err_t 0x103 (ESP_ERR_INVALID_STATE) at 0x400378e2
file: "./components/waveshare__esp32_p4_nano/esp32_p4_nano.c" line 777
func: bsp_display_indev_init
expression: bsp_touch_new(((void *)0), &tp)

abort() was called at PC 0x4ff141a7 on core 0
Backtrace: 0x4ff141fe:0x4ff2ed20 0x4ff141ae:0x4ff2ed20 0x4ff1e38c:0x4ff2ed30 0x4ff141a8:0x4ff2eda0 0x400378e4:0x4ff2edb0 0x4003792c:0x4ff2ede0 0x40014a80:0x4ff2edf0 0x400160ec:0x4ff2ee40 0x40013c2c:0x4ff2ee60 0x40108e20:0x4ff2ee80 0x4ff15624:0x4ff2eea0
--- 0x40014a80: io_mux_enable_lp_io_clock at /home/mauro/esp/v5.5.2/esp-idf/components/esp_hw_support/port/esp32p4/io_mux.c:69
--- 0x400160ec: i2c_common_deinit_pins at /home/mauro/esp/v5.5.2/esp-idf/components/esp_driver_i2c/i2c_common.c:462
--- 0x40013c2c: spi_flash_hal_init at /home/mauro/esp/v5.5.2/esp-idf/components/hal/spi_flash_hal.c:118
Unsupported DWARF opcode 0: 0x00000007




ELF file SHA256: b81c04875
--- Warning: Checksum mismatch between flashed and built applications. Checksum of built application is 88357c1fa76465f0373a0e589c7d6b49848732493ca29c9a023ca489e09d909c

I (3556) esp_core_dump_flash: Save core dump to flash...
I (3561) esp_core_dump_common: Backing up stack @ 0x4ff2ebd0 and use core dump stack @ 0x4ff261a0
I (3570) esp_core_dump_flash: Erase flash 16384 bytes @ 0xe50000
I (3731) esp_core_dump_flash: Write end offset 0x36c4, check sum length 4
I (3732) esp_core_dump_common: Core dump used 980 bytes on stack. 904 bytes left free.
I (3734) esp_core_dump_common: Restoring stack @ 0x4ff2ebd0
I (3739) esp_core_dump_flash: Core dump has been saved to flash.
Rebooting...
ESP-ROM:esp32p4-eco2-20240710
Build:Jul 10 2024
rst:0xc (SW_CPU_RESET),boot:0x30f (SPI_FAST_FLASH_BOOT)
Core0 Saved PC:0x4ff0da2c
--- 0x4ff0da2c: spi_flash_hal_common_command at /home/mauro/esp/v5.5.2/esp-idf/components/hal/spi_flash_hal_common.inc:188
Core1 Saved PC:0x4ff0db74
--- 0x4ff0db74: spimem_flash_ll_get_buffer_data at /home/mauro/esp/v5.5.2/esp-idf/components/hal/esp32p4/include/hal/spimem_flash_ll.h:357
--- (inlined by) spi_flash_hal_common_command at /home/mauro/esp/v5.5.2/esp-idf/components/hal/spi_flash_hal_common.inc:212
SPI mode:DIO, clock div:1
load:0x4ff33ce0,len:0x10b4
load:0x4ff29ed0,len:0xb5c
load:0x4ff2cbd0,len:0x32b8
entry 0x4ff29ed0
--- Error: device reports readiness to read but returned no data (device disconnected or multiple access on port?)