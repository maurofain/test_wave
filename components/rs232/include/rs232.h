#pragma once
#include "esp_err.h"
#include <stddef.h>
#include "hw_common.h"

esp_err_t rs232_init(void);
esp_err_t rs232_deinit(void);
int rs232_receive(uint8_t *data, size_t max_len, uint32_t timeout_ms);
int rs232_send(const uint8_t *data, size_t len);
hw_component_status_t rs232_get_status(void);
