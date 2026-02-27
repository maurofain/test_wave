#include "io_expander.h"
#include "periph_i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* DNA_IO_EXPANDER: imposta a 1 nel CMakeLists del componente per attivare il
 * mockup senza hardware reale (nessun I2C/FXL6408). Default: 0. */
#ifndef DNA_IO_EXPANDER
#define DNA_IO_EXPANDER 0
#endif

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

#if DNA_IO_EXPANDER == 0  /* implementazioni reali — escluse se mockup attivo */

static i2c_master_dev_handle_t io_out_dev, io_in_dev;
static bool s_io_exp_ready = false;

static void io_expander_reset_handles(void)
{
    io_out_dev = NULL;
    io_in_dev = NULL;
    s_io_exp_ready = false;
}

static esp_err_t io_expander_prepare_bus(i2c_master_bus_handle_t *out_bus)
{
    i2c_master_bus_handle_t bus = periph_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "[C] Periph I2C bus non inizializzato: chiama periph_i2c_init() prima");
        return ESP_ERR_INVALID_STATE;
    }
    *out_bus = bus;
    return ESP_OK;
}

static esp_err_t io_expander_attach_devices(i2c_master_bus_handle_t bus)
{
    esp_err_t ret;
    i2c_device_config_t out_cfg = {
        .device_address = FXL6408_ADDR_OUT,
        .scl_speed_hz = CONFIG_APP_I2C_CLOCK_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &out_cfg, &io_out_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore aggiunta device OUTPUT (0x%02X): %s", FXL6408_ADDR_OUT, esp_err_to_name(ret));
        io_expander_reset_handles();
        return ret;
    }

    i2c_device_config_t in_cfg = {
        .device_address = FXL6408_ADDR_IN,
        .scl_speed_hz = CONFIG_APP_I2C_CLOCK_HZ,
    };
    ret = i2c_master_bus_add_device(bus, &in_cfg, &io_in_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore aggiunta device INPUT (0x%02X): %s", FXL6408_ADDR_IN, esp_err_to_name(ret));
        io_expander_reset_handles();
        return ret;
    }

    return ESP_OK;
}

static esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t data[] = {reg, val};
    return i2c_master_transmit(dev, data, 2, pdMS_TO_TICKS(100));
}

static esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val) {
    return i2c_master_transmit_receive(dev, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

esp_err_t io_expander_init(void) {
    if (s_io_exp_ready) {
        return ESP_OK;
    }

    esp_err_t ret;
    i2c_master_bus_handle_t bus = NULL;
    for (int attempt = 0; attempt < 2; attempt++) {
        ret = io_expander_prepare_bus(&bus);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = io_expander_attach_devices(bus);
        if (ret != ESP_OK) {
            if (ret == ESP_ERR_INVALID_STATE && attempt == 0) {
                continue;
            }
            return ESP_ERR_NOT_FOUND;
        }

        ret = write_reg(io_out_dev, REG_DIRECTION, 0xFF); // Tutti i pin come output
        if (ret == ESP_OK) {
            break;
        }

        ESP_LOGE(TAG, "[C] Errore comunicazione chip OUTPUT (0x%02X): %s",
                 FXL6408_ADDR_OUT, esp_err_to_name(ret));
        io_expander_reset_handles();
        if (ret == ESP_ERR_INVALID_STATE && attempt == 0) {
            continue;
        }
        return ESP_ERR_NOT_FOUND;
    }

    if (io_out_dev == NULL || io_in_dev == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    write_reg(io_out_dev, REG_OUTPUT_TRISTATE, 0x00); // Disabilita High-Z (attiva driver)
    write_reg(io_out_dev, REG_OUTPUT_DATA, io_output_state);

    ret = write_reg(io_in_dev, REG_DIRECTION, 0x00);        // Tutti i pin come input
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore comunicazione chip INPUT (0x%02X): %s",
                 FXL6408_ADDR_IN, esp_err_to_name(ret));
        io_expander_reset_handles();
        return ESP_ERR_NOT_FOUND;
    }
    // Abilita Pull-up su tutti i pin di input per stabilizzare bit flottanti (come il bit 4)
    write_reg(io_in_dev, REG_PULL_SELECT, 0xFF); // 1 = Pull-up
    write_reg(io_in_dev, REG_PULL_ENABLE, 0xFF); // 1 = Abilitato

    s_io_exp_ready = true;
    ESP_LOGI(TAG, "[C] Inizializzati FXL6408 ad indirizzi 0x%02X (OUT) e 0x%02X (IN)", FXL6408_ADDR_OUT, FXL6408_ADDR_IN);
    return ESP_OK;
}

void io_set_pin(int pin, int value) {
    if (!s_io_exp_ready) {
        if (io_expander_init() != ESP_OK) {
            return;
        }
    }
    if (pin < 0 || pin > 7) return;
    if (value) {
        io_output_state |= (1 << pin);
    } else {
        io_output_state &= ~(1 << pin);
    }
    esp_err_t ret = write_reg(io_out_dev, REG_OUTPUT_DATA, io_output_state);
    if (ret == ESP_ERR_INVALID_STATE) {
        io_expander_reset_handles();
        if (io_expander_init() == ESP_OK) {
            write_reg(io_out_dev, REG_OUTPUT_DATA, io_output_state);
        }
    }
}

void io_set_port(uint8_t val) {
    if (!s_io_exp_ready) {
        if (io_expander_init() != ESP_OK) {
            return;
        }
    }
    io_output_state = val;
    esp_err_t ret = write_reg(io_out_dev, REG_OUTPUT_DATA, io_output_state);
    if (ret == ESP_ERR_INVALID_STATE) {
        io_expander_reset_handles();
        if (io_expander_init() == ESP_OK) {
            write_reg(io_out_dev, REG_OUTPUT_DATA, io_output_state);
        }
    }
}

bool io_get_pin(int pin) {
    if (pin < 0 || pin > 7) return false;
    uint8_t val = io_get();
    return (val & (1 << pin)) != 0;
}

uint8_t io_get(void) {
    if (!s_io_exp_ready) {
        if (io_expander_init() != ESP_OK) {
            return io_input_state;
        }
    }
    uint8_t val = 0;
    esp_err_t ret = read_reg(io_in_dev, REG_INPUT_DATA, &val);
    if (ret == ESP_OK) {
        io_input_state = val;
    } else if (ret == ESP_ERR_INVALID_STATE) {
        io_expander_reset_handles();
        if (io_expander_init() == ESP_OK && read_reg(io_in_dev, REG_INPUT_DATA, &val) == ESP_OK) {
            io_input_state = val;
        }
    }
    return io_input_state;
}

#endif /* DNA_IO_EXPANDER == 0 */

/*
 * Mockup section: se DNA_IO_EXPANDER==1 vengono fornite versioni fittizie di
 * tutte le API pubbliche. Nessun bus I2C viene toccato; le funzioni aggiornano
 * le variabili globali io_output_state / io_input_state come farebbe il driver
 * reale, consentendo di testare la logica di livello superiore senza hardware.
 */
#if defined(DNA_IO_EXPANDER) && (DNA_IO_EXPANDER == 1)

/* Simuliamo tutti i pin di input alti (pull-up attivi), GPIO3 incluso */
#define IO_MOCK_INPUT_DEFAULT 0xFF

esp_err_t io_expander_init(void)
{
    io_output_state = 0x00;
    io_input_state  = IO_MOCK_INPUT_DEFAULT;
    ESP_LOGI(TAG, "[C] [MOCK] io_expander_init: OUT=0x%02X IN=0x%02X (simulato)",
             io_output_state, io_input_state);
    return ESP_OK;
}

void io_set_pin(int pin, int value)
{
    if (pin < 0 || pin > 7) return;
    if (value) {
        io_output_state |= (uint8_t)(1 << pin);
    } else {
        io_output_state &= (uint8_t)~(1 << pin);
    }
    ESP_LOGD(TAG, "[C] [MOCK] io_set_pin(%d, %d) -> OUT=0x%02X", pin, value, io_output_state);
}

void io_set_port(uint8_t val)
{
    io_output_state = val;
    ESP_LOGD(TAG, "[C] [MOCK] io_set_port(0x%02X)", val);
}

bool io_get_pin(int pin)
{
    if (pin < 0 || pin > 7) return false;
    return (io_input_state & (uint8_t)(1 << pin)) != 0;
}

uint8_t io_get(void)
{
    return io_input_state;
}

#endif /* DNA_IO_EXPANDER */
