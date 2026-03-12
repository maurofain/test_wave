#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Struttura di configurazione Ethernet
 */
typedef struct {
    bool enabled;           ///< Ethernet abilitato
    bool dhcp_enabled;      ///< DHCP abilitato
    char ip[16];           ///< Indirizzo IP statico
    char subnet[16];       ///< Subnet mask
    char gateway[16];      ///< Gateway
    char dns1[16];         ///< DNS primario
    char dns2[16];         ///< DNS secondario
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
    uint32_t led_count;         ///< Numero di LED nella striscia
    bool rs232_enabled;         ///< UART RS232
    bool rs485_enabled;         ///< UART RS485
    bool mdb_enabled;           ///< UART MDB
    bool cctalk_enabled;        ///< CCtalk (uses same UART as RS232)
    bool eeprom_enabled;        ///< EEPROM 24LC16 (visibilità UI/test)
    bool pwm1_enabled;          ///< PWM1
    bool pwm2_enabled;          ///< PWM2
    bool sd_card_enabled;       ///< SD Card
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
    bool enabled;              ///< Display abilitato (true = display on, false = headless)
    uint8_t lcd_brightness;     ///< Luminosità LCD (0-100)
} device_display_config_t;

/**
 * @brief Struttura di configurazione Scanner USB
 */
typedef struct {
    bool enabled;           ///< Scanner abilitato
    uint16_t vid;           ///< Vendor ID
    uint16_t pid;           ///< Product ID
    uint16_t dual_pid;      ///< Dual/alternate PID
    uint32_t cooldown_ms;   ///< Attesa post-lettura per evitare scansioni multiple
} device_scanner_config_t;

/**
 * @brief Struttura timeout applicativi
 */
typedef struct {
    uint32_t language_return_ms;     ///< Timeout inattività per ritorno a pagina lingua
    uint32_t idle_before_ads_ms;     ///< Timeout inattività prima di mostrare ads (default 60000ms=60s)
    uint32_t ad_rotation_ms;         ///< Intervallo rotazione immagini ads (default 30000ms=30s)
    uint32_t credit_reset_timeout_ms; ///< Timeout per reset crediti CVD/ECD (default 300000ms=5min)
} device_timeouts_config_t;

/**
 * @brief Struttura di configurazione UART (RS232/RS485/MDB)
 */
typedef struct {
    int baud_rate;
    int data_bits;      ///< UART_DATA_8_BITS, etc.
    int parity;         ///< 0: None, 1: Odd, 2: Even
    int stop_bits;      ///< 1, 2, 3 (1.5)
    int rx_buf_size;    ///< Consigliato > 128 (es. 1024 o 2048)
    int tx_buf_size;    ///< 0: Modalità bloccante (consigliato per RS485); > 0: Modalità asincrona in RAM
} device_serial_config_t;

/**
 * @brief Struttura di configurazione Modbus RTU su RS485
 */
typedef struct {
    bool enabled;            ///< Abilita servizio Modbus RTU Master
    uint8_t slave_id;        ///< Indirizzo slave Modbus (1..255)
    uint16_t poll_ms;        ///< Intervallo polling task RS485/Modbus
    uint16_t timeout_ms;     ///< Timeout logico richiesta (uso applicativo)
    uint8_t retries;         ///< Numero retry in caso di errore/timeout
    uint16_t relay_start;    ///< Registro coil iniziale (base 0)
    uint16_t relay_count;    ///< Numero coil da gestire
    uint16_t input_start;    ///< Registro DI iniziale (base 0)
    uint16_t input_count;    ///< Numero ingressi discreti da leggere
} device_modbus_config_t;

/**
 * @brief Struttura di configurazione CCtalk
 */
typedef struct {
    uint8_t address;         ///< Indirizzo gettoniera CCtalk (1..255)
} device_cctalk_config_t;

/**
 * @brief Configurazione GPIO configurabili (GPIO33)
 */
typedef enum {
    GPIO_CFG_MODE_INPUT_FLOAT = 0,
    GPIO_CFG_MODE_INPUT_PULLUP = 1,
    GPIO_CFG_MODE_INPUT_PULLDOWN = 2,
    GPIO_CFG_MODE_OUTPUT = 3
} device_gpio_cfg_mode_t;

typedef struct {
    device_gpio_cfg_mode_t mode;    ///< Modalità (IN/OUT/Pull)
    bool initial_state;             ///< Stato iniziale se OUTPUT
} device_gpio_pin_config_t;

typedef struct {
    device_gpio_pin_config_t gpio33; ///< GPIO 33
} device_gpios_config_t;

/**
 * @brief Struttura di configurazione Server/Cloud
 */
typedef struct {
    bool enabled;           ///< Abilita invii al server cloud
    char url[128];          ///< Base URL del server (es. http://host:port/)
    char serial[64];        ///< Seriale usata per il login verso il server remoto
    char password[64];      ///< Password (MD5) usata per il login verso il server remoto
} device_server_config_t;

/**
 * @brief Struttura di configurazione NTP
 */
typedef struct {
    char server1[64];       ///< Server NTP primario
    char server2[64];       ///< Server NTP secondario
    int timezone_offset;    ///< Offset timezone in ore (-12 a +12)
} device_ntp_config_t;

/**
 * @brief Struttura di configurazione Logging Remoto
 */
typedef struct {
    uint16_t server_port;   ///< Porta UDP per broadcast
    bool use_broadcast;     ///< Invia in broadcast UDP
    bool write_to_sd;       ///< Salva anche i log su SD
} device_remote_log_config_t;

