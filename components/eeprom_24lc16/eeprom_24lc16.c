#include "eeprom_24lc16.h"
#include "periph_i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>

static const char *TAG = "EEPROM";

#define EEPROM_BASE_ADDR    0x50
#define EEPROM_PAGE_SIZE    16
#define EEPROM_TOTAL_SIZE   2048
#define EEPROM_WRITE_DELAY_MS 5
#define EEPROM_WRITE_TIMEOUT_MS 10
#define EEPROM_INIT_PROBE_TIMEOUT_MS 50  // Timeout più lungo per rilevazione iniziale

static i2c_master_dev_handle_t eeprom_dev_handle = NULL;
static i2c_master_bus_handle_t eeprom_bus_handle = NULL;
static bool s_eeprom_available = false;

/**
 * @brief Acknowledge Polling via i2c_master_probe (ACK check).
 * Il chip non risponde all'indirizzo I2C finché il ciclo di scrittura interno (max 5ms) non è terminato.
 */
static esp_err_t wait_until_ready(void) {
    for (int i = 0; i < EEPROM_WRITE_TIMEOUT_MS; i++) {
        // i2c_master_probe invia solo l'indirizzo per verificare ACK
        esp_err_t ret = i2c_master_probe(eeprom_bus_handle, EEPROM_BASE_ADDR, pdMS_TO_TICKS(10));
        if (ret == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_ERR_TIMEOUT;
}

static uint8_t get_mem_addr(uint16_t address) {
    // L'indirizzo interno è di 8 bit (0-255) per blocco
    return (uint8_t)(address & 0xFF);
}

esp_err_t eeprom_24lc16_init(void) {
    i2c_master_bus_handle_t bus = periph_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGW(TAG, "[C] Periph I2C bus not initialized");
        return ESP_OK; // Non bloccante
    }
    
    eeprom_bus_handle = bus; // Salva bus handle per polling ACK
    ESP_LOGI(TAG, "[C] Utilizzo bus I2C periferiche (GPIO26 SCL, GPIO27 SDA)");

    i2c_device_config_t dev_cfg = {
        .device_address = EEPROM_BASE_ADDR,
        .scl_speed_hz = CONFIG_APP_I2C_CLOCK_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &eeprom_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[C] Failed to add EEPROM device at 0x%02X: %s", EEPROM_BASE_ADDR, esp_err_to_name(ret));
        return ESP_OK; // Non bloccante
    }

    // Verifichiamo se risponde con timeout allungato
    ESP_LOGI(TAG, "[C] Scanning for EEPROM at 0x50 (timeout=%dms)...", EEPROM_INIT_PROBE_TIMEOUT_MS);
    ret = wait_until_ready();
    
    if (ret == ESP_OK) {
        s_eeprom_available = true;
        ESP_LOGI(TAG, "[C] EEPROM 24LC16BT trovata e pronta");
    } else {
        s_eeprom_available = false;
        ESP_LOGW(TAG, "[C] EEPROM 24LC16BT non presente su indirizzo 0x50 (timeout after %dms)", EEPROM_INIT_PROBE_TIMEOUT_MS);
    }
    return ESP_OK; 
}

bool eeprom_24lc16_is_available(void) {
    return s_eeprom_available;
}


esp_err_t eeprom_24lc16_read(uint16_t address, uint8_t *buffer, size_t length) {
    if (!s_eeprom_available) return ESP_ERR_INVALID_STATE;
    if (address + length > EEPROM_TOTAL_SIZE) return ESP_ERR_INVALID_ARG;
    if (length == 0) return ESP_OK;

    size_t left = length;
    uint16_t cur_addr = address;
    uint8_t *ptr = buffer;

    while (left > 0) {
        uint8_t mem_addr = get_mem_addr(cur_addr);
        
        // Possiamo leggere solo fino alla fine del blocco corrente (256 byte)
        size_t can_read = 256 - mem_addr;
        size_t to_read = (left < can_read) ? left : can_read;

        esp_err_t ret = i2c_master_transmit_receive(eeprom_dev_handle, &mem_addr, 1, ptr, to_read, pdMS_TO_TICKS(200));
        if (ret != ESP_OK) return ret;

        left -= to_read;
        cur_addr += to_read;
        ptr += to_read;
    }

    return ESP_OK;
}


esp_err_t eeprom_24lc16_write(uint16_t address, const uint8_t *buffer, size_t length) {
    if (!s_eeprom_available) return ESP_ERR_INVALID_STATE;
    if (address + length > EEPROM_TOTAL_SIZE) return ESP_ERR_INVALID_ARG;
    if (length == 0) return ESP_OK;

    size_t left = length;
    uint16_t cur_addr = address;
    const uint8_t *ptr = buffer;

    while (left > 0) {
        uint8_t mem_addr = get_mem_addr(cur_addr);
        
        // Gestione page write (16 byte max)
        size_t can_write_in_page = EEPROM_PAGE_SIZE - (mem_addr % EEPROM_PAGE_SIZE);
        size_t to_write = (left < can_write_in_page) ? left : can_write_in_page;

        uint8_t write_buf[EEPROM_PAGE_SIZE + 1];
        write_buf[0] = mem_addr;
        memcpy(&write_buf[1], ptr, to_write);

        esp_err_t ret = i2c_master_transmit(eeprom_dev_handle, write_buf, to_write + 1, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) return ret;

        // Attesa completamento scrittura tramite polling (molto più robusto per 24LC16BT)
        ret = wait_until_ready();
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
