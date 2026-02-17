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
  11. Usare la EEProm per memorizzare i dati di config. Implementiamo la seguente logica :
       1. I dati di config devono avere un valore di CRC
       2. all'avvio verifichiamo se c'è il config in EEPROM e se è valido
          1. altrimenti lo leggiamo da NVS e settiamo il flag di modificato in EEProm
          2. se non c'è in NVS o non è validi (CRC) carichiamo i valori di default, scriviamo EEProm e NVS
          3. 3. alla modifica dei parametri (tasto save in configurazione) salviamo i dati nella EEProm e sempre nella eeprom  settiamo flag di modificato 
          4. al successivo riavvio salviamo i dati (se validi) in NVS - se non validi li riprendiamo da NVS e settiamo il flag di modificato
12. Driver PWM
13. Duplicazione codice main in cartella app per sviluppo produzione
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
- ✅ CCtalk: seriale attivata + task di background (driver in `components/cctalk`, UART = CONFIG_APP_RS232_UART_PORT, baud 4800 — TX=GPIO20, RX=GPIO21)

# ⏸️ RITARDATI

1. abilitazione WiFi
2. display miimale 1.44"

# 📋 DA FARE

- Factory 

 1. CCtals da integrare in /config e /test
 2. Creare una FSM per la gestione del ciclo operativo della macchina
 3. il tasto CLEAR dei monitor non pulisce le aree di testo, togliere le indicazioni TX e RX>, c'è già il colore. Non inserire spazi tra i caratteri in modo TEXT. In modo HEX mettere un . prima del codice esadecimale.
 4. implementa il salvataggio dei log degli errori su SD. Il log deve comprendere i dati di crash con lo stack chiamate
 5. Controllo luminosità schermo: implementarla e collagarla nella pagina web /config
 6. riportare lo schermo in verticale 
 7.
   
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