#include "pwm.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "device_config.h"

static const char *TAG = "PWM_CTRL";

esp_err_t pwm_set_duty(int channel, int duty_percent) {
    if (duty_percent < 0) duty_percent = 0;
    if (duty_percent > 100) duty_percent = 100;

    // Con risoluzione a 10 bit, il valore massimo è 1023
    uint32_t duty = (duty_percent * 1023) / 100;
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (ret != ESP_OK) return ret;
    
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}

int pwm_get_duty(int channel) {
    uint32_t duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    // Riconverti in percentuale (risoluzione 10 bit = 1023)
    return (int)((duty * 100) / 1023);
}

esp_err_t pwm_init(void) {
    device_config_t *cfg = device_config_get();

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_cfg);

    ledc_channel_config_t ch0 = {
        .gpio_num = CONFIG_APP_PWM_OUT1_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ch0);

    ledc_channel_config_t ch1 = {
        .gpio_num = CONFIG_APP_PWM_OUT2_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ch1);
    
    // Applica luminosità LCD (assumiamo canale 0)
    pwm_set_duty(0, cfg->display.lcd_brightness);
    
    ESP_LOGI(TAG, "[C] PWM inizializzato (Canale 0 set a %d%%)", cfg->display.lcd_brightness);
    return ESP_OK;
}
