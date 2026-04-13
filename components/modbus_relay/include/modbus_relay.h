#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "hw_common.h"

#define MODBUS_RELAY_MAX_POINTS 64U
#define MODBUS_RELAY_MAX_BYTES ((MODBUS_RELAY_MAX_POINTS + 7U) / 8U)

typedef struct {
    bool initialized;
    bool running;
    uint8_t slave_id;
    uint16_t relay_start;
    uint16_t relay_count;
    uint16_t input_start;
    uint16_t input_count;
    uint8_t relay_bits[MODBUS_RELAY_MAX_BYTES];
    uint8_t input_bits[MODBUS_RELAY_MAX_BYTES];
    uint32_t poll_ok_count;
    uint32_t poll_err_count;
    int32_t last_error;
    uint32_t last_update_ms;
} modbus_relay_status_t;

esp_err_t modbus_relay_init(void);
esp_err_t modbus_relay_deinit(void);
bool modbus_relay_is_running(void);

esp_err_t modbus_relay_poll_once(void);
esp_err_t modbus_relay_get_status(modbus_relay_status_t *out_status);

esp_err_t modbus_relay_read_coils(uint8_t slave_id,
                                  uint16_t start,
                                  uint16_t count,
                                  uint8_t *out_bits,
                                  size_t out_bits_size);

esp_err_t modbus_relay_read_discrete_inputs(uint8_t slave_id,
                                            uint16_t start,
                                            uint16_t count,
                                            uint8_t *out_bits,
                                            size_t out_bits_size);

esp_err_t modbus_relay_write_single_coil(uint8_t slave_id,
                                         uint16_t coil_addr,
                                         bool on);

esp_err_t modbus_relay_write_multiple_coils(uint8_t slave_id,
                                            uint16_t start,
                                            const uint8_t *packed_bits,
                                            uint16_t count);
hw_component_status_t modbus_relay_get_hw_status(void);
