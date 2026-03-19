# MESSAGES / Mailbox FSM

Questo documento elenca i **messaggi attualmente generati** che transitano nella mailbox condivisa implementata in `main/fsm.c`.

## Criteri usati

- Sono inclusi solo i messaggi con un **call-site attivo** nella build corrente.
- Sono esclusi i file non compilati, ad esempio `components/lvgl_panel/lvgl_page_programmi copy.c`.
- Sono esclusi gli helper senza call-site attivo, ad esempio:
  - `fsm_publish_simple_event()`
  - `tasks_publish_key_event()`
  - `tasks_publish_card_credit_event()`
- Le letture `DIGITAL_IO_GET_OUTPUT`, `DIGITAL_IO_GET_INPUT` e `DIGITAL_IO_GET_SNAPSHOT` **non transitano più** nella mailbox: oggi bypassano la FSM e usano direttamente il lock interno di `digital_io`.

## Consumer mailbox attivi trovati

I receiver attivi trovati tramite `fsm_event_receive(...)` nella build corrente sono:

- `AGN_ID_FSM` → consumer principale nel task FSM
- `AGN_ID_HTTP_SERVICES` → consumer nel task `http_services_task()`
- `AGN_ID_CCTALK` → consumer nel task `cctalk_task_run()`

Non risultano invece consumer mailbox attivi per:

- `AGN_ID_LED`
- `AGN_ID_LVGL`
- `AGN_ID_WEB_UI`

Questo significa che il messaggio `ACTION_ID_PROGRAM_PREFINE_CYCLO`, pur essendo pubblicato verso questi destinatari, **oggi non ha un consumer mailbox attivo** nella build corrente.

## Tabella tipi messaggi prodotti (producer)

Questa tabella raggruppa i **tipi di messaggio effettivamente emessi** nella build corrente.

| Tipo / action | Producer attivi | Descrizione breve |
|---|---|---|
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_LVGL_TEST_ENTER` | Web UI | Richiede alla FSM di entrare nella modalita' test pagine LVGL. |
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_LVGL_TEST_EXIT` | Web UI | Richiede alla FSM di uscire dalla modalita' test pagine LVGL. |
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_PROGRAM_PREFINE_CYCLO` | FSM | Notifica che il programma ha raggiunto la soglia PreFineCiclo. |
| `FSM_INPUT_EVENT_TOUCH` + `ACTION_ID_USER_ACTIVITY` | LVGL ADS page | Segnala attivita' utente da schermata ads/splash. |
| `FSM_INPUT_EVENT_TOUCH` + `ACTION_ID_NONE` | Task touchscreen | Segnala un tocco fisico al sistema FSM. |
| `FSM_INPUT_EVENT_PROGRAM_SELECTED` + `ACTION_ID_PROGRAM_SELECTED` | LVGL programmi, Web UI emulator, FSM verso HTTP services | Seleziona un programma o notifica al backend un pagamento servizio. |
| `FSM_INPUT_EVENT_PROGRAM_SWITCH` + `ACTION_ID_PROGRAM_SELECTED` | LVGL programmi, Web UI emulator | Cambia programma mentre un altro e' gia' in esecuzione o in pausa. |
| `FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` + `ACTION_ID_PROGRAM_PAUSE_TOGGLE` | LVGL programmi, Web UI emulator | Alterna pausa e ripresa del programma attivo. |
| `FSM_INPUT_EVENT_PROGRAM_STOP` + `ACTION_ID_PROGRAM_STOP` | LVGL programmi, Web UI emulator | Arresta o annulla il programma attivo. |
| `FSM_INPUT_EVENT_QR_SCANNED` + `ACTION_ID_USB_CDC_SCANNER_READ` | Scanner USB | Inoltra il barcode letto al consumer HTTP services. |
| `FSM_INPUT_EVENT_QR_CREDIT` + `ACTION_ID_PAYMENT_ACCEPTED` | HTTP services, Web UI emulator | Accredita credito QR alla FSM. |
| `FSM_INPUT_EVENT_CARD_CREDIT` + `ACTION_ID_PAYMENT_ACCEPTED` | Web UI emulator | Accredita credito virtuale/card alla FSM. |
| `FSM_INPUT_EVENT_TOKEN` + `ACTION_ID_PAYMENT_ACCEPTED` | CCtalk, Web UI emulator | Accredita moneta/token/cash alla FSM. |
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_DIGITAL_IO_SET_OUTPUT` | Dispatcher digital I/O via agent | Chiede alla FSM di eseguire una scrittura output I/O. |
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_CCTALK_START` | Web UI test API | Chiede al task CCtalk di avviare l'accettatore. |
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_CCTALK_STOP` | Web UI test API | Chiede al task CCtalk di fermare l'accettatore. |
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_CCTALK_MASK` | Web UI test API | Chiede al task CCtalk di aggiornare la mask/inhibit dei canali. |
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_USB_CDC_SCANNER_ON` | Web UI test API | Evento osservabile che segnala comando scanner ON. |
| `FSM_INPUT_EVENT_NONE` + `ACTION_ID_USB_CDC_SCANNER_OFF` | Web UI test API | Evento osservabile che segnala comando scanner OFF. |

