#include "eeprom_24lc16.h"
#include "periph_i2c.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "EEPROM";

#define EEPROM_BASE_ADDR    0x50
#define EEPROM_BLOCK_COUNT  8
#define EEPROM_PAGE_SIZE    16
#define EEPROM_TOTAL_SIZE   2048
#define EEPROM_WRITE_CYCLE_DELAY_MS 6
#define EEPROM_INIT_CHECK_TIMEOUT_MS 30

static i2c_master_dev_handle_t s_eeprom_dev_handles[EEPROM_BLOCK_COUNT] = {0};
static bool s_eeprom_available = false;

static void cleanup_dev_handles(void)
{
    for (uint8_t i = 0; i < EEPROM_BLOCK_COUNT; i++) {
        if (s_eeprom_dev_handles[i] != NULL) {
            i2c_master_bus_rm_device(s_eeprom_dev_handles[i]);
            s_eeprom_dev_handles[i] = NULL;
        }
    }
}

static uint8_t get_mem_addr(uint16_t address)
{
    // L'indirizzo interno è di 8 bit (0-255) per blocco
    return (uint8_t)(address & 0xFF);
}

static uint8_t get_block_idx(uint16_t address)
{
    return (uint8_t)((address >> 8) & 0x07);
}

static uint8_t get_dev_addr_for_block(uint8_t block_idx)
{
    return (uint8_t)(EEPROM_BASE_ADDR + (block_idx & 0x07));
}

static i2c_master_dev_handle_t get_dev_handle_for_address(uint16_t address)
{
    uint8_t block = get_block_idx(address);
    if (block >= EEPROM_BLOCK_COUNT) {
        return NULL;
    }
    return s_eeprom_dev_handles[block];
}

esp_err_t eeprom_24lc16_init(void) {
    i2c_master_bus_handle_t bus = periph_i2c_get_handle();
    s_eeprom_available = false;
    if (bus == NULL) {
        ESP_LOGW(TAG, "[C] Periph I2C bus not initialized");
        return ESP_OK; // Non bloccante
    }

    ESP_LOGI(TAG, "[C] Utilizzo bus I2C periferiche (GPIO26 SCL, GPIO27 SDA)");

    for (uint8_t block = 0; block < EEPROM_BLOCK_COUNT; block++) {
        if (s_eeprom_dev_handles[block] != NULL) {
            continue;
        }

        i2c_device_config_t dev_cfg = {
            .device_address = get_dev_addr_for_block(block),
            .scl_speed_hz = CONFIG_APP_I2C_CLOCK_HZ,
        };

        esp_err_t add_ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_eeprom_dev_handles[block]);
        if (add_ret != ESP_OK) {
            ESP_LOGW(TAG, "[C] Failed to add EEPROM block dev 0x%02X: %s",
                     get_dev_addr_for_block(block), esp_err_to_name(add_ret));
            cleanup_dev_handles();
            return ESP_OK; // Non bloccante
        }
    }

    // Check presenza EEPROM con lettura reale (evita spam di timeout da i2c_master_probe)
    uint8_t mem_addr = 0x00;
    uint8_t value = 0;
    esp_err_t ret = i2c_master_transmit_receive(s_eeprom_dev_handles[0],
                                                &mem_addr,
                                                1,
                                                &value,
                                                1,
                                                pdMS_TO_TICKS(EEPROM_INIT_CHECK_TIMEOUT_MS));
    
    if (ret == ESP_OK) {
        s_eeprom_available = true;
        ESP_LOGI(TAG, "[C] EEPROM 24LC16BT trovata e pronta");
    } else {
        s_eeprom_available = false;
        ESP_LOGW(TAG, "[C] EEPROM 24LC16BT non presente su indirizzo 0x50 (%s)", esp_err_to_name(ret));
        cleanup_dev_handles();
    }
    return ESP_OK; 
}

bool eeprom_24lc16_is_available(void) {
    return s_eeprom_available;
}


esp_err_t eeprom_24lc16_read(uint16_t address, uint8_t *buffer, size_t length) {
    if (!s_eeprom_available) return ESP_ERR_INVALID_STATE;
    if (length > 0 && buffer == NULL) return ESP_ERR_INVALID_ARG;
    if (address + length > EEPROM_TOTAL_SIZE) return ESP_ERR_INVALID_ARG;
    if (length == 0) return ESP_OK;

    size_t left = length;
    uint16_t cur_addr = address;
    uint8_t *ptr = buffer;

    while (left > 0) {
        i2c_master_dev_handle_t dev = get_dev_handle_for_address(cur_addr);
        if (dev == NULL) {
            return ESP_ERR_INVALID_STATE;
        }

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
    if (!s_eeprom_available) return ESP_ERR_INVALID_STATE;
    if (length > 0 && buffer == NULL) return ESP_ERR_INVALID_ARG;
    if (address + length > EEPROM_TOTAL_SIZE) return ESP_ERR_INVALID_ARG;
    if (length == 0) return ESP_OK;

    size_t left = length;
    uint16_t cur_addr = address;
    const uint8_t *ptr = buffer;

    while (left > 0) {
        i2c_master_dev_handle_t dev = get_dev_handle_for_address(cur_addr);
        if (dev == NULL) {
            return ESP_ERR_INVALID_STATE;
        }
        uint8_t mem_addr = get_mem_addr(cur_addr);
        
        // Gestione page write (16 byte max)
        size_t can_write_in_page = EEPROM_PAGE_SIZE - (mem_addr % EEPROM_PAGE_SIZE);
        size_t can_write_in_block = 256 - mem_addr;
        size_t to_write = (left < can_write_in_page) ? left : can_write_in_page;
        if (to_write > can_write_in_block) {
            to_write = can_write_in_block;
        }

        uint8_t write_buf[EEPROM_PAGE_SIZE + 1];
        write_buf[0] = mem_addr;
        memcpy(&write_buf[1], ptr, to_write);

        esp_err_t ret = i2c_master_transmit(dev, write_buf, to_write + 1, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) return ret;

        // Attesa ciclo interno EEPROM (datasheet max 5ms)
        vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_CYCLE_DELAY_MS));

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
