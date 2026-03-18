#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_check.h"
#include "led_strip.h"
#include "led.h"
#include "sdkconfig.h"
#include "device_config.h"

/* DNA_LED_STRIP: imposta a 1 nel CMakeLists del componente per attivare il
 * mockup senza hardware reale (nessun RMT, nessun WS2812). Default: 0. */
#ifndef DNA_LED_STRIP
#define DNA_LED_STRIP 0
#endif

static const char *TAG = "LED_CTRL";

static led_strip_handle_t s_led_strip = NULL;
static uint32_t s_led_count = 0;
static SemaphoreHandle_t s_led_mutex = NULL;

#if DNA_LED_STRIP == 0  /* implementazioni reali — escluse se mockup attivo */

#define LED_LOCK()   if (s_led_mutex) xSemaphoreTakeRecursive(s_led_mutex, portMAX_DELAY)
#define LED_UNLOCK() if (s_led_mutex) xSemaphoreGiveRecursive(s_led_mutex)


/**
 * @brief Inizializza il sistema di LED.
 * 
 * Questa funzione inizializza il sistema di LED, assicurandosi che il mutex
 * s_led_mutex sia stato correttamente allocato. Se il mutex non è stato
 * precedentemente allocato, la funzione lo alloca.
 * 
 * @return esp_err_t
 * - ESP_OK: Inizializzazione riuscita.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t led_init(void)
{
    if (s_led_mutex == NULL) {
        s_led_mutex = xSemaphoreCreateRecursiveMutex();
    }
    
    LED_LOCK();
    
    uint32_t current_cfg_count = CONFIG_APP_WS2812_LEDS; // Default da Kconfig
    device_config_t *cfg = device_config_get();
    if (cfg) {
        current_cfg_count = cfg->sensors.led_count;
    }

    // Se già inizializzato e il conteggio non è cambiato, non fare nulla
    if (s_led_strip != NULL && s_led_count == current_cfg_count) {
        LED_UNLOCK();
        return ESP_OK;
    }

    // Se il conteggio è cambiato o è la prima init, procediamo
    if (s_led_strip != NULL) {
        ESP_LOGI(TAG, "[C] Cambio led_count rilevato (da %lu a %lu): re-inizializzazione stripe", s_led_count, current_cfg_count);
        led_strip_clear(s_led_strip);
        vTaskDelay(pdMS_TO_TICKS(10));
        led_strip_del(s_led_strip);
        s_led_strip = NULL;
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        ESP_LOGI(TAG, "[C] Inizializzazione driver LED WS2812 (count: %lu)", current_cfg_count);
    }

    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_APP_WS2812_GPIO,
        .max_leds = current_cfg_count,
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config_strip = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config_strip, &s_led_strip);
    if (err != ESP_OK) {
        LED_UNLOCK();
        ESP_LOGE(TAG, "Creazione striscia LED fallita: 0x%x", err);
        return err;
    }
    
    s_led_count = current_cfg_count;
    
    ESP_LOGI(TAG, "[C] LED controller pronto: %lu LED su GPIO %d", s_led_count, CONFIG_APP_WS2812_GPIO);
    LED_UNLOCK();
    return ESP_OK;
}


/**
 * @brief Ottiene un handle per la gestione del LED strip.
 *
 * @return led_strip_handle_t Handle per la gestione del LED strip.
 */
led_strip_handle_t led_get_handle(void)
{
    return s_led_strip;
}


/**
 * @brief Ottiene il numero di LED attivi.
 *
 * Questa funzione restituisce il numero di LED attivi nel sistema.
 *
 * @return Il numero di LED attivi.
 */
uint32_t led_get_count(void)
{
    return s_led_count;
}


