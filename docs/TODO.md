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
 3. Controllo luminosità schermo: implementarla e collagarla nella pagina web /config
 4. interfaccia web: implementare in /config l'abilitazione e la configurazione dello scanner. Nella pagina /test aggiungere la visualizzazione delle letture e i tastoi 'Scanner ON' e 'Scenner OFF'. nella pagina /tasks verificare che tutti i task siano rappresentati.
   

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
- USB
  - letto lo scanner con il sorgente di test in /home/mauro/esp/esp-idf/examples/peripherals/usb/host/cdc/cdc_acm_host/main/usb_cdc_example_main.c
  ```
  I (2318) USB-CDC: Opening CDC ACM device 0x1EAB:0x0006...
*** Device descriptor ***
bLength 18
bDescriptorType 1
bcdUSB 1.10
bDeviceClass 0x2
bDeviceSubClass 0x0
bDeviceProtocol 0x0
bMaxPacketSize0 64
idVendor 0x1eab
idProduct 0x6
bcdDevice 0.00
iManufacturer 1
iProduct 2
iSerialNumber 3
bNumConfigurations 1
*** Configuration descriptor ***
bLength 9
bDescriptorType 2
wTotalLength 67
bNumInterfaces 2
bConfigurationValue 1
iConfiguration 4
bmAttributes 0x80
bMaxPower 500mA
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 0
        bAlternateSetting 0
        bNumEndpoints 1
        bInterfaceClass 0x2
        bInterfaceSubClass 0x2
        bInterfaceProtocol 0x1
        iInterface 7
        *** CDC Header Descriptor ***
        bcdCDC: 1.10
        *** CDC ACM Descriptor ***
        bmCapabilities: 0x02
        *** CDC Union Descriptor ***
        bControlInterface: 0
        bSubordinateInterface[0]: 1
        *** CDC Call Descriptor ***
        bmCapabilities: 0x00
        bDataInterface: 1
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x83   EP 3 IN
                bmAttributes 0x3        INT
                wMaxPacketSize 16
                bInterval 8
        *** Interface descriptor ***
        bLength 9
        bDescriptorType 4
        bInterfaceNumber 1
        bAlternateSetting 0
        bNumEndpoints 2
        bInterfaceClass 0xa
        bInterfaceSubClass 0x0
        bInterfaceProtocol 0x0
        iInterface 8
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x81   EP 1 IN
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
                *** Endpoint descriptor ***
                bLength 7
                bDescriptorType 5
                bEndpointAddress 0x2    EP 2 OUT
                bmAttributes 0x2        BULK
                wMaxPacketSize 64
                bInterval 0
I (2648) USB-CDC: Setting up line coding
I (2648) USB-CDC: Line Get: Rate: 115200, Stop bits: 0, Parity: 0, Databits: 8
I (2648) USB-CDC: Line Set: Rate: 9600, Stop bits: 1, Parity: 1, Databits: 7
I (2648) USB-CDC: Line Get: Rate: 9600, Stop bits: 1, Parity: 1, Databits: 7
I (2658) USB-CDC: Example finished successfully! You can reconnect the device to run again.
```  
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