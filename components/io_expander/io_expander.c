#include "io_expander.h"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "IO_EXP";

#define FXL6408_ADDR_OUT    (0x86 >> 1)  // 0x43
#define FXL6408_ADDR_IN     (0x88 >> 1)  // 0x44

// Registri FXL6408
#define REG_DEVICE_ID       0x01
#define REG_DIRECTION       0x03
#define REG_OUTPUT_DATA     0x05
#define REG_OUTPUT_TRISTATE 0x07
#define REG_PULL_ENABLE     0x0B
#define REG_PULL_SELECT     0x0D
#define REG_INPUT_DATA      0x0F

uint8_t io_output_state = 0x00;
uint8_t io_input_state = 0x00;

static i2c_master_dev_handle_t io_out_dev, io_in_dev;

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t data[] = {reg, val};
    return i2c_master_transmit(dev, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val) {
    return i2c_master_transmit_receive(dev, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t io_expander_init(void) {
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    esp_err_t ret;
    // Inizializzazione chip OUTPUT
    i2c_device_config_t out_cfg = {
        .device_address = FXL6408_ADDR_OUT,
        .scl_speed_hz = CONFIG_APP_I2C_CLOCK_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &out_cfg, &io_out_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore aggiunta device OUTPUT (0x%02X): %s", FXL6408_ADDR_OUT, esp_err_to_name(ret));
        return ESP_ERR_NOT_FOUND;
    }
    ret = write_reg(io_out_dev, REG_DIRECTION, 0xFF);       // Tutti i pin come output
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore comunicazione chip OUTPUT (0x%02X)", FXL6408_ADDR_OUT);
        return ESP_ERR_NOT_FOUND;
    }
    write_reg(io_out_dev, REG_OUTPUT_TRISTATE, 0x00); // Disabilita High-Z (attiva driver)
    write_reg(io_out_dev, REG_OUTPUT_DATA, io_output_state);

    // Inizializzazione chip INPUT
    i2c_device_config_t in_cfg = {
        .device_address = FXL6408_ADDR_IN,
        .scl_speed_hz = CONFIG_APP_I2C_CLOCK_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &in_cfg, &io_in_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore aggiunta device INPUT (0x%02X): %s", FXL6408_ADDR_IN, esp_err_to_name(ret));
        return ESP_ERR_NOT_FOUND;
    }
    ret = write_reg(io_in_dev, REG_DIRECTION, 0x00);        // Tutti i pin come input
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore comunicazione chip INPUT (0x%02X)", FXL6408_ADDR_IN);
        return ESP_ERR_NOT_FOUND;
    }
    // Abilita Pull-up su tutti i pin di input per stabilizzare bit flottanti (come il bit 4)
    write_reg(io_in_dev, REG_PULL_SELECT, 0xFF); // 1 = Pull-up
    write_reg(io_in_dev, REG_PULL_ENABLE, 0xFF); // 1 = Abilitato

    ESP_LOGI(TAG, "[C] Inizializzati FXL6408 ad indirizzi 0x%02X (OUT) e 0x%02X (IN)", FXL6408_ADDR_OUT, FXL6408_ADDR_IN);
    return ESP_OK;
}

void io_set_pin(int pin, int value) {
    if (pin < 0 || pin > 7) return;
    if (value) {
        io_output_state |= (1 << pin);
    } else {
        io_output_state &= ~(1 << pin);
    }
    write_reg(io_out_dev, REG_OUTPUT_DATA, io_output_state);
}

void io_set_port(uint8_t val) {
    io_output_state = val;
    write_reg(io_out_dev, REG_OUTPUT_DATA, io_output_state);
}

bool io_get_pin(int pin) {
    if (pin < 0 || pin > 7) return false;
    uint8_t val = io_get();
    return (val & (1 << pin)) != 0;
}

uint8_t io_get(void) {
    uint8_t val = 0;
    if (read_reg(io_in_dev, REG_INPUT_DATA, &val) == ESP_OK) {
        io_input_state = val;
    }
    return io_input_state;
}