/**
 * @brief Configurazione testi UI per multilingua
 *
 * Now supports two separate language selectors:
 * - user_language: language used by the LVGL user panel
 * - backend_language: language used by the backend / web UI
 */
typedef struct {
    char user_language[8];     ///< Lingua Pannello Utente (es: "it", "en")
    char backend_language[8];  ///< Lingua Backend/Web UI (es: "it", "en")
    char texts_json[512];      ///< Deprecated: non usato (i18n ora su SPIFFS per lingua)
} device_ui_texts_config_t;

/**
 * @brief Struttura di configurazione generale device
 */
typedef struct {
    char device_name[32];       ///< Nome del dispositivo
    char location_name[64];     ///< Nome dell'impianto / locale commerciale
    bool updated;               ///< Indica se la configurazione è stata aggiornata
    uint8_t  num_programs;      ///< Numero pulsanti programma sul pannello (valori ammessi: 1,2,4,6,8,10)
    double   latitude;          ///< Latitudine geografica impianto (gradi decimali, -90..+90)
    double   longitude;         ///< Longitudine geografica impianto (gradi decimali, -180..+180)
    device_eth_config_t eth;
    device_wifi_config_t wifi;
    bool ntp_enabled;           ///< NTP abilitato
    device_ntp_config_t ntp;
    device_server_config_t server;          ///< Configurazione Server/Cloud (base URL + abilitazione)
    device_remote_log_config_t remote_log;  ///< Configurazione logging remoto
    device_sensors_config_t sensors;
    device_mdb_config_t mdb;
    device_display_config_t display;
    device_scanner_config_t scanner;    ///< Configurazione Scanner USB
    device_timeouts_config_t timeouts;  ///< Timeout applicativi
    device_serial_config_t rs232;       ///< Configurazione RS232
    device_serial_config_t rs485;       ///< Configurazione RS485
    device_modbus_config_t modbus;      ///< Configurazione Modbus RTU su RS485
    device_cctalk_config_t cctalk;      ///< Configurazione CCtalk
    device_serial_config_t mdb_serial;  ///< Configurazione Seriale MDB
    device_gpios_config_t gpios;        ///< Configurazione GPIO extra
    device_ui_texts_config_t ui;        ///< Configurazione testi UI multilingua
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
 * @brief Converte la configurazione in JSON
 * @param config Puntatore alla configurazione
 * @return Stringa JSON allocata (da liberare con free())
 */
char* device_config_to_json(const device_config_t *config);

/**
 * @brief Legge il JSON configurazione direttamente da EEPROM (senza fallback NVS)
 * @return Stringa JSON allocata (da liberare con free()) oppure NULL se EEPROM non valida/non disponibile
 */
char* device_config_read_json_from_eeprom(void);

/**
 * @brief Ottiene la configurazione corrente
 * @return Puntatore alla configurazione (globale)
 */
device_config_t* device_config_get(void);
/**
 * @brief Ottiene il CRC32 della configurazione corrente
 * @return Valore CRC32
 */
uint32_t device_config_get_crc(void);

/**
 * @brief Verifica se la configurazione è stata aggiornata
 * @return true se aggiornata
 */
bool device_config_is_updated(void);
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
 * @brief Riavvia il dispositivo sull'ultima app (OTA_0/OTA_1) usata come boot
 */
void device_config_reboot_app_last(void);

/**
 * @brief Riavvia il dispositivo forzando il boot su OTA_0
 */
void device_config_reboot_ota0(void);

/**
 * @brief Riavvia il dispositivo forzando il boot su OTA_1
 */
void device_config_reboot_ota1(void);

/**
 * @brief Ritorna una stringa descrittiva dell'app attualmente avviata
 */
const char* device_config_get_running_app_name(void);

/**
 * @brief Restituisce la lingua UI corrente (compat, ritorna user_language)
 */
const char* device_config_get_ui_language(void);

/**
 * @brief Restituisce la lingua usata dal pannello utente (LVGL)
 */
const char* device_config_get_ui_user_language(void);

/**
 * @brief Restituisce la lingua usata dal backend / web UI
 */
const char* device_config_get_ui_backend_language(void);

/**
 * @brief Restituisce la tabella i18n (array record) della lingua richiesta, letta da SPIFFS
 * @param language Codice lingua ISO 2 caratteri (NULL = lingua corrente)
 * @return JSON allocato dinamicamente (free() a carico chiamante), oppure NULL su errore
 */
char* device_config_get_ui_texts_records_json(const char *language);

/**
 * @brief Salva la tabella i18n (array record) su SPIFFS nel file della lingua indicata
 * @param language Codice lingua ISO 2 caratteri
 * @param records_json JSON array di record i18n.
 *        Formato principale: {scope:<uint8>, key:<uint16>, section:<uint8>, text:<string>}.
 *        Compatibilità legacy mantenuta per record testuali.
 * @return ESP_OK se salvato correttamente
 */
esp_err_t device_config_set_ui_texts_records_json(const char *language, const char *records_json);

/**
 * @brief Recupera testo i18n da scope+key per la lingua corrente
 * @param scope Ambito (es. nav, lvgl, p_config)
 * @param key Chiave univoca nello scope
 * @param fallback Testo fallback
 * @param out Buffer destinazione
 * @param out_len Dimensione buffer destinazione
 * @return ESP_OK se trovata, ESP_ERR_NOT_FOUND se usa fallback
 */
esp_err_t device_config_get_ui_text_scoped(const char *scope, const char *key, const char *fallback, char *out, size_t out_len);
