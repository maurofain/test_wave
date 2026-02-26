#pragma once

/**
 * @file lvgl_panel.h
 * @brief Pannello LVGL emulatore per display 7" 800×1280 verticale.
 *
 * Mostra:
 *  - Header con titolo e orologio
 *  - Box stato/credito con colore in base a stato FSM
 *  - 8 pulsanti programma (2 colonne × 4 righe)
 *  - Sezione inserimento credito: QR, Tessera, Monete
 *  - Area messaggi coda FSM aggiornata ogni 700 ms
 *
 * I pulsanti pubblicano sulla coda FSM gli stessi eventi del pannello web
 * emulatore (ACTION_ID_PAYMENT_ACCEPTED, ACTION_ID_PROGRAM_SELECTED, …).
 *
 * Chiamare lvgl_panel_show() una volta, dopo bsp_display_start_with_config(),
 * all'interno del lock LVGL oppure in un contesto dove il BSP è già inizializzato.
 */

/**
 * @brief Costruisce e visualizza il pannello emulatore sullo schermo attivo.
 *
 * Sostituisce lvgl_show_minimal_screen(); va chiamata una sola volta.
 * Acquisisce internamente il lock LVGL.
 */
void lvgl_panel_show(void);
