# Protocollo ccTalk — Tabella Completa dei Comandi

## Introduzione

**ccTalk** è un protocollo di comunicazione seriale sviluppato da **Money Controls** (ora parte di Crane Payment Innovations), usato principalmente per:
- Accettatori di monete
- Validatori di banconote
- Gettoniere e hopper (dispenser di monete)

### Caratteristiche tecniche
- Comunicazione **single-wire, half-duplex**
- Indirizzamento a **8 bit**
- Velocità tipica: **9600 baud**
- Struttura messaggio: `destinazione | lunghezza dati | sorgente | header | dati | checksum`
- **Checksum**: somma di tutti i byte deve dare `0x00` (mod 256)
- **Indirizzo broadcast**: `0xFF` (255)

---

## Tabella Comandi

| Header | Nome Comando | Direzione | Note |
|--------|-------------|-----------|------|
| 0 | Factory set-up and test | Master→Slave | Uso interno produzione |
| 1 | Reset device | Master→Slave | Soft reset |
| 2 | Read switching factor | Master→Slave | |
| 3 | Write switching factor | Master→Slave | |
| 4 | Teach mode control | Master→Slave | |
| 5 | Read creation date | Master→Slave | Data creazione firmware |
| 6 | Read variable set | Master→Slave | |
| 7 | Write variable set | Master→Slave | |
| 8 | Read default sorter path | Master→Slave | |
| 9 | Write default sorter path | Master→Slave | |
| 10 | Read switch threshold | Master→Slave | |
| 11 | Write switch threshold | Master→Slave | |
| 12 | Read teaching status | Master→Slave | |
| 13 | Write teaching status | Master→Slave | |
| 14 | Read security setting | Master→Slave | |
| 15 | Write security setting | Master→Slave | |
| 16 | Read banknote name | Master→Slave | |
| 17 | Write banknote name | Master→Slave | |
| 18 | Read event counter | Master→Slave | |
| 19 | Read bill operating mode | Master→Slave | |
| 20 | Write bill operating mode | Master→Slave | |
| 21 | Read bill inhibit status | Master→Slave | |
| 22 | Write bill inhibit status | Master→Slave | |
| 23 | Read bill verification setting | Master→Slave | |
| 24 | Write bill verification setting | Master→Slave | |
| 25 | Read bill reject setting | Master→Slave | |
| 26 | Write bill reject setting | Master→Slave | |
| 27 | Request stacker status | Master→Slave | Stato stacker banconote |
| 28 | Request stacker capacity | Master→Slave | |
| 29 | Read bill stacker total | Master→Slave | |
| 30 | Request currency revision | Master→Slave | |
| 31 | Upload currency data | Master→Slave | |
| 32 | Download currency data | Master→Slave | |
| 33 | Request dataset identifier | Master→Slave | |
| 34 | Upload rectification data | Master→Slave | |
| 35 | Download rectification data | Master→Slave | |
| 36–95 | Riservati / uso futuro | — | |
| 96 | Read opto states | Master→Slave | Stato sensori ottici |
| 97 | Read opto voltage | Master→Slave | |
| 98 | Perform self-check | Master→Slave | |
| 99 | Modify inhibit and override registers | Master→Slave | |
| 100 | Request inhibit status | Master→Slave | |
| 101 | Read handshaking flags | Master→Slave | |
| 102 | Read accounting activity register | Master→Slave | |
| 103 | Modify master inhibit status | Master→Slave | Abilita/disabilita accettazione |
| 104 | Request master inhibit status | Master→Slave | |
| 105 | Request insertion counter | Master→Slave | Contatore inserimenti totali |
| 106 | Request accept counter | Master→Slave | Contatore accettazioni totali |
| 107 | Dispense coins | Master→Slave | Hopper: eroga monete |
| 108 | Dispense hopper coins | Master→Slave | |
| 109 | Request hopper status | Master→Slave | |
| 110 | Modify hopper balance | Master→Slave | |
| 111 | Request hopper balance | Master→Slave | Saldo monete nell'hopper |
| 112 | Request hopper dispense count | Master→Slave | |
| 113 | Request hopper coin value | Master→Slave | |
| 114 | Emergency stop | Master→Slave | Stop immediato hopper |
| 115 | Request payout high/low status | Master→Slave | |
| 116 | Request data storage availability | Master→Slave | |
| 117 | Read data block | Master→Slave | |
| 118 | Write data block | Master→Slave | |
| 119 | Request option flags | Master→Slave | |
| 120 | Request coin position | Master→Slave | |
| 121 | Power management control | Master→Slave | |
| 122 | Modify sorter paths | Master→Slave | |
| 123 | Request sorter paths | Master→Slave | |
| 124 | Modify payout absolute count | Master→Slave | |
| 125 | Request payout absolute count | Master→Slave | |
| 126 | Empty payout | Master→Slave | Svuota hopper |
| 127 | Request audit information block | Master→Slave | |
| 128 | Meter control | Master→Slave | |
| 129 | Display control | Master→Slave | |
| 130 | Teach mode procedure | Master→Slave | |
| 131 | Request teach status | Master→Slave | |
| 132 | Upload coin data | Master→Slave | |
| 133 | Download coin data | Master→Slave | |
| 134 | Modify inhibit status | Master→Slave | |
| 135 | Pump RNG | Master→Slave | |
| 136 | Request cipher key | Master→Slave | Sicurezza/cifratura |
| 137 | Read cipher data | Master→Slave | |
| 138 | Write cipher data | Master→Slave | |
| 139 | Request build code | Master→Slave | |
| 140 | Key translation | Master→Slave | |
| 141 | Request configuration info | Master→Slave | |
| 142 | Modify configuration info | Master→Slave | |
| 143 | Request high resolution event counter | Master→Slave | |
| 144 | Read encryption support | Master→Slave | |
| 145 | Modify encryption code | Master→Slave | |
| 146 | Request encryption counter | Master→Slave | |
| 147 | Request country scaling factor | Master→Slave | Fattore di scala valuta |
| 148 | Request bill ID | Master→Slave | ID banconota |
| 149 | Modify bill ID | Master→Slave | |
| 150 | Request base year | Master→Slave | |
| 151 | Request address mode | Master→Slave | |
| 152 | Request coin ID | Master→Slave | ID moneta per canale |
| 153 | Modify coin ID | Master→Slave | |
| 154 | Copy coin ID | Master→Slave | |
| 155 | Modify default sorter path | Master→Slave | |
| 156 | Request error status | Master→Slave | |
| 157 | Read error log | Master→Slave | |
| 158 | Write error log | Master→Slave | |
| 159 | Read softcoded parameter | Master→Slave | |
| 160 | Write softcoded parameter | Master→Slave | |
| 161 | Request software revision | Master→Slave | Versione firmware |
| 162 | Request comms revision | Master→Slave | Versione protocollo ccTalk |
| 163 | Clear comms status variables | Master→Slave | |
| 164 | Request comms status variables | Master→Slave | Statistiche bus (errori CRC, ecc.) |
| 165–191 | Riservati / uso futuro | — | |
| **192** | **Read buffered credit or error codes** | Master→Slave | ⭐ Lettura crediti/errori monete |
| **193** | **Read buffered bill events** | Master→Slave | ⭐ Lettura eventi banconote |
| 194 | Request cipher key | Master→Slave | |
| 195 | Read opto voltage (extended) | Master→Slave | |
| 196 | Request payout status | Master→Slave | |
| 197 | Modify bill operating mode (extended) | Master→Slave | |
| 198–209 | Riservati | — | |
| 210 | Modify money output | Master→Slave | |
| 211 | Request money input | Master→Slave | |
| 212 | Request coin inhibit status (extended) | Master→Slave | |
| 213 | Modify coin inhibit status (extended) | Master→Slave | |
| 214 | Request payout coin inhibit status | Master→Slave | |
| 215 | Modify payout coin inhibit status | Master→Slave | |
| 216–227 | Riservati | — | |
| 228 | Request product code | Master→Slave | Codice prodotto |
| **229** | **Read serial number** | Master→Slave | ⭐ Numero seriale univoco |
| 230 | Request equipment category ID | Master→Slave | Tipo periferica |
| 231 | Request manufacturer ID | Master→Slave | ID produttore |
| 232 | Request variable set | Master→Slave | |
| 233 | Request status | Master→Slave | Stato generale |
| 234 | Modify baud rate | Master→Slave | Cambia velocità seriale |
| 235 | Request build revision | Master→Slave | |
| 236 | Modify default hopper | Master→Slave | |
| 237 | Request default hopper | Master→Slave | |
| 238 | Emergency stop | Master→Slave | Alternativo a 114 |
| 239 | Request hopper coin | Master→Slave | |
| 240–253 | Riservati / uso produttore | — | Comandi proprietari |
| **254** | **Address poll** | Master→Broadcast | Rilevamento periferiche sul bus |
| 255 | Address random | Master→Slave | Assegnazione indirizzo casuale |

