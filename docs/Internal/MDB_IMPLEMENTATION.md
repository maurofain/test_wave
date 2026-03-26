# Implementazione Protocollo MDB (Multi-Drop Bus)

Relazione tecnica sull'implementazione del protocollo MDB nel progetto Waveshare ESP32-P4.

## 1. Architettura di Sistema
L'implementazione è centralizzata nel componente `components/mdb` ed è progettata per gestire periferiche di pagamento (Vending Machine Controller - VMC) in modalità Master/Slave.

### Componenti Principali:
*   **Driver UART**: Gestione della comunicazione seriale a basso livello.
*   **Polling Engine**: Task FreeRTOS (`mdb_engine_task`) che interroga ciclicamente le periferiche.
*   **Macchina a Stati (FSM)**: Gestisce il ciclo di vita delle periferiche (Reset -> Setup -> Enable -> Polling).

---

## 2. Gestione Fisica: Simulazione 9-Bit UART
Il protocollo MDB richiede 9 bit di dati (8 bit dati + 1 bit Mode). Poiché l'hardware UART standard non supporta nativamente il 9° bit come flag di indirizzo in modo semplice, è stata utilizzata la tecnica della **manipolazione dinamica della parità**.

### Trasmissione (TX)
Per forzare il valore del 9° bit, il driver cambia la configurazione della parità prima di inviare ogni byte:
*   **Mode Bit = 1 (Indirizzo)**: La parità viene impostata (ODD o EVEN) affinché l'hardware generi un bit di parità pari a 1 in base al contenuto del byte.
*   **Mode Bit = 0 (Dati/Checksum)**: La parità viene impostata affinché l'hardware generi un bit di parità pari a 0.
*   **Sincronizzazione**: Viene utilizzato `uart_wait_tx_done()` tra i byte per evitare corruzioni nel buffer FIFO durante il cambio di configurazione.

### Ricezione (RX)
La funzione `mdb_receive_packet` gestisce la lettura dei dati provenienti dalle periferiche. Poiché l'hardware non notifica direttamente lo stato del 9° bit in ricezione senza driver custom, l'implementazione adotta una strategia basata sui tempi:

