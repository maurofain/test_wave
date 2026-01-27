# Descrizione scheda Waveshare

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

La pagina di statistiche deve riportare (stato e se ON i valori)

- Stato delle interfacce di rete
- Stato degli I/O expander
- Temperatura e umidità
- Stato delle porte seriali
- Stato dei PWM
