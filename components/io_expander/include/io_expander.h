#pragma once
#include "esp_err.h"
#include <stdint.h>

esp_err_t io_expander_init(void);
esp_err_t io_expander_write_output(uint16_t mask);