## Tabella tipi messaggi consumati (consumer)

Questa tabella raggruppa i **tipi di messaggio effettivamente gestiti** dai receiver mailbox attivi.

| Consumer | Tipo / action gestiti | Descrizione breve |
|---|---|---|
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_TOUCH` + `ACTION_ID_NONE` | Interpreta il tocco fisico come attivita' utente. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_TOUCH` + `ACTION_ID_USER_ACTIVITY` | Interpreta touch/attivita' LVGL come evento utente. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_KEY` + `ACTION_ID_USER_ACTIVITY` | Interpreta un input tasto come attivita' utente. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_TOKEN` + `ACTION_ID_PAYMENT_ACCEPTED` | Aggiunge credito reale/token e avanza lo stato FSM. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_QR_CREDIT` + `ACTION_ID_PAYMENT_ACCEPTED` | Aggiunge credito QR come sessione virtual locked. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_CARD_CREDIT` + `ACTION_ID_PAYMENT_ACCEPTED` | Aggiunge credito card come sessione virtual locked. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_PROGRAM_SELECTED` + `ACTION_ID_PROGRAM_SELECTED` | Avvia un programma in stato `CREDIT`. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_PROGRAM_SWITCH` + `ACTION_ID_PROGRAM_SELECTED` | Scala il tempo residuo e cambia programma in corsa. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` + `ACTION_ID_PROGRAM_PAUSE_TOGGLE` | Porta il programma in pausa o lo riprende. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_PROGRAM_STOP` + `ACTION_ID_PROGRAM_STOP` | Arresta il programma corrente. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_NONE` + `ACTION_ID_LVGL_TEST_ENTER` | Entra nello stato speciale di test LVGL. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_NONE` + `ACTION_ID_LVGL_TEST_EXIT` | Esce dallo stato speciale di test LVGL. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_NONE` + `ACTION_ID_DIGITAL_IO_SET_OUTPUT` | Esegue il comando I/O tramite handler agente interno. |
| `AGN_ID_FSM` | `FSM_INPUT_EVENT_NONE` + `ACTION_ID_USB_CDC_SCANNER_ON/OFF` | Oggi non cambia stato FSM; resta solo evento osservabile/loggabile. |
| `AGN_ID_HTTP_SERVICES` | `FSM_INPUT_EVENT_QR_SCANNED` + `ACTION_ID_USB_CDC_SCANNER_READ` | Esegue lookup customer/backend a partire dal barcode scanner. |
| `AGN_ID_HTTP_SERVICES` | `FSM_INPUT_EVENT_PROGRAM_SELECTED` + `ACTION_ID_PROGRAM_SELECTED` | Invia al backend la registrazione del pagamento servizio. |
| `AGN_ID_CCTALK` | `FSM_INPUT_EVENT_NONE` + `ACTION_ID_CCTALK_START` | Avvia l'accettatore CCtalk nel task driver dedicato. |
| `AGN_ID_CCTALK` | `FSM_INPUT_EVENT_NONE` + `ACTION_ID_CCTALK_STOP` | Ferma l'accettatore CCtalk nel task driver dedicato. |
| `AGN_ID_CCTALK` | `FSM_INPUT_EVENT_NONE` + `ACTION_ID_CCTALK_MASK` | Applica mask/inhibit canali tramite driver CCtalk. |

## Tipi definiti ma non ancora prodotti

Questa tabella elenca i messaggi o le famiglie di messaggi **gia' definite nel codice** ma senza un publisher attivo nella build corrente.

