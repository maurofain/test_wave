#include "tasks.h"
#include "init.h"
#include "esp_log.h"
#include "led_strip.h"
#include "device_config.h"
#include "sht40.h"
#include "esp_lcd_touch.h"
#include "web_ui.h"
#include "usb_cdc_scanner.h" // Scanner QR
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static const char *TAG = "TASKS";
static float s_temperature = 0.0f;
static float s_humidity = 0.0f;

float tasks_get_temperature(void) { return s_temperature; }
float tasks_get_humidity(void) { return s_humidity; }

static void ws2812_blink_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    led_strip_handle_t strip = init_get_ws2812_handle();
    TickType_t last_wake = xTaskGetTickCount();
    uint8_t color = 0;

    while (true) {
        if (!strip) {
            // Se i LED sono disabilitati in config, evitiamo di spammare log
            if (!device_config_get()->sensors.led_enabled) {
                vTaskDelay(pdMS_TO_TICKS(5000));
                strip = init_get_ws2812_handle(); // Riprova a prenderlo in caso sia stato abilitato
                continue;
            }
            ESP_LOGW(TAG, "[M] Handle WS2812 non pronto");
            vTaskDelay(pdMS_TO_TICKS(500));
            strip = init_get_ws2812_handle(); 
            continue;
        }
        color += 25;
        led_strip_set_pixel(strip, 0, color, 0, 255 - color);
        led_strip_refresh(strip);
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

// Task scheletro (solo struttura, logica da implementare)

static void eeprom_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void io_expander_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void sht40_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        if (device_config_get()->sensors.temperature_enabled && sht40_is_ready()) {
            float t, h;
            if (sht40_read(&t, &h) == ESP_OK) {
                s_temperature = t;
                s_humidity = h;
                ESP_LOGD(TAG, "SHT40: T=%.1f C, RH=%.1f %%", t, h);
            }
        }
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void rs232_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void rs485_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void mdb_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void pwm_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void touchscreen_task(void *arg)
{
    esp_lcd_touch_handle_t touch_handle = (esp_lcd_touch_handle_t)arg;
    TickType_t last_wake = xTaskGetTickCount();
    
    if (!touch_handle) {
        ESP_LOGE("TOUCH", "Touch handle is NULL, task terminating");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI("TOUCH", "Touch task started with valid handle");
    
    /* Memorizza il tocco precedente per evitare log ripetuti quando non cambia nulla */
    bool prev_present = false;
    esp_lcd_touch_point_data_t prev = {0};
    const int MOVEMENT_THRESHOLD = 3; /* pixel */

    while (true) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(50)); // Polling ogni 50 ms

        // Legge i dati del touchscreen
        esp_lcd_touch_read_data(touch_handle);

        // Get touch points using new API
        esp_lcd_touch_point_data_t touch_data[1];
        uint8_t touch_points = 0;

        esp_lcd_touch_get_data(touch_handle, touch_data, &touch_points, 1);

        if (touch_points > 0) {
            bool significant = !prev_present ||
                abs((int)touch_data[0].x - (int)prev.x) >= MOVEMENT_THRESHOLD ||
                abs((int)touch_data[0].y - (int)prev.y) >= MOVEMENT_THRESHOLD ||
                touch_data[0].track_id != prev.track_id ||
                touch_data[0].strength != prev.strength;

            if (significant) {
                ESP_LOGI("TOUCH", "Touch detected: x=%d, y=%d, strength=%d, track_id=%d", 
                         touch_data[0].x, touch_data[0].y, touch_data[0].strength, touch_data[0].track_id);
                prev = touch_data[0];
                prev_present = true;
            }
        } else {
            if (prev_present) {
                ESP_LOGI("TOUCH", "Touch released (last x=%d, y=%d, track_id=%d)", prev.x, prev.y, prev.track_id);
                prev_present = false;
                prev.x = prev.y = prev.strength = prev.track_id = 0;
            }
        }
    }
}

static void lvgl_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

