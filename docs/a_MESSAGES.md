# MESSAGES / Mailbox FSM

Questo documento elenca i **messaggi attualmente generati** che transitano nella mailbox condivisa implementata in `main/fsm.c`.

## Criteri usati

- Sono inclusi solo i messaggi con **publisher o consumer attivi** nella build corrente.
- Sono esclusi file non compilati e helper senza call-site attivo.
- Le richieste `DIGITAL_IO_SET_OUTPUT`, `DIGITAL_IO_GET_OUTPUT` e `DIGITAL_IO_GET_SNAPSHOT` transitano nella mailbox verso `AGN_ID_FSM`.
- `ACTION_ID_DIGITAL_IO_GET_INPUT` e' **supportato** dal dispatcher mailbox, ma oggi non ha publisher attivi.
- `ACTION_ID_SYSTEM_ERROR` e `ACTION_ID_SYSTEM_RUN` sono usati nel flusso OOS, ma vengono iniettati internamente da `tasks_fsm_task()` con chiamata diretta a `fsm_handle_input_event(...)` (non passano dalla mailbox `fsm_event_publish/fsm_event_receive`).
- Gli ingressi locali `OPTO1..OPTO4` sono gli **unici** touch-mappable.
- Gli altri ingressi locali (`DIP1`, `DIP2`, `DIP3`, `SERVICE_SWITCH`) vengono inoltrati al consumer `AGN_ID_IO_PROCESS`.
- Le uscite locali `RELAY1..RELAY4` sono quelle usate dai programmi; `WHITE_LED`, `BLUE_LED`, `RED_LED` e `HEATER1` vengono inoltrate a `io_process` quando passano dal dispatcher digital I/O.

## Mappa segnali `digital_io`

### Ingressi locali `IN01..IN08`

| ID | Codice | Ruolo |
|---|---|---|
| `IN01` | `DIP1` | Segnale per `io_process` |
| `IN02` | `DIP2` | Segnale per `io_process` |
| `IN03` | `DIP3` | Segnale per `io_process` |
| `IN04` | `SERVICE_SWITCH` | Segnale per `io_process` |
| `IN05` | `OPTO2` | Touch-mappable |
| `IN06` | `OPTO1` | Touch-mappable |
| `IN07` | `OPTO4` | Touch-mappable |
| `IN08` | `OPTO3` | Touch-mappable |

### Uscite locali `OUT01..OUT08`

| ID | Codice | Ruolo |
|---|---|---|
| `OUT01` | `WHITE_LED` | Segnale per `io_process` |
| `OUT02` | `BLUE_LED` | Segnale per `io_process` |
| `OUT03` | `RELAY3` | Relay programma |
| `OUT04` | `RELAY4` | Relay programma |
| `OUT05` | `RELAY2` | Relay programma |
| `OUT06` | `RELAY1` | Relay programma |
| `OUT07` | `RED_LED` | Segnale per `io_process` |
| `OUT08` | `HEATER1` | Segnale per `io_process` |

## Consumer mailbox attivi trovati

I receiver attivi trovati tramite `fsm_event_receive(...)` nella build corrente sono:

- `(1) AGN_ID_FSM` -> consumer principale nel task FSM
- `(4) AGN_ID_IO_PROCESS` -> consumer fallback dei segnali `digital_io` non gestiti altrove
- `(18) AGN_ID_HTTP_SERVICES` -> consumer nel task `http_services_task()`
- `(12) AGN_ID_CCTALK` -> consumer nel driver/task CCtalk

Non risultano invece consumer mailbox attivi per:

- `(14) AGN_ID_LED`
- `(23) AGN_ID_LVGL`
- `(2) AGN_ID_WEB_UI`

Questo significa che `(102) ACTION_ID_PROGRAM_PREFINE_CYCLO`, pur essendo pubblicato verso questi destinatari, **oggi non ha un consumer mailbox attivo**.

## Tabella tipi messaggi prodotti (producer)

