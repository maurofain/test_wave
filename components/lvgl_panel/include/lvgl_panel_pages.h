#pragma once

#include <stdint.h>

/* Pagine LVGL interne al componente */
void lvgl_page_main_show(void);
void lvgl_page_main_refresh_texts(void);
void lvgl_page_main_deactivate(void);

void lvgl_page_language_show(void);
void lvgl_page_out_of_service_show(uint32_t reboots);
