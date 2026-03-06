# Protocollo ccTalk — Riferimento comandi (allineato al PDF)

## Scopo

Questa pagina è allineata a `docs/cctalk44-3.pdf` (Part 3 v4.4, Appendix 1 - *cctalk Command Cross Reference*) e sostituisce il mapping precedente non coerente.

## Parametri base protocollo

- Comunicazione seriale single-wire, half-duplex
- Velocità tipica: 9600 baud
- Struttura frame: `dest | len | src | header | data | checksum`
- Checksum: somma modulo 256 pari a `0x00`

---

## Header standard più usati

| Header | Funzione standard (PDF) | Note |
|---:|---|---|
| 255 | Factory set-up and test | Comando di fabbrica |
| 254 | Simple poll | Poll semplice presenza/periferica |
| 253 | Address poll | Discovery indirizzi su bus multidrop |
| 248 | Request status | Stato dispositivo |
| 246 | Request manufacturer id | Identificativo produttore |
| 245 | Request equipment category id | Categoria periferica |
| 244 | Request product code | Codice prodotto |
| 242 | Request serial number | Seriale elettronico |
| 241 | Request software revision | Revisione software |
| 231 | Modify inhibit status | Maschere inibizione canali |
| 230 | Request inhibit status | Lettura maschere inibizione |
| 229 | Read buffered credit or error codes | Eventi/crediti bufferizzati |
| 228 | Modify master inhibit status | Abilita/disabilita accettazione globale |
| 226 | Request insertion counter | Contatore inserimenti |
| 210 | Modify sorter paths | Configurazione smistamento |
| 192 | Request build code | Build code firmware |
| 184 | Request coin id | ID moneta per canale |
| 4 | Request comms revision | Revisione protocollo |
| 3 | Clear comms status variables | Reset statistiche comunicazione |
| 2 | Request comms status variables | Lettura statistiche comunicazione |
| 1 | Reset device | Riavvio software periferica |
| 0 | Return Message Header | Header di risposta standard |

---

## Range standard Appendix 1

| Range / Header | Significato nel PDF |
|---|---|
| 128..104 | Available for future products |
| 103 | Expansion header 4 |
| 102 | Expansion header 3 |
| 101 | Expansion header 2 |
| 100 | Expansion header 1 |
| 99..20 | Application specific |
| 19..7 | Reserved |
| 6 | BUSY message |
| 5 | NAK message |

---

## Nota su varianti di periferica

Alcuni dispositivi possono usare in modo proprietario header in area *Application specific* (99..20) o adottare convenzioni differenti nella manualistica commerciale.

Nel progetto attuale:

- Profilo dispositivo: `docs/gettoniera.md`
- Differenze implementative rispetto allo standard: `docs/cctalk_diff_vs_standard.md`

In caso di conflitto, la fonte di riferimento resta il PDF standard e, per l'integrazione reale, il manuale della periferica specifica.

---

Fonte: `docs/cctalk44-3.pdf` (Money Controls / CPI, Part 3 v4.4).
