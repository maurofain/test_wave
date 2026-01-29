#pragma once
#include "esp_err.h"
#include <stddef.h>

esp_err_t rs485_init(void);
int rs485_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms);
int rs485_send(const uint8_t *data, size_t len);
