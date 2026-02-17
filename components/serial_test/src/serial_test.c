#include "serial_test.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static const char *TAG = "SERIAL_TEST";

// Buffer per il monitoraggio delle risposte (separati per porta)
#define MONITOR_BUF_SIZE 512
static char s_monitor_232[MONITOR_BUF_SIZE];
static int  s_ptr_232 = 0;
static char s_monitor_485[MONITOR_BUF_SIZE];
static int  s_ptr_485 = 0;
static char s_monitor_mdb[MONITOR_BUF_SIZE];
static int  s_ptr_mdb = 0;

// CCtalk monitor buffer (separato per chiarezza)
static char s_monitor_cctalk[MONITOR_BUF_SIZE];
static int  s_ptr_cctalk = 0;

static SemaphoreHandle_t s_monitor_mux = NULL;

void serial_test_init(void) {
    if (!s_monitor_mux) s_monitor_mux = xSemaphoreCreateMutex();
}

static void add_to_monitor(int port, const uint8_t *data, size_t len, const char *prefix) {
    if (!s_monitor_mux) serial_test_init();
    if (!s_monitor_mux) return;
    if (xSemaphoreTake(s_monitor_mux, pdMS_TO_TICKS(100))) {
        char *buf;
        int *ptr;
        
        // Usiamo gli ID porta configurati
        if (port == CONFIG_APP_RS232_UART_PORT) { buf = s_monitor_232; ptr = &s_ptr_232; }
        else if (port == CONFIG_APP_RS485_UART_PORT) { buf = s_monitor_485; ptr = &s_ptr_485; }
        else if (port == CONFIG_APP_MDB_UART_PORT) { buf = s_monitor_mdb; ptr = &s_ptr_mdb; }
        else { xSemaphoreGive(s_monitor_mux); return; } // Porta non supportata nel monitor

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

// Pubblica una voce di monitor per CCtalk (o etichetta custom)
void serial_test_push_monitor_entry(const char *label, const uint8_t *data, size_t len) {
    if (!s_monitor_mux) serial_test_init();
    if (!s_monitor_mux) return;
    if (xSemaphoreTake(s_monitor_mux, pdMS_TO_TICKS(100))) {
        char *buf = NULL;
        int *ptr = NULL;
        if (label && strcmp(label, "CCTALK") == 0) { buf = s_monitor_cctalk; ptr = &s_ptr_cctalk; }
        else { xSemaphoreGive(s_monitor_mux); return; }

        for (size_t i = 0; i < len; ++i) {
            char c = (isprint(data[i]) ? data[i] : '.');
            *ptr += snprintf(buf + *ptr, MONITOR_BUF_SIZE - *ptr, "R|%02X|%c|", data[i], c);
            if (*ptr > MONITOR_BUF_SIZE - 40) *ptr = 0;
        }
        xSemaphoreGive(s_monitor_mux);
    }
}

const char* serial_test_get_cctalk_monitor(void) {
    return s_monitor_cctalk;
}

void serial_test_clear_cctalk_monitor(void) {
    if (!s_monitor_mux) serial_test_init();
    if (!s_monitor_mux) return;
    if (xSemaphoreTake(s_monitor_mux, pdMS_TO_TICKS(100))) {
        memset(s_monitor_cctalk, 0, MONITOR_BUF_SIZE);
        s_ptr_cctalk = 0;
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
        add_to_monitor(port, buf, (size_t)written, "T");
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t serial_test_send_hex_uart(int port, const char *hex_str) {
    if (!hex_str) return ESP_ERR_INVALID_ARG;

    uint8_t buf[256];
    size_t len = 0;
    const char *cursor = hex_str;

    while (*cursor != '\0') {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',' || *cursor == ';') cursor++;
        if (*cursor == '\0') break;

        char token[16] = {0};
        size_t token_len = 0;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != ',' && *cursor != ';') {
            if (token_len < sizeof(token) - 1) token[token_len++] = *cursor;
            cursor++;
        }
        token[token_len] = '\0';

        const char *hex_ptr = token;
        if (hex_ptr[0] == '\\') hex_ptr++;
        if ((hex_ptr[0] == '0') && (hex_ptr[1] == 'x' || hex_ptr[1] == 'X')) hex_ptr += 2;
        if (*hex_ptr == '\0') return ESP_ERR_INVALID_ARG;

        char *endptr = NULL;
        long value = strtol(hex_ptr, &endptr, 16);
        if (*endptr != '\0' || value < 0 || value > 255) return ESP_ERR_INVALID_ARG;
        if (len >= sizeof(buf)) return ESP_ERR_INVALID_SIZE;

        buf[len++] = (uint8_t)value;
    }

    if (len == 0) return ESP_ERR_INVALID_ARG;

    int written = uart_write_bytes(port, buf, len);
    if (written > 0) {
        add_to_monitor(port, buf, (size_t)written, "T");
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t serial_test_read_uart(int port, uint8_t *out, size_t max_len, size_t *out_len) {
    int len = uart_read_bytes(port, out, max_len, pdMS_TO_TICKS(50));
    if (len > 0) {
        *out_len = (size_t)len;
        add_to_monitor(port, out, (size_t)len, "R");
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
    if (!s_monitor_mux) serial_test_init();
    if (!s_monitor_mux) return;
    if (xSemaphoreTake(s_monitor_mux, pdMS_TO_TICKS(100))) {
        if (port == CONFIG_APP_RS232_UART_PORT) { memset(s_monitor_232, 0, MONITOR_BUF_SIZE); s_ptr_232 = 0; }
        else if (port == CONFIG_APP_RS485_UART_PORT) { memset(s_monitor_485, 0, MONITOR_BUF_SIZE); s_ptr_485 = 0; }
        else if (port == CONFIG_APP_MDB_UART_PORT) { memset(s_monitor_mdb, 0, MONITOR_BUF_SIZE); s_ptr_mdb = 0; }
        xSemaphoreGive(s_monitor_mux);
    }
}
