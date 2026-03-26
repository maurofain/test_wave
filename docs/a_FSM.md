# FSM — Specifica operativa aggiornata

Documento allineato al comportamento reale implementato in `main/fsm.h` e `main/fsm.c`.

## 1) Stati disponibili

La FSM usa i seguenti stati runtime:

1. `FSM_STATE_IDLE`
2. `FSM_STATE_ADS`
3. `FSM_STATE_CREDIT`
4. `FSM_STATE_RUNNING`
5. `FSM_STATE_PAUSED`
6. `FSM_STATE_OUT_OF_SERVICE`
7. `FSM_STATE_LVGL_PAGES_TEST`

## 2) Eventi principali di ingresso

Ingressi tipici verso `fsm_handle_input_event(...)`:

- attività utente: `FSM_INPUT_EVENT_TOUCH`, `FSM_INPUT_EVENT_KEY`, `ACTION_ID_USER_ACTIVITY`
- pagamenti: `FSM_INPUT_EVENT_TOKEN`, `FSM_INPUT_EVENT_QR_CREDIT`, `FSM_INPUT_EVENT_CARD_CREDIT`, `ACTION_ID_PAYMENT_ACCEPTED`
- programma: `FSM_INPUT_EVENT_PROGRAM_SELECTED`, `FSM_INPUT_EVENT_PROGRAM_SWITCH`, `FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE`, `FSM_INPUT_EVENT_PROGRAM_STOP`
- servizio: `ACTION_ID_LVGL_TEST_ENTER`, `ACTION_ID_LVGL_TEST_EXIT`
- stato sistema/OOS: `ACTION_ID_SYSTEM_ERROR`, `ACTION_ID_SYSTEM_RUN`

Nota: è attivo il bridge `action -> type` per compatibilità durante la migrazione.

## 3) Transizioni stato (sintesi)

### `IDLE` / `ADS`
- su attività utente o pagamento: transizione a `CREDIT`

### `CREDIT`
- su selezione programma valida: transizione a `RUNNING`
- su timeout inattività (`splash_screen_time_ms`):
  - trattiene ECD, azzera VCD
  - va in `ADS` se `ads_enabled=true`, altrimenti `IDLE`

### `RUNNING`
- su pausa: `PAUSED`
- su stop: `CREDIT` se rimane credito, altrimenti `IDLE`
- a fine ciclo:
  - tenta auto-rinnovo se credito disponibile e stop-fine-ciclo non richiesto
  - se credito esaurito forza `IDLE`

### `PAUSED`
- su toggle pausa: `RUNNING`
- su stop: `CREDIT` o `IDLE` come da disponibilità credito
- su timeout pausa (`pause_max_ms`): ripresa automatica in `RUNNING`

### `OUT_OF_SERVICE`
- stato bloccante: gli eventi normali sono ignorati
- uscita solo con `ACTION_ID_SYSTEM_RUN`

### `LVGL_PAGES_TEST`
- ingresso/uscita solo con `ACTION_ID_LVGL_TEST_ENTER/EXIT`
- durante il test gli eventi normali non guidano transizioni applicative

## 4) Regole credito/sessione

- Sessioni `OPEN_PAYMENTS`: consentono pagamenti aggiuntivi (coin/token).
- Sessioni `VIRTUAL_LOCKED`: tipicamente QR/Card, bloccano pagamenti aggiuntivi non consentiti.
- Il costo programma scala prima ECD poi VCD (`fsm_try_charge_program_cycle`).
- A fine timeout in `CREDIT` vengono mantenuti solo i crediti ECD.

## 5) OOS (Out Of Service) — comportamento corrente

- Ingresso OOS: evento con `ACTION_ID_SYSTEM_ERROR`.
- Uscita OOS: evento con `ACTION_ID_SYSTEM_RUN`.
- Questi due eventi sono usati nel task FSM come **iniezione diretta** su `fsm_handle_input_event(...)` (non tramite mailbox).
- Dopo uscita OOS il task applicativo tenta la riabilitazione runtime di:
  - gettoniera CCTALK (`ACTION_ID_CCTALK_START`)
  - scanner USB CDC (`setup` + `on`)

## 6) Tick periodico (`fsm_tick`)

Ogni tick aggiorna la logica temporale:

- inattività generale (`inactivity_ms`)
- countdown running (`running_elapsed_ms`)
- timeout pausa (`pause_elapsed_ms`)
- soglia `PreFineCiclo` con pubblicazione evento `ACTION_ID_PROGRAM_PREFINE_CYCLO`

## 7) Mailbox vs chiamata diretta

- Percorso mailbox standard: `fsm_event_publish(...)` -> `fsm_event_receive(...)` -> `fsm_handle_input_event(...)`.
- Percorso diretto: usato per alcune transizioni di controllo interne (es. `SYSTEM_ERROR`/`SYSTEM_RUN` nel flusso OOS).

## 8) Nomi programma e payload evento

- I nomi programma usati in `event.text` arrivano dalla tabella programmi runtime (`/spiffs/programs.json`, seed `data/programs.json`).
- Non sono più basati su placeholder `__i18n__` in fase di composizione evento.
