#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * @brief Struttura di configurazione Ethernet
 */
typedef struct {
    bool enabled;           ///< Ethernet abilitato
    bool dhcp_enabled;      ///< DHCP abilitato
    char ip[16];           ///< Indirizzo IP statico
    char subnet[16];       ///< Subnet mask
    char gateway[16];      ///< Gateway
} device_eth_config_t;

/**
 * @brief Struttura di configurazione WiFi
 */
typedef struct {
    bool sta_enabled;       ///< STA abilitato
    bool dhcp_enabled;      ///< DHCP abilitato
    char ssid[32];         ///< SSID WiFi
    char password[64];     ///< Password WiFi
    char ip[16];           ///< Indirizzo IP statico
    char subnet[16];       ///< Subnet mask
    char gateway[16];      ///< Gateway
} device_wifi_config_t;

/**
 * @brief Struttura di configurazione sensori
 */
typedef struct {
    bool io_expander_enabled;   ///< I/O Expander
    bool temperature_enabled;   ///< Sensore temperatura
    bool led_enabled;           ///< LED strip
    bool rs232_enabled;         ///< UART RS232
    bool rs485_enabled;         ///< UART RS485
    bool mdb_enabled;           ///< UART MDB
    bool pwm1_enabled;          ///< PWM1
    bool pwm2_enabled;          ///< PWM2
} device_sensors_config_t;

/**
 * @brief Struttura di configurazione MDB
 */
typedef struct {
    bool coin_acceptor_en;      ///< Gettoniera (0x08)
    bool bill_validator_en;     ///< Lettore banconote (0x30)
    bool cashless_en;           ///< Lettore carte/NFC (0x10)
} device_mdb_config_t;

/**
 * @brief Struttura di configurazione Display
 */
typedef struct {
    uint8_t lcd_brightness;     ///< Luminosità LCD (0-100)
} device_display_config_t;

/**
 * @brief Configurazione generale device
 */
typedef struct {
    char device_name[32];       ///< Nome del dispositivo
    bool updated;               ///< Indica se la configurazione è stata aggiornata
    device_eth_config_t eth;
    device_wifi_config_t wifi;
    device_sensors_config_t sensors;
    device_mdb_config_t mdb;
    device_display_config_t display;
} device_config_t;

/**
 * @brief Inizializza il sistema di configurazione
 * @return ESP_OK se riuscito
 */
esp_err_t device_config_init(void);

/**
 * @brief Carica la configurazione da NVS
 * @param config Puntatore a struttura di configurazione
 * @return ESP_OK se riuscito
 */
esp_err_t device_config_load(device_config_t *config);

/**
 * @brief Salva la configurazione in NVS
 * @param config Puntatore a struttura di configurazione
 * @return ESP_OK se riuscito
 */
esp_err_t device_config_save(const device_config_t *config);

/**
 * @brief Ottiene la configurazione corrente
 * @return Puntatore alla configurazione (globale)
 */
device_config_t* device_config_get(void);

/**
 * @brief Reset configurazione ai valori di default
 * @return ESP_OK se riuscito
 */
esp_err_t device_config_reset_defaults(void);

/**
 * @brief Riavvia il dispositivo in modalità Factory
 */
void device_config_reboot_factory(void);

/**
 * @brief Riavvia il dispositivo in modalità Produzione (OTA_0)
 */
void device_config_reboot_app(void);

/**
 * @brief Ritorna una stringa descrittiva dell'app attualmente avviata
 */
const char* device_config_get_running_app_name(void);