---

## Comandi più usati ⭐

| Header | Comando | Utilizzo tipico |
|--------|---------|----------------|
| 1 | Reset device | Inizializzazione periferica |
| 103 | Modify master inhibit status | Abilitare/disabilitare accettazione monete |
| 192 | Read buffered credit or error codes | Polling continuo per rilevare monete inserite |
| 193 | Read buffered bill events | Polling continuo per rilevare banconote inserite |
| 229 | Read serial number | Identificazione univoca periferica |
| 231 | Request manufacturer ID | Lettura produttore |
| 254 | Address poll | Scansione bus per trovare periferiche |

---

## Dettaglio Header 192 — Read buffered credit or error codes

Risposta: buffer di **5 eventi** (2 byte ciascuno)

| Byte | Significato |
|------|-------------|
| Byte A | Contatore eventi (0–255, incrementale) |
| Byte B | Codice moneta (1–16) o codice errore (0 = nessun evento) |

Se il contatore cambia rispetto alla lettura precedente, ci sono nuovi eventi da processare.

---

## Dettaglio Header 229 — Read serial number

Risposta: **3 byte** (numero a 24 bit)

| Byte | Significato |
|------|-------------|
| Byte 1 | LSB (meno significativo) |
| Byte 2 | Byte centrale |
| Byte 3 | MSB (più significativo) |

```
serial = Byte1 + (Byte2 × 256) + (Byte3 × 65536)
```

---

## Gruppi funzionali

| Range Header | Categoria |
|-------------|-----------|
| 0–35 | Validatori banconote |
| 96–165 | Accettatori monete e hopper |
| 192–197 | Lettura eventi e crediti |
| 228–239 | Identificazione periferica |
| 240–253 | Comandi proprietari del produttore |
| 254–255 | Gestione indirizzi bus |

---

*Fonte: Specifiche ccTalk — Crane Payment Innovations / Money Controls*
