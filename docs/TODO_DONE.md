# TODO_DONE — MicroHard test_wave

## ✅ Completati

- [x] Integraiamo della logica DNA anche le funzioni relative all'audio
- [x] L'orario usato per l'invio al server è stato riallineato a UTC per le comunicazioni server, mantenendo UTC + Timezone + DST per l'interfaccia
- [x] Corretto il flusso scanner QR dalla pagina Programmi: il credito viene ora mostrato correttamente anche senza passare da ADS
- [x] Corretto lo stato Modbus in home: ora usa il runtime Modbus reale invece dei campi MDB/gettoniera
- [x] Home `/`: nella sezione Stato servizi i device esclusi via DNA ora mostrano `Disab.` in blu
- [x] Aggiunta l'icona stato Modbus `Skio` nel chrome LVGL come prima da sinistra, agganciata al runtime Modbus reale
- [x] Ridimensionate `SkioOk.png` e `SkioKo.png` a 48x48 con padding nero e aggiunte alla lista di conversione embedded in `scripts/embed_icons.sh`
- [x] Alzati orologio, icone stato e bandiera del chrome LVGL per evitare la sovrapposizione con la progress bar nella pagina Programmi
- [x] Allineato il payload `payment`: `services[].code` ora usa i codici enum server (`CWASH`, `CGATE`, `CLANE`, `CCLEAN`)
- [x] Allineato il payload `payment`: `paymenttype` ora e' una stringa codificata secondo la specifica server (`CASH`, `CREC`, `BCOIN`, `COIN`, `SATI`, `VOUC`, `CASHL`)
- [x] Aggiornato il piano MDB cashless con il flusso NFC prepagato, il pagamento `CASHL`, la utility backend di ricarica e le funzioni riusabili individuate in `/home/mauro/1P/MicroHard/docs/cashless`
- [x] Modifiche all'uso dei puntini sopra la bandiera lingue
   1. I puntini vanno cambiati con le icone presenti in `/docs/icone/normalized` (icona embedded nel firmware o servita da SD/SPIFFS a seconda della scelta).
   2. L'icona `CloudKo.png` e `CloudOk.png` vanno visualizzate al posto del primo punto a sinistra e rappresentano lo stato della connessione con i servizi HTTPS (KO e OK rispettivamente).
   3. L'icona `CreditCardKo.png` e `CreditCardOk.png` vanno visualizzate al posto del secondo punto da sinistra e rappresentano lo stato della connessione con i device MDB (OK e KO rispettivamente).
   4. L'icona `MoneteKo.png` e `MoneteOk.png` vanno visualizzate al posto del terzo punto da sinistra e rappresentano lo stato della connessione con i device CCTalk (OK e KO rispettivamente).
   5. L'icona `QrKo.png` e `QrOk.png` vanno visualizzate al posto del punto più a destra e rappresentano lo stato della connessione con lo scanner QRCode (OK e KO rispettivamente).