| Tipo / action o famiglia | Dove definito | Stato attuale | Descrizione breve |
|---|---|---|---|
| `fsm_publish_simple_event(...)` | `main/fsm.c` | Helper definito ma senza call-site attivi | Wrapper legacy per pubblicare eventi generici verso `AGN_ID_FSM`. |
| `FSM_INPUT_EVENT_KEY` + `ACTION_ID_USER_ACTIVITY` | `tasks_publish_key_event()` in `main/tasks.c` | Producer helper definito ma non chiamato | Hook per futuro pulsante fisico dedicato. |
| `FSM_INPUT_EVENT_CARD_CREDIT` + `ACTION_ID_PAYMENT_ACCEPTED` | `tasks_publish_card_credit_event()` in `main/tasks.c` | Producer helper definito ma non chiamato | Hook per futura integrazione cashless/card lato task. |
| `FSM_INPUT_EVENT_USER_ACTIVITY` (tipo legacy diretto) | `main/fsm.h` | Tipo definito, nessun producer diretto attivo | Variante legacy di attivita' utente non emessa oggi come `type` puro. |
| `FSM_INPUT_EVENT_COIN` | `main/fsm.h` | Tipo definito, nessun producer diretto attivo | Credito reale in forma legacy; oggi si usano soprattutto `TOKEN`, `QR_CREDIT` e `CARD_CREDIT`. |
| `FSM_INPUT_EVENT_CREDIT_ENDED` + `ACTION_ID_CREDIT_ENDED` | `main/fsm.h` | Definito ma senza publisher attivo | Evento previsto per azzeramento credito/fine credito. |
| `ACTION_ID_BUTTON_PRESSED` | `main/fsm.h` | Definito ma senza publisher attivo | Azione generica per pressione pulsante non ancora cablata. |
| `ACTION_ID_SYSTEM_IDLE`, `ACTION_ID_SYSTEM_RUN`, `ACTION_ID_SYSTEM_ERROR` | `main/fsm.h` | Definiti ma senza publisher attivo | Eventi di stato sistema previsti ma non ancora emessi. |
| Famiglia `ACTION_ID_GPIO_*` | `main/fsm.h` | Definita ma senza publisher attivo | Operazioni mailbox per GPIO legacy non oggi usate. |
| Famiglia `ACTION_ID_CCTALK_RX_DATA/TX_DATA/CONFIG/RESET` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi CCtalk previsti oltre a `START/STOP/MASK`. |
| Famiglia `ACTION_ID_RS232_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox per RS232 non oggi emessi. |
| Famiglia `ACTION_ID_RS485_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox per RS485 non oggi emessi. |
| Famiglia `ACTION_ID_MDB_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox MDB non oggi emessi. |
| Famiglia `ACTION_ID_IOEXP_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox I/O expander non oggi emessi. |
| `ACTION_ID_DIGITAL_IO_GET_OUTPUT/GET_INPUT/GET_SNAPSHOT` | `main/fsm.h`, `main/tasks.c` | Definiti ma oggi bypassano la mailbox | Le letture digital I/O usano accesso diretto con lock interno. |
| Famiglia `ACTION_ID_PWM_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox PWM previsti ma non usati. |
| Famiglia `ACTION_ID_LED_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox LED previsti ma non usati. |
| Famiglia `ACTION_ID_SHT40_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox sensore SHT40 previsti ma non usati. |
| Famiglia `ACTION_ID_USB_CDC_SCANNER_RX/TX/CONNECT/DISCONNECT` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi scanner USB previsti ma non emessi nella build attuale. |
| Famiglia `ACTION_ID_SD_CARD_*` | `main/fsm.h` | Definita ma senza publisher attivo | Eventi mailbox SD card previsti ma non usati. |

## Tipi definiti ma non ancora consumati davvero

Questa sezione distingue tra:

- messaggi per cui esiste **gia' una logica consumer**, ma oggi manca un producer attivo
- messaggi definiti nelle enum, ma senza un vero consumer mailbox attivo nella build corrente

### Consumer implementati ma oggi senza producer attivo

| Consumer / funzione | Tipo / action | Stato attuale | Descrizione breve |
|---|---|---|---|
| `AGN_ID_FSM` via `fsm_handle_input_event()` | `FSM_INPUT_EVENT_USER_ACTIVITY` | Branch presente, nessun producer diretto attivo | Gestione legacy attivita' utente come tipo puro. |
| `AGN_ID_FSM` via `fsm_handle_input_event()` | `FSM_INPUT_EVENT_KEY` + `ACTION_ID_USER_ACTIVITY` | Branch presente, helper producer non usato | Supporto pronto per futuro input tasto dedicato. |
| `AGN_ID_FSM` via `fsm_handle_input_event()` | `FSM_INPUT_EVENT_COIN` + `ACTION_ID_PAYMENT_ACCEPTED` | Branch presente, nessun producer diretto attivo | Supporto legacy per credito reale in forma `COIN`. |
| `AGN_ID_FSM` via `fsm_handle_input_event()` | `FSM_INPUT_EVENT_CREDIT_ENDED` + `ACTION_ID_CREDIT_ENDED` | Branch presente, nessun producer attivo | Permetterebbe di forzare l'azzeramento del credito. |
| `tasks_handle_digital_io_agent_event()` | `ACTION_ID_DIGITAL_IO_GET_OUTPUT` | Handler presente, producer mailbox oggi assente | La lettura output non passa piu' dalla mailbox. |
| `tasks_handle_digital_io_agent_event()` | `ACTION_ID_DIGITAL_IO_GET_INPUT` | Handler presente, producer mailbox oggi assente | La lettura input non passa piu' dalla mailbox. |
| `tasks_handle_digital_io_agent_event()` | `ACTION_ID_DIGITAL_IO_GET_SNAPSHOT` | Handler presente, producer mailbox oggi assente | Lo snapshot ingressi/uscite non passa piu' dalla mailbox. |
| Recipients `AGN_ID_LED`, `AGN_ID_LVGL`, `AGN_ID_WEB_UI` | `ACTION_ID_PROGRAM_PREFINE_CYCLO` | Messaggio pubblicato ma nessun receiver task attivo | La FSM lo emette, ma oggi non esiste un consumer mailbox attivo per questi destinatari. |

### Tipi definiti senza consumer mailbox attivo

| Tipo / action o famiglia | Stato attuale | Descrizione breve |
|---|---|---|
| `ACTION_ID_BUTTON_PRESSED` | Definito senza consumer mailbox attivo | Evento pulsante generico previsto ma non ancora gestito. |
| `ACTION_ID_SYSTEM_IDLE/RUN/ERROR` | Definiti senza consumer mailbox attivo | Eventi stato sistema previsti ma non ancora cablati. |
| Famiglia `ACTION_ID_GPIO_*` | Definita senza consumer mailbox attivo | Letture/scritture GPIO legacy previste ma senza handler mailbox dedicato attivo. |
| Famiglia `ACTION_ID_CCTALK_RX_DATA/TX_DATA/CONFIG/RESET` | Definita senza consumer mailbox attivo | La consumer CCtalk oggi gestisce solo `START/STOP/MASK`. |
| Famiglia `ACTION_ID_RS232_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a RS232. |
| Famiglia `ACTION_ID_RS485_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a RS485. |
| Famiglia `ACTION_ID_MDB_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a MDB. |
| Famiglia `ACTION_ID_IOEXP_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a I/O expander. |
| Famiglia `ACTION_ID_PWM_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a PWM. |
| Famiglia `ACTION_ID_LED_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a LED/barra LED. |
| Famiglia `ACTION_ID_SHT40_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a misura/errori SHT40. |
| Famiglia `ACTION_ID_USB_CDC_SCANNER_RX/TX/CONNECT/DISCONNECT` | Definita senza consumer mailbox attivo | Gli unici eventi scanner oggi osservabili via mailbox sono `READ`, `ON` e `OFF`. |
| Famiglia `ACTION_ID_SD_CARD_*` | Definita senza consumer mailbox attivo | Nessun receiver mailbox attivo dedicato a SD card. |