| Tipo / action | Producer attivi | Descrizione breve |
|---|---|---|
| `(0) FSM_INPUT_EVENT_NONE` + `(108) ACTION_ID_LVGL_TEST_ENTER` | `(2) Web UI` | Richiede alla FSM di entrare nella modalita' test pagine LVGL. |
| `(0) FSM_INPUT_EVENT_NONE` + `(109) ACTION_ID_LVGL_TEST_EXIT` | `(2) Web UI` | Richiede alla FSM di uscire dalla modalita' test pagine LVGL. |
| `(0) FSM_INPUT_EVENT_NONE` + `(102) ACTION_ID_PROGRAM_PREFINE_CYCLO` | `(1) FSM` | Notifica che il programma ha raggiunto la soglia PreFineCiclo. |
| `(2) FSM_INPUT_EVENT_TOUCH` + `(97) ACTION_ID_USER_ACTIVITY` | `(23) LVGL ADS page` | Segnala attivita' utente dalla schermata ADS/splash. |
| `(2) FSM_INPUT_EVENT_TOUCH` + `(96) ACTION_ID_NONE` | `(3) Task touchscreen` | Segnala un tocco fisico al sistema FSM. |
| `(8) FSM_INPUT_EVENT_PROGRAM_SELECTED` + `(99) ACTION_ID_PROGRAM_SELECTED` | `(23) LVGL programmi, (2) Web UI emulator, (5) task digital_io tramite input OPTOx mappati, (1) FSM verso HTTP services` | Avvia un programma o notifica al backend il pagamento servizio. |
| `(11) FSM_INPUT_EVENT_PROGRAM_SWITCH` + `(99) ACTION_ID_PROGRAM_SELECTED` | `(23) LVGL programmi, (2) Web UI emulator, (5) task digital_io tramite input OPTOx mappati` | Cambia programma mentre un altro e' gia' in esecuzione o in pausa. |
| `(9) FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` + `(101) ACTION_ID_PROGRAM_PAUSE_TOGGLE` | `(23) LVGL programmi, (2) Web UI emulator, (5) task digital_io tramite input OPTOx mappati` | Alterna pausa e ripresa del programma attivo. |
| `(7) FSM_INPUT_EVENT_PROGRAM_STOP` + `(100) ACTION_ID_PROGRAM_STOP` | `(23) LVGL programmi, (2) Web UI emulator` | Arresta o annulla il programma attivo. |
| `(5) FSM_INPUT_EVENT_QR_SCANNED` + `(178) ACTION_ID_USB_CDC_SCANNER_READ` | `(13) Scanner USB` | Inoltra il barcode letto al consumer HTTP services. |
| `(4) FSM_INPUT_EVENT_QR_CREDIT` + `(98) ACTION_ID_PAYMENT_ACCEPTED` | `(18) HTTP services, (2) Web UI emulator` | Accredita credito QR alla FSM. |
| `(3) FSM_INPUT_EVENT_CARD_CREDIT` + `(98) ACTION_ID_PAYMENT_ACCEPTED` | `(2) Web UI emulator` | Accredita credito virtuale/card alla FSM. |
| `(6) FSM_INPUT_EVENT_TOKEN` + `(98) ACTION_ID_PAYMENT_ACCEPTED` | `(12) CCtalk, (2) Web UI emulator` | Accredita moneta/token/cash alla FSM. |
| `(0) FSM_INPUT_EVENT_NONE` + `(149) ACTION_ID_DIGITAL_IO_SET_OUTPUT` | `Dispatcher digital I/O via agent; (1) FSM come forward condizionale verso (4) io_process` | Scrive una uscita digitale; se l'uscita e' `WHITE_LED`/`BLUE_LED`/`RED_LED`/`HEATER1`, il dispatcher inoltra anche lo stato a `io_process`. |
| `(0) FSM_INPUT_EVENT_NONE` + `(150) ACTION_ID_DIGITAL_IO_GET_OUTPUT` | `Dispatcher digital I/O via agent` | Chiede alla FSM di leggere lo stato di una uscita digitale. |
| `(0) FSM_INPUT_EVENT_NONE` + `(152) ACTION_ID_DIGITAL_IO_GET_SNAPSHOT` | `Dispatcher digital I/O via agent, (2) Web UI test API` | Chiede alla FSM lo snapshot completo di ingressi e uscite digitali. |
| `(0) FSM_INPUT_EVENT_NONE` + `(153) ACTION_ID_DIGITAL_IO_INPUT_RISING` | `(5) Task digital_io verso (4) io_process` | Segnala un fronte di salita `0->1` su un ingresso non touch-mappable. |
| `(0) FSM_INPUT_EVENT_NONE` + `(123) ACTION_ID_CCTALK_START` | `(2) Web UI test API` | Chiede al task CCtalk di avviare l'accettatore. |
| `(0) FSM_INPUT_EVENT_NONE` + `(124) ACTION_ID_CCTALK_STOP` | `(2) Web UI test API` | Chiede al task CCtalk di fermare l'accettatore. |
| `(0) FSM_INPUT_EVENT_NONE` + `(125) ACTION_ID_CCTALK_MASK` | `(2) Web UI test API` | Chiede al task CCtalk di aggiornare la mask/inhibit dei canali. |
| `(0) FSM_INPUT_EVENT_NONE` + `(179) ACTION_ID_USB_CDC_SCANNER_ON` | `(2) Web UI test API` | Evento osservabile che segnala comando scanner ON. |
| `(0) FSM_INPUT_EVENT_NONE` + `(180) ACTION_ID_USB_CDC_SCANNER_OFF` | `(2) Web UI test API` | Evento osservabile che segnala comando scanner OFF. |

## Tabella tipi messaggi consumati (consumer)

| Consumer | Tipo / action gestiti | Descrizione breve |
|---|---|---|
| `(1) AGN_ID_FSM` | `(2) FSM_INPUT_EVENT_TOUCH` + `(96) ACTION_ID_NONE` | Interpreta il tocco fisico come attivita' utente. |
| `(1) AGN_ID_FSM` | `(2) FSM_INPUT_EVENT_TOUCH` + `(97) ACTION_ID_USER_ACTIVITY` | Interpreta touch/attivita' LVGL come evento utente. |
| `(1) AGN_ID_FSM` | `(3) FSM_INPUT_EVENT_KEY` + `(97) ACTION_ID_USER_ACTIVITY` | Interpreta un input tasto come attivita' utente. |
| `(1) AGN_ID_FSM` | `(6) FSM_INPUT_EVENT_TOKEN` + `(98) ACTION_ID_PAYMENT_ACCEPTED` | Aggiunge credito reale/token e avanza lo stato FSM. |
| `(1) AGN_ID_FSM` | `(4) FSM_INPUT_EVENT_QR_CREDIT` + `(98) ACTION_ID_PAYMENT_ACCEPTED` | Aggiunge credito QR come sessione virtual locked. |
| `(1) AGN_ID_FSM` | `(3) FSM_INPUT_EVENT_CARD_CREDIT` + `(98) ACTION_ID_PAYMENT_ACCEPTED` | Aggiunge credito card come sessione virtual locked. |
| `(1) AGN_ID_FSM` | `(8) FSM_INPUT_EVENT_PROGRAM_SELECTED` + `(99) ACTION_ID_PROGRAM_SELECTED` | Avvia un programma in stato `CREDIT`. |
| `(1) AGN_ID_FSM` | `(11) FSM_INPUT_EVENT_PROGRAM_SWITCH` + `(99) ACTION_ID_PROGRAM_SELECTED` | Scala il tempo residuo e cambia programma in corsa. |
| `(1) AGN_ID_FSM` | `(9) FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` + `(101) ACTION_ID_PROGRAM_PAUSE_TOGGLE` | Porta il programma in pausa o lo riprende. |
| `(1) AGN_ID_FSM` | `(7) FSM_INPUT_EVENT_PROGRAM_STOP` + `(100) ACTION_ID_PROGRAM_STOP` | Arresta il programma corrente. |
| `(1) AGN_ID_FSM` | `(0) FSM_INPUT_EVENT_NONE` + `(108) ACTION_ID_LVGL_TEST_ENTER` | Entra nello stato speciale di test LVGL. |
| `(1) AGN_ID_FSM` | `(0) FSM_INPUT_EVENT_NONE` + `(109) ACTION_ID_LVGL_TEST_EXIT` | Esce dallo stato speciale di test LVGL. |
| `(1) AGN_ID_FSM` | `(0) FSM_INPUT_EVENT_NONE` + `(149) ACTION_ID_DIGITAL_IO_SET_OUTPUT` | Esegue il comando I/O di scrittura uscita tramite handler agente interno. |
| `(1) AGN_ID_FSM` | `(0) FSM_INPUT_EVENT_NONE` + `(150) ACTION_ID_DIGITAL_IO_GET_OUTPUT` | Esegue la lettura dello stato di una uscita digitale tramite handler agente interno. |
| `(1) AGN_ID_FSM` | `(0) FSM_INPUT_EVENT_NONE` + `(152) ACTION_ID_DIGITAL_IO_GET_SNAPSHOT` | Esegue la lettura dello snapshot digitale completo tramite handler agente interno. |
| `(1) AGN_ID_FSM` | `(0) FSM_INPUT_EVENT_NONE` + `(179) ACTION_ID_USB_CDC_SCANNER_ON/OFF` | Oggi non cambia stato FSM; resta solo evento osservabile/loggabile. |
| `(4) AGN_ID_IO_PROCESS` | `(0) FSM_INPUT_EVENT_NONE` + `(153) ACTION_ID_DIGITAL_IO_INPUT_RISING` | Riceve i fronti `0->1` di ingressi non mappati a touch; oggi logga senza azione applicativa. |
| `(4) AGN_ID_IO_PROCESS` | `(0) FSM_INPUT_EVENT_NONE` + `(149) ACTION_ID_DIGITAL_IO_SET_OUTPUT` | Riceve lo stato delle uscite `WHITE_LED`/`BLUE_LED`/`RED_LED`/`HEATER1`; oggi logga senza azione applicativa. |
| `(18) AGN_ID_HTTP_SERVICES` | `(5) FSM_INPUT_EVENT_QR_SCANNED` + `(178) ACTION_ID_USB_CDC_SCANNER_READ` | Esegue lookup customer/backend a partire dal barcode scanner. |
| `(18) AGN_ID_HTTP_SERVICES` | `(8) FSM_INPUT_EVENT_PROGRAM_SELECTED` + `(99) ACTION_ID_PROGRAM_SELECTED` | Invia al backend la registrazione del pagamento servizio. |
| `(12) AGN_ID_CCTALK` | `(0) FSM_INPUT_EVENT_NONE` + `(123) ACTION_ID_CCTALK_START` | Avvia l'accettatore CCtalk nel task driver dedicato. |
| `(12) AGN_ID_CCTALK` | `(0) FSM_INPUT_EVENT_NONE` + `(124) ACTION_ID_CCTALK_STOP` | Ferma l'accettatore CCtalk nel task driver dedicato. |
| `(12) AGN_ID_CCTALK` | `(0) FSM_INPUT_EVENT_NONE` + `(125) ACTION_ID_CCTALK_MASK` | Applica mask/inhibit canali tramite driver CCtalk. |

