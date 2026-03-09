Offset  Byte         Descrizione
────────────────────────────────────────────────────
0       destination   Indirizzo destinatario (0x02 = gettoniera)
1       length        Lunghezza dei dati (0x00 a 0xFF)
2       source        Indirizzo mittente (0x01 = master)
3       header        Codice comando (0xFE, 0xF8, 0xE7, ecc.)
4...N   data[]        Dati payload (lunghezza variabile)
N+1     checksum      Checksum (somma modulo 256 = 0x00)

02 00 01 FE FF
↓  ↓  ↓  ↓  ↓
┌─ dest: 0x02 (gettoniera)
   ┌─ length: 0x00 (nessun dato)
      ┌─ source: 0x01 (master)
         ┌─ header: 0xFE (254 = Address Poll)
            └─ checksum: 0xFF (calcolato)

02 00 01 F8 05
↓  ↓  ↓  ↓  ↓
┌─ dest: 0x02
   ┌─ length: 0x00
      ┌─ source: 0x01
         ┌─ header: 0xF8 (248 = Request Status)
            └─ checksum: 0x05

02 01 01 E7 00 17
↓  ↓  ↓  ↓  ↓  ↓
┌─ dest: 0x02
   ┌─ length: 0x01 (1 byte di dati)
      ┌─ source: 0x01
         ┌─ header: 0xE7 (231 = Modify Master Inhibit)
            ┌─ data: 0x00 (disabilita)
               └─ checksum: 0x17

02 02 01 E7 00 00 16
↓  ↓  ↓  ↓  ↓  ↓  ↓
┌─ dest: 0x02
   ┌─ length: 0x02 (2 byte di dati)
      ┌─ source: 0x01
         ┌─ header: 0xE7 (231 = Modify Inhibit Status)
            ┌─ data[0]: 0x00 (mask_low = tutti canali attivi)
               ┌─ data[1]: 0x00 (mask_high = tutti canali attivi)
                  └─ checksum: 0x16

02 01 01 E7 01 16
↓  ↓  ↓  ↓  ↓  ↓
┌─ dest: 0x02
   ┌─ length: 0x01
      ┌─ source: 0x01
         ┌─ header: 0xE7 (231 = Modify Master Inhibit)
            ┌─ data: 0x01 (abilita)
               └─ checksum: 0x16

02 00 01 E6 04
↓  ↓  ↓  ↓  ↓
┌─ dest: 0x02
   ┌─ length: 0x00
      ┌─ source: 0x01
         ┌─ header: 0xE6 (230 = Request Inhibit Status)
            └─ checksum: 0x04