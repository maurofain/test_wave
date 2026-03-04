#ifndef DNA_SHT40
#define DNA_SHT40 0
#endif

#include "sht40.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "SHT40";

static bool s_sht40_ready = false;

/* ============================================================
 * Implementazione REALE (hardware I2C)
 * Attiva quando DNA_SHT40 == 0
 * ============================================================ */
#if DNA_SHT40 == 0

#include "periph_i2c.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SHT40_ADDR          0x45  // 7-bit addr (8-bit: 0x8A W, 0x8B R)
#define SHT40_CMD_MEAS      0xFD  // High precision measurement

static i2c_master_dev_handle_t sht_dev;


/**
 * @brief Prepara il dispositivo SHT40 per l'utilizzo.
 *
 * Questa funzione configura il dispositivo SHT40 per iniziare a misurare le temperature e l'umidità.
 *
 * @return esp_err_t
 * @retval ESP_OK Se la preparazione del dispositivo è stata completata con successo.
 * @retval ESP_FAIL Se si è verificato un errore durante la preparazione del dispositivo.
 */
static esp_err_t sht40_prepare_device(void)
{
    i2c_master_bus_handle_t bus = periph_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "[C] Periph I2C bus non inizializzato: chiama periph_i2c_init() prima");
        return ESP_ERR_INVALID_STATE;
    }

    if (sht_dev != NULL) {
        return ESP_OK;
    }

    i2c_device_config_t cfg = {
        .device_address = SHT40_ADDR,
        .scl_speed_hz = CONFIG_APP_I2C_CLOCK_HZ,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &cfg, &sht_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Sensore SHT40 non risponde a 0x%02X: %s", SHT40_ADDR, esp_err_to_name(ret));
        sht_dev = NULL;
        return ret;
    }

    return ESP_OK;
}


/**
 * @brief Disconnette il dispositivo SHT40.
 * 
 * Questa funzione disconnette il dispositivo SHT40 dal sistema.
 * 
 * @param [in/out] sht_dev Puntatore al dispositivo SHT40 da disconnettere.
 * @return Niente.
 */
static void sht40_drop_device(void)
{
    if (sht_dev == NULL) {
        return;
    }
    (void)i2c_master_bus_rm_device(sht_dev);
    sht_dev = NULL;
    s_sht40_ready = false;
}


/**
 * @brief Calcola il valore CRC8 per un blocco di dati.
 * 
 * Questa funzione calcola il valore CRC8 utilizzando il polinomio 0x07.
 * 
 * @param [in] data Puntatore al blocco di dati per cui calcolare il CRC8.
 * @param [in] len Lunghezza del blocco di dati.
 * @return uint8_t Il valore CRC8 calcolato.
 */
static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}


/**
 * @brief Inizializza il sensore SHT40.
 * 
 * Questa funzione inizializza il sensore SHT40 per la misurazione delle temperature e umidità.
 * 
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t sht40_init(void) {
    s_sht40_ready = false;

    esp_err_t ret = sht40_prepare_device();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Soft Reset
    uint8_t cmd_reset = 0x94;
    ESP_LOGI(TAG, "[C] Inizializzazione SHT40: invio Soft Reset (0x94)...");
    ret = i2c_master_transmit(sht_dev, &cmd_reset, 1, pdMS_TO_TICKS(100));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore invio Soft Reset: %s", esp_err_to_name(ret));
        if (ret == ESP_ERR_INVALID_STATE) {
            sht40_drop_device();
        }
        return ret;
    }
    
    // Attesa dopo reset (1ms richiesto da datasheet)
    vTaskDelay(pdMS_TO_TICKS(10));

    s_sht40_ready = true;
    ESP_LOGI(TAG, "[C] Sensore SHT40 trovato e resettato all'indirizzo 0x%02X", SHT40_ADDR);
    return ESP_OK;
}


/**
 * @brief Legge la temperatura e l'umidità dal sensore SHT40.
 * 
 * @param [out] temp Puntatore alla variabile dove verrà memorizzata la temperatura.
 * @param [out] hum Puntatore alla variabile dove verrà memorizzata l'umidità.
 * @return esp_err_t Errore generato dalla funzione.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione fallita.
 *         - ESP_ERR_INVALID_STATE: Il sensore non è pronto o non è stato inizializzato.
 */
