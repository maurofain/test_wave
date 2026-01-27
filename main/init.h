#pragma once

#include "esp_err.h"
#include "led_strip.h"

esp_err_t init_run_factory(void);
led_strip_handle_t init_get_ws2812_handle(void);
