#include "device_config.h"
#include "esp_log.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "DEVICE_CFG";
static const char *NVS_NAMESPACE = "device_config";

static device_config_t s_config = {0};
static bool s_initialized = false;

// Default configuration
static void _set_defaults(device_config_t *config)
{
    // Ethernet defaults
    config->eth.enabled = true;
    config->eth.dhcp_enabled = true;
    strncpy(config->eth.ip, "192.168.1.100", sizeof(config->eth.ip) - 1);
    strncpy(config->eth.subnet, "255.255.255.0", sizeof(config->eth.subnet) - 1);
    strncpy(config->eth.gateway, "192.168.1.1", sizeof(config->eth.gateway) - 1);

    // WiFi defaults
    config->wifi.sta_enabled = false;
    config->wifi.dhcp_enabled = true;
    strncpy(config->wifi.ssid, "", sizeof(config->wifi.ssid) - 1);
    strncpy(config->wifi.password, "", sizeof(config->wifi.password) - 1);
    strncpy(config->wifi.ip, "192.168.1.101", sizeof(config->wifi.ip) - 1);
    strncpy(config->wifi.subnet, "255.255.255.0", sizeof(config->wifi.subnet) - 1);
    strncpy(config->wifi.gateway, "192.168.1.1", sizeof(config->wifi.gateway) - 1);

    // Sensors defaults (all enabled by default)
    config->sensors.io_expander_enabled = true;
    config->sensors.temperature_enabled = true;
    config->sensors.led_enabled = true;
    config->sensors.rs232_enabled = true;
    config->sensors.rs485_enabled = true;
    config->sensors.mdb_enabled = true;
    config->sensors.pwm1_enabled = true;
    config->sensors.pwm2_enabled = true;

    // MDB defaults
    config->mdb.coin_acceptor_en = true;
    config->mdb.bill_validator_en = false;
    config->mdb.cashless_en = false;
}

esp_err_t device_config_init(void)
{
    ESP_LOGI(TAG, "[C] Inizializzazione sistema configurazione");
    _set_defaults(&s_config);
    s_initialized = true;
    return device_config_load(&s_config);
}

