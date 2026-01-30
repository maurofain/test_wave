#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

void serial_test_init(void);
size_t serial_test_parse_escape(const char *src, uint8_t *dest);
esp_err_t serial_test_send_uart(int port, const char *data_str);
esp_err_t serial_test_read_uart(int port, uint8_t *out, size_t max_len, size_t *out_len);
const char* serial_test_get_monitor(int port);
void serial_test_clear_monitor(int port);
