#pragma once

#include "lvgl.h"

void lvgl_page_chrome_add(lv_obj_t *scr);
void lvgl_page_chrome_remove(void);
void lvgl_page_chrome_preload_status_icons(void);
void lvgl_page_chrome_set_status_icon_state(uint8_t idx, bool ok);
void lvgl_page_chrome_set_status_icons(bool cloud_ok, bool card_ok, bool coin_ok, bool qr_ok);

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