static void ntp_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "[NTP] NTP task started, period: %d seconds", 600);
    
    while (true) {
        device_config_t *cfg = device_config_get();
        
        if (cfg->ntp_enabled) {
            ESP_LOGI(TAG, "[NTP] Checking NTP sync...");
            // Try to sync NTP
            esp_err_t ret = init_sync_ntp();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "[NTP] NTP sync successful");
            } else {
                ESP_LOGW(TAG, "[NTP] NTP sync failed");
            }
        } else {
            ESP_LOGD(TAG, "[NTP] NTP disabled in configuration");
        }
        
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

// wrapper per lo scanner USB: inizializza il driver e poi entra nella sua routine
static void scanner_on_barcode_cb(const char *barcode)
{
    ESP_LOGI("SCANNER", "Barcode: %s", barcode);
    /* Barcode readings: use distinct tag so UI can display readings separately */
    web_ui_add_log("INFO", "SCANNER_DATA", barcode);
}

static void usb_scanner_task_wrapper(void *arg)
{
    usb_cdc_scanner_config_t cfg = {.on_barcode = scanner_on_barcode_cb};
    usb_cdc_scanner_init(&cfg);
    // usa la task routine del componente scanner che gestisce sia la modalità reale (CDC-ACM) che simulata
    usb_cdc_scanner_task(NULL);
}

static task_param_t s_tasks[] = {
    {
        .name = "ws2812_blink",
        .state = TASK_STATE_RUN,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = ws2812_blink_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "eeprom",
        .state = TASK_STATE_IDLE,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(1000),
        .task_fn = eeprom_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "io_expander",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = io_expander_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "sht40",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(2000),
        .task_fn = sht40_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "rs232",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(10),
        .task_fn = rs232_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "rs485",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(10),
        .task_fn = rs485_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "mdb",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(10),
        .task_fn = mdb_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "pwm",
        .state = TASK_STATE_IDLE,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(20),
        .task_fn = pwm_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "touchscreen",
        .state = TASK_STATE_IDLE,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(20),
        .task_fn = touchscreen_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "lvgl",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(16),
        .task_fn = lvgl_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "ntp",
        .state = TASK_STATE_RUN,
        .priority = 3,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(600000), // 600 secondi = 10 minuti
        .task_fn = ntp_task,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "usb_scanner",
        .state = TASK_STATE_IDLE, /* default: idle, abilita da /tasks o /config */
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(1000),
        .task_fn = usb_scanner_task_wrapper,
        .stack_words = 8192,
        .arg = NULL,
        .handle = NULL,
    },
};

// -----------------------------------------------------------------------------
// Caricatore configurazione (CSV da SPIFFS)
// -----------------------------------------------------------------------------

static task_param_t *find_task_by_name(const char *name)
{
    for (size_t i = 0; i < sizeof(s_tasks) / sizeof(s_tasks[0]); ++i) {
        if (strcmp(s_tasks[i].name, name) == 0) {
            return &s_tasks[i];
        }
    }
    return NULL;
}

static task_state_t parse_state(const char *s, task_state_t def)
{
    if (!s) return def;
    if (strcasecmp(s, "run") == 0) return TASK_STATE_RUN;
    if (strcasecmp(s, "idle") == 0) return TASK_STATE_IDLE;
    if (strcasecmp(s, "pause") == 0) return TASK_STATE_PAUSE;
    return def;
}

void tasks_set_touchscreen_handle(void *handle)
{
    task_param_t *t = find_task_by_name("touchscreen");
    if (t) {
        t->arg = handle;
    }
}

