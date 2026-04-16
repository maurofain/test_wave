# Ricostruzione Branch Modifiche_incomplete_MDB_rollback

## Scopo

Questo documento ricostruisce il contenuto del branch `Modifiche_incomplete_MDB_rollback` rispetto a `master`, per poter riprendere in modo selettivo il lavoro sul flusso MDB cashless senza reintrodurre subito le parti che hanno probabilmente causato regressioni sul rilevamento del TAG.

## Base Git Ricostruita

- `master` punta a `f33ab4123397cdad225ba0e97b1dd2813dff26ab`
- `Modifiche_incomplete_MDB_rollback` punta a `38879ef2315817c4cd952dde209e2af7798e40ed`
- Dal reflog locale il branch risulta creato da `master` e contiene un solo commit aggiuntivo con messaggio `Modifiche incomplete MDB rollback`

Conclusione pratica: il branch non rappresenta una cronologia di commit piccoli e isolabili, ma un unico checkpoint WIP che aggrega piu' tentativi stratificati nella stessa sessione.

## Passaggi Logici Ricostruiti

1. Stabilizzazione del driver cashless MDB.
   - Riduzione del rumore di bootstrap.
   - Gestione meno aggressiva dei timeout attesi.
   - Retry su `SESSION_COMPLETE`.
   - Migliore osservabilita' del bootstrap e del polling.

2. Riduzione della latenza di polling MDB.
   - Aggiornamento di `data/tasks.json` con periodo `mdb_engine` portato a 20 ms.
   - Aggiornamento di `main/tasks.c` per fare passare il periodo configurato dal wrapper `mdb_engine_wrapper()` al driver MDB.
   - Aggiornamento di `components/mdb/mdb.c` per rimuovere il ciclo fisso interno a 500 ms e usare invece il periodo ricevuto dal task.
   - Allineamento del fallback interno del task `mdb_engine` a 20 ms anche nel descrittore statico dei task.

3. Revisione del flusso di ricarica NFC/tag.
   - Distinzione tra `REVALUE_LIMIT` e `REVALUE_REQUEST`.
   - Introduzione di guardrail per evitare sovrapposizioni tra inquiry del limite, ricarica effettiva e `VEND_REQUEST`.

4. Modifiche a FSM e task orchestration per la sessione CARD.
   - Tentativo di mantenere piu' a lungo il contesto della sessione card.
   - Tentativo di gestire in modo piu' robusto rimozione TAG, stop programma e chiusura sessione.

5. Aggiunta UI di ricarica nella pagina `/test`.
   - Polling dello stato cashless.
   - Sequenza frontend `revalue_limit -> attesa -> revalue`.

6. Aggiornamento di i18n e documentazione di supporto.

7. Presenza di almeno una modifica collaterale non direttamente legata a MDB.

## Tabella di Ricostruzione

| Area | File principali | Modifiche introdotte nel branch | Valore funzionale | Rischio o regressione | Priorita' di ripresa |
| --- | --- | --- | --- | --- | --- |
| Driver MDB cashless | `components/mdb/mdb.c` | Retry `SESSION_COMPLETE`, soppressione log per timeout attesi, retry `REQUEST_ID`, `VEND_SUCCESS` senza retry infinito, `mdb_engine_run()` con periodo configurabile | Alto: migliora timing e diagnosi del lettore | Medio: i retry di chiusura sessione possono influenzare riarmo e rimozione TAG | Alta |
| Stato cashless e revalue | `components/mdb/mdb_cashless.c`, `components/mdb/include/mdb_cashless.h` | Nuovo flag `revalue_limit_requested`, distinzione `REVALUE_LIMIT` vs `REVALUE_REQUEST`, reset stato revalue su cancel/end/out_of_sequence, blocco `VEND_REQUEST` durante revalue attivo | Alto: corregge il flusso di ricarica | Medio: puo' interferire con sequenze cashless se introdotto tutto insieme | Alta |
| Task timing MDB | `data/tasks.json`, `main/tasks.c`, `components/mdb/mdb.c` | `mdb_engine` da 500 ms a 20 ms, wrapper che passa il periodo reale al driver | Molto alto: riduce il ritardo prima del `VEND_APPROVED` | Basso: cambiamento puntuale e misurabile | Molto alta |
| FSM e sessione CARD | `main/fsm.c` | Helper `fsm_request_card_session_complete_if_open`, preservazione sessione su touch/key, log auto-renew, chiusura sessione condizionata allo stato hardware | Medio: prova a rendere stabile la sessione card | Alto: candidato forte per regressioni sul rilevamento e sul rientro TAG | Bassa |
| Tasks e rimozione TAG | `main/tasks.c`, `main/tasks.h` | `tasks_is_card_session_open`, estensione della rilevazione rimozione TAG a `RUNNING/PAUSED`, deduplica con `card_session_removed_notified`, `keep_cashless_active` piu' permissivo | Medio | Alto: puo' alterare il punto in cui il sistema considera ancora viva la sessione card | Bassa |
| Web UI ricarica | `data/www/js/test_ioexp_fix.js` | Ricostruzione dinamica della sezione MDB di `/test`, campo credito attuale, importo ricarica, bottone recharge, sequenza frontend `revalue_limit -> wait -> revalue` | Alto per collaudo e diagnostica | Basso sul firmware, medio sul test flow | Alta |
| i18n UI ricarica | `data/i18n_v2.json` | Nuove stringhe per `Credito attuale` e `Ricarica` | Basso ma necessario per coerenza UI | Basso | Media |
| Documentazione e analisi | `docs/TODO.md`, `docs/analisi sorgent MH.md` | Traccia dei lavori e analisi dettagliata spec-vs-codice con roadmap | Medio: aiuta a non perdere il contesto | Basso | Media |
| Modifiche collaterali | `data/programs.json` | Cambio durata programma 10 da 60 a 30 secondi | Nessuno per MDB | Medio: rumore nel branch, non legato al cashless | Escludere |

