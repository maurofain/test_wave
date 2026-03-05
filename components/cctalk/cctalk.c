#include "cctalk.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

#ifndef DNA_CCTALK
#define DNA_CCTALK 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_CCTALK == 0

#define CCTALK_UART_BUF_SIZE 512
#define CCTALK_FRAME_MAX_LEN 260

static const char *TAG = "CCTALK";
static int s_cctalk_uart_num = 1;
static bool s_cctalk_ready = false;
static SemaphoreHandle_t s_cctalk_lock = NULL;

extern void serial_test_push_monitor_entry(const char *label, const uint8_t *data, size_t len);

static TickType_t cctalk_ms_to_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        return 0;
    }
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if (ticks == 0) {
        ticks = 1;
    }
    return ticks;
}

static bool cctalk_lock_take(uint32_t timeout_ms)
{
    if (!s_cctalk_lock) {
        return false;
    }
    return (xSemaphoreTake(s_cctalk_lock, cctalk_ms_to_ticks(timeout_ms)) == pdTRUE);
}

static void cctalk_lock_give(void)
{
    if (s_cctalk_lock) {
        xSemaphoreGive(s_cctalk_lock);
    }
}

static void cctalk_monitor_frame(const char *label, const cctalk_frame_t *frame)
{
    if (!frame) {
        return;
    }

    uint16_t frame_len = (uint16_t)frame->data_len + 5U;
    if (frame_len > CCTALK_FRAME_MAX_LEN) {
        return;
    }

    uint8_t raw[CCTALK_FRAME_MAX_LEN] = {0};
    raw[0] = frame->destination;
    raw[1] = frame->data_len;
    raw[2] = frame->source;
    raw[3] = frame->header;
    if (frame->data_len > 0U) {
        memcpy(&raw[4], frame->data, frame->data_len);
    }
    raw[frame_len - 1U] = cctalk_checksum(raw, (uint8_t)(frame_len - 1U));

    serial_test_push_monitor_entry(label, raw, frame_len);
}

static bool cctalk_send_frame_unlocked(uint8_t dest_addr,
                                       uint8_t src_addr,
                                       uint8_t header,
                                       const uint8_t *data,
                                       uint8_t len,
                                       bool monitor_tx)
{
    if (!s_cctalk_ready) {
        return false;
    }

    uint16_t frame_len = (uint16_t)len + 5U;
    uint8_t packet[CCTALK_FRAME_MAX_LEN] = {0};

    packet[0] = dest_addr;
    packet[1] = len;
    packet[2] = src_addr;
    packet[3] = header;
    if (len > 0U && data) {
        memcpy(&packet[4], data, len);
    }
    packet[frame_len - 1U] = cctalk_checksum(packet, (uint8_t)(frame_len - 1U));

    int sent = uart_write_bytes(s_cctalk_uart_num, (const char *)packet, (size_t)frame_len);
    if (sent != (int)frame_len) {
        return false;
    }

    (void)uart_wait_tx_done(s_cctalk_uart_num, pdMS_TO_TICKS(50));

    if (monitor_tx) {
        serial_test_push_monitor_entry("CCTALK_TX", packet, frame_len);
    }

    return true;
}

static bool cctalk_receive_frame_unlocked(cctalk_frame_t *frame, uint32_t timeout_ms, bool monitor_rx)
{
    if (!s_cctalk_ready || !frame) {
        return false;
    }

    uint8_t raw[CCTALK_FRAME_MAX_LEN] = {0};
    uint16_t idx = 0;
    uint16_t expected_len = 0;
    int64_t deadline_us = esp_timer_get_time() + ((int64_t)timeout_ms * 1000LL);

    while (esp_timer_get_time() < deadline_us) {
        uint8_t byte = 0;
        int read = uart_read_bytes(s_cctalk_uart_num, &byte, 1, pdMS_TO_TICKS(10));
        if (read != 1) {
            continue;
        }

        if (idx >= CCTALK_FRAME_MAX_LEN) {
            idx = 0;
            expected_len = 0;
        }

        raw[idx++] = byte;

        if (idx == 2U) {
            expected_len = (uint16_t)raw[1] + 5U;
            if (expected_len < 5U || expected_len > CCTALK_FRAME_MAX_LEN) {
                idx = 0;
                expected_len = 0;
            }
            continue;
        }

        if (expected_len > 0U && idx == expected_len) {
            uint8_t computed = cctalk_checksum(raw, (uint8_t)(expected_len - 1U));
            uint8_t received = raw[expected_len - 1U];
            if (computed != received) {
                idx = 0;
                expected_len = 0;
                continue;
            }

            frame->destination = raw[0];
            frame->data_len = raw[1];
            frame->source = raw[2];
            frame->header = raw[3];
            if (frame->data_len > 0U) {
                memcpy(frame->data, &raw[4], frame->data_len);
            }

            if (monitor_rx) {
                serial_test_push_monitor_entry("CCTALK_RX", raw, expected_len);
            }
            return true;
        }
    }

    return false;
}

