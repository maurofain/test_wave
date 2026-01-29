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
14. Driver RS485 e ricezione dati (timeout hardware 10ms)
15. Test EEprom: tasto "Leggi JSON" e visualizzazione CRC/Updated nell'interfaccia web
16. Implementata la scheda SD




# ⏸️ RITARDATI

1. abilitazione WiFi

   

# 📋 DA FARE

- Factory 
    1.  aggiungi nel test PWM il settaggio manuale di frequenza e duty
    2.  aggiungi nel test Led il settaggio manuale di colore (codice HTML)  e luminosità per tutti i led
    
- App
  1. Configurazione monitor/touch e integrazione LVGL 
  2. Creazione Slideshow iniziale
  3. Creazione Pagina di test delle operazioni