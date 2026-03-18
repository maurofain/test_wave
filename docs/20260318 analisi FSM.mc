  Relay

   - components/web_ui/include/web_ui_programs.h
    - OK: la tabella programma contiene duration_sec, pause_max_suspend_sec, relay_mask.
   - components/lvgl_panel/lvgl_page_programmi.c:273
    - OK: apply_program_relays() attiva i relay previsti da relay_mask per duration_sec.
   - components/lvgl_panel/lvgl_page_programmi.c:264
    - OK: clear_all_program_relays() spegne tutto.
   - components/web_ui/web_ui_programs.c:363
    - ATTENZIONE: web_ui_virtual_relay_control() è uno stub virtuale, non hardware reale.
   - main/tasks.c:567
    - OK: quando si esce da RUNNING/PAUSED verso CREDIT, i relay vengono forzati OFF.

  Pause

   - main/fsm.c:394
    - OK: PROGRAM_PAUSE porta a PAUSED.
   - main/fsm.c:687
    - OK: in pausa conteggia pause_elapsed_ms; a scadenza pause_max_ms fa STOP.
   - components/lvgl_panel/lvgl_page_programmi.c:710
    - OK: premere lo stesso programma fa toggle pausa/ripresa.
   - components/lvgl_panel/lvgl_page_programmi.c:731
    - OK: in pausa spegne i relay; in resume li riapplica.

  Stop

   - components/lvgl_panel/lvgl_page_programmi.c:795
    - PARZIALE: lo STOP UI è a doppia pressione: prima pausa+conferma, seconda stop vero.
   - main/fsm.c:400
    - OK: PROGRAM_STOP resetta il runtime.
   - main/fsm.c:402
    - OK: sessione VIRTUAL_LOCKED chiude subito.
   - main/fsm.c:405
    - OK: sessione aperta torna a CREDIT.

  Timeout

   - components/device_config/include/device_config.h:84
    - OK: exit_programs_ms = timeout inattività in scelta programmi.
   - main/tasks.c:492
    - OK: exit_programs_ms caricato in fsm.splash_screen_time_ms.
   - main/fsm.c:698
    - OK: in CREDIT, al timeout scatta FSM_EVENT_TIMEOUT.
   - main/fsm.c:373
    - OK: su timeout, se c’è ecd azzera solo l’effettivo; se resta solo virtuale chiude sessione.
   - components/device_config/include/device_config.h:88
    - ATTENZIONE: credit_reset_timeout_ms esiste, ma in questo flusso qui non è il timeout principale usato per il ritorno da CREDIT; quello operativo è exit_programs_ms.

  Esito

  Flusso relay/pause/stop/timeout: implementato ma con 2 caveat.

   1. I relay oggi sono gestiti tramite virtual relay stub.
   2. Lo STOP reale lato UI richiede conferma a due pressioni, quindi non è identico al testo funzionale.

