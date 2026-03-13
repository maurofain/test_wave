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

