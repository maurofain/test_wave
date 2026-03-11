#pragma once

#include <stdint.h>
/**
 * @file lvgl_panel.h
 * @brief Pannello LVGL emulatore per display 7" 720×1280 verticale.
 *
 * Mostra:
 *  - Splash iniziale con logo `MicroHard` centrato
 *  - Landing `select_language` con 5 pulsanti lingua (bandiera + nome)
 *  - Schermata principale a 4 colonne:
 *    | programmi 1-5 | counter | barra tempo | programmi 6-10 |
 *  - Aggiornamento runtime da snapshot FSM ogni 700 ms
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
 * @brief Mostra la splash iniziale con logo MicroHard al centro schermo.
 *
 * La schermata resta visibile finché non viene esplicitamente sostituita
 * da `lvgl_panel_show_language_select()`.
 */
void lvgl_panel_show_boot_logo(void);

/**
 * @brief Mostra la schermata di selezione lingua.
 *
 * Tipicamente chiamata al termine della finestra di stabilità del boot.
 */
void lvgl_panel_show_language_select(void);

/* Aggiorna i testi visibili in LVGL dopo un cambio lingua */
void lvgl_panel_refresh_texts(void);

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

/**
 * @brief Mostra la schermata principale.
 */
void lvgl_panel_show_main_page(void);

/**
 * @brief Mostra la schermata degli annunci.
 */
void lvgl_panel_show_ads_page(void);

/**
 * @brief Aggiorna la label di stato init mostrata nella schermata boot logo.
 *
 * Se la schermata logo non è attiva, la chiamata viene ignorata.
 *
 * @param text Testo da mostrare nella label.
 */
void lvgl_panel_set_init_status(const char *text);
