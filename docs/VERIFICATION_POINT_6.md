# Verifica Punto 6 — Riconsione di Rete e Aggiornamento Indicatori

> Data: 8 aprile 2026  
> Obiettivo: Verificare che il sistema riconosca gli errori di rete e aggiorni i puntini indicatori di stato

---

## Overview

Il punto 6 richiede di verificare il comportamento del sistema in caso di:
1. **Errore di rete**: Perdita di connessione o connessione al server remoto non disponibile
2. **Aggiornamento puntini**: Gli indicatori deve passare da bianco (connesso) a rosso (disconnesso) e viceversa

I 4 puntini sopra la bandiera di scelta lingua rappresentano:
- **Indice 0**: Connessione di rete e server remoto (Network + HTTP_SERVICES)
- **Indice 1**: Scanner USB CDC
- **Indice 2**: CCTALK (Gettoniera)
- **Indice 3**: MDB

---

## Prerequisiti

- Device test_wave flashato con versione >= v0.6.14
- Display LVGL attivo e pronto
- Connessione Ethernet o WiFi disponibile
- Accesso ai log seriali (monitor ESP-IDF)

---

## Test Case 1: Verifica Stato Iniziale

**Passo 1: Boot del sistema**
- Avviare il device
- Osservare i 4 puntini sopra la bandiera durante il boot

**Risultato atteso:**
- [ ] Al boot: tutti i 4 puntini **rossi** (inizializzazione)
- [ ] Dopo completamento init (10-15 sec):
  - Puntino 0 (Network): **BIANCO** se Ethernet collegato e server raggiungibile
  - Puntino 1 (Scanner): **BIANCO** se scanner USB collegato
  - Puntino 2 (CCTALK): **BIANCO** se gettoniera collegata
  - Puntino 3 (MDB): **BIANCO** se lettore MDB collegato

**Log di verifica in monitor ESP-IDF:**
```
[M] ╔════════════════════════════════════════════════════╗
[M] ║ STATUS PERIFERICA ALLOCATA                         ║
[M] ║ Network: INIT_AGENT_ERR_NONE                       ║
[M] ║ USB Scanner: INIT_AGENT_ERR_NONE (o DISABLED)      ║
[M] ║ CCTALK: INIT_AGENT_ERR_NONE (o DISABLED)           ║
[M] ║ MDB: INIT_AGENT_ERR_NONE (o DISABLED)              ║
[M] ╚════════════════════════════════════════════════════╝
```

---

## Test Case 2: Disconnessione di Rete (Scenario Ethernet)

**Passo 1: Verificare connessione iniziale**
- Device connesso via Ethernet, puntino 0 (Network) **bianco**

**Passo 2: Disconnettere il cavo Ethernet**
- Staccare fisicamente il cavo Ethernet dal device
- Osservare il display

**Risultato atteso:**
- [ ] Entro 30-60 secondi: puntino 0 diventa **rosso**
- [ ] Log mostra timeout di connessione al server

**Comando per verificare i log:**
```bash
monitor | grep -E "HTTP_SERVICES|NET|timeout|connection"
```

**Passo 3: Ricollegare il cavo Ethernet**
- Riconnettere il cavo Ethernet
- Osservare il display

**Risultato atteso:**
- [ ] Entro 30-60 secondi: puntino 0 torna **bianco**
- [ ] Log mostra riconnessione con successo
- [ ] Inizializzazione completata senza errori

---

## Test Case 3: Errore di Raggiungibilità Server

**Passo 1: Verificare connessione iniziale**
- Device connesso a rete, puntino 0 (Network) **bianco**

**Passo 2: Modificare configurazione server remoto**
- Accedere a `/config` (Web UI o touch panel)
- Modificare indirizzo server remoto con uno non raggiungibile (es. `999.999.999.999`)
- Eseguire **Salva** e riavviare il device

**Passo 3: Osservare il comportamento**
- After boot: tutti i puntini rossi
- Attesa 30-60 sec per timeout di connessione al server