1. **Strategia a "Due Tempi"**:
   *   **Attesa Risposta**: Il driver attende il primo byte per un tempo pari al timeout configurato (es. 20-100ms).
   *   **Inter-byte Timeout**: Una volta iniziato il pacchetto, ogni byte successivo deve arrivare entro **5ms**. Se il bus rimane in silenzio per più di 5ms, il pacchetto è considerato concluso (conforme allo standard MDB per l'identificazione della fine messaggio).
2. **Gestione Risposte Brevi**: Se viene ricevuto un solo byte (es. `0x00` per ACK, `0xFF` per NAK), la funzione lo valida immediatamente senza cercare un checksum, permettendo comunicazioni veloci.
3. **Verifica Integrità (Checksum)**: Per pacchetti multi-byte, l'ultimo byte è trattato come Checksum. Il sistema calcola la somma dei byte precedenti e la confronta con l'ultimo ricevuto. Se non corrispondono, ritorna `ESP_ERR_INVALID_CRC`.
4. **Semplificazione Hardware**: L'implementazione non verifica fisicamente il 9° bit in RX (che dovrebbe essere 1 solo sul checksum), ma si affida alla combinazione di timing e CRC, garantendo un'ottima affidabilità senza complessità driver aggiuntive.
5. La funzione di ricezione mdb_receive_packet, definita in mdb.c:219, è l'anima critica del driver MDB lato Slave-to-Master. A differenza della trasmissione, dove abbiamo il controllo totale del 9° bit, in ricezione dobbiamo interpretare un flusso di dati variabile nel tempo.

   Ecco l'analisi dettagliata del suo funzionamento:

   1. Strategia di Lettura a "Due Tempi"
   La funzione non legge tutto il pacchetto in un unico blocco, ma utilizza una strategia ibrida per gestire i tempi di risposta delle periferiche:

   Attesa del Primo Byte: Utilizza il timeout_ms passato come argomento (tipicamente 20-100ms). Questo è il tempo che concediamo alla periferica per iniziare a rispondere dopo un nostro comando.
   Lettura del Corpo (Inter-byte Timeout): Una volta ricevuto il primo carattere, il driver passa a un ciclo di lettura molto più aggressivo con un timeout di soli 5ms.
   Se arriva un byte entro 5ms, lo aggiunge al buffer.
   Se passano più di 5ms di silenzio, la funzione assume che la periferica abbia finito di trasmettere e chiude il pacchetto. Questo valore di 5ms è scelto in conformità con lo standard MDB che definisce il silenzio sul bus (Inter-Byte Silence) come terminatore di messaggio.
   2. Gestione delle Risposte Singole (ACK/NAK)
   Il protocollo MDB usa spesso risposte di un solo byte che non hanno checksum:

   0x00: ACK (Tutto OK)
   0xFF: NAK (Riprova)
   0xAA: RET (Richiesta di rinvio)
   La funzione gestisce questo caso alla riga mdb.c:249: se received == 1, la funzione ritorna immediatamente ESP_OK, saltando la logica del checksum che altrimenti fallirebbe.

   3. Validazione dell'Integrità (Checksum)
   Per i pacchetti contenenti dati (lunghezza > 1), l'ultimo byte ricevuto è per definizione il Checksum.

   Calcolo: La funzione somma tutti i byte ricevuti eccetto l'ultimo.
   Verifica: Confronta la somma (troncata a 8 bit) con l'ultimo byte.
   Sicurezza: Se non corrispondono, viene generato un warning ESP_LOGW e la funzione ritorna ESP_ERR_INVALID_CRC. Questo è fondamentale perché l'MDB è un bus elettricamente rumoroso (spesso ci sono motori di gettoniere e solenoidi vicini).
   4. Limitazioni e Note Tecniche
   Mancata verifica del 9° Bit in RX: Nello standard MDB rigoroso, lo Slave dovrebbe alzare il 9° bit solo sull'ultimo byte (il checksum). L'attuale implementazione non verifica questo bit via hardware, ma si affida esclusivamente al timeout di 5ms e al checksum. Questa è una semplificazione comune che rende il codice più portabile e meno dipendente da interrupt di basso livello.
   Buffer Safety: La funzione accetta un parametro max_len per evitare Buffer Overflow se una periferica dovesse inviare più dati del previsto.
   Esempio di flusso RX per un evento moneta:
   0ms: Il master finisce di inviare il POLL.
   5ms: Arriva il primo byte 0x12 (Moneta tipo 2).
   7ms: Arriva il secondo byte 0x12 (Checksum, poiché 0x12 + nulla = 0x12).
   12ms: Passano 5ms senza dati -> La funzione termina la lettura.
   Risultato: Checksum OK -> Ritorna il pacchetto al Polling Engine.

---

## 3. Flusso del Protocollo
Il flusso operativo segue tre fasi distinte:

### A. Inizializzazione (Boot)
1.  **RESET**: Comando inviato alla periferica per portarla in stato noto.
2.  **Attesa**: Pausa di 500ms per il reboot del dispositivo.

### B. Configurazione (Setup)
1.  **SETUP**: Il Master interroga la periferica per ottenere:
    *   Fattore di scala (moltiplicatore per i valori monetari).
    *   Numero di decimali.
    *   Tabella dei valori nominali delle monete.
2.  **EXPANSION**: (Opzionale) Identificazione avanzata del produttore/modello.
3.  **COIN TYPE ENABLE**: Il Master abilita l'accettazione delle monete e la gestione del resto.

### C. Polling Ciclico (Operatività)
Il Master invia un comando `POLL` ogni 500ms:
*   **Risposta ACK (0x00)**: Nessun evento, la periferica è in attesa.
*   **Risposta Dati**: La periferica segnala eventi come:
    *   **Moneta Accettata**: Viene estratto il tipo moneta, calcolato il valore reale (`valore * scaling_factor`) e aggiornato il credito globale.
    *   **Just Reset (0x0B)**: La periferica segnala un riavvio imprevisto; il Master riesegue la fase di Setup.

---

## 4. Gestione Errori
*   **Timeout**: Se una periferica non risponde, viene marcata come `is_online = false`.
*   **CRC Error**: Se il checksum non corrisponde, il pacchetto viene scartato e l'evento non viene confermato (ACK non inviato), forzando la periferica a ripetere l'invio al prossimo ciclo.
*   **Conflitti GPIO**: Il sistema controlla esplicitamente (in `init.c`) che i pin MDB non entrino in conflitto con altri bus (es. Ethernet).

---

## 5. Stato dell'Implementazione
| Funzionalità | Stato | Note |
| :--- | :--- | :--- |
| Coin Acceptor (Gettoniera) | Completata | Gestione completa credito e setup. |
| Bill Validator (Lettore Banconote) | Predisposta | Struttura FSM presente, da completare parsing. |
| Cashless Device | Iniziale | Supporto indirizzi definito. |
| Web UI Integration | Completata | Visualizzazione stato e abilitazione da config. |
