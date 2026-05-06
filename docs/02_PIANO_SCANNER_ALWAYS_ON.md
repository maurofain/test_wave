# Piano — Scanner sempre attivo con filtro logico (ACCEPT/IGNORE)

**Data:** 2026-05-06  
**Progetto:** MicroHard fw_esp32p4  
**Obiettivo:** evitare enable/disable dello scanner nelle varie fasi, lasciandolo sempre attivo e filtrando i codici in base alla fase.

---

## 1. Obiettivo
Lasciare **lo scanner sempre attivo** (niente disable/enable nelle fasi) e gestire a software una **finestra di accettazione**:
- codici letti fuori finestra → **ignorati** (o messi in coda)
- codici letti dentro finestra → **accettati**

---

## 2. Strategia (alto livello)
- **Scanner sempre ON:** la parte HW/driver continua a leggere e produrre eventi/codici.
- **Filtro logico centralizzato:** un unico punto nel firmware decide se un codice è “valido ora”.
- **Stato applicativo:** le “varie fasi” diventano uno **state machine**; ogni stato dichiara se la lettura è **ACCEPT** o **IGNORE**.

---

## 3. Piano di implementazione

### 3.1 Definire la policy per ogni fase
Per ogni fase del flusso, stabilire:
- `scanner_policy = ACCEPT | IGNORE`

Opzionale:
- distinguere `IGNORE_AND_DROP` vs `IGNORE_AND_BUFFER` (vedi 3.3)

### 3.2 Introdurre un gate unico (ScannerGate)
Creare un modulo/funzione centrale (nome indicativo):
- `bool scanner_gate_accept(code, now, current_state)`

Regole base:
- se fase = `ACCEPT` → passa il codice al normale handler
- se fase = `IGNORE` → scarta (o bufferizza) senza effetti sul flusso

### 3.3 Gestire il “rumore” quando IGNORE
Se scarti sempre:
- log minimo o solo contatori per diagnosi (evitare spam)

Se bufferizzi:
- mantenere **solo l’ultimo** codice letto (o una coda piccola) con timestamp

### 3.4 Gestire transizioni di fase senza perdere il “primo codice valido”
Problema tipico: un codice arriva “a cavallo” della transizione.

Soluzione consigliata:
- quando passi a `ACCEPT`, avvia una finestra “clean”:
  - **flush** del buffer (o reset dedup) e poi accetta

Alternativa:
- se esiste buffer “ultimo codice ignorato”, quando entri in `ACCEPT` puoi accettarlo **solo se**:
  - è recente (es. < 200–500 ms)
  - non è un duplicato già processato

### 3.5 Dedup / anti-ripetizione
Quasi tutti gli scanner ripetono lo stesso codice più volte.

Implementare:
- `last_code`, `last_accept_time`
- rifiuta lo stesso `code` se ripetuto entro una soglia (es. 300–1000 ms), **solo in ACCEPT**

### 3.6 Architettura eventi (consigliata)
- task/ISR dello scanner → pubblica eventi “RAW_CODE”
- un task “ScannerRouter” → applica `ScannerGate` e pubblica “VALID_CODE” al resto dell’app
- il resto del firmware non parla più direttamente con lo scanner: consuma solo “VALID_CODE”

### 3.7 Telemetria e sicurezza operativa
- contatori: `codes_ignored`, `codes_accepted`, `codes_deduped`
- (opzionale) un flag “emergenza” che forza `IGNORE` in stati critici

---

## 4. Decisioni da fissare (per evitare ambiguità)
- **Quando IGNORE:** scartare sempre o tenere l’ultimo codice in buffer?
- **Finestra dedup:** quanti ms senza accettare duplicati?
- **Transizione a ACCEPT:** flush totale oppure accettare anche “ultimo codice ignorato” se recente?