## Tabella messaggi attivi

| # | Messaggio descrittivo | Tipo / action | Mittente | Destinatari | Funzione che emette | Funzione di processo |
|---|---|---|---|---|---|---|
| 1 | Entrata in modalita' test LVGL | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_LVGL_TEST_ENTER` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `fsm_publish_control_event()` chiamata da `fsm_enter_lvgl_pages_test()` (usata da `lvgl_test_show_page()`) | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_ENTER_LVGL_TEST)` |
| 2 | Uscita da modalita' test LVGL | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_LVGL_TEST_EXIT` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `fsm_publish_control_event()` chiamata da `fsm_exit_lvgl_pages_test()` (usata da `api_display_lvgl_test()`) | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_EXIT_LVGL_TEST)` |
| 3 | Notifica PreFineCiclo | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_PROGRAM_PREFINE_CYCLO` | `AGN_ID_FSM` | `AGN_ID_LED`, `AGN_ID_LVGL`, `AGN_ID_WEB_UI` | `fsm_tick()` | Nessun consumer mailbox attivo trovato nella build corrente |
| 4 | Touch su pagina ADS LVGL per uscita da splash/ads | `type=FSM_INPUT_EVENT_TOUCH`, `action=ACTION_ID_USER_ACTIVITY` | `AGN_ID_LVGL` | `AGN_ID_FSM` | `switch_from_ads_async()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_USER_ACTIVITY)` |
| 5 | Selezione programma da pannello LVGL | `type=FSM_INPUT_EVENT_PROGRAM_SELECTED`, `action=ACTION_ID_PROGRAM_SELECTED` | `AGN_ID_LVGL` | `AGN_ID_FSM` | `on_prog_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_SELECTED` |
| 6 | Cambio programma a macchina gia' running/paused da pannello LVGL | `type=FSM_INPUT_EVENT_PROGRAM_SWITCH`, `action=ACTION_ID_PROGRAM_SELECTED` | `AGN_ID_LVGL` | `AGN_ID_FSM` | `on_prog_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_SWITCH` |
| 7 | Toggle pausa/ripresa programma attivo da pannello LVGL | `type=FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE`, `action=ACTION_ID_PROGRAM_PAUSE_TOGGLE` | `AGN_ID_LVGL` | `AGN_ID_FSM` | `on_prog_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` |
| 8 | Prima pressione STOP su pannello LVGL: richiesta pausa per conferma annullamento | `type=FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE`, `action=ACTION_ID_PROGRAM_PAUSE_TOGGLE` | `AGN_ID_LVGL` | `AGN_ID_FSM` | `on_stop_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` |
| 9 | Seconda pressione STOP su pannello LVGL: stop/annullamento programma | `type=FSM_INPUT_EVENT_PROGRAM_STOP`, `action=ACTION_ID_PROGRAM_STOP` | `AGN_ID_LVGL` | `AGN_ID_FSM` | `on_stop_btn()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_STOP` |
| 10 | Touch fisico touchscreen | `type=FSM_INPUT_EVENT_TOUCH`, `action=ACTION_ID_NONE` | `AGN_ID_TOUCH` | `AGN_ID_FSM` | `touchscreen_task()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `fsm_handle_event(FSM_EVENT_USER_ACTIVITY)` |
| 11 | Barcode letto da scanner USB e inoltrato a HTTP services | `type=FSM_INPUT_EVENT_QR_SCANNED`, `action=ACTION_ID_USB_CDC_SCANNER_READ` | `AGN_ID_USB_CDC_SCANNER` | `AGN_ID_HTTP_SERVICES` | `scanner_on_barcode_cb()` | `http_services_task()` -> lookup `http_services_getcustomers()` -> eventuale `publish_qr_credit_event()` |
| 12 | Credito QR pubblicato verso FSM dopo lookup backend | `type=FSM_INPUT_EVENT_QR_CREDIT`, `action=ACTION_ID_PAYMENT_ACCEPTED` | `AGN_ID_HTTP_SERVICES` | `AGN_ID_FSM` | `publish_qr_credit_event()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_QR_CREDIT` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 13 | Notifica pagamento servizio al backend quando un programma entra in RUNNING | `type=FSM_INPUT_EVENT_PROGRAM_SELECTED`, `action=ACTION_ID_PROGRAM_SELECTED` | `AGN_ID_FSM` | `AGN_ID_HTTP_SERVICES` | `publish_program_payment_event()` (chiamata da `tasks_fsm_task()` quando `CREDIT -> RUNNING`) | `http_services_task()` -> `http_services_payment()` |
| 14 | Comando digitale set output via agent I/O | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_DIGITAL_IO_SET_OUTPUT` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `tasks_dispatch_digital_io_agent_request()` chiamata da `tasks_digital_io_set_output_via_agent()`; call-site attivi: `web_ui_virtual_relay_control()` e branch `/api/test/dio_set` in `api_test_handler()` | `tasks_fsm_task()` -> `tasks_handle_digital_io_agent_event()` -> `tasks_execute_digital_io_action()` |
| 15 | Pagamento emulato da Web UI con sorgente cash/token | `type=FSM_INPUT_EVENT_TOKEN`, `action=ACTION_ID_PAYMENT_ACCEPTED` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `api_emulator_coin_post()` con `source != card/qr` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_TOKEN` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 16 | Pagamento emulato da Web UI con sorgente QR | `type=FSM_INPUT_EVENT_QR_CREDIT`, `action=ACTION_ID_PAYMENT_ACCEPTED` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `api_emulator_coin_post()` con `source=qr` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_QR_CREDIT` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 17 | Pagamento emulato da Web UI con sorgente card | `type=FSM_INPUT_EVENT_CARD_CREDIT`, `action=ACTION_ID_PAYMENT_ACCEPTED` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `api_emulator_coin_post()` con `source=card` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_CARD_CREDIT` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 18 | Avvio programma da emulatore Web UI | `type=FSM_INPUT_EVENT_PROGRAM_SELECTED`, `action=ACTION_ID_PROGRAM_SELECTED` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `api_emulator_program_start()` quando FSM non e' gia' in `RUNNING/PAUSED` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_SELECTED` |
| 19 | Cambio programma da emulatore Web UI a macchina gia' running/paused | `type=FSM_INPUT_EVENT_PROGRAM_SWITCH`, `action=ACTION_ID_PROGRAM_SELECTED` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `api_emulator_program_start()` quando FSM e' gia' in `RUNNING/PAUSED` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_SWITCH` |
| 20 | Stop programma da emulatore Web UI | `type=FSM_INPUT_EVENT_PROGRAM_STOP`, `action=ACTION_ID_PROGRAM_STOP` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `api_emulator_program_stop()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_STOP` |
| 21 | Pausa/ripresa programma da emulatore Web UI | `type=FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE`, `action=ACTION_ID_PROGRAM_PAUSE_TOGGLE` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | `api_emulator_program_pause_toggle()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE` |
| 22 | Moneta/credito da gettoniera CCtalk | `type=FSM_INPUT_EVENT_TOKEN`, `action=ACTION_ID_PAYMENT_ACCEPTED` | `AGN_ID_CCTALK` | `AGN_ID_FSM` | `cctalk_handle_buffered_credit()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> `FSM_INPUT_EVENT_TOKEN` -> `fsm_handle_event(FSM_EVENT_PAYMENT_ACCEPTED)` |
| 23 | Comando CCTalk START da Web UI test API | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_CCTALK_START` | `AGN_ID_WEB_UI` | `AGN_ID_CCTALK` | branch `cctalk_start` in `api_test_handler()` | `cctalk_task_run()` -> `cctalk_driver_start_acceptor()` |
| 24 | Comando CCTalk STOP da Web UI test API | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_CCTALK_STOP` | `AGN_ID_WEB_UI` | `AGN_ID_CCTALK` | branch `cctalk_stop` in `api_test_handler()` | `cctalk_task_run()` -> `cctalk_driver_stop_acceptor()` |
| 25 | Comando CCTalk MASK / abilita canali da Web UI test API | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_CCTALK_MASK` | `AGN_ID_WEB_UI` | `AGN_ID_CCTALK` | branch `cctalk_retention_ch1_2` in `api_test_handler()` | `cctalk_task_run()` -> `cctalk_driver_init()` + `cctalk_modify_master_inhibit_std()` + `cctalk_modify_inhibit_status()` |
| 26 | Scanner ON osservabile via mailbox | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_USB_CDC_SCANNER_ON` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | branch `scanner_on` in `api_test_handler()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> nessuna branch specifica attiva, evento oggi solo osservabile/loggabile |
| 27 | Scanner OFF osservabile via mailbox | `type=FSM_INPUT_EVENT_NONE`, `action=ACTION_ID_USB_CDC_SCANNER_OFF` | `AGN_ID_WEB_UI` | `AGN_ID_FSM` | branch `scanner_off` in `api_test_handler()` | `tasks_fsm_task()` -> `fsm_handle_input_event()` -> nessuna branch specifica attiva, evento oggi solo osservabile/loggabile |

## Note operative

- La mailbox e' implementata in `main/fsm.c` con publish in `fsm_event_publish()` e receive in `fsm_event_receive()`.
- Il consumer principale `AGN_ID_FSM` gira nel task FSM in `main/tasks.c`.
- Il consumer `AGN_ID_HTTP_SERVICES` riceve due famiglie di messaggi:
  - barcode scanner (`QR_SCANNED` / `USB_CDC_SCANNER_READ`)
  - notifica pagamento servizio (`PROGRAM_SELECTED` / `PROGRAM_SELECTED`)
- Il consumer `AGN_ID_CCTALK` gestisce oggi solo:
  - `ACTION_ID_CCTALK_START`
  - `ACTION_ID_CCTALK_STOP`
  - `ACTION_ID_CCTALK_MASK`