/**
 * @brief Cancella lo stato dei LED.
 *
 * Questa funzione cancella lo stato attivo dei LED, impostandoli tutti a uno stato inattivo.
 *
 * @return
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t led_clear(void)
{
    LED_LOCK();
    if (!s_led_strip) {
        LED_UNLOCK();
        ESP_LOGE(TAG, "[C] LED strip non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = led_strip_clear(s_led_strip);
    LED_UNLOCK();
    return err;
}


/**
 * @brief Aggiorna lo stato del LED.
 *
 * Questa funzione aggiorna lo stato del LED, applicando le modifiche
 * recentemente apportate.
 *
 * @return
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t led_refresh(void)
{
    LED_LOCK();
    if (!s_led_strip) {
        LED_UNLOCK();
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = led_strip_refresh(s_led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Refresh LED fallito: 0x%x", err);
    }
    LED_UNLOCK();
    return err;
}


/**
 * @brief Imposta il colore di tutti i LED con i valori di rosso, verde e blu forniti.
 *
 * @param [in] red Valore del canale rosso (0-255).
 * @param [in] green Valore del canale verde (0-255).
 * @param [in] blue Valore del canale blu (0-255).
 *
 * @return esp_err_t Codice di errore.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione fallita.
 */
esp_err_t led_fill_color(uint8_t red, uint8_t green, uint8_t blue)
{
    LED_LOCK();
    if (!s_led_strip) {
        LED_UNLOCK();
        ESP_LOGE(TAG, "[C] LED strip non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = ESP_OK;
    // Invertiti G e B per correggere i colori del chip LED
    for (uint32_t i = 0; i < s_led_count; i++) {
        err = led_strip_set_pixel(s_led_strip, i, red, blue, green);
        if (err != ESP_OK) break;
    }
    
    if (err == ESP_OK) {
        err = led_strip_refresh(s_led_strip);
        ESP_LOGI(TAG, "[C] Riempimento colore RGB(%d, %d, %d)", red, green, blue);
    }
    
    LED_UNLOCK();
    return err;
}


/**
 * @brief Imposta il colore di un singolo pixel del LED.
 * 
 * @param [in] index Indice del pixel da impostare.
 * @param [in] red Valore del canale rosso (0-255).
 * @param [in] green Valore del canale verde (0-255).
 * @param [in] blue Valore del canale blu (0-255).
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t led_set_pixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    LED_LOCK();
    if (!s_led_strip) {
        LED_UNLOCK();
        ESP_LOGE(TAG, "[C] LED strip non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (index >= s_led_count) {
        LED_UNLOCK();
        ESP_LOGE(TAG, "[C] Indice LED %lu fuori range (max: %lu)", index, s_led_count);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Invertiti G e B per correggere i colori del chip LED
    esp_err_t err = led_strip_set_pixel(s_led_strip, index, red, blue, green);
    LED_UNLOCK();
    return err;
}

// Helper per generare colore HSV

/**
 * @brief Converte i valori HSV in RGB.
 *
 * Questa funzione converte i valori di colore in formato HSV (Tono, Saturazione, Valore)
 * in formato RGB (Rosso, Verde, Blu).
 *
 * @param hue [in] Il valore del tono (0-360).
 * @param sat [in] La saturazione del colore (0-100).
 * @param val [in] Il valore del colore (0-100).
 * @param r [out] Puntatore alla variabile dove verrà memorizzato il valore del rosso.
 * @param g [out] Puntatore alla variabile dove verrà memorizzato il valore del verde.
 * @param b [out] Puntatore alla variabile dove verrà memorizzato il valore del blu.
 *
 * @return Nessun valore di ritorno.
 */
static void hsv_to_rgb(uint16_t hue, uint8_t sat, uint8_t val, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint16_t h = (hue / 60) % 6;
    uint16_t f = (hue % 60);
    
    uint8_t p = val * (255 - sat) / 255;
    uint8_t q = val * (255 - (sat * f) / 60) / 255;
    uint8_t t = val * (255 - (sat * (60 - f)) / 60) / 255;
    
    switch (h) {
        case 0: *r = val; *g = t; *b = p; break;
        case 1: *r = q; *g = val; *b = p; break;
        case 2: *r = p; *g = val; *b = t; break;
        case 3: *r = p; *g = q; *b = val; break;
        case 4: *r = t; *g = p; *b = val; break;
        default: *r = val; *g = p; *b = q; break;
    }
}


/**
 * @brief Imposta lo stato di un singolo pixel del LED utilizzando i valori HSV.
 *
 * @param [in] index Indice del pixel del LED da impostare.
 * @param [in] hue Valore di tonalità (0-65535).
 * @param [in] sat Valore di saturazione (0-255).
 * @param [in] val Valore di luminosità (0-255).
 *
 * @return esp_err_t Codice di errore.
 *         - ESP_OK: Operazione completata con successo.
 *         - ESP_ERR_INVALID_ARG: Argomento non valido.
 *         - ESP_ERR_NOT_SUPPORTED: Funzionalità non supportata.
 */
esp_err_t led_set_pixel_hsv(uint32_t index, uint16_t hue, uint8_t sat, uint8_t val)
{
    uint8_t r, g, b;
    hsv_to_rgb(hue, sat, val, &r, &g, &b);
    return led_set_pixel(index, r, g, b);
}


/**
 * @brief Fa lampeggiare un LED con i colori specificati per una durata data.
 * 
 * @param [in] red Valore del canale rosso del LED (0-255).
 * @param [in] green Valore del canale verde del LED (0-255).
 * @param [in] blue Valore del canale blu del LED (0-255).
 * @param [in] duration_ms Durata del lampeggiamento in millisecondi.
 * @return esp_err_t Errore generato durante l'operazione.
 */
esp_err_t led_breathe(uint8_t red, uint8_t green, uint8_t blue, uint32_t duration_ms)
{
    if (!s_led_strip) {
        ESP_LOGE(TAG, "[C] LED strip non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint32_t step_duration = duration_ms / 100;
    if (step_duration < 10) step_duration = 10;
    
    ESP_LOGI(TAG, "[C] Animazione pulsante (breathe) RGB(%d, %d, %d) durata %lu ms", red, green, blue, duration_ms);
    
    // Pulsazione in avanti
    for (int i = 0; i <= 100; i += 5) {
        uint8_t r = (red * i) / 100;
        uint8_t g = (green * i) / 100;
        uint8_t b = (blue * i) / 100;
        
        LED_LOCK();
        for (uint32_t idx = 0; idx < s_led_count; idx++) {
            led_strip_set_pixel(s_led_strip, idx, r, g, b);
        }
        led_strip_refresh(s_led_strip);
        LED_UNLOCK();
        vTaskDelay(pdMS_TO_TICKS(step_duration));
    }
    
    // Pulsazione indietro
    for (int i = 100; i >= 0; i -= 5) {
        uint8_t r = (red * i) / 100;
        uint8_t g = (green * i) / 100;
        uint8_t b = (blue * i) / 100;
        
        LED_LOCK();
        for (uint32_t idx = 0; idx < s_led_count; idx++) {
            led_strip_set_pixel(s_led_strip, idx, r, g, b);
        }
        led_strip_refresh(s_led_strip);
        LED_UNLOCK();
        vTaskDelay(pdMS_TO_TICKS(step_duration));
    }
    
    return ESP_OK;
}


/**
 * @brief Fa lampeggiare i led in un effetto arcobaleno.
 * 
 * @param duration_ms Durata dell'effetto in millisecondi.
 * @return esp_err_t Errore se la strip non è inizializzata, altrimenti ESP_OK.
 */
esp_err_t led_rainbow(uint32_t duration_ms)
{
    if (!s_led_strip) {
        ESP_LOGE(TAG, "[C] LED strip non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    uint32_t step_duration = duration_ms / 360;
    if (step_duration < 5) step_duration = 5;
    
    ESP_LOGI(TAG, "[C] Animazione rainbow durata %lu ms", duration_ms);
    
    for (uint16_t hue = 0; hue < 360; hue += 6) {
        LED_LOCK();
        for (uint32_t i = 0; i < s_led_count; i++) {
            uint8_t r, g, b;
            uint16_t pixel_hue = (hue + (i * 360 / s_led_count)) % 360;
            hsv_to_rgb(pixel_hue, 255, 255, &r, &g, &b);
            led_strip_set_pixel(s_led_strip, i, r, g, b);
        }
        led_strip_refresh(s_led_strip);
        LED_UNLOCK();
        vTaskDelay(pdMS_TO_TICKS(step_duration));
    }
    
    return ESP_OK;
}


/**
 * @brief Effettua un fade in per il LED.
 * 
 * @param [in] red Valore del canale rosso (0-255).
 * @param [in] green Valore del canale verde (0-255).
 * @param [in] blue Valore del canale blu (0-255).
 * @param [in] steps Numero di passi per il fade in.
 * @param [in] step_duration_ms Durata di ciascun passo in millisecondi.
 * @return esp_err_t Errore generato durante l'operazione.
 */
esp_err_t led_fade_in(uint8_t red, uint8_t green, uint8_t blue, uint32_t steps, uint32_t step_duration_ms)
{
    if (!s_led_strip) {
        ESP_LOGE(TAG, "[C] LED strip non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (steps == 0) steps = 50;
    
    ESP_LOGI(TAG, "[C] Dissolvenza in entrata (fade in) RGB(%d, %d, %d) in %lu passi", red, green, blue, steps);
    
    for (uint32_t step = 0; step <= steps; step++) {
        uint8_t r = (red * step) / steps;
        uint8_t g = (green * step) / steps;
        uint8_t b = (blue * step) / steps;
        
        LED_LOCK();
        for (uint32_t i = 0; i < s_led_count; i++) {
            led_strip_set_pixel(s_led_strip, i, r, g, b);
        }
        led_strip_refresh(s_led_strip);
        LED_UNLOCK();
        vTaskDelay(pdMS_TO_TICKS(step_duration_ms));
    }
    
    return ESP_OK;
}


/**
 * @brief Spegni gradualmente un LED strip.
 * 
 * Questa funzione spegne gradualmente un LED strip utilizzando un numero specifico di passi e durata per ogni passo.
 * 
 * @param steps Numero di passi per la spegnimento graduale.
 * @param step_duration_ms Durata in millisecondi per ogni passo di spegnimento.
 * @return esp_err_t Codice di errore.
 *         - ESP_OK: Operazione completata con successo.
 *         - ESP_FAIL: Operazione fallita.
 */
esp_err_t led_fade_out(uint32_t steps, uint32_t step_duration_ms)
{
    if (!s_led_strip) {
        ESP_LOGE(TAG, "[C] LED strip non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (steps == 0) steps = 50;
    
    ESP_LOGI(TAG, "[C] Dissolvenza in uscita (fade out) in %lu passi", steps);
    
    for (uint32_t step = steps; step > 0; step--) {
        uint8_t brightness = (255 * step) / steps;
        
        LED_LOCK();
        for (uint32_t i = 0; i < s_led_count; i++) {
            led_strip_set_pixel(s_led_strip, i, brightness, brightness, brightness);
        }
        led_strip_refresh(s_led_strip);
        LED_UNLOCK();
        vTaskDelay(pdMS_TO_TICKS(step_duration_ms));
    }
    
    led_clear();
    
    return ESP_OK;
}

#endif /* DNA_LED_STRIP == 0 */

/*
 * Mockup section: se DNA_LED_STRIP==1 vengono fornite versioni fittizie di
 * tutte le API pubbliche del modulo. Nessun hardware RMT/WS2812 viene
 * toccato; le funzioni si comportano come se la striscia fosse presente e
 * funzionante, restituendo ESP_OK e dati plausibili. Questo consente di
 * compilare e testare la logica di livello superiore senza hardware reale.
 */
#if defined(DNA_LED_STRIP) && (DNA_LED_STRIP == 1)

/* handle sentinella: la mock non alloca strutture RMT reali */
static led_strip_handle_t s_mock_handle = NULL;
static uint32_t           s_mock_count  = 8;   /* LED fittizi di default */
static bool               s_mock_inited = false;


/**
 * @brief Inizializza il sistema di LED.
 *
 * Questa funzione inizializza il sistema di LED, preparandolo per l'uso successivo.
 *
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t led_init(void)
{
    s_mock_inited = true;
    s_mock_count  = CONFIG_APP_WS2812_LEDS;
    ESP_LOGI(TAG, "[C] [MOCK] led_init: %lu LED simulati su GPIO %d",
             s_mock_count, CONFIG_APP_WS2812_GPIO);
    return ESP_OK;
}


/**
 * @brief Ottiene un handle per la gestione del LED strip.
 *
 * @return led_strip_handle_t Handle per la gestione del LED strip.
 */
led_strip_handle_t led_get_handle(void)
{
    return s_mock_handle; /* NULL: i caller devono tollerare handle NULL */
}


/**
 * @brief Ottiene il numero di LED attivi.
 *
 * Questa funzione restituisce il numero di LED attivi nel sistema.
 *
 * @return Il numero di LED attivi.
 */
uint32_t led_get_count(void)
{
    return s_mock_count;
}


/**
 * @brief Cancella lo stato dei LED.
 *
 * Questa funzione cancella lo stato attivo dei LED, impostandoli tutti a uno stato inattivo.
 *
 * @return
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t led_clear(void)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    ESP_LOGD(TAG, "[C] [MOCK] led_clear");
    return ESP_OK;
}


/**
 * @brief Aggiorna lo stato del LED.
 *
 * Questa funzione aggiorna lo stato del LED, applicando le modifiche
 * recentemente apportate.
 *
 * @return
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t led_refresh(void)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}


/**
 * @brief Imposta il colore di tutti i LED con i valori di rosso, verde e blu forniti.
 *
 * @param [in] red Valore del canale rosso (0-255).
 * @param [in] green Valore del canale verde (0-255).
 * @param [in] blue Valore del canale blu (0-255).
 *
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t led_fill_color(uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    ESP_LOGD(TAG, "[C] [MOCK] led_fill_color RGB(%u,%u,%u)", red, green, blue);
    return ESP_OK;
}


/**
 * @brief Imposta il colore di un singolo pixel del LED.
 *
 * @param [in] index Indice del pixel da impostare.
 * @param [in] red Valore del canale rosso (0-255).
 * @param [in] green Valore del canale verde (0-255).
 * @param [in] blue Valore del canale blu (0-255).
 *
 * @return esp_err_t Codice di errore.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_ERR_INVALID_ARG: Argomento non valido.
 *         - ESP_ERR_NOT_SUPPORTED: Funzionalità non supportata.
 */
esp_err_t led_set_pixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    if (index >= s_mock_count) return ESP_ERR_INVALID_ARG;
    ESP_LOGD(TAG, "[C] [MOCK] led_set_pixel[%lu] RGB(%u,%u,%u)", index, red, green, blue);
    return ESP_OK;
}


/**
 * @brief Imposta lo stato di un singolo pixel del LED utilizzando i valori HSV.
 *
 * @param [in] index Indice del pixel del LED da impostare.
 * @param [in] hue Valore di tonalità (0-65535).
 * @param [in] sat Valore di saturazione (0-255).
 * @param [in] val Valore di luminosità (0-255).
 * @return esp_err_t Codice di errore.
 */
esp_err_t led_set_pixel_hsv(uint32_t index, uint16_t hue, uint8_t sat, uint8_t val)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    if (index >= s_mock_count) return ESP_ERR_INVALID_ARG;
    ESP_LOGD(TAG, "[C] [MOCK] led_set_pixel_hsv[%lu] H=%u S=%u V=%u", index, hue, sat, val);
    return ESP_OK;
}


/**
 * @brief Fa lampeggiare un LED con i colori specificati.
 *
 * Questa funzione controlla un LED per farlo lampeggiare con i colori specificati (rosso, verde, blu) 
 * durante una durata specificata in millisecondi.
 *
 * @param [in] red Valore del canale rosso del LED (0-255).
 * @param [in] green Valore del canale verde del LED (0-255).
 * @param [in] blue Valore del canale blu del LED (0-255).
 * @param [in] duration_ms Durata del lampeggiamento in millisecondi.
 *
 * @return
 * - ESP_OK: Operazione completata con successo.
 * - ESP_ERR_INVALID_ARG: Argomento non valido.
 * - ESP_FAIL: Operazione non riuscita.
 */
esp_err_t led_breathe(uint8_t red, uint8_t green, uint8_t blue, uint32_t duration_ms)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "[C] [MOCK] led_breathe RGB(%u,%u,%u) %lums", red, green, blue, (unsigned long)duration_ms);
    vTaskDelay(pdMS_TO_TICKS(10)); /* breve pausa simbolica */
    return ESP_OK;
}


/**
 * @brief Fa lampeggiare i led in un effetto arcobaleno.
 *
 * Questa funzione attiva un effetto arcobaleno sui led, facendoli lampeggiare
 * per un periodo di tempo specificato.
 *
 * @param duration_ms Durata dell'effetto in millisecondi.
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t led_rainbow(uint32_t duration_ms)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "[C] [MOCK] led_rainbow %lums", (unsigned long)duration_ms);
    vTaskDelay(pdMS_TO_TICKS(10));
    return ESP_OK;
}


/**
 * @brief Effettua un fade in per il LED.
 *
 * Questa funzione gradualmente aumenta l'intensità del LED specificato
 * passando attraverso un numero di passi, ciascuno con una durata specificata.
 *
 * @param [in] red Valore del canale rosso del LED (0-255).
 * @param [in] green Valore del canale verde del LED (0-255).
 * @param [in] blue Valore del canale blu del LED (0-255).
 * @param [in] steps Numero di passi per il fade in.
 * @param [in] step_duration_ms Durata di ciascun passo in millisecondi.
 * @return esp_err_t Codice di errore.
 */
esp_err_t led_fade_in(uint8_t red, uint8_t green, uint8_t blue, uint32_t steps, uint32_t step_duration_ms)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "[C] [MOCK] led_fade_in RGB(%u,%u,%u) steps=%lu", red, green, blue, (unsigned long)steps);
    return ESP_OK;
}


/**
 * @brief Effettua un fade out su un LED.
 *
 * Questa funzione gradualmente diminuisce l'intensità di un LED
 * dividendo il processo in un numero specificato di passi.
 *
 * @param steps Numero di passi per il fade out.
 * @param step_duration_ms Durata di ciascun passo in millisecondi.
 * @return esp_err_t Codice di errore.
 */
esp_err_t led_fade_out(uint32_t steps, uint32_t step_duration_ms)
{
    if (!s_mock_inited) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "[C] [MOCK] led_fade_out steps=%lu", (unsigned long)steps);
    return ESP_OK;
}

#endif /* DNA_LED_STRIP */

/* =========================================================================
 * LED BAR - Device virtuale per gestione 2 strisce sincrone
 * ========================================================================= */

static struct {
    bool initialized;
    uint32_t total_leds;
    uint32_t half_leds;  /* LED per ogni striscia */
    led_bar_state_t state;
    uint8_t progress_percent;
    uint32_t last_update_ms;
    bool blink_state;
    uint32_t blink_counter;
} s_led_bar = {0};

/* Colori definiti */
#define LED_BAR_COLOR_IDLE_R     0
#define LED_BAR_COLOR_IDLE_G     50
#define LED_BAR_COLOR_IDLE_B     100
#define LED_BAR_COLOR_RUN_R      0
#define LED_BAR_COLOR_RUN_G      255
#define LED_BAR_COLOR_RUN_B      0
#define LED_BAR_COLOR_PREFINE_R  128
#define LED_BAR_COLOR_PREFINE_G  0
#define LED_BAR_COLOR_PREFINE_B  128
#define LED_BAR_COLOR_FINISH_R   128
#define LED_BAR_COLOR_FINISH_G   0
#define LED_BAR_COLOR_FINISH_B   128

esp_err_t led_bar_init(uint32_t total_leds)
{
    if (total_leds == 0 || total_leds % 2 != 0) {
        ESP_LOGE(TAG, "[LED_BAR] Numero LED deve essere pari e > 0");
        return ESP_ERR_INVALID_ARG;
    }
    
    s_led_bar.total_leds = total_leds;
    s_led_bar.half_leds = total_leds / 2;
    s_led_bar.state = LED_BAR_STATE_OFF;
    s_led_bar.progress_percent = 0;
    s_led_bar.last_update_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    s_led_bar.blink_state = false;
    s_led_bar.blink_counter = 0;
    s_led_bar.initialized = true;
    
    ESP_LOGI(TAG, "[LED_BAR] Inizializzato con %u LED (%u per striscia)", 
             total_leds, s_led_bar.half_leds);
    
    return led_bar_clear();
}

esp_err_t led_bar_set_state(led_bar_state_t state)
{
    if (!s_led_bar.initialized) return ESP_ERR_INVALID_STATE;
    
    if (s_led_bar.state != state) {
        ESP_LOGI(TAG, "[LED_BAR] Stato cambiato: %d -> %d", s_led_bar.state, state);
        s_led_bar.state = state;
        s_led_bar.blink_counter = 0;
        s_led_bar.blink_state = false;
        
        /* Reset progress se necessario */
        if (state == LED_BAR_STATE_IDLE || state == LED_BAR_STATE_OFF) {
            s_led_bar.progress_percent = 0;
        }
    }
    
    return ESP_OK;
}

esp_err_t led_bar_set_progress(uint8_t progress_percent)
{
    if (!s_led_bar.initialized) return ESP_ERR_INVALID_STATE;
    if (progress_percent > 100) progress_percent = 100;
    
    s_led_bar.progress_percent = progress_percent;
    
    /* Transizione automatica a PRE-FINE se configurato */
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->timeouts.pre_fine_ciclo_percent > 0) {
        if (progress_percent >= cfg->timeouts.pre_fine_ciclo_percent && 
            s_led_bar.state == LED_BAR_STATE_RUNNING) {
            led_bar_set_state(LED_BAR_STATE_PREFINE);
        }
    }
    
    return ESP_OK;
}

static void led_bar_set_strip_color(uint8_t red, uint8_t green, uint8_t blue)
{
    /* Imposta lo stesso colore su entrambe le strisce */
    for (uint32_t i = 0; i < s_led_bar.total_leds; i++) {
        led_set_pixel(i, red, green, blue);
    }
    led_refresh();
}

static void led_bar_set_progress_color(uint8_t red, uint8_t green, uint8_t blue)
{
    /* Spegne tutto prima */
    led_clear();
    
    /* Calcola LED da accendere su ogni striscia */
    uint32_t leds_to_light = (s_led_bar.half_leds * s_led_bar.progress_percent) / 100;
    if (leds_to_light > s_led_bar.half_leds) leds_to_light = s_led_bar.half_leds;
    
    /* Accende LED progressivi su entrambe le strisce */
    for (uint32_t i = 0; i < leds_to_light; i++) {
        /* Prima striscia */
        led_set_pixel(i, red, green, blue);
        /* Seconda striscia (sincrona) */
        led_set_pixel(s_led_bar.half_leds + i, red, green, blue);
    }
    
    led_refresh();
}

esp_err_t led_bar_update(void)
{
    if (!s_led_bar.initialized) return ESP_ERR_INVALID_STATE;
    
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    switch (s_led_bar.state) {
        case LED_BAR_STATE_IDLE:
            /* Effetto respirazione lenta in IDLE */
            if (now_ms - s_led_bar.last_update_ms >= 1000) {
                uint8_t brightness = (sin(now_ms / 500.0) + 1.0) * 127.5;
                led_bar_set_strip_color(
                    LED_BAR_COLOR_IDLE_R * brightness / 255,
                    LED_BAR_COLOR_IDLE_G * brightness / 255,
                    LED_BAR_COLOR_IDLE_B * brightness / 255
                );
                s_led_bar.last_update_ms = now_ms;
            }
            break;
            
        case LED_BAR_STATE_RUNNING:
            led_bar_set_progress_color(LED_BAR_COLOR_RUN_R, LED_BAR_COLOR_RUN_G, LED_BAR_COLOR_RUN_B);
            break;
            
        case LED_BAR_STATE_PREFINE:
            led_bar_set_progress_color(LED_BAR_COLOR_PREFINE_R, LED_BAR_COLOR_PREFINE_G, LED_BAR_COLOR_PREFINE_B);
            break;
            
        case LED_BAR_STATE_FINISHED:
            /* Lampeggio 2Hz porpora su tutte le strisce */
            if (now_ms - s_led_bar.last_update_ms >= 250) {  /* 2Hz = 500ms, toggle ogni 250ms */
                s_led_bar.blink_state = !s_led_bar.blink_state;
                if (s_led_bar.blink_state) {
                    led_bar_set_strip_color(LED_BAR_COLOR_FINISH_R, LED_BAR_COLOR_FINISH_G, LED_BAR_COLOR_FINISH_B);
                } else {
                    led_clear();
                }
                s_led_bar.last_update_ms = now_ms;
                s_led_bar.blink_counter++;
            }
            break;
            
        case LED_BAR_STATE_OFF:
            led_clear();
            break;
    }
    
    return ESP_OK;
}

esp_err_t led_bar_clear(void)
{
    if (!s_led_bar.initialized) return ESP_ERR_INVALID_STATE;
    
    led_clear();
    s_led_bar.state = LED_BAR_STATE_OFF;
    s_led_bar.progress_percent = 0;
    s_led_bar.blink_state = false;
    s_led_bar.blink_counter = 0;
    
    return ESP_OK;
}

led_bar_state_t led_bar_get_state(void)
{
    return s_led_bar.initialized ? s_led_bar.state : LED_BAR_STATE_OFF;
}
