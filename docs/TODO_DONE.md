# TODO_DONE — MicroHard test_wave

## ✅ Completati

- [x] Integraiamo della logica DNA anche le funzioni relative all'audio
- [x] L'orario usato per l'invio al server è stato riallineato a UTC per le comunicazioni server, mantenendo UTC + Timezone + DST per l'interfaccia
- [x] Corretto il flusso scanner QR dalla pagina Programmi: il credito viene ora mostrato correttamente anche senza passare da ADS
- [x] Corretto lo stato Modbus in home: ora usa il runtime Modbus reale invece dei campi MDB/gettoniera
- [x] Allineato il payload `payment`: `services[].code` ora usa i codici enum server (`CWASH`, `CGATE`, `CLANE`, `CCLEAN`)
- [x] Allineato il payload `payment`: `paymenttype` ora e' una stringa codificata secondo la specifica server (`CASH`, `CREC`, `BCOIN`, `COIN`, `SATI`, `VOUC`, `CASHL`)
