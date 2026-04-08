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
#define EEPROM_INIT_SCL_SPEED_HZ 50000
#define EEPROM_PROBE_TIMEOUT_MS 100
#define EEPROM_RW_TIMEOUT_MS 200

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

static esp_err_t detect_eeprom_address(i2c_master_bus_handle_t bus, uint8_t *detected_addr)
{
    if (bus == NULL || detected_addr == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t addr = EEPROM_BASE_ADDR; addr < (EEPROM_BASE_ADDR + EEPROM_BLOCK_COUNT); addr++) {
        esp_err_t ret = i2c_master_probe(bus, addr, EEPROM_PROBE_TIMEOUT_MS);
        if (ret == ESP_OK) {
            *detected_addr = addr;
            ESP_LOGI("INIT", "[M] EEPROM init: dispositivo rilevato su 0x%02X", addr);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t eeprom_24lc16_init(void) {
#if F_EEPROM == 1
    ESP_LOGW(TAG, "[M] EEPROM mock disabled (F_EEPROM=1)");
    s_eeprom_available = false;
    return ESP_OK;
#endif
    if (s_eeprom_available) {
        ESP_LOGI(TAG, "[M] EEPROM init: dispositivo gia' disponibile");
        return ESP_OK;
    }

    cleanup_dev_handles();
    s_eeprom_available = false;

    ESP_LOGI("INIT", "[M] EEPROM init: INIZIO funzione");
    ESP_LOGI(TAG, "[M] EEPROM init: INIZIO funzione");
    
    // Atendi che periph_i2c sia completamente inizializzato
    int retry_count = 0;
    i2c_master_bus_handle_t bus = NULL;
    
    while (retry_count < 10 && bus == NULL) {
        bus = periph_i2c_get_handle();
        if (bus == NULL) {
            ESP_LOGI(TAG, "[M] EEPROM init: Attendo periph_i2c... tentativo %d/10", retry_count + 1);
            vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay
            retry_count++;
        }
    }
    
    ESP_LOGI("INIT", "[M] DEBUG: bus handle dopo retry = %p, retry_count=%d", bus, retry_count);
    if (bus == NULL) {
        ESP_LOGW("INIT", "[M] EEPROM init: Periph I2C bus not initialized dopo %d tentativi", retry_count);
        ESP_LOGW(TAG, "[M] EEPROM init: Periph I2C bus not initialized dopo %d tentativi", retry_count);
        return ESP_OK; // Non bloccante
    }

    ESP_LOGI("INIT", "[M] EEPROM init: Utilizzo bus I2C periferiche (GPIO%d SCL, GPIO%d SDA)",
             CONFIG_APP_I2C_SCL_GPIO, CONFIG_APP_I2C_SDA_GPIO);

    uint8_t detected_addr = 0;
    esp_err_t detect_ret = detect_eeprom_address(bus, &detected_addr);
    if (detect_ret != ESP_OK) {
        ESP_LOGW("INIT", "[M] EEPROM init: nessuna EEPROM rilevata tra 0x50 e 0x57 (%s)",
                 esp_err_to_name(detect_ret));
        cleanup_dev_handles();
        return ESP_OK;
    }

    for (uint8_t block = 0; block < EEPROM_BLOCK_COUNT; block++) {
        if (s_eeprom_dev_handles[block] != NULL) {
            continue;
        }

        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = get_dev_addr_for_block(block),
            .scl_speed_hz = EEPROM_INIT_SCL_SPEED_HZ,
            .flags.disable_ack_check = false,
        };

        esp_err_t add_ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_eeprom_dev_handles[block]);
        if (add_ret != ESP_OK) {
            ESP_LOGW("INIT", "[M] EEPROM init: fallita add_device su 0x%02X: %s",
                     get_dev_addr_for_block(block), esp_err_to_name(add_ret));
            cleanup_dev_handles();
            return ESP_OK; // Non bloccante
        }
    }

    // Check presenza EEPROM con lettura reale
    uint8_t check_block = (uint8_t)(detected_addr - EEPROM_BASE_ADDR);
    if (check_block >= EEPROM_BLOCK_COUNT || s_eeprom_dev_handles[check_block] == NULL) {
        ESP_LOGW("INIT", "[M] EEPROM init: handle non valido per blocco %u", check_block);
        cleanup_dev_handles();
        return ESP_OK;
    }

    uint8_t mem_addr = 0x00;
    uint8_t value = 0;
    esp_err_t ret = i2c_master_transmit_receive(s_eeprom_dev_handles[check_block],
                                                &mem_addr,
                                                1,
                                                &value,
                                                1,
                                                EEPROM_INIT_CHECK_TIMEOUT_MS);
    
    if (ret == ESP_OK) {
        s_eeprom_available = true;
        ESP_LOGI("INIT", "[M] EEPROM init: 24LC16BT trovata e pronta");
        
        // Test EEPROM: leggi, modifica, verifica, ripristina byte alla locazione 400
        // uint8_t original_value = 0;
        // esp_err_t read_ret = eeprom_24lc16_read_byte(400, &original_value);
        /*
        if (read_ret == ESP_OK) {
            ESP_LOGI("INIT", "[M] EEPROM TEST: Lettura locazione 400 -> valore originale: 0x%02X (%d)", original_value, original_value);
            
            uint8_t modified_value = (uint8_t)(original_value ^ 0xFF);
            ESP_LOGI("INIT", "[M] EEPROM TEST: Valore modificato (XOR 256): 0x%02X (%d)", modified_value, modified_value);
            
            esp_err_t write_ret = eeprom_24lc16_write_byte(400, modified_value);
            if (write_ret == ESP_OK) {
                ESP_LOGI("INIT", "[M] EEPROM TEST: Scrittura completata");
                
                // Verifica lettura
                uint8_t verify_value = 0;
                esp_err_t verify_ret = eeprom_24lc16_read_byte(400, &verify_value);
                if (verify_ret == ESP_OK) {
                    ESP_LOGI("INIT", "[M] EEPROM TEST: Verifica lettura -> valore: 0x%02X (%d)", verify_value, verify_value);
                    
                    if (verify_value == modified_value) {
                        ESP_LOGI("INIT", "[M] EEPROM TEST: ✅ Verifica SUCCESSO - dato corretto");
                        
                        // Ripristina valore originale
                        esp_err_t restore_ret = eeprom_24lc16_write_byte(400, original_value);
                        if (restore_ret == ESP_OK) {
                            ESP_LOGI("INIT", "[M] EEPROM TEST: ✅ Valore originale ripristinato con successo");
                        } else {
                            ESP_LOGE("INIT", "[M] EEPROM TEST: ❌ Errore ripristino valore originale: %s", esp_err_to_name(restore_ret));
                        }
                    } else {
                        ESP_LOGE("INIT", "[M] EEPROM TEST: ❌ Verifica FALLITA - atteso: 0x%02X, letto: 0x%02X", modified_value, verify_value);
                    }
                } else {
                    ESP_LOGE("INIT", "[M] EEPROM TEST: ❌ Errore verifica lettura: %s", esp_err_to_name(verify_ret));
                }
            } else {
                ESP_LOGE("INIT", "[M] EEPROM TEST: ❌ Errore scrittura: %s", esp_err_to_name(write_ret));
            }
        } else {
            ESP_LOGE("INIT", "[M] EEPROM TEST: ❌ Errore lettura iniziale: %s", esp_err_to_name(read_ret));
        }*/
    } else {
        s_eeprom_available = false;
        ESP_LOGW("INIT",
                 "[M] EEPROM init: verifica lettura fallita su indirizzo 0x%02X (%s)",
                 detected_addr,
                 esp_err_to_name(ret));
        cleanup_dev_handles();
    }
       
    return ESP_OK; 
}

bool eeprom_24lc16_is_available(void) {
#if F_EEPROM == 1
    return false;
#endif
    return s_eeprom_available;
}


 #if F_EEPROM == 1
esp_err_t eeprom_24lc16_read(uint16_t address, uint8_t *buffer, size_t length) {
    return ESP_ERR_NOT_SUPPORTED;
}
#else
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

        esp_err_t ret = i2c_master_transmit_receive(dev, &mem_addr, 1, ptr, to_read, EEPROM_RW_TIMEOUT_MS);
        if (ret != ESP_OK) return ret;

        left -= to_read;
        cur_addr += to_read;
        ptr += to_read;
    }

    return ESP_OK;
}
#endif


#if F_EEPROM == 1
esp_err_t eeprom_24lc16_write(uint16_t address, const uint8_t *buffer, size_t length) {
    return ESP_ERR_NOT_SUPPORTED;
}
#else
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

        esp_err_t ret = i2c_master_transmit(dev, write_buf, to_write + 1, EEPROM_RW_TIMEOUT_MS);
        if (ret != ESP_OK) return ret;

        // Attesa ciclo interno EEPROM (datasheet max 5ms)
        vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_CYCLE_DELAY_MS));

        left -= to_write;
        cur_addr += to_write;
        ptr += to_write;
    }

    return ESP_OK;
}
#endif


esp_err_t eeprom_24lc16_read_byte(uint16_t address, uint8_t *val) {
    return eeprom_24lc16_read(address, val, 1);
}

esp_err_t eeprom_24lc16_write_byte(uint16_t address, uint8_t val) {
    return eeprom_24lc16_write(address, &val, 1);
}
