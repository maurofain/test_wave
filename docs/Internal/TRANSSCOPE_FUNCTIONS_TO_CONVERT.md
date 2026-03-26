  Elenco delle operazioni trans‑scope trovate (origine → destinazione), cosa fanno, stato attuale e raccomandazione (uso mailbox FSM):

   1. components/web_ui/web_ui_test_api.c

    - Origine: Web UI (HTTP test API) → Destinazione: components/cctalk/cctalk_driver.c
    - Chiamate dirette: start/stop/inhibit dell'accettore CCtalk (comandi test).
    - Stato: chiamate dirette dal thread HTTP (rischio di blocco e concorrenza sul driver).
    - Raccomandazione: pubblicare eventi ACTION_CCTALK_START / ACTION_CCTALK_STOP verso AGN_ID_CCTALK; il CCTALK module esegue i comandi nel suo task.
   2. components/web_ui/web_ui.c (dispatcher endpoints)

    - Origine: HTTP worker → Destinazione: LVGL (components/lvgl_panel/*.c)
    - Chiamate dirette: lvgl_panel_show_*(), lvgl_panel_set_*() per mostrare pagine/aggiornare UI.
    - Stato: HTTP thread richiama UI direttamente (possibili chiamate fuori dal thread UI).
    - Raccomandazione: pubblicare ACTION_UI_SHOW_PAGE / ACTION_UI_SET_STATUS; UI subscriber esegue lv_async_call o gestisce internamente.
   3. components/lvgl_panel/lvgl_page_*.c (es. lvgl_page_programmi.c)

    - Origine: UI pages → Destinazione: main / cctalk (main_cctalk_* functions)
    - Chiamate dirette: main_cctalk_send_initialization_sequence(_async)
    - Stato: UI lancia sequenze CCTALK (diretto).
    - Raccomandazione: UI pubblica ACTION_CCTALK_INIT_SEQ; CCTALK handler esegue sequenza in proprio contesto.
   4. main/init.c

    - Origine: Init task → Destinazione: LVGL / Web UI / Driver
    - Chiamate dirette: lvgl_panel_set_init_status(), web_ui_init(), cctalk_driver_init(), ecc.
    - Stato: init aggiorna UI e avvia driver direttamente (alcune sono accettabili in init; le update UI dovrebbero essere eventi).
    - Raccomandazione: mantenere init→driver init sincrono quando necessario; trasformare update UI in ACTION_UI_SET_INIT_STATUS events.
   5. main/main.c (boot path)

    - Origine: main → Destinazione: cctalk_driver
    - Chiamata precedente: main_cctalk_send_initialization_sequence() (blocking) — già sostituita con wrapper asincrono (main_cctalk_start_acceptor_async()).
    - Stato: fix parziale già applicato.
    - Raccomandazione: considerare uniformare a evento ACTION_CCTALK_START per coerenza.
   6. main/fsm.c / main/tasks.c

    - Origine: FSM / tasks → Destinazione: web_ui (virtual relay)
    - Chiamate dirette: web_ui_virtual_relay_control(relay, status, duration)
    - Stato: FSM gestisce side‑effects direttamente su web UI.
    - Raccomandazione: pubblicare ACTION_RELAY_CONTROL (to=AGN_ID_WEB_UI); web_ui handler esegue la pulsantiera virtuale.
   7. components/cctalk/cctalk_driver.c

    - Origine: driver → Destinazione: FSM
    - Comportamento: pubblica eventi di credito/coin tramite fsm_event_publish(...) — già corretto (pattern desiderato).
    - Raccomandazione: mantenere questo pattern; estendere schema eventi per comandi in ingresso.
   8. components/serial_test/src/serial_test.c

    - Origine: vari moduli → Destinazione: buffer monitor seriale
    - Chiamate dirette: serial_test_push_monitor_action / _entry (usato per logging/monitor)
    - Stato: thread‑safe tramite mutex; non usa mailbox/event bus.
    - Raccomandazione: va bene per monitor locale; se serve che altri moduli "ascoltino" il monitor in tempo reale, esporre come evento ACTION_SERIAL_MONITOR.
   9. Altri test modules (led_test, pwm_test, rs232_test, modbus APIs)

    - Origine: web_ui_test_api.c → Destinazione: moduli test/hardware
    - Stato: molte chiamate test rimangono dirette (accettabile per test ma sono trans‑scope).
    - Raccomandazione: convertire i comandi di controllo hardware esposti via HTTP in eventi (priorità più bassa per test, alta per hardware produttivo).

  Priorità di conversione (consigliata)

   - Alta: chiamate HTTP → CCTALK (hardware control), UI → CCTALK (evitare blocchi/race).
   - Media: FSM → Web UI (relay control), Init → UI (status updates).
   - Bassa: Test APIs → hardware (se sono solo per debug, conversione opzionale).