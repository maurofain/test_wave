#pragma once

#include "esp_err.h"
#include "driver/uart.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Parsa una stringa gestendo sequenze di escape (\xHH, \0xHH, \r, \n)
 */
size_t test_serial_parse_escapes(const char* input, uint8_t* output, size_t max_len);

/**
 * @brief Invia dati su una porta UART decodificando gli escape
 */
esp_err_t test_serial_send(uart_port_t port, const char* hex_or_str);

/**
 * @brief Legge la risposta e la formatta come stringa HEX
 */
int test_serial_read_response(uart_port_t port, char* out_hex, size_t max_out);
