#pragma once

#include "lvgl.h"

void lvgl_page_chrome_add(lv_obj_t *scr);
void lvgl_page_chrome_remove(void);

/**
 * @brief Registra un callback per il click sulla bandiera del chrome.
 *
 * Deve essere chiamata PRIMA di lvgl_page_chrome_add().
 * Ogni pagina che vuole la bandiera cliccabile la imposta prima di add;
 * lvgl_page_chrome_remove() azzera automaticamente il callback.
 *
 * @param cb        LVGL event callback (NULL = bandiera non cliccabile)
 * @param user_data Puntatore passato al callback
 */
void lvgl_page_chrome_set_flag_callback(lv_event_cb_t cb, void *user_data);

/**
 * [M] @brief Aggiorna il colore di uno dei 4 indicatori di stato periferiche.
 *
 * Gli indici rappresentano:
 * 0 = Network connection (bianco se OK, rosso se KO)
 * 1 = USB Scanner (bianco se OK, rosso se KO)
 * 2 = CCTALK coin acceptor (bianco se OK, rosso se KO)
 * 3 = MDB (bianco se OK, rosso se KO)
 *
 * @param index Indice dell'indicatore (0-3)
 * @param is_ok true = bianco, false = rosso
 */
void lvgl_page_chrome_update_status_indicator(int index, bool is_ok);
