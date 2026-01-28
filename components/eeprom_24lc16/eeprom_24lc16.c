#include "eeprom_24lc16.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "EEPROM";

#define I2C_PORT            CONFIG_APP_I2C_PORT
#define EEPROM_BASE_ADDR    0x50
#define EEPROM_PAGE_SIZE    16
#define EEPROM_TOTAL_SIZE   2048
#define EEPROM_WRITE_DELAY_MS 5

#define EEPROM_WRITE_TIMEOUT_MS 10

static bool s_eeprom_available = false;

static uint8_t get_device_addr(uint16_t address) {
    // L'indirizzo I2C include i 3 bit più significativi dell'indirizzo di memoria (0-2047)
    // Per chip 24LC16BT: 1010 B2 B1 B0
    return EEPROM_BASE_ADDR | ((address >> 8) & 0x07);
}

static uint8_t get_mem_addr(uint16_t address) {
    // L'indirizzo interno è di 8 bit (0-255) per blocco
    return (uint8_t)(address & 0xFF);
}

/**
 * @brief Acknowledge Polling
 * Il chip non risponde all'indirizzo I2C finché il ciclo di scrittura interno (max 5ms) non è terminato.
 */
static esp_err_t wait_until_ready(uint8_t dev_addr) {
    uint8_t dummy;
    for (int i = 0; i < EEPROM_WRITE_TIMEOUT_MS; i++) {
        // Usiamo una lettura di 1 byte invece di una scrittura a 0 byte per evitare "i2c null address error" su alcuni driver
        esp_err_t ret = i2c_master_read_from_device(I2C_PORT, dev_addr, &dummy, 1, pdMS_TO_TICKS(10));
        if (ret == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t eeprom_24lc16_init(void) {
    // Nota: L'I2C driver dovrebbe essere già installato da io_expander_init o init.c
    // Verifichiamo se risponde ad almeno uno degli 8 indirizzi del blocco
    esp_err_t ret = wait_until_ready(EEPROM_BASE_ADDR);
    
    if (ret == ESP_OK) {
        s_eeprom_available = true;
        ESP_LOGI(TAG, "[C] EEPROM 24LC16BT trovata e pronta (I2C port %d)", I2C_PORT);
    } else {
        s_eeprom_available = false;
        ESP_LOGW(TAG, "[C] EEPROM 24LC16BT non risponde (check I2C pins o indirizzo 0x50)");
    }
    return ESP_OK; // Restituiamo sempre OK per permettere il boot con fallback su NVS
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
        uint8_t dev_addr = get_device_addr(cur_addr);
        uint8_t mem_addr = get_mem_addr(cur_addr);
        
        // Possiamo leggere solo fino alla fine del blocco corrente (256 byte)
        size_t can_read = 256 - mem_addr;
        size_t to_read = (left < can_read) ? left : can_read;

        esp_err_t ret = i2c_master_write_read_device(I2C_PORT, dev_addr, &mem_addr, 1, ptr, to_read, pdMS_TO_TICKS(200));
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
        uint8_t dev_addr = get_device_addr(cur_addr);
        uint8_t mem_addr = get_mem_addr(cur_addr);
        
        // Gestione page write (16 byte max)
        size_t can_write_in_page = EEPROM_PAGE_SIZE - (mem_addr % EEPROM_PAGE_SIZE);
        size_t to_write = (left < can_write_in_page) ? left : can_write_in_page;

        uint8_t write_buf[EEPROM_PAGE_SIZE + 1];
        write_buf[0] = mem_addr;
        memcpy(&write_buf[1], ptr, to_write);

        esp_err_t ret = i2c_master_write_to_device(I2C_PORT, dev_addr, write_buf, to_write + 1, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) return ret;

        // Attesa completamento scrittura tramite polling (molto più robusto per 24LC16BT)
        ret = wait_until_ready(dev_addr);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[C] Timeout scrittura EEPROM all'indirizzo 0x%03X", cur_addr);
            return ret;
        }

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
