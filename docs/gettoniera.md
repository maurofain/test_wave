# Protocollo ccTalk - Specifiche Gettoniere Microhard

Documentazione tecnica dei comandi ccTalk per l'integrazione con microcontrollori (ESP32). 
*Modelli di riferimento: Microhard HBD5, HBD2, serie ST.*

## 1. Parametri di Comunicazione
- **Baud Rate**: 9600 bps
- **Data bits**: 8
- **Parity**: None
- **Stop bits**: 1
- **Livello Fisico**: TTL 0-5V (Single-wire Half-Duplex)
- **Indirizzo di Default**: `2` (Coin Acceptor)

---

## 2. Struttura del Pacchetto
| Byte | Descrizione | Note |
|:---:|:---|:---|
| 1 | **Dest Address** | Indirizzo della gettoniera (default 2) |
| 2 | **Data Length** | Numero di byte nel campo dati |
| 3 | **Source Address** | Indirizzo del Master (solitamente 1) |
| 4 | **Header** | Il comando da eseguire |
| 5..N | **Data** | Argomenti opzionali |
| Last | **Checksum** | Somma di controllo (8-bit sum to 0) |

---

## 3. Lista Comandi (Header)

### A. Identificazione e Stato (Power-Up)
| Header | Comando | Descrizione | Risposta Microhard |
|:---:|:---|:---|:---|
| **254** | `Address Poll` | Verifica presenza periferica | ACK (0 data bytes) |
| **248** | `Request Status` | Stato operativo | 0: OK, 1: Errore/Inceppamento |
| **246** | `Manufacturer ID` | Identifica il produttore | `"Microhard"` |
| **245** | `Equipment Cat.` | Categoria dispositivo | `"Coin Acceptor"` |
| **244** | `Product Code` | Modello specifico | Es: `"HBD5-ST"` |
| **242** | `Serial Number` | Numero di serie | 3 byte binari |
| **192** | `Build Code` | Versione Firmware | Stringa ASCII |

### B. Gestione Inibizioni e Canali
| Header | Comando | Descrizione |
|:---:|:---|:---|
| **231** | `Modify Master Inhibit` | `[1]` = Accetta monete, `[0]` = Rifiuta tutto |
| **230** | `Request Inhibit Status` | Ritorna 2 byte (bitmask dei 16 canali attivi) |
| **184** | `Request Coin ID` | Chiede il nome della moneta al canale N (es: `"EU200A"`) |
| **210** | `Modify Sorter Paths` | Configura l'uscita fisica della moneta (Cassa/Scarto) |

### C. Fase Operativa (Polling)
| Header | Comando | Descrizione |
|:---:|:---|:---|
| **229** | `Read Buffered Credit` | Legge gli eventi di incasso (11 byte di risposta) |
| **226** | `Request Insertion Status` | Dice se c'è una moneta nel canale di lettura |
| **1** | `Reset Device` | Riavvio software della gettoniera |

---

## 4. Analisi Risposta "Buffered Credit" (Header 229)
La risposta è composta da **11 byte**:
`[Event Counter] [C1][E1] [C2][E2] [C3][E3] [C4][E4] [C5][E5]`

- **Event Counter**: Incrementa ad ogni nuova moneta o evento. Se non cambia, non ci sono nuovi eventi.
- **Cn**: Numero del canale della moneta (1-16).
- **En**: Codice di errore o stato sorter (0 = OK).

### Codici di Errore Comuni (En):
- `0`: Moneta accettata correttamente.
- `1`: Moneta rifiutata (non riconosciuta).
- `2`: Moneta inibita (canale spento via Master Inhibit).
- `8`: **Stringing**: Tentativo di frode (moneta con filo).
- `13`: Inceppamento nel sensore.
- `254`: Problema generico hardware.

---

## 5. Esempio Implementazione C (Struct)
```c
typedef struct {
    uint8_t event_counter;
    struct {
        uint8_t coin_id;
        uint8_t error_code;
    } events[5];
} cctalk_buffer_t;