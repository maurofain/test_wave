#include "io_expander.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "IO_EXP";

esp_err_t io_expander_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = CONFIG_APP_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_APP_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    esp_err_t ret = i2c_param_config(CONFIG_APP_I2C_PORT, &conf);
    if (ret != ESP_OK) return ret;
    return i2c_driver_install(CONFIG_APP_I2C_PORT, conf.mode, 0, 0, 0);
}

esp_err_t io_expander_write_output(uint16_t mask) {
    uint8_t data[] = {0x02, (uint8_t)(mask & 0xFF), (uint8_t)(mask >> 8)};
    return i2c_master_write_to_device(CONFIG_APP_I2C_PORT, 0x20, data, 3, pdMS_TO_TICKS(100));
}
