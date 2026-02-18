# TABELLA PROGRAMMI

## Scopo
Questo documento raccoglie le indicazioni funzionali emerse dai documenti esterni disponibili in `/home/mauro/Progetti/0.Clienti/MicroHard/docs`, in particolare:
- `Specifiche.docx`
- `MANUALE PAYWASH (1).pdf` (sezione 5.4 CONFIGURATIONS)

L’obiettivo è definire in modo operativo la “tabella programmi” da usare nel firmware/interfaccia.

## Requisiti ricavati

### Da Specifiche.docx
- Assegnazione dei programmi ad ogni input e output.
- Assegnazione del tempo per ogni programma.
- Valore di ciascun metodo di pagamento (gettone/moneta/qrcode/tessera).
- Visualizzazione e aggiornamento credito a display.
- Selezione programma da tasto fisico o touch.
- Avvio countdown tempo associato al programma.
- Possibilità di cambio programma/sospensione con ricalcolo proporzionale dal residuo.
- Invio eventi/movimenti al server e logging locale.

### Da MANUALE PAYWASH (1).pdf (5.4 CONFIGURATIONS)
- Price: prezzo dei programmi.
- Program times: durata di ogni programma (in secondi).
- Program RELAY: relay associati al programma.
- Enable buttons: abilitazione pulsanti programma.
- Inc/Dec by PLC: gestione incremento/decremento tempo da unità o PLC.
- ProgEnd Time: attivazione relay a fine programma per tempo definito.
- Tokens: abilitazione/disabilitazione accettazione token.
- Decimal places / Euro: formato e simbolo prezzo visualizzato.

## Struttura consigliata della tabella programmi

Ogni riga rappresenta un programma (es. 1..8 o 1..10):

| Campo | Tipo | Obbligatorio | Descrizione |
|---|---|---:|---|
| program_id | int | Sì | Identificativo univoco programma |
| name | string | Sì | Nome/etichetta visualizzata |
| enabled | bool | Sì | Programma selezionabile |
| price_units | int | Sì | Costo programma in unità credito (coin/token centesimi) |
| duration_sec | int | Sì | Durata iniziale in secondi |
| relay_mask / relay_list | string/array | Sì | Relay da attivare durante il programma |
| prog_end_relay_sec | int | No | Durata impulso relay fine programma |
| allow_pause | bool | No | Abilita pausa programma |
| pause_timeout_sec | int | No | Timeout pausa (0 = no pausa) |
| input_binding | string/array | No | Input fisici associati al programma |
| output_binding | string/array | No | Output/logiche associate |
| priority | int | No | Priorità in caso di conflitti |

## Tabella valori pagamento (separata ma collegata)

| Metodo | Campo suggerito | Note |
|---|---|---|
| Moneta | coin_value | Valore incrementale credito |
| Gettone | token_value | Valore incrementale credito |
| QR Code | qrcode_value | Valore incrementale credito |
| Tessera MDB | card_value | Valore incrementale credito |

## Regole funzionali minime
- Il credito aumenta all’evento di pagamento e viene mostrato a display.
- La selezione programma è consentita solo se `enabled=true` e credito sufficiente.
- All’avvio, il tempo residuo viene caricato da `duration_sec` (o da logica PLC se configurato).
- Durante esecuzione:
  - decremento tempo/credito secondo logica impostata;
  - attivazione relay configurati per il programma.
- A fine programma:
  - disattivazione relay di programma;
  - eventuale attivazione relay fine ciclo per `prog_end_relay_sec`.
- Cambio programma durante esecuzione:
  - consentito se configurato;
  - ricalcolo proporzionale dal residuo secondo regola di business.

## Note implementative per il progetto corrente
- L’emulatore web deve leggere questa struttura e riflettere:
  - pulsanti programma abilitati/disabilitati;
  - costo e durata per programma;
  - relay associati visualizzati nel quadro elettrico.
- La configurazione deve restare modificabile localmente (web/HMI) e aggiornabile da server.
- Eventi principali da loggare/inviare: credito aggiunto, start/stop programma, cambio programma, fine programma, errori I/O.

## Decisioni aperte
- Numero definitivo programmi (8 o 10) nel prodotto target.
- Unità monetaria interna (`coin`, centesimi, secondi equivalenti).
- Formula esatta del ricalcolo proporzionale nel cambio programma.
- Mappatura finale input/output per ciascun modello impianto.

## Sintesi
La “tabella programmi” deve coprire almeno: prezzo, durata, relay associati, abilitazione pulsanti e regole di esecuzione. Le fonti disponibili confermano che questi sono i parametri minimi necessari per coerenza tra UI, logica firmware e integrazione con PLC/server.
