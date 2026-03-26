## Uscite programma e servizio

| Numero Digital_IO | Origine | porta/numero | Note |
| ----------------- | ------- | ------------ | ---- |
| R1                | MH1001  | O6           | `DIGITAL_IO_OUTPUT_RELAY1` |
| R2                | MH1001  | O5           | `DIGITAL_IO_OUTPUT_RELAY2` |
| R3                | MH1001  | O3           | `DIGITAL_IO_OUTPUT_RELAY3` |
| R4                | MH1001  | O4           | `DIGITAL_IO_OUTPUT_RELAY4` |
| LED1              | MH1001  | O1           | `DIGITAL_IO_OUTPUT_WHITE_LED` |
| LED2              | MH1001  | O2           | `DIGITAL_IO_OUTPUT_BLUE_LED` |
| LED3              | MH1001  | O7           | `DIGITAL_IO_OUTPUT_RED_LED` |
| HEATER            | MH1001  | O8           | `DIGITAL_IO_OUTPUT_HEATER1` |
| R5                | MODBUS01| O1           | `OUT09` (coil `relay_start+0`) |
| R6                | MODBUS01| O2           | `OUT10` (coil `relay_start+1`) |
| R7                | MODBUS01| O3           | `OUT11` (coil `relay_start+2`) |
| R8                | MODBUS01| O4           | `OUT12` (coil `relay_start+3`) |
| R9                | MODBUS01| O5           | `OUT13` (coil `relay_start+4`) |
| R10               | MODBUS01| O6           | `OUT14` (coil `relay_start+5`) |
| R11               | MODBUS01| O7           | `OUT15` (coil `relay_start+6`) |
| R12               | MODBUS01| O8           | `OUT16` (coil `relay_start+7`) |

## Ingressi

| Numero Digital_IO | Origine | porta/numero | Note |
| ----------------- | ------- | ------------ | ---- |
| I1                | MH1001  | I6           | `IN06` = `OPTO1` |
| I2                | MH1001  | I5           | `IN05` = `OPTO2` |
| I3                | MH1001  | I8           | `IN08` = `OPTO3` |
| I4                | MH1001  | I7           | `IN07` = `OPTO4` |
| I5                | MH1001  | I1           | `IN01` = `DIP1` |
| I6                | MH1001  | I2           | `IN02` = `DIP2` |
| I7                | MH1001  | I3           | `IN03` = `DIP3` |
| I8                | MH1001  | I4           | `IN04` = `SERVICE_SWITCH` |
| I9                | MODBUS01| I1           | `IN09` (discrete input `input_start+0`) |
| I10               | MODBUS01| I2           | `IN10` (discrete input `input_start+1`) |
| I11               | MODBUS01| I3           | `IN11` (discrete input `input_start+2`) |
| I12               | MODBUS01| I4           | `IN12` (discrete input `input_start+3`) |
| I13               | MODBUS01| I5           | `IN13` (discrete input `input_start+4`) |
| I14               | MODBUS01| I6           | `IN14` (discrete input `input_start+5`) |
| I15               | MODBUS01| I7           | `IN15` (discrete input `input_start+6`) |
| I16               | MODBUS01| I8           | `IN16` (discrete input `input_start+7`) |

## Analisi estensione a ulteriori moduli Modbus I/O expander

### Stato attuale

L'architettura corrente assume un solo blocco Modbus remoto:

- uscite locali `OUT01..OUT08`
- uscite Modbus `OUT09..OUT16`
- ingressi locali `IN01..IN08`
- ingressi Modbus `IN09..IN16`

Nel codice questa impostazione emerge in modo esplicito:

- `device_config_t` contiene un solo oggetto `modbus` con `slave_id`, `relay_start`, `relay_count`, `input_start`, `input_count`
- `digital_io` definisce conteggi fissi `8 locali + 8 Modbus`
- `digital_io_program_relay_to_output_id()` mappa `R01..R04` ai relay locali e `R05..R12` a `OUT09..OUT16`
- `modbus_relay_status_t` descrive un solo slave/finestra di registri
- la logica programmi usa `WEB_UI_VIRTUAL_RELAY_MAX = 12`

In pratica il sistema oggi rappresenta "un bus RS485 con un solo modulo remoto significativo" anche se i test manuali Modbus possono gia' interrogare slave differenti.

### Assunzioni hardcoded che impattano l'estensione

I punti piu' sensibili sono:

- numerazione I/O totale limitata a 16 ingressi e 16 uscite
- snapshot digitale basato su `uint16_t`, quindi non scalabile oltre 16 canali senza cambiare formato
- mapping relay programma basato su 12 relay logici fissi
- configurazione Modbus salvata come singolo profilo globale
- stato diagnostico Modbus esposto come singolo stato, non come elenco di moduli

Queste assunzioni non impediscono l'uso manuale di piu' slave sulla pagina `/test`, ma impediscono di integrarli in modo coerente nel layer unificato `digital_io`, nel setup persistente e nella logica programmi.

### Modelli possibili di estensione

#### Modello A - Un bus RS485 con piu' slave

Ogni modulo remoto viene descritto con:

- `slave_id`
- base coil/input
- numero coil/input
- eventuale nome/logical index del modulo

Pro:

- aderisce bene all'hardware Modbus reale
- si integra bene con la scansione bus gia' presente nella pagina `/test`
- permette moduli eterogenei sullo stesso bus
- e' il modello piu' naturale per diagnosi, setup e manutenzione

Contro:

