#include "pwm_test.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "PWM_TEST";

static void pwm_sweep_task(void *arg) {
    int channel = (int)arg;
    ledc_channel_t ch = (channel == 1) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
    ESP_LOGI(TAG, "Avvio Sweep PWM canale %d", channel);
    
    while(1) {
        for (int freq = 1000; freq <= 10000; freq += 1000) {
            ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
            ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, 512); // 50%
            ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

static TaskHandle_t s_pwm1_h = NULL, s_pwm2_h = NULL;

esp_err_t pwm_test_start(int channel) {
    if (channel == 1) xTaskCreate(pwm_sweep_task, "p1test", 2048, (void*)1, 5, &s_pwm1_h);
    else xTaskCreate(pwm_sweep_task, "p2test", 2048, (void*)2, 5, &s_pwm2_h);
    return ESP_OK;
}

esp_err_t pwm_test_stop(int channel) {
    if (channel == 1 && s_pwm1_h) { vTaskDelete(s_pwm1_h); s_pwm1_h = NULL; }
    if (channel == 2 && s_pwm2_h) { vTaskDelete(s_pwm2_h); s_pwm2_h = NULL; }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (channel==1)?LEDC_CHANNEL_0:LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (channel==1)?LEDC_CHANNEL_0:LEDC_CHANNEL_1);
    return ESP_OK;
}
