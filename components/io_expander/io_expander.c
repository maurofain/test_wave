#include "io_expander.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "IO_EXP";

#define I2C_PORT            CONFIG_APP_I2C_PORT
#define FXL6408_ADDR_OUT    (0x86 >> 1)  // 0x43
#define FXL6408_ADDR_IN     (0x88 >> 1)  // 0x44

// Registri FXL6408
#define REG_DEVICE_ID       0x01
#define REG_DIRECTION       0x03
#define REG_OUTPUT_DATA     0x05
#define REG_OUTPUT_TRISTATE 0x07
#define REG_INPUT_DATA      0x0F

uint8_t io_output_state = 0x00;
uint8_t io_input_state = 0x00;

static esp_err_t write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t data[] = {reg, val};
    return i2c_master_write_to_device(I2C_PORT, addr, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t read_reg(uint8_t addr, uint8_t reg, uint8_t *val) {
    return i2c_master_write_read_device(I2C_PORT, addr, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t io_expander_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_APP_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_APP_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = CONFIG_APP_I2C_CLOCK_HZ,
    };
    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) return ret;
    
    ret = i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) return ret;

    // Inizializzazione chip OUTPUT
    write_reg(FXL6408_ADDR_OUT, REG_DIRECTION, 0xFF);       // Tutti i pin come output
    write_reg(FXL6408_ADDR_OUT, REG_OUTPUT_TRISTATE, 0x00); // Disabilita High-Z (attiva driver)
    write_reg(FXL6408_ADDR_OUT, REG_OUTPUT_DATA, io_output_state);

    // Inizializzazione chip INPUT
    write_reg(FXL6408_ADDR_IN, REG_DIRECTION, 0x00);        // Tutti i pin come input

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
    write_reg(FXL6408_ADDR_OUT, REG_OUTPUT_DATA, io_output_state);
}

void io_set_port(uint8_t val) {
    io_output_state = val;
    write_reg(FXL6408_ADDR_OUT, REG_OUTPUT_DATA, io_output_state);
}

bool io_get_pin(int pin) {
    if (pin < 0 || pin > 7) return false;
    uint8_t val = io_get();
    return (val & (1 << pin)) != 0;
}

uint8_t io_get(void) {
    uint8_t val = 0;
    if (read_reg(FXL6408_ADDR_IN, REG_INPUT_DATA, &val) == ESP_OK) {
        io_input_state = val;
    }
    return io_input_state;
}
