#pragma once

#include "lvgl.h"

const char *get_flag_path_for_language(const char *lang_code);
const lv_image_dsc_t *get_flag_bitmap_for_language(const char *lang_code);
const void *get_flag_src_for_language(const char *lang_code);