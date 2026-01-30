#include "serial_test.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "SERIAL_TEST";

// Buffer per il monitoraggio delle risposte (separati per porta)
#define MONITOR_BUF_SIZE 512
static char s_monitor_232[MONITOR_BUF_SIZE];
static int  s_ptr_232 = 0;
static char s_monitor_485[MONITOR_BUF_SIZE];
static int  s_ptr_485 = 0;
static char s_monitor_mdb[MONITOR_BUF_SIZE];
static int  s_ptr_mdb = 0;

static SemaphoreHandle_t s_monitor_mux = NULL;

void serial_test_init(void) {
    if (!s_monitor_mux) s_monitor_mux = xSemaphoreCreateMutex();
}

static void add_to_monitor(int port, const uint8_t *data, size_t len, const char *prefix) {
    if (!s_monitor_mux) return;
    if (xSemaphoreTake(s_monitor_mux, pdMS_TO_TICKS(100))) {
        char *buf;
        int *ptr;
        
        // Usiamo gli ID porta configurati
        if (port == CONFIG_APP_RS232_UART_PORT) { buf = s_monitor_232; ptr = &s_ptr_232; }
        else if (port == CONFIG_APP_RS485_UART_PORT) { buf = s_monitor_485; ptr = &s_ptr_485; }
        else if (port == CONFIG_APP_MDB_UART_PORT) { buf = s_monitor_mdb; ptr = &s_ptr_mdb; }
        else return; // Porta non supportata nel monitor

        for (size_t i = 0; i < len; i++) {
            // Logghiamo sia il carattere che il valore HEX separati da pipe o simili 
            // per permettere al JS di scegliere cosa visualizzare
            // Formato: [prefisso]|HEX|CHAR|
            char c = (isprint(data[i]) ? data[i] : '.');
            *ptr += snprintf(buf + *ptr, MONITOR_BUF_SIZE - *ptr, "%s|%02X|%c|", prefix, data[i], c);
            if (*ptr > MONITOR_BUF_SIZE - 40) *ptr = 0; 
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
        add_to_monitor(port, buf, (size_t)written, "TX>");
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t serial_test_read_uart(int port, uint8_t *out, size_t max_len, size_t *out_len) {
    int len = uart_read_bytes(port, out, max_len, pdMS_TO_TICKS(50));
    if (len > 0) {
        *out_len = (size_t)len;
        add_to_monitor(port, out, (size_t)len, "RX<");
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

const char* serial_test_get_monitor(int port) {
    if (port == CONFIG_APP_RS232_UART_PORT) return s_monitor_232;
    if (port == CONFIG_APP_RS485_UART_PORT) return s_monitor_485;
    if (port == CONFIG_APP_MDB_UART_PORT) return s_monitor_mdb;
    return "";
}

void serial_test_clear_monitor(int port) {
    if (!s_monitor_mux) return;
    if (xSemaphoreTake(s_monitor_mux, pdMS_TO_TICKS(100))) {
        if (port == CONFIG_APP_RS232_UART_PORT) { memset(s_monitor_232, 0, MONITOR_BUF_SIZE); s_ptr_232 = 0; }
        else if (port == CONFIG_APP_RS485_UART_PORT) { memset(s_monitor_485, 0, MONITOR_BUF_SIZE); s_ptr_485 = 0; }
        else if (port == CONFIG_APP_MDB_UART_PORT) { memset(s_monitor_mdb, 0, MONITOR_BUF_SIZE); s_ptr_mdb = 0; }
        xSemaphoreGive(s_monitor_mux);
    }
}
