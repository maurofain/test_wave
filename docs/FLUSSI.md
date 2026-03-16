# Flusso operativo del programma

 📊 TIMELINE COMPLESSIVA   

1. Boot
2. Init periferiche
   1. Log (ms 0)
   2. Boot banner (ms 10)
   3. I2C init (ms 50) + I2C scan (ms 100)
   4. Factory init (ms 200-3000)
      1. NVS init (ms 200)
      2. SPIFFS mount (ms 300)
      3. Event loop (ms 400)
      4. Device config (ms 500)
      5. Remote logging (ms 600)
      6. FSM event queue (ms 650)
      7.  GPIO ausiliari (ms 700)
      8. Display + LVGL (ms 800-1500)
      9. Ethernet (ms 1600)
      10. Web UI server (ms 1700)
      11. Periferiche (ms 1800-2500)
      12. SD card (ms 2600)
   5. Log policy (ms 3000)
   6. Tasks init (ms 3100)
   7.  Stabilization window (ms 3200-13200) <- CRITICAL 10sec
   8. CCTalk pre-logo (ms 13300)
   9. Display slideshow (ms 13400)
   10. Boot complete (ms 13500)
   11. Heartbeat loop (ms 13600+)
   12. 

####  🔍 ORDINE DIPENDENZE CRITICHE

```
│ │ NVS ────────┐                                                                                                                 │ │             ├─→ Device config                                                                                                 │ │ SPIFFS ─────┤     (required by all)                                                                                           │ │             │                                                                                                                 │ │ Event loop ─┘                                                                                                                 
│ │                                                                                                                               
│ │ Device config ─────────┐                                                                                                     
│ │                        ├─→ I/O Expander ─┐                                                                                   
│ │                        │                 ├─→ Task system                                                                     
│ │                        ├─→ RS232 ───┐   │                                                                                     
│ │                        │           ├─→ FSM ─┐                                                                                 
│ │                        ├─→ MDB ────┤        │                                                                                 
│ │                        │           ├─→ Tasks                                                                                 
│ │                        ├─→ Display─┤                                                                                         
│ │                        │           └─→ Web UI                                                                                 
│ │                        └─→ Ethernet                                                                                           
│ │                                                                                                                               
│ │ FSM ──────────────────┐                                                                                                       
│ │                       ├─→ HTTP services                                                                                       
│ │ Web UI ───────────────┤                                                                                                       
│ │                       └─→ Tasks              
```

## FLUSSO OPERATIVO CLIENTE

1. alla fine dell'init si abilitano :
   1. gettoniera
   2. QRCode
   3. MDB (chip card)
2. Se le ads sono abilitate lo schermo presenta la scherma ads con la pubblicità e con il tasto 'Scegli programma', se ads non abilita si passa direttamente al punto 3.
3. si attende un evento:
   1. pressione del touch (al pulsante sceli programma o in un punto qualsiai) 
   2. pressione di un pulsante (mettere un hook ad una funzione da definire in seguito) 
   3. lettura barcode
   4. inserimento moneta (credito effettivo)
   5. lettura chipcard 
4. si passa alla scelta programmi abilitando i programmi che hanno una richiesta di credito <=  a quello letto nella fase 3, continuando ad accettare nuovo credito 
   1. se si è rilevato la pressionetouch/tasto o l'inserimento di una moneta: ogni tipo di pagamento
   2. se si è rilevato QRCode o card (credito virtuale): non si accettano ulteriori crediti
5. Si esegue il programma attivando i relays definiti nella tabella programmi per il tempo definito nella stessa tabella
   1. Si preleva il credito (prima il credito effettivo e poi il virtuale) e si procede all'esecuzione del programma scelto per più cicli finché c'è disponibilità di credito (terminato il credito effettivo si preleva dal virtuale quanto necessario all'esecuzione del programma )
   2. Si può cambiare il programma premendo su un altro tasto e il sistema adegua il tempo rimanente in base al rateo credito residuo / tempo.x.1.credito
   3. Si può sospendere il programma per un tempo massimo come definito in tabella programmi. La sospensione si ottiene premendo lo stesso programma e la riattivazione premendo un programma qualsiasi
   4. Il tasto stop ferma l'esecuzione del programma corrente e si azzera il tempo rimanente
      1. se il credito è effettivo lo schermo rimane in scelta programmi per il tempo definito in Config->Timeouts , dopo di che azzera il credito effettivo e ritorna in ads o scelta programmi con credito azzerato.
      2. se il credito rimanente è virtuale la sessione è conclusa ritorna in ads o scelta programmi con credito azzerato.
6. In ogni momento è possibile cambiare la lingua premendo la bandierina in alto a dx
