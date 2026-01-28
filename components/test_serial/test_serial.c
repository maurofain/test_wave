#include "test_serial.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "TEST_SERIAL";

size_t test_serial_parse_escapes(const char* input, uint8_t* output, size_t max_len) {
    size_t len = 0;
    const char* p = input;
    while (*p && len < max_len) {
        if (*p == '\\') {
            p++;
            if (*p == 'x') { // \x41
                p++;
                char hex[3] = {p[0], p[1], 0};
                output[len++] = (uint8_t)strtol(hex, NULL, 16);
                p += 2;
            } else if (*p == '0' && *(p+1) == 'x') { // \0x41
                p += 2;
                char hex[3] = {p[0], p[1], 0};
                output[len++] = (uint8_t)strtol(hex, NULL, 16);
                p += 2;
            } else if (*p == 'r') {
                output[len++] = '\r'; p++;
            } else if (*p == 'n') {
                output[len++] = '\n'; p++;
            } else {
                output[len++] = *p++;
            }
        } else {
            output[len++] = *p++;
        }
    }
    return len;
}

esp_err_t test_serial_send(uart_port_t port, const char* hex_or_str) {
    uint8_t buf[256];
    size_t len = test_serial_parse_escapes(hex_or_str, buf, sizeof(buf));
    if (len > 0) {
        uart_write_bytes(port, buf, len);
        ESP_LOGI(TAG, "Sent %d bytes to UART %d", (int)len, port);
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

int test_serial_read_response(uart_port_t port, char* out_hex, size_t max_out) {
    uint8_t rx_buf[128];
    int len = uart_read_bytes(port, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(100));
    if (len > 0) {
        int pos = 0;
        for (int i = 0; i < len && (pos + 4) < max_out; i++) {
            pos += snprintf(out_hex + pos, max_out - pos, "%02X ", rx_buf[i]);
        }
        return len;
    }
    return 0;
}