static bool cctalk_copy_ascii_response(const cctalk_frame_t *response, char *out, size_t out_len)
{
    if (!response || !out || out_len == 0U || response->data_len == 0U) {
        return false;
    }

    size_t copy_len = response->data_len;
    if (copy_len >= out_len) {
        copy_len = out_len - 1U;
    }

    memcpy(out, response->data, copy_len);
    out[copy_len] = '\0';

    for (size_t i = 0; i < copy_len; ++i) {
        unsigned char ch = (unsigned char)out[i];
        if (ch < 32U || ch > 126U) {
            out[i] = '.';
        }
    }

    return true;
}

static bool cctalk_expect_ack(const cctalk_frame_t *response)
{
    return (response && response->header == 0U);
}

void cctalk_init(int uart_num, int tx_pin, int rx_pin, int baudrate)
{
    if (!s_cctalk_lock) {
        s_cctalk_lock = xSemaphoreCreateMutex();
    }

    if (s_cctalk_uart_num != uart_num && uart_is_driver_installed(s_cctalk_uart_num)) {
        (void)uart_driver_delete(s_cctalk_uart_num);
    }

    s_cctalk_uart_num = uart_num;

    if (uart_is_driver_installed(s_cctalk_uart_num)) {
        (void)uart_driver_delete(s_cctalk_uart_num);
    }

    uart_config_t uart_config = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_param_config(s_cctalk_uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] uart_param_config fallita: %s", esp_err_to_name(err));
        s_cctalk_ready = false;
        return;
    }

    err = uart_set_pin(s_cctalk_uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] uart_set_pin fallita: %s", esp_err_to_name(err));
        s_cctalk_ready = false;
        return;
    }

    err = uart_driver_install(s_cctalk_uart_num, CCTALK_UART_BUF_SIZE, CCTALK_UART_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[C] uart_driver_install fallita: %s", esp_err_to_name(err));
        s_cctalk_ready = false;
        return;
    }

    (void)uart_flush_input(s_cctalk_uart_num);
    s_cctalk_ready = true;
}

bool cctalk_send_frame(uint8_t dest_addr,
                       uint8_t src_addr,
                       uint8_t header,
                       const uint8_t *data,
                       uint8_t len)
{
    if (!cctalk_lock_take(50)) {
        return false;
    }

    bool ok = cctalk_send_frame_unlocked(dest_addr, src_addr, header, data, len, true);
    cctalk_lock_give();
    return ok;
}

bool cctalk_receive_frame(cctalk_frame_t *frame, uint32_t timeout_ms)
{
    if (!cctalk_lock_take(timeout_ms)) {
        return false;
    }

    bool ok = cctalk_receive_frame_unlocked(frame, timeout_ms, true);
    cctalk_lock_give();
    return ok;
}

bool cctalk_command(uint8_t dest_addr,
                    uint8_t src_addr,
                    uint8_t header,
                    const uint8_t *data,
                    uint8_t len,
                    cctalk_frame_t *response,
                    uint32_t timeout_ms)
{
    if (!cctalk_lock_take(timeout_ms)) {
        return false;
    }

    (void)uart_flush_input(s_cctalk_uart_num);

    bool monitor_traffic = (header != 229U);
    bool tx_ok = cctalk_send_frame_unlocked(dest_addr, src_addr, header, data, len, monitor_traffic);
    if (!tx_ok) {
        cctalk_lock_give();
        return false;
    }

    int64_t start_us = esp_timer_get_time();
    int64_t timeout_us = (int64_t)timeout_ms * 1000LL;

    while ((esp_timer_get_time() - start_us) < timeout_us) {
        int64_t elapsed_us = esp_timer_get_time() - start_us;
        uint32_t remaining_ms = (uint32_t)((timeout_us - elapsed_us) / 1000LL);
        if (remaining_ms == 0U) {
            remaining_ms = 1U;
        }

        cctalk_frame_t rx = {0};
        if (!cctalk_receive_frame_unlocked(&rx, remaining_ms, false)) {
            continue;
        }

        if (rx.source == src_addr && rx.destination == dest_addr) {
            continue;
        }

        if (rx.source != dest_addr || rx.destination != src_addr) {
            continue;
        }

        if (monitor_traffic) {
            cctalk_monitor_frame("CCTALK_RX", &rx);
        }

        if (response) {
            *response = rx;
        }

        cctalk_lock_give();
        return true;
    }

    cctalk_lock_give();
    return false;
}

