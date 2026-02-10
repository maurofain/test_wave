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



# ⏸️ RITARDATI

1. abilitazione WiFi

# 📋 DA FARE

- Factory 

 1. il tasto CLEAR dei monitor non pulisce le aree di testo, togliere le indicazioni TX e RX>, c'è già il colore. Non inserire spazi tra i caratteri in modo TEXT. In modo HEX mettere un . prima del codice esadecimale.
 2. implementa il salvataggio dei log degli errori su SD. Il log deve comprendere i dati di crash con lo stack chiamate

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
  
   
  
  - IP Server API 195.231.69.227:5556/api/login
    
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