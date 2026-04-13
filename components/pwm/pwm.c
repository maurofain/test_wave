#include "pwm.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "device_config.h"

static const char *TAG = "PWM_CTRL";

#ifndef DNA_PWM
#define DNA_PWM 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_PWM == 0

/* [C] Tracciamento stato inizializzazione PWM */
static bool s_pwm_initialized = false;


/**
 * @brief Imposta il duty cycle del segnale PWM.
 * 
 * @param channel [in] Il canale PWM su cui impostare il duty cycle.
 * @param duty_percent [in] Il valore del duty cycle in percentuale (0-100).
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t pwm_set_duty(int channel, int duty_percent) {
    if (duty_percent < 0) duty_percent = 0;
    if (duty_percent > 100) duty_percent = 100;

    // Con risoluzione a 10 bit, il valore massimo è 1023
    uint32_t duty = (duty_percent * 1023) / 100;
    
    esp_err_t ret = ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
    if (ret != ESP_OK) return ret;
    
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
}


/**
 * @brief Ottiene il valore del duty cycle per un canale PWM.
 * 
 * @param channel [in] Il numero del canale PWM per cui si desidera ottenere il duty cycle.
 * @return int Il valore del duty cycle corrispondente al canale specificato.
 */
int pwm_get_duty(int channel) {
    uint32_t duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
    // Riconverti in percentuale (risoluzione 10 bit = 1023)
    return (int)((duty * 100) / 1023);
}


/**
 * @brief Inizializza il sistema PWM.
 * 
 * Questa funzione inizializza il sistema PWM, configurando i registri necessari e preparando il sistema per l'uso del PWM.
 * 
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t pwm_init(void) {
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
    
    ESP_LOGI(TAG, "[C] PWM inizializzato");
    s_pwm_initialized = true;
    return ESP_OK;
}

/* [C] Restituisce lo stato operativo del driver PWM */
hw_component_status_t pwm_get_status(void)
{
    return s_pwm_initialized ? HW_STATUS_ONLINE : HW_STATUS_DISABLED;
}

#endif /* DNA_PWM == 0 */

/*
 * Mockup — nessun driver LEDC reale.
 * Attiva quando DNA_PWM == 1
 */
#if defined(DNA_PWM) && (DNA_PWM == 1)

static int s_mock_duty[2] = {0, 0};


/**
 * @brief Imposta il duty cycle del segnale PWM.
 *
 * @param [in] channel Numero del canale PWM da configurare.
 * @param [in] duty_percent Percentuale del duty cycle da impostare (0-100).
 * @return esp_err_t Codice di errore.
 */
esp_err_t pwm_set_duty(int channel, int duty_percent)
{
    if (channel < 0 || channel > 1) return ESP_ERR_INVALID_ARG;
    if (duty_percent < 0) duty_percent = 0;
    if (duty_percent > 100) duty_percent = 100;
    s_mock_duty[channel] = duty_percent;
    ESP_LOGI(TAG, "[C] [MOCK] pwm_set_duty: ch=%d duty=%d%%", channel, duty_percent);
    return ESP_OK;
}


/**
 * @brief Ottiene il valore del duty cycle per un canale PWM.
 *
 * @param channel [in] Il numero del canale PWM per cui si desidera ottenere il valore del duty cycle.
 * @return int Il valore del duty cycle corrente per il canale specificato.
 */
int pwm_get_duty(int channel)
{
    if (channel < 0 || channel > 1) return 0;
    return s_mock_duty[channel];
}


/**
 * @brief Inizializza il sistema PWM.
 *
 * Questa funzione inizializza il sistema PWM, preparandolo per l'uso successivo.
 *
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t pwm_init(void)
{
    s_mock_duty[0] = 0;
    s_mock_duty[1] = 0;
    ESP_LOGI(TAG, "[C] [MOCK] pwm_init: 2 canali simulati");
    return ESP_OK;
}

/* [C] Mockup: get_status restituisce DISABLED */
hw_component_status_t pwm_get_status(void)
{
    return HW_STATUS_DISABLED;
}

#endif /* DNA_PWM == 1 */