bool cctalk_send(uint8_t dest_addr, const uint8_t *data, uint8_t len)
{
    if (!data || len == 0U) {
        return false;
    }

    uint8_t header = data[0];
    const uint8_t *payload = (len > 1U) ? &data[1] : NULL;
    uint8_t payload_len = (len > 1U) ? (uint8_t)(len - 1U) : 0U;
    return cctalk_send_frame(dest_addr, CCTALK_MASTER_ADDRESS, header, payload, payload_len);
}

bool cctalk_receive(uint8_t *src_addr, uint8_t *data, uint8_t *len, uint32_t timeout_ms)
{
    if (!src_addr || !data || !len) {
        return false;
    }

    cctalk_frame_t frame = {0};
    if (!cctalk_receive_frame(&frame, timeout_ms)) {
        return false;
    }

    *src_addr = frame.source;
    *len = (uint8_t)(frame.data_len + 1U);
    data[0] = frame.header;
    if (frame.data_len > 0U) {
        memcpy(&data[1], frame.data, frame.data_len);
    }
    return true;
}

uint8_t cctalk_checksum(const uint8_t *packet, uint8_t len)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; ++i) {
        sum = (uint16_t)(sum + packet[i]);
    }
    return (uint8_t)(-sum);
}

bool cctalk_address_poll(uint8_t dest_addr, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 254U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}

bool cctalk_request_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!status) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 248U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 1U) {
        return false;
    }
    *status = response.data[0];
    return true;
}

bool cctalk_request_manufacturer_id(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 246U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}

bool cctalk_request_equipment_category(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 245U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}

bool cctalk_request_product_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 244U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}

bool cctalk_request_serial_number(uint8_t dest_addr, uint8_t serial[3], uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!serial) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 242U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 3U) {
        return false;
    }
    serial[0] = response.data[0];
    serial[1] = response.data[1];
    serial[2] = response.data[2];
    return true;
}

bool cctalk_request_build_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 192U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}

bool cctalk_modify_master_inhibit(uint8_t dest_addr, bool enable, uint32_t timeout_ms)
{
    uint8_t data = enable ? 1U : 0U;
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 231U, &data, 1U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}

bool cctalk_request_inhibit_status(uint8_t dest_addr, uint8_t *mask_low, uint8_t *mask_high, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!mask_low || !mask_high) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 230U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 2U) {
        return false;
    }
    *mask_low = response.data[0];
    *mask_high = response.data[1];
    return true;
}

bool cctalk_request_coin_id(uint8_t dest_addr, uint8_t channel, char *out, size_t out_len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    uint8_t data = channel;
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 184U, &data, 1U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_copy_ascii_response(&response, out, out_len);
}

bool cctalk_modify_sorter_paths(uint8_t dest_addr, const uint8_t *paths, uint8_t len, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (len > 0U && !paths) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 210U, paths, len, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}

bool cctalk_read_buffered_credit(uint8_t dest_addr, cctalk_buffer_t *buffer, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!buffer) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 229U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 11U) {
        return false;
    }

    buffer->event_counter = response.data[0];
    for (size_t i = 0; i < 5; ++i) {
        size_t base = 1U + (i * 2U);
        buffer->events[i].coin_id = response.data[base];
        buffer->events[i].error_code = response.data[base + 1U];
    }
    return true;
}

bool cctalk_request_insertion_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!status) {
        return false;
    }
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 226U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    if (response.data_len < 1U) {
        return false;
    }
    *status = response.data[0];
    return true;
}