## Tipi definiti ma non ancora prodotti via mailbox

| Tipo / action o famiglia | Dove definito | Stato attuale | Descrizione breve |
|---|---|---|---|
| `fsm_publish_simple_event(...)` | `main/fsm.c` | Helper definito ma senza call-site attivi | Wrapper legacy per pubblicare eventi generici verso `(1) AGN_ID_FSM`. |
| `(3) FSM_INPUT_EVENT_KEY` + `(97) ACTION_ID_USER_ACTIVITY` | `tasks_publish_key_event()` in `main/tasks.c` | Producer helper definito ma non chiamato | Hook per futuro pulsante fisico dedicato. |
| `(3) FSM_INPUT_EVENT_CARD_CREDIT` + `(98) ACTION_ID_PAYMENT_ACCEPTED` | `tasks_publish_card_credit_event()` in `main/tasks.c` | Producer helper definito ma non chiamato | Hook per futura integrazione cashless/card lato task. |
| `(1) FSM_INPUT_EVENT_USER_ACTIVITY` (tipo legacy diretto) | `main/fsm.h` | Tipo definito, nessun producer diretto attivo | Variante legacy di attivita' utente non emessa oggi come `type` puro. |
| `(5) FSM_INPUT_EVENT_COIN` | `main/fsm.h` | Tipo definito, nessun producer diretto attivo | Credito reale in forma legacy; oggi si usano soprattutto `(6) TOKEN`, `(4) QR_CREDIT` e `(3) CARD_CREDIT`. |
| `(10) FSM_INPUT_EVENT_CREDIT_ENDED` + `(103) ACTION_ID_CREDIT_ENDED` | `main/fsm.h` | Definito ma senza publisher attivo | Evento previsto per azzeramento credito/fine credito. |
| `(104) ACTION_ID_BUTTON_PRESSED` | `main/fsm.h` | Definito ma senza publisher attivo | Azione generica per pressione pulsante non ancora cablata. |
| `(105) ACTION_ID_SYSTEM_IDLE`, `(106) ACTION_ID_SYSTEM_RUN`, `(107) ACTION_ID_SYSTEM_ERROR` | `main/fsm.h` | Nessun publisher mailbox attivo | `SYSTEM_ERROR`/`SYSTEM_RUN` sono oggi usati con iniezione diretta interna (OOS) e gestiti da `fsm_handle_input_event(...)`; non transitano nella mailbox. |
| Famiglia `(112-116) ACTION_ID_GPIO_*` | `main/fsm.h` | Definita ma senza publisher attivo | Operazioni mailbox per GPIO legacy non oggi usate. |
| Famiglia `(119-125) ACTION_ID_CCTALK_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi CCtalk previsti oltre a `(123) START/(124) STOP/(125) MASK`. |
| Famiglia `(127-134) ACTION_ID_RS232_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox per RS232 non oggi emessi. |
| Famiglia `(132-135) ACTION_ID_RS485_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox per RS485 non oggi emessi. |
| Famiglia `(137-140) ACTION_ID_MDB_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox MDB non oggi emessi. |
| Famiglia `(143-146) ACTION_ID_IOEXP_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox I/O expander non oggi emessi. |
| `(151) ACTION_ID_DIGITAL_IO_GET_INPUT` | `main/fsm.h`, `main/tasks.c` | Definito ma senza producer attivo | La lettura singola ingresso e' supportata dalla mailbox, ma oggi non esistono call-site che la pubblichino. |
| Famiglia `(155-159) ACTION_ID_PWM_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox PWM previsti ma non usati. |
| Famiglia `(162-167) ACTION_ID_LED_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox LED previsti ma non usati. |
| Famiglia `(170-171) ACTION_ID_SHT40_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox sensore SHT40 previsti ma non usati. |
| Famiglia `(174-180) ACTION_ID_USB_CDC_SCANNER_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi scanner USB previsti ma non emessi nella build attuale. |
| Famiglia `(183-188) ACTION_ID_SD_CARD_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox SD card previsti ma non usati. |

