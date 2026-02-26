#pragma once

#include <stdint.h>
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

/**
 * @brief Mostra una schermata di blocco "Fuori servizio" a tutto schermo.
 *
 * Visualizza il messaggio in rosso con font 48 px (Montserrat) e il numero
 * di reboot consecutivi che hanno causato il blocco.
 * Va chiamata dopo che il display è già stato inizializzato.
 * Acquisisce internamente il lock LVGL.
 *
 * @param reboots Numero di reboot consecutivi rilevati (mostrato a video).
 */
void lvgl_panel_show_out_of_service(uint32_t reboots);
