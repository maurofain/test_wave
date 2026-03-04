#include "pwm_test.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "PWM_TEST";


/** 
 * @brief Gestisce il task di sweep PWM.
 * 
 * Questa funzione esegue un sweep PWM sui canali specificati.
 * 
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 * 
 * @return Nessun valore di ritorno.
 */
static void pwm_sweep_task(void *arg) {
    int channel = (int)arg;
    ledc_channel_t ch = (channel == 1) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
    ESP_LOGI(TAG, "Avvio scansione PWM canale %d", channel);
    
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


/**
 * @brief Avvia il test del PWM.
 * 
 * @param channel [in] Il canale del PWM da utilizzare.
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t pwm_test_start(int channel) {
    if (channel == 1) xTaskCreate(pwm_sweep_task, "p1test",
            /* lots of delay loops but also logging; allocate more to be safe */
            4096, (void*)1, 5, &s_pwm1_h);
    else xTaskCreate(pwm_sweep_task, "p2test",
            4096, (void*)2, 5, &s_pwm2_h);
    return ESP_OK;
}


/**
 * @brief Arresta il PWM sul canale specificato.
 * 
 * @param [in] channel Numero del canale PWM da arrestare. Il canale 1 è gestito da s_pwm1_h.
 * @return esp_err_t Codice di errore che indica il successo o la fallita dell'operazione.
 */
esp_err_t pwm_test_stop(int channel) {
    if (channel == 1 && s_pwm1_h) { vTaskDelete(s_pwm1_h); s_pwm1_h = NULL; }
    if (channel == 2 && s_pwm2_h) { vTaskDelete(s_pwm2_h); s_pwm2_h = NULL; }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (channel==1)?LEDC_CHANNEL_0:LEDC_CHANNEL_1, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (channel==1)?LEDC_CHANNEL_0:LEDC_CHANNEL_1);
    return ESP_OK;
}


/**
 * @brief Imposta i parametri del PWM per un canale specifico.
 * 
 * @param [in] channel Numero del canale PWM da configurare.
 * @param [in] freq Frequenza del segnale PWM in Hertz.
 * @param [in] duty_percent Percentuale del ciclo di impulso del segnale PWM.
 * @return esp_err_t Codice di errore che indica il successo o la fallita dell'operazione.
 */
esp_err_t pwm_test_set_param(int channel, uint32_t freq, uint32_t duty_percent) {
    // Ferma i test automatici se attivi
    pwm_test_stop(channel);
    
    if (duty_percent > 100) duty_percent = 100;
    
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq);
    
    ledc_channel_t ch = (channel == 1) ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1;
    uint32_t duty_val = (duty_percent * 1023) / 100;
    
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty_val);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
    
    ESP_LOGI(TAG, "PWM manuale canale %d: Freq=%luHz, Duty=%lu%%", channel, freq, duty_percent);
    return ESP_OK;
}