## Tipi definiti ma non ancora consumati davvero

### Consumer implementati ma oggi senza producer attivo

| Consumer / funzione | Tipo / action | Stato attuale | Descrizione breve |
|---|---|---|---|
| `(1) AGN_ID_FSM` via `fsm_handle_input_event()` | `(1) FSM_INPUT_EVENT_USER_ACTIVITY` | Branch presente, nessun producer diretto attivo | Gestione legacy attivita' utente come tipo puro. |
| `(1) AGN_ID_FSM` via `fsm_handle_input_event()` | `(3) FSM_INPUT_EVENT_KEY` + `(97) ACTION_ID_USER_ACTIVITY` | Branch presente, helper producer non usato | Supporto pronto per futuro input tasto dedicato. |
| `(1) AGN_ID_FSM` via `fsm_handle_input_event()` | `(5) FSM_INPUT_EVENT_COIN` + `(98) ACTION_ID_PAYMENT_ACCEPTED` | Branch presente, nessun producer diretto attivo | Supporto legacy per credito reale in forma `COIN`. |
| `(1) AGN_ID_FSM` via `fsm_handle_input_event()` | `(10) FSM_INPUT_EVENT_CREDIT_ENDED` + `(103) ACTION_ID_CREDIT_ENDED` | Branch presente, nessun producer attivo | Permetterebbe di forzare l'azzeramento del credito. |
| `tasks_handle_digital_io_agent_event()` | `(151) ACTION_ID_DIGITAL_IO_GET_INPUT` | Handler presente, producer mailbox oggi assente | La lettura singola ingresso e' gestita, ma non ci sono publisher attivi nella build corrente. |
| `tasks_handle_digital_io_agent_event()` | `(153) ACTION_ID_DIGITAL_IO_INPUT_RISING` verso `(1) AGN_ID_FSM` | Branch presente, producer attivo assente | Il fronte `0->1` oggi viene pubblicato verso `(4) AGN_ID_IO_PROCESS`; il branch FSM resta disponibile per eventuali publisher futuri. |
| Recipients `(14) AGN_ID_LED`, `(23) AGN_ID_LVGL`, `(2) AGN_ID_WEB_UI` | `(102) ACTION_ID_PROGRAM_PREFINE_CYCLO` | Messaggio pubblicato ma nessun receiver task attivo | La FSM lo emette, ma oggi non esiste un consumer mailbox attivo per questi destinatari. |

### Tipi definiti senza consumer mailbox attivo

| Tipo / action o famiglia | Stato attuale | Descrizione breve |
|---|---|---|
| `(104) ACTION_ID_BUTTON_PRESSED` | Definito senza consumer mailbox attivo | Evento pulsante generico previsto ma non ancora gestito. |
| `(105) ACTION_ID_SYSTEM_IDLE/RUN/ERROR` | Nessun consumer mailbox dedicato | La FSM li gestisce internamente (`fsm_handle_input_event(...)`) quando ricevuti come eventi diretti; non esiste un consumer mailbox separato. |
| Famiglia `(112-116) ACTION_ID_GPIO_*` | Definita senza consumer mailbox attivo | Letture/scritture GPIO legacy previste ma senza handler mailbox dedicato attivo. |
| Famiglia `(119-125) ACTION_ID_CCTALK_*` | Definita senza consumer mailbox attivo | Il consumer `(12) CCtalk` oggi gestisce solo `(123) START/(124) STOP/(125) MASK`. |
| Famiglia `(127-134) ACTION_ID_RS232_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a RS232. |
| Famiglia `(132-135) ACTION_ID_RS485_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a RS485. |
| Famiglia `(137-140) ACTION_ID_MDB_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a MDB. |
| Famiglia `(143-146) ACTION_ID_IOEXP_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a I/O expander. |
| Famiglia `(155-159) ACTION_ID_PWM_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a PWM. |
| Famiglia `(162-167) ACTION_ID_LED_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a LED/barra LED. |
| Famiglia `(170-171) ACTION_ID_SHT40_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a misura/errori SHT40. |
| Famiglia `(174-180) ACTION_ID_USB_CDC_SCANNER_*` | Definita senza consumer mailbox attivo | Gli unici eventi scanner oggi osservabili via mailbox sono `(178) READ`, `(179) ON` e `(180) OFF`. |
| Famiglia `(183-188) ACTION_ID_SD_CARD_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a SD card. |

## Tabella messaggi attivi

