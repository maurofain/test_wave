#include "cctalk.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define CCTALK_UART_BUF_SIZE 256

static int cctalk_uart_num = 1;

void cctalk_init(int uart_num, int tx_pin, int rx_pin, int baudrate) {
    cctalk_uart_num = uart_num;
    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(uart_num, &uart_config);
    uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(uart_num, CCTALK_UART_BUF_SIZE, CCTALK_UART_BUF_SIZE, 0, NULL, 0);
}

bool cctalk_send(uint8_t dest_addr, const uint8_t *data, uint8_t len) {
    uint8_t packet[260];
    packet[0] = dest_addr;
    packet[1] = len;
    memcpy(&packet[2], data, len);
    uint8_t checksum = cctalk_checksum(packet, len + 2);
    packet[len + 2] = checksum;
    int to_send = len + 3;
    int sent = uart_write_bytes(cctalk_uart_num, (const char *)packet, to_send);
    return sent == to_send;
}

bool cctalk_receive(uint8_t *src_addr, uint8_t *data, uint8_t *len, uint32_t timeout_ms) {
    uint8_t buf[260];
    int idx = 0;
    int64_t start = esp_timer_get_time();
    while (esp_timer_get_time() - start < timeout_ms * 1000) {
        int n = uart_read_bytes(cctalk_uart_num, &buf[idx], 1, 10 / portTICK_PERIOD_MS);
        if (n == 1) {
            idx++;
            if (idx >= 3) {
                uint8_t expected_len = buf[1];
                if (idx == expected_len + 3) {
                    uint8_t checksum = cctalk_checksum(buf, expected_len + 2);
                    if (checksum == buf[expected_len + 2]) {
                        *src_addr = buf[0];
                        *len = expected_len;
                        memcpy(data, &buf[2], expected_len);
                        return true;
                    }
                    return false;
                }
            }
        }
    }
    return false;
}

uint8_t cctalk_checksum(const uint8_t *packet, uint8_t len) {
    uint16_t sum = 0;
    for (int i = 0; i < len; ++i) sum += packet[i];
    return (uint8_t)(-sum);
}