bool cctalk_reset_device(uint8_t dest_addr, uint32_t timeout_ms)
{
    cctalk_frame_t response = {0};
    if (!cctalk_command(dest_addr, CCTALK_MASTER_ADDRESS, 1U, NULL, 0U, &response, timeout_ms)) {
        return false;
    }
    return cctalk_expect_ack(&response);
}

#endif /* DNA_CCTALK == 0 */

/*
 * Mockup — nessuna UART CCtalk reale.
 * Attiva quando DNA_CCTALK == 1
 */
#if defined(DNA_CCTALK) && (DNA_CCTALK == 1)

void cctalk_init(int uart_num, int tx_pin, int rx_pin, int baudrate)
{
    (void)uart_num; (void)tx_pin; (void)rx_pin; (void)baudrate;
}

bool cctalk_send(uint8_t dest_addr, const uint8_t *data, uint8_t len)
{
    (void)dest_addr; (void)data; (void)len;
    return true;
}

bool cctalk_receive(uint8_t *src_addr, uint8_t *data, uint8_t *len, uint32_t timeout_ms)
{
    (void)src_addr; (void)data; (void)len; (void)timeout_ms;
    return false;
}

uint8_t cctalk_checksum(const uint8_t *packet, uint8_t len)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; ++i) {
        sum = (uint16_t)(sum + packet[i]);
    }
    return (uint8_t)(-sum);
}

bool cctalk_send_frame(uint8_t dest_addr,
                       uint8_t src_addr,
                       uint8_t header,
                       const uint8_t *data,
                       uint8_t len)
{
    (void)dest_addr; (void)src_addr; (void)header; (void)data; (void)len;
    return true;
}

bool cctalk_receive_frame(cctalk_frame_t *frame, uint32_t timeout_ms)
{
    (void)frame; (void)timeout_ms;
    return false;
}

bool cctalk_command(uint8_t dest_addr,
                    uint8_t src_addr,
                    uint8_t header,
                    const uint8_t *data,
                    uint8_t len,
                    cctalk_frame_t *response,
                    uint32_t timeout_ms)
{
    (void)dest_addr; (void)src_addr; (void)header;
    (void)data; (void)len; (void)response; (void)timeout_ms;
    return false;
}

bool cctalk_address_poll(uint8_t dest_addr, uint32_t timeout_ms)
{
    (void)dest_addr; (void)timeout_ms;
    return false;
}

bool cctalk_request_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms)
{
    (void)dest_addr; (void)status; (void)timeout_ms;
    return false;
}

bool cctalk_request_manufacturer_id(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}

bool cctalk_request_equipment_category(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}

bool cctalk_request_product_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}

bool cctalk_request_serial_number(uint8_t dest_addr, uint8_t serial[3], uint32_t timeout_ms)
{
    (void)dest_addr; (void)serial; (void)timeout_ms;
    return false;
}

bool cctalk_request_build_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}

bool cctalk_modify_master_inhibit(uint8_t dest_addr, bool enable, uint32_t timeout_ms)
{
    (void)dest_addr; (void)enable; (void)timeout_ms;
    return true;
}

bool cctalk_request_inhibit_status(uint8_t dest_addr, uint8_t *mask_low, uint8_t *mask_high, uint32_t timeout_ms)
{
    (void)dest_addr; (void)mask_low; (void)mask_high; (void)timeout_ms;
    return false;
}

bool cctalk_request_coin_id(uint8_t dest_addr, uint8_t channel, char *out, size_t out_len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)channel; (void)out; (void)out_len; (void)timeout_ms;
    return false;
}

bool cctalk_modify_sorter_paths(uint8_t dest_addr, const uint8_t *paths, uint8_t len, uint32_t timeout_ms)
{
    (void)dest_addr; (void)paths; (void)len; (void)timeout_ms;
    return true;
}

bool cctalk_read_buffered_credit(uint8_t dest_addr, cctalk_buffer_t *buffer, uint32_t timeout_ms)
{
    (void)dest_addr; (void)buffer; (void)timeout_ms;
    return false;
}

bool cctalk_request_insertion_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms)
{
    (void)dest_addr; (void)status; (void)timeout_ms;
    return false;
}

bool cctalk_reset_device(uint8_t dest_addr, uint32_t timeout_ms)
{
    (void)dest_addr; (void)timeout_ms;
    return true;
}

#endif /* DNA_CCTALK == 1 */
