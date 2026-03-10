#pragma once

#include <stdint.h>

/* Pagine LVGL interne al componente */
void lvgl_page_main_show(void);
void lvgl_page_main_refresh_texts(void);
void lvgl_page_main_deactivate(void);

void lvgl_page_language_show(void);
void lvgl_page_out_of_service_show(uint32_t reboots);
void lvgl_page_ads_show(void);
void lvgl_page_ads_deactivate(void);
void lvgl_page_ads_unload_images(void);

/* Nuova pagina programmi v2 */
void lvgl_page_programs_v2_show(void);
void lvgl_page_programs_v2_hide(void);
void lvgl_page_programs_v2_set_footer(const char *text);