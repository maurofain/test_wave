#pragma once

#include <stdint.h>

/* Pagine LVGL interne al componente */
void lvgl_page_main_show(void);
void lvgl_page_main_refresh_texts(void);
void lvgl_page_main_deactivate(void);

void lvgl_page_language_show(void);
void lvgl_page_language_2_show(void (*return_cb)(void));  ///< Mostra selezione lingua; return_cb = pagina da cui tornare
void lvgl_page_language_2_deactivate(void);               ///< Ferma timer lingua (sicuro se non attiva)
void lvgl_page_out_of_service_show(uint32_t reboots);
void lvgl_page_out_of_service_show_message(const char *message_key, const char *fallback_message);
void lvgl_page_out_of_service_show_reason(const char *reason_key,
										  const char *reason_fallback,
										  const char *agent_name);
void lvgl_page_ads_show(void);
void lvgl_page_ads_deactivate(void);
void lvgl_page_ads_preload_images(void);   ///< Pre-carica immagini SPIFFS fuori dal lock LVGL (evita blocco display)
void lvgl_page_ads_unload_images(void);
void lvgl_page_ads_set_error_message(const char *msg);