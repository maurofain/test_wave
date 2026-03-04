#include "periph_i2c.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "PERIPH_I2C";

static i2c_master_bus_handle_t s_periph_i2c_handle = NULL;


/**
 * @brief Inizializza il periferico I2C.
 * 
 * Questa funzione inizializza il periferico I2C e restituisce un errore se già inizializzato.
 * 
 * @param [in] Nessun parametro di input.
 * @return esp_err_t Errore generato dalla funzione.
 *         - ESP_OK: Inizializzazione avvenuta con successo.
 *         - ESP_FAIL: Inizializzazione fallita perché il periferico è già inizializzato.
 */
esp_err_t periph_i2c_init(void)
{
    if (s_periph_i2c_handle != NULL) {
        return ESP_OK; /* già inizializzato */
    }

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port        = CONFIG_APP_I2C_PORT,
        .sda_io_num      = CONFIG_APP_I2C_SDA_GPIO,
        .scl_io_num      = CONFIG_APP_I2C_SCL_GPIO,
        .clk_source      = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_periph_i2c_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Inizializzazione bus I2C periferiche (port=%d SDA=%d SCL=%d) fallita: %s",
                 CONFIG_APP_I2C_PORT, CONFIG_APP_I2C_SDA_GPIO, CONFIG_APP_I2C_SCL_GPIO,
                 esp_err_to_name(ret));
        s_periph_i2c_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "Bus I2C periferiche inizializzato: port=%d SDA=GPIO%d SCL=GPIO%d @%dHz",
             CONFIG_APP_I2C_PORT, CONFIG_APP_I2C_SDA_GPIO, CONFIG_APP_I2C_SCL_GPIO,
             CONFIG_APP_I2C_CLOCK_HZ);
    return ESP_OK;
}


/**
 * @brief Ottiene il handle del bus I2C master.
 *
 * Questa funzione restituisce il handle del bus I2C master utilizzato per
 * le operazioni di comunicazione I2C.
 *
 * @return i2c_master_bus_handle_t Handle del bus I2C master.
 */
i2c_master_bus_handle_t periph_i2c_get_handle(void)
{
    return s_periph_i2c_handle;
}