- richiede refactor dei dati persistenti
- richiede una nuova astrazione per tradurre `OUTxx/INxx` globali verso `(slave_id, address)`
- richiede revisione delle API di stato e dei test automatici

#### Modello B - Un solo slave con piu' banchi indirizzi

Si mantiene un solo `slave_id`, ma si supportano piu' blocchi di coil/input sullo stesso dispositivo remoto.

Pro:

- impatto minore sul driver Modbus
- compatibilita' piu' semplice con l'attuale struttura `cfg->modbus`
- utile se il dispositivo remoto espone gia' espansioni come registri contigui o banchi configurabili

Contro:

- meno generale
- non copre bene il caso di piu' moduli fisici con ID diversi
- rischia di diventare un caso speciale poco chiaro lato setup

#### Modello C - Astrazione di moduli remoti generici

Si introduce un layer "remote digital module" indipendente dal fatto che il backend sia Modbus, altro protocollo, o banchi multipli.

Pro:

- massima flessibilita'
- separa bene logica applicativa e trasporto hardware
- prepara il terreno a future espansioni non Modbus

Contro:

- refactor piu' ampio
- costo iniziale piu' alto
- probabilmente eccessivo se il requisito reale e' solo "piu' slave RS485 Modbus"

### Impatto sulle strutture dati

Per supportare piu' moduli servirebbe passare da un singolo oggetto `modbus` a una collezione di moduli, ad esempio concettualmente:

- configurazione bus RS485 comune
- elenco moduli remoti
- per ogni modulo: `enabled`, `slave_id`, `relay_start`, `relay_count`, `input_start`, `input_count`, eventuale `label`

Impatti principali:

- `device_config_t` e relativo JSON di persistenza
- limiti `DIGITAL_IO_*_COUNT`
- `digital_io_snapshot_t`, che non puo' piu' basarsi su sole mask a 16 bit
- funzioni `digital_io_get_input_infos()` e simili, che dovrebbero enumerare dinamicamente gli I/O
- eventuali codici descrittivi `INxx/OUTxx` che oggi presuppongono una numerazione globale corta

Una scelta pratica potrebbe essere mantenere una numerazione globale continua:

- locali: `OUT01..OUT08`, `IN01..IN08`
- modulo 1: `OUT09..OUT16`, `IN09..IN16`
- modulo 2: `OUT17..OUT24`, `IN17..IN24`
- modulo 3: `OUT25..OUT32`, `IN25..IN32`

Questo rende semplice la UI, ma obbliga a rendere dinamico il mapping globale -> modulo -> indirizzo Modbus.

### Impatto sulla logica programmi

La logica programmi oggi e' allineata a 12 relay logici.

Se si vogliono usare nuovi moduli anche nei programmi automatici, andrebbero rivisti:

- `WEB_UI_VIRTUAL_RELAY_MAX`
- `relay_mask` dei programmi, oggi a 16 bit
- `digital_io_program_relay_to_output_id()`
- la UI configurazione programmi, che oggi ragiona su un insieme limitato di relay

Se invece i nuovi moduli servono solo per test/manuale e non per i programmi, il refactor puo' essere piu' contenuto e concentrarsi su `digital_io` e `/test`.

### Impatto sull'interfaccia di test

La pagina `/test` e' gia' il punto piu' vicino a un modello multi-modulo:

- ha una scansione bus
- costruisce card dinamiche per modulo trovato
- invia richieste specificando `slave_id`

Tuttavia il backend di test usa ancora un profilo singolo come default per:

- `modbus/status`
- `modbus/poll`
- `modbus/read_inputs`
- `modbus/write/coil`
- `modbus/write/coils`

Per un supporto pulito a N moduli sarebbe utile:

- distinguere chiaramente tra "bus status" e "module status"
- permettere test salvati per modulo configurato, non solo per slave scoperti al volo
- esporre eventuali errori e statistiche per ciascun modulo

### Impatto sul setup e sulla configurazione utente

Il backend `/api/config` gestisce gia' i campi Modbus, ma la pagina `/config` non li espone al momento in modo utilizzabile per l'utente finale.

Con piu' moduli il setup dovrebbe prevedere:

- configurazione bus RS485 una sola volta
- elenco moduli remoti aggiungibili/rimuovibili
- per ogni modulo: slave ID, range coil, range input, conteggi e label
- possibilmente un pulsante "scansiona bus e proponi moduli"

Dal punto di vista UX, e' preferibile separare:

- configurazione del bus
- configurazione dei moduli
- pagina di test/diagnostica live

### Raccomandazione pratica

La soluzione piu' pragmatica e' il **Modello A: bus RS485 unico con piu' slave configurabili**.

Motivi:

- e' coerente con il modo in cui Modbus RTU viene normalmente distribuito
- si appoggia bene alla scansione bus gia' esistente
- mantiene chiara la diagnostica
- evita di forzare tutto dentro un solo `slave_id`

Percorso consigliato in fasi:

1. introdurre una rappresentazione dati multi-modulo in configurazione, mantenendo compatibilita' retro con il singolo oggetto `modbus`
2. rendere dinamico il layer `digital_io` per conteggi, mapping e descrizione I/O
3. aggiornare `/test` per distinguere moduli configurati e moduli scoperti
4. solo dopo decidere se estendere anche i relay dei programmi oltre gli attuali 12

In sintesi: il collo di bottiglia principale non e' il driver Modbus, ma il fatto che il modello dati applicativo oggi considera un solo blocco remoto fisso.
