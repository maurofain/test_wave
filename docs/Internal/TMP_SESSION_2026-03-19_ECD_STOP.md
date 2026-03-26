# Stato temporaneo sessione - 2026-03-19

## Obiettivo
Allineare la logica ECD/monete nella pagina programmi LVGL:
- Nessun messaggio ECD in `s_pause_lbl`
- Avviso ECD solo in `s_stop_btn` negli ultimi 60 secondi
- Touch area credito per reset timer/dismiss avviso

## File modificato
- `components/lvgl_panel/lvgl_page_programmi.c`

## Modifiche applicate
1. Aggiunto stato avviso:
   - `static bool s_ecd_warning_dismissed = false;`

2. Aggiunto callback touch area credito:
   - `on_credit_box_touch(lv_event_t *e)`
   - Azioni callback:
     - `mark_user_interaction();`
     - `s_ecd_warning_dismissed = true;`

3. Rimosso messaggio ECD da `s_pause_lbl`:
   - Eliminato il ramo:
     - `else if (snap->state == FSM_STATE_CREDIT && snap->ecd_coins > 0)`
   - `s_pause_lbl` mostra solo stato pausa (`s_tr_pause_fmt`)

4. Aggiunta logica avviso in `s_stop_btn`:
   - Nuove variabili locali:
     - `ecd_warning_active`
     - `ecd_rem_s`
     - `ecd_warning_threshold_s = 60U`
   - Avviso attivo solo se:
     - `snap->state == FSM_STATE_CREDIT`
     - `snap->ecd_coins > 0`
     - `!s_ecd_warning_dismissed`
     - `ecd_rem_s <= 60`
   - Testo avviso su due righe in `s_stop_lbl`:
     - `s_tr_ecd_expire_fmt`
     - `s_tr_ecd_touch_hint`

5. Resa cliccabile area credito in `build_status`:
   - `lv_obj_add_flag(s_credit_box, LV_OBJ_FLAG_CLICKABLE);`
   - `lv_obj_add_event_cb(s_credit_box, on_credit_box_touch, LV_EVENT_CLICKED, NULL);`

## Nota i18n
La stringa chiave `ecd_expire_line1_fmt` è caricata in:
- `panel_load_translations()` in `components/lvgl_panel/lvgl_page_programmi.c`

## Verifica da fare alla prossima sessione (su device)
1. Flash firmware:
   - `idfc -ff`
2. Test funzionale:
   - Inserire moneta
   - Verificare che in `s_pause_lbl` NON appaia "Il credito scadrà..."
   - Attendere ultimi 60s e verificare avviso in `s_stop_btn`
   - Toccare area credito e verificare reset timer + scomparsa avviso

## Stato finale
Modifiche salvate nel sorgente, pronte per test su hardware alla riapertura IDE.
