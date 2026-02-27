#include "eeprom_24lc16.h"
#include "periph_i2c.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "EEPROM";

#define EEPROM_BASE_ADDR    0x50
#define EEPROM_PAGE_SIZE    16
#define EEPROM_TOTAL_SIZE   2048
#define EEPROM_WRITE_DELAY_MS 5

static i2c_master_dev_handle_t eeprom_dev_handles[8];

static bool s_eeprom_available = false;

static uint8_t get_mem_addr(uint16_t address) {
    // L'indirizzo interno è di 8 bit (0-255) per blocco
    return (uint8_t)(address & 0xFF);
}

esp_err_t eeprom_24lc16_init(void) {
    i2c_master_bus_handle_t bus = periph_i2c_get_handle();
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_STATE, TAG, "Periph I2C bus not initialized");

    for (int i = 0; i < 8; i++) {
        i2c_device_config_t dev_cfg = {
            .device_address = EEPROM_BASE_ADDR | i,
            .scl_speed_hz = CONFIG_APP_I2C_CLOCK_HZ,
        };
        esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &eeprom_dev_handles[i]);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add EEPROM device 0x%02X: %s", EEPROM_BASE_ADDR | i, esp_err_to_name(ret));
            s_eeprom_available = false;
            return ret;
        }
    }

    // Test one device
    uint8_t dummy;
    esp_err_t ret = i2c_master_receive(eeprom_dev_handles[0], &dummy, 1, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        uint8_t probe[16] = {0};
        char hex_line[(16 * 3) + 1] = {0};

        s_eeprom_available = true;
        ESP_LOGI(TAG, "[C] EEPROM 24LC16BT found and ready");

        esp_err_t probe_ret = eeprom_24lc16_read(0, probe, sizeof(probe));
        if (probe_ret == ESP_OK) {
            size_t off = 0;
            for (size_t i = 0; i < sizeof(probe); i++) {
                off += (size_t)snprintf(&hex_line[off], sizeof(hex_line) - off, "%02X", probe[i]);
                if (i + 1 < sizeof(probe) && off + 2 < sizeof(hex_line)) {
                    hex_line[off++] = ' ';
                    hex_line[off] = '\0';
                }
            }
            ESP_LOGI(TAG, "[C] EEPROM probe [0x0000..0x000F]: %s", hex_line);
        } else {
            ESP_LOGW(TAG, "[C] EEPROM probe read fallita: %s", esp_err_to_name(probe_ret));
        }
    } else {
        s_eeprom_available = false;
        ESP_LOGW(TAG, "[C] EEPROM 24LC16BT not responding: %s", esp_err_to_name(ret));
    }
    return ESP_OK;
}

bool eeprom_24lc16_is_available(void) {
    return s_eeprom_available;
}

esp_err_t eeprom_24lc16_read(uint16_t address, uint8_t *buffer, size_t length) {
    if (address + length > EEPROM_TOTAL_SIZE) return ESP_ERR_INVALID_ARG;
    if (length == 0) return ESP_OK;

    size_t left = length;
    uint16_t cur_addr = address;
    uint8_t *ptr = buffer;

    while (left > 0) {
        uint8_t dev_idx = (cur_addr >> 8) & 0x07;
        i2c_master_dev_handle_t dev = eeprom_dev_handles[dev_idx];
        uint8_t mem_addr = get_mem_addr(cur_addr);
        
        // Possiamo leggere solo fino alla fine del blocco corrente (256 byte)
        size_t can_read = 256 - mem_addr;
        size_t to_read = (left < can_read) ? left : can_read;

        esp_err_t ret = i2c_master_transmit_receive(dev, &mem_addr, 1, ptr, to_read, pdMS_TO_TICKS(200));
        if (ret != ESP_OK) return ret;

        left -= to_read;
        cur_addr += to_read;
        ptr += to_read;
    }

    return ESP_OK;
}

esp_err_t eeprom_24lc16_write(uint16_t address, const uint8_t *buffer, size_t length) {
    if (address + length > EEPROM_TOTAL_SIZE) return ESP_ERR_INVALID_ARG;
    if (length == 0) return ESP_OK;

    size_t left = length;
    uint16_t cur_addr = address;
    const uint8_t *ptr = buffer;

    while (left > 0) {
        uint8_t dev_idx = (cur_addr >> 8) & 0x07;
        i2c_master_dev_handle_t dev = eeprom_dev_handles[dev_idx];
        uint8_t mem_addr = get_mem_addr(cur_addr);
        
        // Gestione page write (16 byte max)
        size_t can_write_in_page = EEPROM_PAGE_SIZE - (mem_addr % EEPROM_PAGE_SIZE);
        size_t to_write = (left < can_write_in_page) ? left : can_write_in_page;

        uint8_t write_buf[EEPROM_PAGE_SIZE + 1];
        write_buf[0] = mem_addr;
        memcpy(&write_buf[1], ptr, to_write);

        esp_err_t ret = i2c_master_transmit(dev, write_buf, to_write + 1, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) return ret;

        // Attesa completamento scrittura (5ms tipico per 24LC16BT)
        vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_DELAY_MS));

        left -= to_write;
        cur_addr += to_write;
        ptr += to_write;
    }

    return ESP_OK;
}

esp_err_t eeprom_24lc16_read_byte(uint16_t address, uint8_t *val) {
    return eeprom_24lc16_read(address, val, 1);
}

esp_err_t eeprom_24lc16_write_byte(uint16_t address, uint8_t val) {
    return eeprom_24lc16_write(address, &val, 1);
}