**Risultato atteso:**
- [ ] Puntino 0 rimane **rosso** (server non raggiungibile)
- [ ] Altre periferiche diventano **bianche** (se collegate)
- [ ] Log mostra errore di connessione/timeout
- [ ] Device continua a funzionare in modalità offline

---

## Test Case 4: Riconsione Automatica

**Passo 1: Partire dallo stato precedente**
- Device offline (puntino 0 rosso)

**Passo 2: Ripristinare configurazione server**
- Accedere a `/config`
- Modificare indirizzo server con uno corretto
- Eseguire **Salva** e riavviare il device

**Passo 3: Osservare il comportamento**
- After boot: tutti i puntini rossi
- Attesa 30-60 sec per connection retry

**Risultato atteso:**
- [ ] Puntino 0 diventa **bianco** (server raggiungibile)
- [ ] Log mostra connessione riuscita
- [ ] Sistema operativo normalmente

---

## Test Case 5: Cambiamento di Stato Dinamico

**Passo 1: Osservare cambio stato scanner USB**
- Device avviato senza scanner USB collegato
- Puntino 1 (Scanner) **rosso**

**Passo 2: Collegare scanner USB**
- Inserire scanner USB in porta USB
- Osservare il display

**Risultato atteso:**
- [ ] Entro 5-10 secondi: puntino 1 diventa **bianco**
- [ ] Log mostra riconoscimento scanner
- [ ] Scanner operativo

**Passo 3: Disconnettere scanner USB**
- Rimuovere lo scanner USB

**Risultato atteso:**
- [ ] Entro 5-10 secondi: puntino 1 diventa **rosso**
- [ ] Log mostra disconnessione scanner

---

## Log di Debug Esperti

Per debugging avanzato, abilitare i log dettagliati nel monitor:

```bash
# Mostra SOLO eventi di cambio stato agente
monitor | grep "init_agent_status_set\|chrome_update_status"

# Mostra errori di connessione di rete
monitor | grep "AGN_ID_HTTP_SERVICES\|REMOTE_LOGIN_FAILED\|NETWORK_NO_IP"

# Mostra tutti gli agenti e loro stato
monitor | grep -E "\[M\].*STATUS|error_code"
```

---

## Criteri di Successo

✅ **TUTTI i test case devono passare:**

1. [ ] Puntini iniziali rossi al boot
2. [ ] Puntini diventano bianchi solo quando periferica OK
3. [ ] Cambio stato da connesso → disconnesso → riconnesso funziona
4. [ ] Device continua a operare anche offline
5. [ ] Riconsione automatica quando servizio disponibile di nuovo
6. [ ] Cambi di stato sono visibili entro 1 minuto
7. [ ] Log sono coerenti con stato visivo

---

## Note Tecniche

### Mapping Periferiche → Estados

```c
// main/init.c
AGN_ID_HTTP_SERVICES     → Indice 0 (Network)
AGN_ID_USB_CDC_SCANNER   → Indice 1 (USB Scanner)
AGN_ID_CCTALK            → Indice 2 (CCTALK)
AGN_ID_MDB               → Indice 3 (MDB)
```

### Colori Utilizzati

```c
// Bianco (OK): lv_color_hex(0xFFFFFF)
// Rosso (KO): lv_color_hex(0xFF0000)
```

### Funzioni di Update

```c
// components/lvgl_panel/lvgl_page_chrome.c
void lvgl_page_chrome_update_status_indicator(int index, bool is_ok)
{
    // is_ok = true  → Colore bianco
    // is_ok = false → Colore rosso
}

// main/init.c
static void update_periph_status_indicator(int32_t agn_value, init_agent_error_code_t error_code)
{
    // Traccia cambio stato agente e aggiorna puntino
}
```

---

## Prossimi Step

- [ ] Documentare eventuali bug riscontrati
- [ ] Verificare tempistiche di update
- [ ] Testare con diverse configurazioni di rete
- [ ] Validare behavior offline del device
- [ ] Confermare letture coerenti su ripetute connessioni/disconnessioni

---

**Documento versione**: v1.0  
**Data creazione**: 8 aprile 2026  
**Status**: ✅ Pronto per test