esp_err_t device_config_load(device_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    
    // Se il namespace non esiste al primo avvio, usa i default
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "[C] Namespace NVS non trovato, uso configurazione default");
        _set_defaults(config);
        return ESP_OK;
    }
    
    ESP_RETURN_ON_ERROR(ret, TAG, "NVS open failed");

    // Leggi JSON dalla NVS
    size_t json_size = 0;
    char *json_str = NULL;
    
    ret = nvs_get_str(handle, "config_json", NULL, &json_size);
    if (ret == ESP_OK && json_size > 0) {
        json_str = malloc(json_size);
        if (json_str) {
            nvs_get_str(handle, "config_json", json_str, &json_size);
            
            cJSON *root = cJSON_Parse(json_str);
            if (root) {
                // Parse Ethernet config
                cJSON *eth_obj = cJSON_GetObjectItem(root, "eth");
                if (eth_obj) {
                    config->eth.enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "enabled"));
                    config->eth.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(eth_obj, "dhcp_enabled"));
                    cJSON *ip = cJSON_GetObjectItem(eth_obj, "ip");
                    if (ip && ip->valuestring) strncpy(config->eth.ip, ip->valuestring, sizeof(config->eth.ip) - 1);
                    cJSON *subnet = cJSON_GetObjectItem(eth_obj, "subnet");
                    if (subnet && subnet->valuestring) strncpy(config->eth.subnet, subnet->valuestring, sizeof(config->eth.subnet) - 1);
                    cJSON *gateway = cJSON_GetObjectItem(eth_obj, "gateway");
                    if (gateway && gateway->valuestring) strncpy(config->eth.gateway, gateway->valuestring, sizeof(config->eth.gateway) - 1);
                }

                // Parse WiFi config
                cJSON *wifi_obj = cJSON_GetObjectItem(root, "wifi");
                if (wifi_obj) {
                    config->wifi.sta_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "sta_enabled"));
                    config->wifi.dhcp_enabled = cJSON_IsTrue(cJSON_GetObjectItem(wifi_obj, "dhcp_enabled"));
                    cJSON *ssid = cJSON_GetObjectItem(wifi_obj, "ssid");
                    if (ssid && ssid->valuestring) strncpy(config->wifi.ssid, ssid->valuestring, sizeof(config->wifi.ssid) - 1);
                    cJSON *password = cJSON_GetObjectItem(wifi_obj, "password");
                    if (password && password->valuestring) strncpy(config->wifi.password, password->valuestring, sizeof(config->wifi.password) - 1);
                    cJSON *ip = cJSON_GetObjectItem(wifi_obj, "ip");
                    if (ip && ip->valuestring) strncpy(config->wifi.ip, ip->valuestring, sizeof(config->wifi.ip) - 1);
                    cJSON *subnet = cJSON_GetObjectItem(wifi_obj, "subnet");
                    if (subnet && subnet->valuestring) strncpy(config->wifi.subnet, subnet->valuestring, sizeof(config->wifi.subnet) - 1);
                    cJSON *gateway = cJSON_GetObjectItem(wifi_obj, "gateway");
                    if (gateway && gateway->valuestring) strncpy(config->wifi.gateway, gateway->valuestring, sizeof(config->wifi.gateway) - 1);
                }

                // Parse Sensors config
                cJSON *sensors_obj = cJSON_GetObjectItem(root, "sensors");
                if (sensors_obj) {
                    config->sensors.io_expander_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "io_expander_enabled"));
                    config->sensors.temperature_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "temperature_enabled"));
                    config->sensors.led_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "led_enabled"));
                    config->sensors.rs232_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs232_enabled"));
                    config->sensors.rs485_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "rs485_enabled"));
                    config->sensors.mdb_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "mdb_enabled"));
                    config->sensors.pwm1_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm1_enabled"));
                    config->sensors.pwm2_enabled = cJSON_IsTrue(cJSON_GetObjectItem(sensors_obj, "pwm2_enabled"));
                }

                // Parse MDB config
                cJSON *mdb_obj = cJSON_GetObjectItem(root, "mdb");
                if (mdb_obj) {
                    config->mdb.coin_acceptor_en = cJSON_IsTrue(cJSON_GetObjectItem(mdb_obj, "coin_en"));
                    config->mdb.bill_validator_en = cJSON_IsTrue(cJSON_GetObjectItem(mdb_obj, "bill_en"));
                    config->mdb.cashless_en = cJSON_IsTrue(cJSON_GetObjectItem(mdb_obj, "cashless_en"));
                }

                cJSON_Delete(root);
                ESP_LOGI(TAG, "[C] Configurazione caricata da NVS");
            }
            free(json_str);
        }
    } else {
        ESP_LOGI(TAG, "[C] Nessuna configurazione trovata, usando defaults");
        _set_defaults(config);
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t device_config_save(const device_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle), TAG, "NVS open failed");

    // Crea JSON
    cJSON *root = cJSON_CreateObject();

    // Ethernet
    cJSON *eth_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(eth_obj, "enabled", config->eth.enabled);
    cJSON_AddBoolToObject(eth_obj, "dhcp_enabled", config->eth.dhcp_enabled);
    cJSON_AddStringToObject(eth_obj, "ip", config->eth.ip);
    cJSON_AddStringToObject(eth_obj, "subnet", config->eth.subnet);
    cJSON_AddStringToObject(eth_obj, "gateway", config->eth.gateway);
    cJSON_AddItemToObject(root, "eth", eth_obj);

    // WiFi
    cJSON *wifi_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_obj, "sta_enabled", config->wifi.sta_enabled);
    cJSON_AddBoolToObject(wifi_obj, "dhcp_enabled", config->wifi.dhcp_enabled);
    cJSON_AddStringToObject(wifi_obj, "ssid", config->wifi.ssid);
    cJSON_AddStringToObject(wifi_obj, "password", config->wifi.password);
    cJSON_AddStringToObject(wifi_obj, "ip", config->wifi.ip);
    cJSON_AddStringToObject(wifi_obj, "subnet", config->wifi.subnet);
    cJSON_AddStringToObject(wifi_obj, "gateway", config->wifi.gateway);
    cJSON_AddItemToObject(root, "wifi", wifi_obj);

    // Sensors
    cJSON *sensors_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(sensors_obj, "io_expander_enabled", config->sensors.io_expander_enabled);
    cJSON_AddBoolToObject(sensors_obj, "temperature_enabled", config->sensors.temperature_enabled);
    cJSON_AddBoolToObject(sensors_obj, "led_enabled", config->sensors.led_enabled);
    cJSON_AddBoolToObject(sensors_obj, "rs232_enabled", config->sensors.rs232_enabled);
    cJSON_AddBoolToObject(sensors_obj, "rs485_enabled", config->sensors.rs485_enabled);
    cJSON_AddBoolToObject(sensors_obj, "mdb_enabled", config->sensors.mdb_enabled);
    cJSON_AddBoolToObject(sensors_obj, "pwm1_enabled", config->sensors.pwm1_enabled);
    cJSON_AddBoolToObject(sensors_obj, "pwm2_enabled", config->sensors.pwm2_enabled);
    cJSON_AddItemToObject(root, "sensors", sensors_obj);

    // MDB
    cJSON *mdb_obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(mdb_obj, "coin_en", config->mdb.coin_acceptor_en);
    cJSON_AddBoolToObject(mdb_obj, "bill_en", config->mdb.bill_validator_en);
    cJSON_AddBoolToObject(mdb_obj, "cashless_en", config->mdb.cashless_en);
    cJSON_AddItemToObject(root, "mdb", mdb_obj);

    char *json_str = cJSON_Print(root);
    if (json_str) {
        nvs_set_str(handle, "config_json", json_str);
        nvs_commit(handle);
        free(json_str);
        ESP_LOGI(TAG, "[C] Configurazione salvata in NVS");
    }

    cJSON_Delete(root);
    nvs_close(handle);
    return ESP_OK;
}

device_config_t* device_config_get(void)
{
    return &s_config;
}

esp_err_t device_config_reset_defaults(void)
{
    _set_defaults(&s_config);
    return device_config_save(&s_config);
}
