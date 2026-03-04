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
#define EEPROM_WRITE_DELAY_MS 8
#define EEPROM_WRITE_RETRY_MAX 4
#define EEPROM_WRITE_RETRY_DELAY_MS 2
#define EEPROM_READY_POLL_TIMEOUT_MS 25
#define EEPROM_READY_POLL_STEP_MS 1

static i2c_master_dev_handle_t eeprom_dev_handles[8];

static bool s_eeprom_available = false;


/**
 * @brief Attende che la EEPROM torni pronta dopo un page-write.
 *
 * Usa polling ACK inviando il solo indirizzo memoria del blocco corrente.
 */
static esp_err_t eeprom_wait_ready(i2c_master_dev_handle_t dev, uint8_t mem_addr)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(EEPROM_READY_POLL_TIMEOUT_MS);
    esp_err_t last_err = ESP_FAIL;

    while ((xTaskGetTickCount() - start) <= timeout_ticks) {
        last_err = i2c_master_transmit(dev, &mem_addr, 1, pdMS_TO_TICKS(20));
        if (last_err == ESP_OK) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(EEPROM_READY_POLL_STEP_MS));
    }

    return last_err;
}


/**
 * @brief Ottiene l'indirizzo di memoria.
 * 
 * @param [in] address L'indirizzo di memoria da ottenere.
 * @return uint8_t Il byte contenente l'indirizzo di memoria.
 */
static uint8_t get_mem_addr(uint16_t address) {
    // L'indirizzo interno è di 8 bit (0-255) per blocco
    return (uint8_t)(address & 0xFF);
}


/**
 * @brief Inizializza il dispositivo EEPROM 24LC16.
 * 
 * Questa funzione inizializza il dispositivo EEPROM 24LC16 collegato al sistema.
 * 
 * @return esp_err_t
 * @retval ESP_OK Se l'inizializzazione è stata completata con successo.
 * @retval ESP_FAIL Se l'inizializzazione ha fallito.
 */
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


/**
 * @brief Controlla se l'EEPROM 24LC16 è disponibile.
 * 
 * Questa funzione verifica la presenza dell'EEPROM 24LC16 collegata al sistema.
 * 
 * @return true se l'EEPROM è disponibile, false altrimenti.
 */
bool eeprom_24lc16_is_available(void) {
    return s_eeprom_available;
}


/**
 * @brief Legge dati dalla EEPROM 24LC16.
 * 
 * @param address L'indirizzo di partenza dalla quale leggere i dati.
 * @param buffer Il buffer in cui memorizzare i dati letti.
 * @param length Il numero di byte da leggere.
 * @return esp_err_t Errore generato dalla funzione.
 */
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


/**
 * @brief Scrive dati in EEPROM 24LC16.
 * 
 * @param address L'indirizzo di partenza in EEPROM dove i dati saranno scritti.
 * @param buffer Puntatore al buffer contenente i dati da scrivere.
 * @param length Il numero di byte da scrivere.
 * @return esp_err_t Errore generato dalla funzione.
 */
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

        esp_err_t ret = ESP_FAIL;
        for (int attempt = 0; attempt < EEPROM_WRITE_RETRY_MAX; attempt++) {
            ret = i2c_master_transmit(dev, write_buf, to_write + 1, pdMS_TO_TICKS(120));
            if (ret == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_DELAY_MS));
                ret = eeprom_wait_ready(dev, mem_addr);
                if (ret == ESP_OK) {
                    break;
                }
            }

            if (attempt + 1 < EEPROM_WRITE_RETRY_MAX) {
                vTaskDelay(pdMS_TO_TICKS(EEPROM_WRITE_RETRY_DELAY_MS));
            }
        }

        if (ret != ESP_OK) {
            ESP_LOGE(TAG,
                     "[C] EEPROM write fallita: addr=0x%03X len=%u dev=0x%02X err=%s",
                     (unsigned int)cur_addr,
                     (unsigned int)to_write,
                     (unsigned int)(EEPROM_BASE_ADDR | dev_idx),
                     esp_err_to_name(ret));
            return ret;
        }

        left -= to_write;
        cur_addr += to_write;
        ptr += to_write;
    }

    return ESP_OK;
}


/**
 * @brief Legge un byte dall'EEPROM 24LC16.
 * 
 * @param address L'indirizzo dell'EEPROM da cui leggere il byte.
 * @param val Puntatore al byte letto.
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t eeprom_24lc16_read_byte(uint16_t address, uint8_t *val) {
    return eeprom_24lc16_read(address, val, 1);
}


/**
 * @brief Scrive un byte nell'EEPROM 24LC16.
 * 
 * @param address L'indirizzo dell'EEPROM dove scrivere il byte.
 * @param val Il byte da scrivere nell'EEPROM.
 * @return esp_err_t Errore generato durante la scrittura.
 */
esp_err_t eeprom_24lc16_write_byte(uint16_t address, uint8_t val) {
    return eeprom_24lc16_write(address, &val, 1);
}