esp_err_t sht40_read(float *temp, float *hum) {
    if (!s_sht40_ready || sht_dev == NULL) {
        esp_err_t init_ret = sht40_init();
        if (init_ret != ESP_OK) {
            return init_ret;
        }
    }

    uint8_t cmd = SHT40_CMD_MEAS;
    uint8_t data[6] = {0};

    // Invia comando di misura
    ESP_LOGI(TAG, "[C] Lettura SHT40: invio comando 0x%02X", cmd);
    esp_err_t ret = i2c_master_transmit(sht_dev, &cmd, 1, pdMS_TO_TICKS(100));
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "[C] SHT40 handle non valido, provo re-init");
        sht40_drop_device();
        esp_err_t init_ret = sht40_init();
        if (init_ret == ESP_OK) {
            ret = i2c_master_transmit(sht_dev, &cmd, 1, pdMS_TO_TICKS(100));
        } else {
            ret = init_ret;
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore invio comando: %s", esp_err_to_name(ret));
        return ret;
    }

    // Attesa per la misura (High precision richiede max 10ms)
    vTaskDelay(pdMS_TO_TICKS(20));

    // Legge i 6 byte (Temp MSB, Temp LSB, Temp CRC, Hum MSB, Hum LSB, Hum CRC)
    ret = i2c_master_receive(sht_dev, data, 6, pdMS_TO_TICKS(100));
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "[C] SHT40 receive in invalid state, provo re-init");
        sht40_drop_device();
        esp_err_t init_ret = sht40_init();
        if (init_ret == ESP_OK) {
            ret = i2c_master_transmit(sht_dev, &cmd, 1, pdMS_TO_TICKS(100));
            if (ret == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(20));
                ret = i2c_master_receive(sht_dev, data, 6, pdMS_TO_TICKS(100));
            }
        } else {
            ret = init_ret;
        }
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore ricezione dati: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "[C] Raw data: %02X %02X [%02X] %02X %02X [%02X]",
             data[0], data[1], data[2], data[3], data[4], data[5]);

    if (data[0] == 0xFF && data[1] == 0xFF && data[2] == 0xFF &&
        data[3] == 0xFF && data[4] == 0xFF && data[5] == 0xFF) {
        ESP_LOGW(TAG, "[C] Lettura SHT40 tutta 0xFF: bus/device non pronto");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Verifica CRC Temperatura
    uint8_t calc_crc_t = crc8(data, 2);
    if (calc_crc_t != data[2]) {
        ESP_LOGE(TAG, "[C] Errore CRC Temperatura: calc=0x%02X, recv=0x%02X", calc_crc_t, data[2]);
        return ESP_ERR_INVALID_CRC;
    }

    // Verifica CRC Umidità
    uint8_t calc_crc_h = crc8(data + 3, 2);
    if (calc_crc_h != data[5]) {
        ESP_LOGE(TAG, "[C] Errore CRC Umidità: calc=0x%02X, recv=0x%02X", calc_crc_h, data[5]);
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t raw_temp = (data[0] << 8) | data[1];
    uint16_t raw_hum = (data[3] << 8) | data[4];

    // Formule da datasheet:
    // T = -45 + 175 * raw_temp / 65535
    // RH = -6 + 125 * raw_hum / 65535
    *temp = -45.0f + 175.0f * (float)raw_temp / 65535.0f;
    *hum = -6.0f + 125.0f * (float)raw_hum / 65535.0f;

    ESP_LOGI(TAG, "[C] Valori convertiti: T=%.2f, H=%.2f (raw_t=%u, raw_h=%u)", *temp, *hum, raw_temp, raw_hum);

    // Clipping umidità (0-100%)
    if (*hum < 0) *hum = 0;
    if (*hum > 100) *hum = 100;

    return ESP_OK;
}


/**
 * @brief Controlla se il sensore SHT40 è pronto per l'acquisizione dei dati.
 *
 * @return true se il sensore è pronto, false altrimenti.
 */
bool sht40_is_ready(void)
{
    return s_sht40_ready;
}

#endif /* DNA_SHT40 == 0 */

/* ============================================================
 * Implementazione MOCK (nessun hardware)
 * Attiva quando DNA_SHT40 == 1
 * ============================================================ */
#if defined(DNA_SHT40) && (DNA_SHT40 == 1)

#define SHT40_MOCK_TEMP_C   25.0f
#define SHT40_MOCK_HUM_PCT  50.0f


/**
 * @brief Inizializza il sensore SHT40.
 *
 * Questa funzione inizializza il sensore SHT40 per la misurazione delle temperature e umidità.
 *
 * @return esp_err_t
 * - ESP_OK: Inizializzazione riuscita.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t sht40_init(void)
{
    s_sht40_ready = true;
    ESP_LOGI(TAG, "[MOCK] SHT40 inizializzato (T=%.1f°C, H=%.1f%%)",
             SHT40_MOCK_TEMP_C, SHT40_MOCK_HUM_PCT);
    return ESP_OK;
}


/**
 * @brief Legge la temperatura e l'umidità dal sensore SHT40.
 * 
 * @param [out] temp Puntatore alla variabile dove verrà memorizzata la temperatura in gradi Celsius.
 * @param [out] hum Puntatore alla variabile dove verrà memorizzata l'umidità in percentuale.
 * @return esp_err_t Codice di errore che indica il successo o la causa dell'errore.
 */
esp_err_t sht40_read(float *temp, float *hum)
{
    if (!s_sht40_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (temp) *temp = SHT40_MOCK_TEMP_C;
    if (hum)  *hum  = SHT40_MOCK_HUM_PCT;
    ESP_LOGI(TAG, "[MOCK] sht40_read → T=%.1f°C, H=%.1f%%",
             SHT40_MOCK_TEMP_C, SHT40_MOCK_HUM_PCT);
    return ESP_OK;
}


/**
 * @brief Controlla se il sensore SHT40 è pronto.
 *
 * Questa funzione verifica lo stato del sensore SHT40 per determinare
 * se è pronto a ricevere nuove richieste di misurazione.
 *
 * @return true se il sensore è pronto, false altrimenti.
 */
bool sht40_is_ready(void)
{
    return s_sht40_ready;
}

#endif /* DNA_SHT40 == 1 */