| # | Messaggio descrittivo | Tipo / action | Mittente | Destinatari | Funzione che emette | Funzione di processo |
|---|---|---|---|---|---|---|
| 1 | Entrata in modalita' test LVGL | `(0) type=FSM_INPUT_EVENT_NONE`, `(108) action=ACTION_ID_LVGL_TEST_ENTER` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `fsm_publish_control_event()` chiamata da `fsm_enter_lvgl_pages_test()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_ENTER_LVGL_TEST)` |
| 2 | Uscita da modalita' test LVGL | `(0) type=FSM_INPUT_EVENT_NONE`, `(109) action=ACTION_ID_LVGL_TEST_EXIT` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `fsm_publish_control_event()` chiamata da `fsm_exit_lvgl_pages_test()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_EXIT_LVGL_TEST)` |
| 3 | Notifica PreFineCiclo | `(0) type=FSM_INPUT_EVENT_NONE`, `(102) action=ACTION_ID_PROGRAM_PREFINE_CYCLO` | `(1) AGN_ID_FSM` | `(14) AGN_ID_LED`, `(23) AGN_ID_LVGL`, `(2) AGN_ID_WEB_UI` | `fsm_tick()` | Nessun consumer mailbox attivo trovato nella build corrente |
| 4 | Touch su pagina ADS LVGL per uscita da splash/ads | `(2) type=FSM_INPUT_EVENT_TOUCH`, `(97) action=ACTION_ID_USER_ACTIVITY` | `(23) AGN_ID_LVGL` | `(1) AGN_ID_FSM` | `switch_from_ads_async()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_USER_ACTIVITY)` |
| 5 | Selezione programma da pannello LVGL | `(8) type=FSM_INPUT_EVENT_PROGRAM_SELECTED`, `(99) action=ACTION_ID_PROGRAM_SELECTED` | `(23) AGN_ID_LVGL` | `(1) AGN_ID_FSM` | `on_prog_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |
| 6 | Cambio programma da pannello LVGL con macchina gia' running/paused | `(11) type=FSM_INPUT_EVENT_PROGRAM_SWITCH`, `(99) action=ACTION_ID_PROGRAM_SELECTED` | `(23) AGN_ID_LVGL` | `(1) AGN_ID_FSM` | `on_prog_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |
| 7 | Toggle pausa/ripresa programma attivo da pannello LVGL | `(9) type=FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE`, `(101) action=ACTION_ID_PROGRAM_PAUSE_TOGGLE` | `(23) AGN_ID_LVGL` | `(1) AGN_ID_FSM` | `on_prog_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |
| 8 | Prima pressione STOP su pannello LVGL: richiesta pausa per conferma annullamento | `(9) type=FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE`, `(101) action=ACTION_ID_PROGRAM_PAUSE_TOGGLE` | `(23) AGN_ID_LVGL` | `(1) AGN_ID_FSM` | `on_stop_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |
| 9 | Seconda pressione STOP su pannello LVGL: stop/annullamento programma | `(7) type=FSM_INPUT_EVENT_PROGRAM_STOP`, `(100) action=ACTION_ID_PROGRAM_STOP` | `(23) AGN_ID_LVGL` | `(1) AGN_ID_FSM` | `on_stop_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |
| 10 | Touch fisico touchscreen | `(2) type=FSM_INPUT_EVENT_TOUCH`, `(96) action=ACTION_ID_NONE` | `(3) AGN_ID_TOUCH` | `(1) AGN_ID_FSM` | `touchscreen_task()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_USER_ACTIVITY)` |
| 11 | Barcode letto da scanner USB e inoltrato a HTTP services | `(5) type=FSM_INPUT_EVENT_QR_SCANNED`, `(178) action=ACTION_ID_USB_CDC_SCANNER_READ` | `(13) AGN_ID_USB_CDC_SCANNER` | `(18) AGN_ID_HTTP_SERVICES` | `scanner_on_barcode_cb()` | `http_services_task()` -> `http_services_getcustomers()` -> eventuale `publish_qr_credit_event()` |
| 12 | Credito QR pubblicato verso FSM dopo lookup backend | `(4) type=FSM_INPUT_EVENT_QR_CREDIT`, `(98) action=ACTION_ID_PAYMENT_ACCEPTED` | `(18) AGN_ID_HTTP_SERVICES` | `(1) AGN_ID_FSM` | `publish_qr_credit_event()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 13 | Notifica pagamento servizio al backend quando un programma entra in RUNNING | `(8) type=FSM_INPUT_EVENT_PROGRAM_SELECTED`, `(99) action=ACTION_ID_PROGRAM_SELECTED` | `(1) AGN_ID_FSM` | `(18) AGN_ID_HTTP_SERVICES` | `publish_program_payment_event()` | `http_services_task()` -> `http_services_payment()` |
| 14 | Comando digitale set output via agent I/O | `(0) type=FSM_INPUT_EVENT_NONE`, `(149) action=ACTION_ID_DIGITAL_IO_SET_OUTPUT` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `tasks_dispatch_digital_io_agent_request()` chiamata da `tasks_digital_io_set_output_via_agent()`; call-site attivi: `web_ui_virtual_relay_control()` e branch `/api/test/dio_set` | `tasks_fsm_task()` -> `tasks_handle_digital_io_agent_event()` -> `tasks_execute_digital_io_action()` |
| 15 | Comando digitale get output via agent I/O | `(0) type=FSM_INPUT_EVENT_NONE`, `(150) action=ACTION_ID_DIGITAL_IO_GET_OUTPUT` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `tasks_dispatch_digital_io_agent_request()` chiamata da `tasks_digital_io_get_output_via_agent()`; call-site attivo: `web_ui_virtual_relay_get()` | `tasks_fsm_task()` -> `tasks_handle_digital_io_agent_event()` -> `tasks_execute_digital_io_action()` |
| 16 | Comando digitale get snapshot via agent I/O | `(0) type=FSM_INPUT_EVENT_NONE`, `(152) action=ACTION_ID_DIGITAL_IO_GET_SNAPSHOT` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `tasks_dispatch_digital_io_agent_request()` chiamata da `tasks_digital_io_get_snapshot_via_agent()`; call-site attivo: branch `/api/test/dio_get` | `tasks_fsm_task()` -> `tasks_handle_digital_io_agent_event()` -> `tasks_execute_digital_io_action()` |
| 17 | Fronte `0->1` su ingresso non touch-mappable (`DIPx` / `SERVICE_SWITCH`) | `(0) type=FSM_INPUT_EVENT_NONE`, `(153) action=ACTION_ID_DIGITAL_IO_INPUT_RISING` | `(5) AGN_ID_DIGITAL_IO` | `(4) AGN_ID_IO_PROCESS` | `tasks_process_other_digital_input_rising()` chiamata da `digital_io_task()` | `io_process_task()` |
| 18 | Scrittura di uscita segnale (`WHITE_LED` / `BLUE_LED` / `RED_LED` / `HEATER1`) inoltrata a `io_process` | `(0) type=FSM_INPUT_EVENT_NONE`, `(149) action=ACTION_ID_DIGITAL_IO_SET_OUTPUT` | Mittente originale dell'evento, oggi tipicamente `(2) AGN_ID_WEB_UI` | `(4) AGN_ID_IO_PROCESS` | `tasks_handle_digital_io_agent_event()` dopo `digital_io_set_output()` riuscito | `io_process_task()` |
| 19 | Pagamento emulato da Web UI con sorgente cash/token | `(6) type=FSM_INPUT_EVENT_TOKEN`, `(98) action=ACTION_ID_PAYMENT_ACCEPTED` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `api_emulator_coin_post()` con `source != card/qr` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 20 | Pagamento emulato da Web UI con sorgente QR | `(4) type=FSM_INPUT_EVENT_QR_CREDIT`, `(98) action=ACTION_ID_PAYMENT_ACCEPTED` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `api_emulator_coin_post()` con `source=qr` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 21 | Pagamento emulato da Web UI con sorgente card | `(3) type=FSM_INPUT_EVENT_CARD_CREDIT`, `(98) action=ACTION_ID_PAYMENT_ACCEPTED` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `api_emulator_coin_post()` con `source=card` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 22 | Avvio/cambio/toggle programma da emulatore Web UI | `(8) type=FSM_INPUT_EVENT_PROGRAM_SELECTED` oppure `(11) FSM_INPUT_EVENT_PROGRAM_SWITCH` oppure `(9) FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `api_emulator_program_start()` tramite `tasks_publish_program_button_action()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |
| 23 | Stop programma da emulatore Web UI | `(7) type=FSM_INPUT_EVENT_PROGRAM_STOP`, `(100) action=ACTION_ID_PROGRAM_STOP` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `api_emulator_program_stop()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |
| 24 | Pausa/ripresa programma da emulatore Web UI | `(9) type=FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE`, `(101) action=ACTION_ID_PROGRAM_PAUSE_TOGGLE` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | `api_emulator_program_pause_toggle()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |
| 25 | Moneta/credito da gettoniera CCtalk | `(6) type=FSM_INPUT_EVENT_TOKEN`, `(98) action=ACTION_ID_PAYMENT_ACCEPTED` | `(12) AGN_ID_CCTALK` | `(1) AGN_ID_FSM` | `cctalk_handle_buffered_credit()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 26 | Comando CCtalk START da Web UI test API | `(0) type=FSM_INPUT_EVENT_NONE`, `(123) action=ACTION_ID_CCTALK_START` | `(2) AGN_ID_WEB_UI` | `(12) AGN_ID_CCTALK` | branch `cctalk_start` in `api_test_handler()` | `cctalk_task_run()` / consumer CCtalk -> `cctalk_driver_start_acceptor()` |
| 27 | Comando CCtalk STOP da Web UI test API | `(0) type=FSM_INPUT_EVENT_NONE`, `(124) action=ACTION_ID_CCTALK_STOP` | `(2) AGN_ID_WEB_UI` | `(12) AGN_ID_CCTALK` | branch `cctalk_stop` in `api_test_handler()` | `cctalk_task_run()` / consumer CCtalk -> `cctalk_driver_stop_acceptor()` |
| 28 | Comando CCtalk MASK da Web UI test API | `(0) type=FSM_INPUT_EVENT_NONE`, `(125) action=ACTION_ID_CCTALK_MASK` | `(2) AGN_ID_WEB_UI` | `(12) AGN_ID_CCTALK` | branch `cctalk_retention_ch1_2` in `api_test_handler()` | consumer CCtalk -> `cctalk_modify_master_inhibit_std()` + `cctalk_modify_inhibit_status()` |
| 29 | Scanner ON osservabile via mailbox | `(0) type=FSM_INPUT_EVENT_NONE`, `(179) action=ACTION_ID_USB_CDC_SCANNER_ON` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | branch `scanner_on` in `api_test_handler()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> evento oggi solo osservabile/loggabile |
| 30 | Scanner OFF osservabile via mailbox | `(0) type=FSM_INPUT_EVENT_NONE`, `(180) action=ACTION_ID_USB_CDC_SCANNER_OFF` | `(2) AGN_ID_WEB_UI` | `(1) AGN_ID_FSM` | branch `scanner_off` in `api_test_handler()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> evento oggi solo osservabile/loggabile |
| 31 | Azione equivalente al tasto programma generata da ingresso `OPTOx` mappato | `(8) type=FSM_INPUT_EVENT_PROGRAM_SELECTED` oppure `(11) FSM_INPUT_EVENT_PROGRAM_SWITCH` oppure `(9) FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` | `(5) AGN_ID_DIGITAL_IO` | `(1) AGN_ID_FSM` | `digital_io_task()` tramite `tasks_publish_program_button_action()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` |

## Note operative

- La mailbox e' implementata in `main/fsm.c` con publish in `fsm_event_publish()` e receive in `fsm_event_receive()`.
- Il consumer principale `(1) AGN_ID_FSM` gira nel task FSM in `main/tasks.c`.
- Il consumer `(4) AGN_ID_IO_PROCESS` oggi e' intenzionalmente un placeholder: riceve gli eventi I/O non-touch e gli stati delle uscite segnale, ma per ora effettua solo logging.
- Il task `digital_io_task()` **non** pubblica piu' `(153) ACTION_ID_DIGITAL_IO_INPUT_RISING` verso `(1) AGN_ID_FSM`: per gli `OPTOx` mappati pubblica direttamente l'azione equivalente al tasto programma; per gli altri ingressi inoltra il fronte a `(4) AGN_ID_IO_PROCESS`.
- I relay logici programma `R01..R12` passano da `digital_io_program_relay_to_output_id()`:
  - `R01..R04` -> `RELAY1..RELAY4` locali
  - `R05..R12` -> uscite Modbus `OUT09..OUT16`

## Appendice: Tabelle Definizioni Complete

### Agent ID (AGN_ID_*)

| Valore | Nome | Descrizione |
|---|---|---|
| 0 | `AGN_ID_NONE` | Nessun agente (valore di default) |
| 1 | `AGN_ID_FSM` | Finite state machine core |
| 2 | `AGN_ID_WEB_UI` | Web interface / emulator |
| 3 | `AGN_ID_TOUCH` | Touch driver |
| 4 | `AGN_ID_TOKEN` | Token reader |
| 5 | `AGN_ID_DIGITAL_IO` | Unified digital I/O task |
| 6 | `AGN_ID_IO_PROCESS` | Consumer fallback per segnali I/O |
| 7 | `AGN_ID_AUX_GPIO` | Aux GPIO (GPIO33 etc) |
| 8 | `AGN_ID_IO_EXPANDER` | I/O expander component |
| 9 | `AGN_ID_PWM1` | PWM output driver |
| 10 | `AGN_ID_PWM2` | PWM output driver |
| 11 | `AGN_ID_LED` | LED controller |
| 12 | `AGN_ID_SHT40` | Temperature/humidity sensor |
| 13 | `AGN_ID_CCTALK` | CCtalk bus |
| 14 | `AGN_ID_RS232` | RS-232 UART |
| 15 | `AGN_ID_RS485` | RS-485 UART |
| 16 | `AGN_ID_MDB` | MDB bus |
| 17 | `AGN_ID_USB_CDC_SCANNER` | USB CDC barcode scanner |
| 18 | `AGN_ID_USB_HOST` | USB host CDC-ACM |
| 19 | `AGN_ID_SD_CARD` | SD card interface |
| 20 | `AGN_ID_EEPROM` | External EEPROM |
| 21 | `AGN_ID_REMOTE_LOGGING` | Network log sender |
| 22 | `AGN_ID_HTTP_SERVICES` | Internal HTTP services |
| 23 | `AGN_ID_DEVICE_CONFIG` | Configuration manager |
| 24 | `AGN_ID_DEVICE_ACTIVITY` | Activity logging service |
| 25 | `AGN_ID_ERROR_LOG` | Error/crash logger |
| 26 | `AGN_ID_LVGL` | LVGL graphics engine |
| 27 | `AGN_ID_WAVESHARE_LCD` | Display driver |

### Input Event Types (FSM_INPUT_EVENT_*)

| Valore | Nome | Descrizione |
|---|---|---|
| 0 | `FSM_INPUT_EVENT_NONE` | Nessun evento (default) |
| 1 | `FSM_INPUT_EVENT_USER_ACTIVITY` | Attività utente generica |
| 2 | `FSM_INPUT_EVENT_TOUCH` | Evento touch |
| 3 | `FSM_INPUT_EVENT_KEY` | Pressione tasto |
| 4 | `FSM_INPUT_EVENT_COIN` | Moneta/credito reale (legacy) |
| 5 | `FSM_INPUT_EVENT_TOKEN` | Gettone/token |
| 6 | `FSM_INPUT_EVENT_CARD_CREDIT` | Credito da tessera |
| 7 | `FSM_INPUT_EVENT_QR_CREDIT` | Credito da QR |
| 8 | `FSM_INPUT_EVENT_QR_SCANNED` | Barcode QR scansionato |
| 9 | `FSM_INPUT_EVENT_PROGRAM_SELECTED` | Programma selezionato |
| 10 | `FSM_INPUT_EVENT_PROGRAM_STOP` | Programma fermato |
| 11 | `FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` | Toggle pausa programma |
| 12 | `FSM_INPUT_EVENT_CREDIT_ENDED` | Credito terminato |
| 13 | `FSM_INPUT_EVENT_PROGRAM_SWITCH` | Cambio programma a macchina running |

### Action IDs (ACTION_ID_*)

#### **Core Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 96 | `ACTION_ID_NONE` | Nessuna azione (default) |
| 97 | `ACTION_ID_USER_ACTIVITY` | Attività utente |
| 98 | `ACTION_ID_PAYMENT_ACCEPTED` | Pagamento accettato |
| 99 | `ACTION_ID_PROGRAM_SELECTED` | Programma selezionato |
| 100 | `ACTION_ID_PROGRAM_STOP` | Programma fermato |
| 101 | `ACTION_ID_PROGRAM_PAUSE_TOGGLE` | Toggle pausa programma |
| 102 | `ACTION_ID_PROGRAM_PREFINE_CYCLO` | Attivazione segnalazione PreFineCiclo |
| 103 | `ACTION_ID_CREDIT_ENDED` | Credito terminato |
| 104 | `ACTION_ID_BUTTON_PRESSED` | Pulsante premuto |
| 105 | `ACTION_ID_SYSTEM_IDLE` | Sistema in idle |
| 106 | `ACTION_ID_SYSTEM_RUN` | Sistema in esecuzione |
| 107 | `ACTION_ID_SYSTEM_ERROR` | Errore sistema |
| 108 | `ACTION_ID_LVGL_TEST_ENTER` | Entra test LVGL |
| 109 | `ACTION_ID_LVGL_TEST_EXIT` | Esci test LVGL |

#### **GPIO Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 112 | `ACTION_ID_GPIO_READ_PORT` | Leggi singola porta GPIO |
| 113 | `ACTION_ID_GPIO_READ_ALL` | Leggi tutte le porte GPIO |
| 114 | `ACTION_ID_GPIO_WRITE_PORT` | Scrivi singola porta GPIO |
| 115 | `ACTION_ID_GPIO_WRITE_ALL` | Scrivi tutte le porte GPIO |
| 116 | `ACTION_ID_GPIO_RESET_ALL` | Reset/clear tutte le porte GPIO |

#### **CCtalk Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 119 | `ACTION_ID_CCTALK_RX_DATA` | Dati ricevuti su CCtalk |
| 120 | `ACTION_ID_CCTALK_TX_DATA` | Dati trasmessi su CCtalk |
| 121 | `ACTION_ID_CCTALK_CONFIG` | Cambio configurazione |
| 122 | `ACTION_ID_CCTALK_RESET` | Comando reset |
| 123 | `ACTION_ID_CCTALK_START` | Avvia sequenza accettatore |
| 124 | `ACTION_ID_CCTALK_STOP` | Ferma sequenza accettatore |
| 125 | `ACTION_ID_CCTALK_MASK` | Imposta maschera canali |

#### **RS232 Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 127 | `ACTION_ID_RS232_RX_DATA` | Dati ricevuti RS232 |
| 128 | `ACTION_ID_RS232_TX_DATA` | Dati trasmessi RS232 |
| 129 | `ACTION_ID_RS232_CONFIG` | Configurazione RS232 |
| 130 | `ACTION_ID_RS232_RESET` | Reset RS232 |

#### **RS485 Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 132 | `ACTION_ID_RS485_RX_DATA` | Dati ricevuti RS485 |
| 133 | `ACTION_ID_RS485_TX_DATA` | Dati trasmessi RS485 |
| 134 | `ACTION_ID_RS485_CONFIG` | Configurazione RS485 |
| 135 | `ACTION_ID_RS485_RESET` | Reset RS485 |

#### **MDB Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 137 | `ACTION_ID_MDB_RX_DATA` | Dati ricevuti MDB |
| 138 | `ACTION_ID_MDB_TX_DATA` | Dati trasmessi MDB |
| 139 | `ACTION_ID_MDB_CONFIG` | Configurazione MDB |
| 140 | `ACTION_ID_MDB_RESET` | Reset MDB |

#### **I/O Expander Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 143 | `ACTION_ID_IOEXP_READ_PORT` | Leggi porta I/O expander |
| 144 | `ACTION_ID_IOEXP_WRITE_PORT` | Scrivi porta I/O expander |
| 145 | `ACTION_ID_IOEXP_CONFIG` | Configurazione I/O expander |
| 146 | `ACTION_ID_IOEXP_RESET` | Reset I/O expander |

#### **Digital I/O Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 149 | `ACTION_ID_DIGITAL_IO_SET_OUTPUT` | Imposta uscita digitale |
| 150 | `ACTION_ID_DIGITAL_IO_GET_OUTPUT` | Leggi uscita digitale |
| 151 | `ACTION_ID_DIGITAL_IO_GET_INPUT` | Leggi ingresso digitale |
| 152 | `ACTION_ID_DIGITAL_IO_GET_SNAPSHOT` | Leggi snapshot completo I/O |
| 153 | `ACTION_ID_DIGITAL_IO_INPUT_RISING` | Fronte salita ingresso digitale |

#### **PWM Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 156 | `ACTION_ID_PWM_SET_DUTY` | Imposta duty cycle PWM |
| 157 | `ACTION_ID_PWM_START` | Avvia PWM |
| 158 | `ACTION_ID_PWM_STOP` | Ferma PWM |
| 159 | `ACTION_ID_PWM_CONFIG` | Configurazione PWM |

#### **LED Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 162 | `ACTION_ID_LED_SET_RGBCOLOR` | Imposta colore RGB LED |
| 163 | `ACTION_ID_LED_ALL_OFF` | Spegni tutti i LED |
| 164 | `ACTION_ID_LED_CONFIG` | Configurazione LED |
| 165 | `ACTION_ID_LED_BAR_SET_STATE` | Imposta stato barra LED |
| 166 | `ACTION_ID_LED_BAR_SET_PROGRESS` | Imposta progressione barra LED |
| 167 | `ACTION_ID_LED_BAR_CLEAR` | Cancella barra LED |

#### **SHT40 Sensor Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 170 | `ACTION_ID_SHT40_MEASURE_READY` | Misurazione SHT40 pronta |
| 171 | `ACTION_ID_SHT40_ERROR` | Errore sensore SHT40 |

#### **USB CDC Scanner Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 174 | `ACTION_ID_USB_CDC_SCANNER_RX` | Dati ricevuti scanner |
| 175 | `ACTION_ID_USB_CDC_SCANNER_TX` | Dati trasmessi scanner |
| 176 | `ACTION_ID_USB_CDC_SCANNER_CONNECT` | Scanner connesso |
| 177 | `ACTION_ID_USB_CDC_SCANNER_DISCONNECT` | Scanner disconnesso |
| 178 | `ACTION_ID_USB_CDC_SCANNER_READ` | Richiedi lettura barcode |
| 179 | `ACTION_ID_USB_CDC_SCANNER_ON` | Accendi scanner |
| 180 | `ACTION_ID_USB_CDC_SCANNER_OFF` | Spegni scanner |

#### **SD Card Actions**
| Valore | Nome | Descrizione |
|---|---|---|
| 183 | `ACTION_ID_SD_CARD_INSERT` | SD card inserita |
| 184 | `ACTION_ID_SD_CARD_REMOVE` | SD card rimossa |
| 185 | `ACTION_ID_SD_CARD_READ` | Leggi da SD card |
| 186 | `ACTION_ID_SD_CARD_WRITE` | Scrivi su SD card |
| 187 | `ACTION_ID_SD_CARD_DELETE` | Cancella da SD card |
| 188 | `ACTION_ID_SD_CARD_ERROR` | Errore SD card |

### FSM States (FSM_STATE_*)

| Valore | Nome | Descrizione |
|---|---|---|
| 0 | `FSM_STATE_IDLE` | Sistema in attesa |
| 1 | `FSM_STATE_ADS` | Visualizzazione ADS |
| 2 | `FSM_STATE_CREDIT` | Inserimento credito |
| 3 | `FSM_STATE_RUNNING` | Programma in esecuzione |
| 4 | `FSM_STATE_PAUSED` | Programma in pausa |
| 5 | `FSM_STATE_LVGL_PAGES_TEST` | Test pagine LVGL |

### Session Sources (FSM_SESSION_SOURCE_*)

| Valore | Nome | Descrizione |
|---|---|---|
| 0 | `FSM_SESSION_SOURCE_NONE` | Nessuna sorgente |
| 1 | `FSM_SESSION_SOURCE_TOUCH` | Sorgente touch |
| 2 | `FSM_SESSION_SOURCE_KEY` | Sorgente tasto |
| 3 | `FSM_SESSION_SOURCE_COIN` | Sorgente moneta |
| 4 | `FSM_SESSION_SOURCE_QR` | Sorgente QR |
| 5 | `FSM_SESSION_SOURCE_CARD` | Sorgente tessera |

### Session Modes (FSM_SESSION_MODE_*)

| Valore | Nome | Descrizione |
|---|---|---|
| 0 | `FSM_SESSION_MODE_NONE` | Nessuna modalità |
| 1 | `FSM_SESSION_MODE_OPEN_PAYMENTS` | Pagamenti aperti |
| 2 | `FSM_SESSION_MODE_VIRTUAL_LOCKED` | Sessione virtuale bloccata |

### FSM Events (FSM_EVENT_*)

| Valore | Nome | Descrizione |
|---|---|---|
| 0 | `FSM_EVENT_NONE` | Nessun evento |
| 1 | `FSM_EVENT_USER_ACTIVITY` | Attività utente |
| 2 | `FSM_EVENT_PAYMENT_ACCEPTED` | Pagamento accettato |
| 3 | `FSM_EVENT_PROGRAM_SELECTED` | Programma selezionato |
| 4 | `FSM_EVENT_PROGRAM_STOP` | Programma fermato |
| 5 | `FSM_EVENT_PROGRAM_PAUSE` | Programma in pausa |
| 6 | `FSM_EVENT_PROGRAM_RESUME` | Programma ripreso |
| 7 | `FSM_EVENT_TIMEOUT` | Timeout |
| 8 | `FSM_EVENT_CREDIT_ENDED` | Credito terminato |
| 9 | `FSM_EVENT_ENTER_LVGL_TEST` | Entra test LVGL |
| 10 | `FSM_EVENT_EXIT_LVGL_TEST` | Esci test LVGL |
