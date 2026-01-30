#pragma once
#include "esp_err.h"

esp_err_t led_test_start(void);
esp_err_t led_test_stop(void);
esp_err_t led_test_set_brightness(uint8_t brightness);
esp_err_t led_test_set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
