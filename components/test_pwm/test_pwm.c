#include "test_pwm.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TEST_PWM";

void test_pwm_task(void *arg) {
    int channel_num = (int)arg;
    ledc_channel_t ch = (channel_num == 1) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
    int freqs[] = {100, 1000, 10000};
    ESP_LOGI(TAG, "PWM%d Test: Avvio", channel_num);
    
    while(1) {
        for (int f=0; f<3; f++) {
            ESP_LOGI(TAG, "PWM%d - Frequenza: %d Hz", channel_num, freqs[f]);
            ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freqs[f]);
            for (int i=50; i>=0; i--) { 
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, (i*255)/100); 
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ch); 
                vTaskDelay(pdMS_TO_TICKS(50)); 
            }
            for (int i=0; i<=100; i++) { 
                ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, (i*255)/100); 
                ledc_update_duty(LEDC_LOW_SPEED_MODE, ch); 
                vTaskDelay(pdMS_TO_TICKS(50)); 
            }
        }
    }
}
