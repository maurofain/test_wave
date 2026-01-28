#include "rs485.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "RS485";

esp_err_t rs485_init(void) {
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t ret = uart_driver_install(CONFIG_APP_RS485_UART_PORT, 2048, 0, 0, NULL, 0);
    if (ret != ESP_OK) return ret;
    ret = uart_param_config(CONFIG_APP_RS485_UART_PORT, &cfg);
    if (ret != ESP_OK) return ret;
    ret = uart_set_pin(CONFIG_APP_RS485_UART_PORT, CONFIG_APP_RS485_TX_GPIO, CONFIG_APP_RS485_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) return ret;
    ret = uart_set_mode(CONFIG_APP_RS485_UART_PORT, UART_MODE_RS485_HALF_DUPLEX);
    if (ret != ESP_OK) return ret;

    gpio_set_direction(CONFIG_APP_RS485_DE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_APP_RS485_DE_GPIO, 0);
    return ESP_OK;
}
