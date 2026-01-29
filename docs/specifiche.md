# Descrizione Progetto con  scheda Waveshare

- creiamo in questa cartella un progetto in esp-idf che usa la scheda ESP32-P4 Waveshare https://www.waveshare.com/product/arduino/boards-kits/esp32-p4/esp32-p4-module-dev-kit.htm e che ha una scheda di espansione con l'hardware descritto in HARDWARE_SETUP al punto 1 e 8

- la scheda deve usare la tabella di partizioni già presente nel progetto, implementare l'interfaccia ethernet e il wifi per la comunicazione, il protocollo http, la funzione OTA con la partizione factory che contiene un firmaware per il recovery della partizione ota_0 e funzioni di test e configurazione , la partizione ota_0 ha la app principale del dispositivo. Per ora implementiamo la app factory

- Va utilizzato freeRTOs. implementiamo un sorgente init.c con tutte le funzioni di inizializzazione, un sorgente tasks.c con tutte le funzioni che prevedeno la creazione di tasks. Va creata una struttura task_param  per la definizione dei task in cui indichiamo stato (run, idle, pause) , priorità, core da utilizzare, frequenza di attivazione della funzione. Per ogni task utilizzato va compilata un istanza di task_param memorizzata in un array.

- creiamo un task per ognuna delle seguenti funzioni:

  - gestione della EEPROM
  - gestione degli i/o expander
  - gestione del sensore temperatura e umidità
  - gestione della striscia led
  - gestore linea rs232
  - gestore linea rs485
  - gestore linea MDB
  - gestione dei PWM
  - gestione del touchscreen (implementazione futura)
  - gestione della grafica tramite LVGL (implementazione futura)

- la scheda waveshare usa il chip IP101

- nell'interfaccia web OTA creiamo le funzioni per il caricamento OTA della partizione main e un pulsante per una pagina di configurazione e per una pagina di statistiche. 
  La pagina di configurazione devge permettere di editare :

  - i dati di connessione per Ethernet e Wifi : per ognuna di queste sezioni deve esserci:
    - Abilitazione ON/OFF
    - se abilitata 
      - DHCP on/off
      - se DHCP = OFF
        - IP Address
        - subnet
        - gateway

  - Switch di abilitazione dei sensori 
    - i/o expander, 
    - temperatura,
    - led,
    - RS232,
    - RS485,
    - MDB,
    - PWM1,
    - PWM2

### Pagina statistiche

La pagina di statistiche deve riportare (stato e se ON i valori)

Stato delle interfacce di rete

Stato degli I/O expander

Temperatura e umidità

Stato delle porte seriali

Stato dei PWM

### Pagina Test

- aggiungi una pagina test e predisponi i seguenti test (ognuno con start e stop):
  - test led_stripe 
    - inizializza la led stripe ed esegue un cambio colri RGB per 20 secondi e poi un running led per altri 20 secondi, con una dinamica visibile dall'utente (non troppo veloce)
  - test pwm1
    - genera un segnale PWM con variazione del duty ctcle da 50% a 0% a 100% in 10 secondi ripetuto per frequenza di 100, 1000, 10000 Hz
  - test pwm2
    - genera un segnale PWM con variazione del duty ctcle da 50% a 0% a 100% in 10 secondi ripetuto per frequenza di 100, 1000, 10000 Hz
  - test I/O expander
    - ON/OFF di tutte le porte a 1hz
  - test RS232
    - Invia ripetutamente il carattere 0x55 per 1 secondo, poi lo stesso per 0xAA, 0x01, 0x07 poi ricomincia
  - test RS485
    - Invia ripetutamente il carattere 0x55 per 1 secondo, poi lo stesso per 0xAA, 0x01, 0x07 poi ricomincia
  - test MDB
    - da definire

### Test

nelle funzioni di test dei dispositivi seriali aggiugi la possibilità di inviare una stringa (con gestione degli escape esempio \0x41 per A ) con un tasto invia e monitoraggio delle risposte. Crea glie endpoint dei test e predisponi nella cartella components una sottocartella per ogni componenete con il sorgente per i test

### I/O Expander

gli I/O expander usano 2 chip FXL6408UMX : 

- il primo con tutte le porte in output usa l'indirizzo i2c 0x86 in Write e 0x87 in Read

- il secondo con tutte le porte in input usa l'indirizzo i2c 0x88 in Write e 0x89 in Read 

  Dobbiamo verificare se esiste una libreria espressif ufficiale, se manca creare una libreria per i comandi I2C

  Vanno creare due variabili di tipo uint_8t per lo stato delle le porte con possibilità di settare e leggere le singole porte e funzioni tipo void io_set_pin(port_n, value), void io_set_port(uint_8t val),  bool io_get_pin(port_n), uint_8t io_get()

## Integrazione Monitor e Touchscreen (Strategia Ibrida)

Per l'implementazione futura del monitor e del touchscreen si adotterà un approccio **ibrido**:

1.  **Framework BSP / Driver Ufficiali**: Si utilizzeranno i driver specifici del framework BSP o la libreria `esp_lcd` di ESP-IDF per la gestione del controller LCD e del controller Touch (es. via MIPI-DSI e I2C).
2.  **Mantenimento Codice Custom**: Tutte le periferiche già implementate (Ethernet RMII, SD 4-bit, Seriali, MDB, I2C Expander) continueranno a utilizzare il codice custom attuale per garantire il massimo controllo e la compatibilità con la mappatura GPIO specifica della scheda MicroHard.
3.  **Bus I2C Condiviso**: Il touchscreen verrà integrato sul bus I2C principale (GPIO 26/27) insieme agli expander e ai sensori esistenti.
4.  **Interfaccia Grafica**: Si utilizzerà la libreria LVGL per lo sviluppo della UI, che girerà in un task dedicato coordinandosi con gli altri task di sistema tramite le strutture `task_param` e code di messaggi.
5.  **Risorse GPIO**: L'uso dell'interfaccia MIPI-DSI nativa dell'ESP32-P4 permetterà di pilotare il display senza sottrarre GPIO alle periferiche critiche già configurate.
