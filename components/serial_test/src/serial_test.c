#include "serial_test.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "SERIAL_TEST";

// Buffer per il monitoraggio delle risposte (semplificato)
#define MONITOR_BUF_SIZE 1024
static char s_monitor_buf[MONITOR_BUF_SIZE];
static int s_monitor_ptr = 0;
static SemaphoreHandle_t s_monitor_mux = NULL;

void serial_test_init(void) {
    if (!s_monitor_mux) s_monitor_mux = xSemaphoreCreateMutex();
}

static void add_to_monitor(const uint8_t *data, size_t len, const char *prefix) {
    if (xSemaphoreTake(s_monitor_mux, pdMS_TO_TICKS(100))) {
        for (size_t i = 0; i < len; i++) {
            s_monitor_ptr += snprintf(s_monitor_buf + s_monitor_ptr, MONITOR_BUF_SIZE - s_monitor_ptr, 
                                      "%s %02X ", prefix, data[i]);
            if (s_monitor_ptr > MONITOR_BUF_SIZE - 20) s_monitor_ptr = 0; // Wrap semplice
        }
        xSemaphoreGive(s_monitor_mux);
    }
}

size_t serial_test_parse_escape(const char *src, uint8_t *dest) {
    size_t d_idx = 0;
    for (size_t s_idx = 0; src[s_idx] != '\0'; ) {
        if (src[s_idx] == '\\' && src[s_idx+1] == '0' && src[s_idx+2] == 'x') {
            char hex[3] = { src[s_idx+3], src[s_idx+4], '\0' };
            dest[d_idx++] = (uint8_t)strtol(hex, NULL, 16);
            s_idx += 5;
        } else if (src[s_idx] == '\\' && src[s_idx+1] == 'r') {
            dest[d_idx++] = '\r'; s_idx += 2;
        } else if (src[s_idx] == '\\' && src[s_idx+1] == 'n') {
            dest[d_idx++] = '\n'; s_idx += 2;
        } else {
            dest[d_idx++] = src[s_idx++];
        }
    }
    return d_idx;
}

esp_err_t serial_test_send_uart(int port, const char *data_str) {
    uint8_t buf[256];
    size_t len = serial_test_parse_escape(data_str, buf);
    int written = uart_write_bytes(port, buf, len);
    if (written > 0) {
        add_to_monitor(buf, written, "TX>");
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t serial_test_read_uart(int port, uint8_t *out, size_t max_len, size_t *out_len) {
    int len = uart_read_bytes(port, out, max_len, pdMS_TO_TICKS(50));
    if (len > 0) {
        *out_len = len;
        add_to_monitor(out, len, "RX<");
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

const char* serial_test_get_monitor(void) {
    return s_monitor_buf;
}

void serial_test_clear_monitor(void) {
    memset(s_monitor_buf, 0, MONITOR_BUF_SIZE);
    s_monitor_ptr = 0;
}