## Lettura Tecnica per Area

- `components/mdb/mdb.c`
  - E' il cuore del branch.
  - Contiene sia modifiche probabilmente buone, come polling a 20 ms e periodo parametrico, sia il blocco piu' delicato di retry `SESSION_COMPLETE`.

- `components/mdb/mdb_cashless.c`
  - Raccoglie le modifiche sul protocollo revalue.
  - Qui il branch salva la distinzione concettuale corretta tra richiesta limite e ricarica effettiva.

- `main/fsm.c` e `main/tasks.c`
  - Rappresentano il tentativo di far sopravvivere meglio il contesto CARD oltre il confine stretto della sessione hardware.
  - E' l'area piu' facilmente responsabile delle regressioni sul rilevamento TAG.

- `data/www/js/test_ioexp_fix.js`
  - Modifica di supporto al debug e al test.
  - Utile per riprendere la parte recharge senza toccare subito il firmware.

## Parti Probabilmente Buone da Recuperare per Prime

1. Polling MDB a 20 ms e periodo passato dal task al driver.
2. Riduzione del rumore di log nel bootstrap MDB.
3. Distinzione `REVALUE_LIMIT` vs `REVALUE_REQUEST` con stato dedicato.
4. UI `/test` per la ricarica e relativi testi i18n.

## Parti da Recuperare Solo Dopo Verifica Mirata

1. Retry di `SESSION_COMPLETE` in `components/mdb/mdb.c`.
2. Estensione della rilevazione rimozione TAG fuori da `FSM_STATE_CREDIT` in `main/tasks.c`.
3. Helper `tasks_is_card_session_open` e `fsm_request_card_session_complete_if_open`.
4. Logica `keep_cashless_active` piu' permissiva e preservazione sessione su touch e key.

## Parti da Escludere o Isolare

1. Modifica a `data/programs.json`.
2. Note di TODO o documentazione che non servono alla reintroduzione del codice.

## Roadmap Consigliata di Reintroduzione Selettiva

1. Reintrodurre dal branch solo il blocco timing e polling MDB.
2. Verificare su log che il ritardo pre-avvio programma torni ridotto.
3. Reintrodurre il blocco revalue backend e poi la UI `/test`.
4. Validare la ricarica senza toccare ancora FSM e tasks della sessione CARD.
5. Solo dopo, reintrodurre un pezzo alla volta la logica di chiusura sessione e rilevazione rimozione TAG.

## Conclusione

Il branch non e' un ramo storico da cherry-pickare commit per commit. E' uno snapshot WIP con un unico commit monolitico sopra `master`.

La strategia corretta e' trattarlo come serbatoio di cambiamenti e ripescare in quest'ordine:

1. timing MDB
2. revalue
3. UI test
4. FSM e tasks CARD

L'ultima area e' quella a piu' alto rischio regressione.