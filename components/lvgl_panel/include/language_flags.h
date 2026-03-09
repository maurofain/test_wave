#pragma once

#include "lvgl.h"

extern const lv_image_dsc_t g_flag_it_24x16;
extern const lv_image_dsc_t g_flag_en_24x16;
extern const lv_image_dsc_t g_flag_de_24x16;
extern const lv_image_dsc_t g_flag_fr_24x16;
extern const lv_image_dsc_t g_flag_es_24x16;
const lv_image_dsc_t *get_flag_for_language(const char *lang_code);