void tasks_load_config(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "[M] Impossibile aprire %s; uso configurazioni predefinite", path);
        return;
    }

    char line[160];
    // Salta intestazione
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        // name,state,priority,core,period_ms,stack_words
        char *save = NULL;
        char *name = strtok_r(line, ",\r\n", &save);
        char *state = strtok_r(NULL, ",\r\n", &save);
        char *prio = strtok_r(NULL, ",\r\n", &save);
        char *core = strtok_r(NULL, ",\r\n", &save);
        char *period = strtok_r(NULL, ",\r\n", &save);
        char *stack = strtok_r(NULL, ",\r\n", &save);
        if (!name) continue;

        task_param_t *t = find_task_by_name(name);
        if (!t) {
            ESP_LOGW(TAG, "[M] Task sconosciuto '%s' nella configurazione", name);
            continue;
        }

        if (state) t->state = parse_state(state, t->state);
        if (prio) t->priority = (UBaseType_t)atoi(prio);
        if (core) t->core_id = (BaseType_t)atoi(core);
        if (period) t->period_ticks = pdMS_TO_TICKS(atoi(period));
        if (stack) {
            uint32_t val = (uint32_t)atoi(stack);
            if (val < 8192) val = 8192; // Forza minimo 8192 words (32KB) per sicurezza
            t->stack_words = val;
        }
    }
    fclose(f);
}

void tasks_start_all(void)
{
    for (size_t i = 0; i < sizeof(s_tasks) / sizeof(s_tasks[0]); ++i) {
        task_param_t *t = &s_tasks[i];
        // Rispetta la configurazione di display: se headless salta lvgl/touchscreen
        if ((strcmp(t->name, "lvgl") == 0 || strcmp(t->name, "touchscreen") == 0) && !device_config_get()->display.enabled) {
            ESP_LOGI(TAG, "[M] Task saltato %s (display disabilitato da config)", t->name);
            continue;
        }
        if (t->state != TASK_STATE_RUN) {
            ESP_LOGI(TAG, "[M] Task saltato %s (stato=%d)", t->name, (int)t->state);
            continue;
        }
        ESP_LOGI(TAG, "[M] Avvio task %s (stack=%lu words)...", t->name, (unsigned long)t->stack_words);
        // Non sovrascrivere arg se già impostato (es. touchscreen handle)
        if (t->arg == NULL) {
            t->arg = t;
        }
        BaseType_t res = xTaskCreatePinnedToCore(t->task_fn, t->name, t->stack_words, t->arg, t->priority, &t->handle, t->core_id);
        if (res != pdPASS) {
            ESP_LOGE(TAG, "[M] Fallimento avvio task %s", t->name);
        }
    }
}

void tasks_apply_n_run(void)
{
    ESP_LOGI(TAG, "Applicazione nuovi stati task...");
    device_config_t *cfg = device_config_get();
    for (size_t i = 0; i < sizeof(s_tasks) / sizeof(s_tasks[0]); ++i) {
        task_param_t *t = &s_tasks[i];

        // Forza comportamento dei task legati al display in base alla config
        if ((strcmp(t->name, "lvgl") == 0 || strcmp(t->name, "touchscreen") == 0)) {
            if (!cfg->display.enabled) {
                t->state = TASK_STATE_IDLE; // headless: garantiamo che siano disabilitati
            } else {
                t->state = TASK_STATE_RUN; // display abilitato => assicuriamo che partano
            }
        }

        if (t->state == TASK_STATE_RUN) {
            if (t->handle == NULL) {
                ESP_LOGI(TAG, "Avvio task %s (stack=%lu words)...", t->name, (unsigned long)t->stack_words);
                // Non sovrascrivere arg se già impostato (es. touchscreen handle)
                if (t->arg == NULL) {
                    t->arg = t;
                }
                xTaskCreatePinnedToCore(t->task_fn, t->name, t->stack_words, t->arg, t->priority, &t->handle, t->core_id);
            } else {
                vTaskResume(t->handle);
            }
        } else if (t->state == TASK_STATE_PAUSE) {
            if (t->handle != NULL) {
                vTaskSuspend(t->handle);
            }
        } else if (t->state == TASK_STATE_IDLE) {
            if (t->handle != NULL) {
                vTaskDelete(t->handle);
                t->handle = NULL;
            }
        }
    }
}
