#include "aux_gpio.h"
#include "driver/gpio.h"
#include "device_config.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "AUX_GPIO";

#ifndef DNA_GPIO
#define DNA_GPIO 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_GPIO == 0

esp_err_t aux_gpio_init(void)
{
    device_config_t *cfg = device_config_get();
    
    gpio_config_t io_conf = {0};
    
    // GPIO 33
    if (cfg->gpios.gpio33.mode == GPIO_CFG_MODE_OUTPUT) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = 0;
        io_conf.pull_down_en = 0;
    } else {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = (cfg->gpios.gpio33.mode == GPIO_CFG_MODE_INPUT_PULLUP);
        io_conf.pull_down_en = (cfg->gpios.gpio33.mode == GPIO_CFG_MODE_INPUT_PULLDOWN);
    }
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_33);
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    if (cfg->gpios.gpio33.mode == GPIO_CFG_MODE_OUTPUT) {
        gpio_set_level(GPIO_NUM_33, cfg->gpios.gpio33.initial_state);
    }
    
    ESP_LOGI(TAG, "Configured GPIO33 (mode %d)", (int)cfg->gpios.gpio33.mode);
    return ESP_OK;
}

esp_err_t aux_gpio_get_all_json(char *json_buffer, size_t max_len)
{
    device_config_t *cfg = device_config_get();
    cJSON *root = cJSON_CreateObject();
    
    // GPIO 33
    cJSON *g33 = cJSON_CreateObject();
    cJSON_AddNumberToObject(g33, "mode", cfg->gpios.gpio33.mode);
    cJSON_AddNumberToObject(g33, "level", gpio_get_level(GPIO_NUM_33));
    cJSON_AddItemToObject(root, "33", g33);
    
    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        strncpy(json_buffer, json, max_len);
        free(json);
        cJSON_Delete(root);
        return ESP_OK;
    }
    
    cJSON_Delete(root);
    return ESP_FAIL;
}

esp_err_t aux_gpio_set_level(int pin, int level)
{
    if (pin == 33) {
        return gpio_set_level((gpio_num_t)pin, level);
    }
    return ESP_ERR_INVALID_ARG;
}

#endif /* DNA_GPIO == 0 */

/*
 * Mockup — nessun GPIO reale configurato.
 * Attiva quando DNA_GPIO == 1
 */
#if defined(DNA_GPIO) && (DNA_GPIO == 1)

esp_err_t aux_gpio_init(void)
{
    ESP_LOGI(TAG, "[C] [MOCK] aux_gpio_init: GPIO simulati");
    return ESP_OK;
}

esp_err_t aux_gpio_get_all_json(char *json_buffer, size_t max_len)
{
    const char *mock_json = "{\"33\":{\"mode\":0,\"level\":0}}";
    strncpy(json_buffer, mock_json, max_len);
    json_buffer[max_len - 1] = '\0';
    ESP_LOGI(TAG, "[C] [MOCK] aux_gpio_get_all_json");
    return ESP_OK;
}

esp_err_t aux_gpio_set_level(int pin, int level)
{
    ESP_LOGI(TAG, "[C] [MOCK] aux_gpio_set_level: pin=%d level=%d", pin, level);
    return ESP_OK;
}

#endif /* DNA_GPIO == 1 */
