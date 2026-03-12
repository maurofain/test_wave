#include "tasks.h"
#include "init.h"
#include "esp_log.h"
#include "esp_system.h"
#include "led_strip.h"
#include "device_config.h"
#include "sht40.h"
#include "esp_lcd_touch.h"
#include "web_ui.h"
#include "web_ui_programs.h"
#include "fsm.h"
#include "lvgl_panel.h"
#include "usb_cdc_scanner.h" // Scanner QR
#include "cctalk.h"          // cctalk_driver_init + cctalk_task_run
#include "mdb.h"             // mdb_init + mdb_engine_run
#include "sd_card.h"         // sd_card_init_monitor + sd_card_monitor_run
#include "http_services.h"
#include "modbus_relay.h"
#include "freertos/idf_additions.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include "cJSON.h"

#ifndef DNA_LED_STRIP
#define DNA_LED_STRIP 0
#endif
#ifndef DNA_SHT40
#define DNA_SHT40 0
#endif
#ifndef DNA_IO_EXPANDER
#define DNA_IO_EXPANDER 0
#endif

static const char *TAG = "TASKS";
static float s_temperature = 0.0f;
static float s_humidity = 0.0f;

#define SCANNER_QR_COOLDOWN_DEFAULT_MS 10000U
#define SCANNER_QR_COOLDOWN_MIN_MS 500U
#define SCANNER_QR_COOLDOWN_MAX_MS 60000U
#define FSM_LANGUAGE_RETURN_DEFAULT_MS 10000U
#define FSM_LANGUAGE_RETURN_MIN_MS 1000U
#define FSM_LANGUAGE_RETURN_MAX_MS 600000U
static volatile bool s_scanner_cooldown_active = false;
static volatile bool s_scanner_reenable_pending = false;
static volatile TickType_t s_scanner_cooldown_until = 0;
static volatile uint32_t s_scanner_cooldown_active_ms = SCANNER_QR_COOLDOWN_DEFAULT_MS;
static volatile uint32_t s_scanner_reenable_attempts = 0;
static http_services_customer_t s_last_customer = {0};
static bool s_last_customer_available = false;

static void publish_program_payment_event(const fsm_ctx_t *ctx, const fsm_input_event_t *source_event);


/**
 * @brief Ottiene il timeout in millisecondi per la restituzione della lingua.
 *
 * @return uint32_t Il timeout in millisecondi.
 */
static uint32_t fsm_get_language_return_timeout_ms(void)
{
    uint32_t timeout_ms = FSM_LANGUAGE_RETURN_DEFAULT_MS;
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->timeouts.exit_programs_ms > 0U) {
        timeout_ms = cfg->timeouts.exit_programs_ms;
    }

    if (timeout_ms < FSM_LANGUAGE_RETURN_MIN_MS) {
        timeout_ms = FSM_LANGUAGE_RETURN_MIN_MS;
    }
    if (timeout_ms > FSM_LANGUAGE_RETURN_MAX_MS) {
        timeout_ms = FSM_LANGUAGE_RETURN_MAX_MS;
    }

    return timeout_ms;
}


/**
 * @brief Applica la gestione del timeout per il ritorno alla lingua.
 * 
 * @param [in] fsm Puntatore al contesto del finite state machine.
 * @return Nessun valore di ritorno.
 */
static void fsm_apply_language_return_timeout(fsm_ctx_t *fsm)
{
    if (!fsm) {
        return;
    }

    uint32_t timeout_ms = fsm_get_language_return_timeout_ms();
    if (fsm->splash_screen_time_ms == timeout_ms) {
        return;
    }

    fsm->splash_screen_time_ms = timeout_ms;
    if (fsm->inactivity_ms > (timeout_ms + 5000U)) {
        fsm->inactivity_ms = timeout_ms + 5000U;
    }

    ESP_LOGI(TAG, "[M] Timeout inattività ritorno lingua impostato a %lu ms", (unsigned long)timeout_ms);
}


/**
 * @brief Ottiene la temperatura corrente.
 * 
 * @return float La temperatura in gradi Celsius.
 */
float tasks_get_temperature(void) { return s_temperature; }

/**
 * @brief Ottiene l'umidità del sistema.
 * 
 * @return float La umidità corrente del sistema, espressa in percento.
 */
float tasks_get_humidity(void) { return s_humidity; }

/* Wrapper: crea il task allocando lo stack in DRAM interna o PSRAM
 * in base al campo stack_caps del descrittore. */

/**
 * @brief Crea un nuovo task.
 *
 * @param [in] t Puntatore ai parametri del task da creare.
 * @return BaseType_t Valore di ritorno che indica il successo o l'errore dell'operazione.
 */
static BaseType_t task_create(task_param_t *t)
{
    return xTaskCreatePinnedToCoreWithCaps(
        t->task_fn, t->name, t->stack_words, t->arg,
        t->priority, &t->handle, t->core_id, t->stack_caps);
}

/* ============================================================
 * ws2812_task — implementazione REALE (hardware WS2812/RMT)
 * Attiva quando DNA_LED_STRIP == 0
 * ============================================================ */
#ifndef DNA_LED_STRIP
#define DNA_LED_STRIP 0
#endif

#if DNA_LED_STRIP == 0

/**
 * @brief Gestisce il task per la comunicazione con il dispositivo WS2812.
 *
 * Questa funzione si occupa di inviare i dati di colore ai LED WS2812
 * in base alle istruzioni ricevute.
 *
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questo contesto).
 */
static void ws2812_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    led_strip_handle_t strip = init_get_ws2812_handle();
    TickType_t last_wake = xTaskGetTickCount();
    uint8_t color = 0;

    while (true) {
        if (!strip) {
            if (!device_config_get()->sensors.led_enabled) {
                vTaskDelay(pdMS_TO_TICKS(5000));
                strip = init_get_ws2812_handle();
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
#endif /* DNA_LED_STRIP == 0 */

/* ============================================================
 * ws2812_task — implementazione MOCK (nessun hardware)
 * Attiva quando DNA_LED_STRIP == 1
 * ============================================================ */
#if defined(DNA_LED_STRIP) && (DNA_LED_STRIP == 1)

/**
 * @brief Gestisce la task per la comunicazione con il dispositivo WS2812.
 * 
 * Questa funzione si occupa di inviare i dati di colore ai LED WS2812.
 * 
 * @param arg Puntatore agli argomenti passati alla task.
 */
static void ws2812_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    ESP_LOGI(TAG, "[MOCK] ws2812_task avviata: DNA_LED_STRIP=1, nessun hardware RMT/WS2812");
    while (true) {
        vTaskDelay(param->period_ticks);
    }
}
#endif /* DNA_LED_STRIP == 1 */

// Task scheletro (solo struttura, logica da implementare)


/**
 * @brief Gestisce il task per l'accesso all'EEPROM.
 * 
 * Questa funzione viene eseguita come task e si occupa di leggere e scrivere
 * dati sull'EEPROM in base alle richieste ricevute.
 * 
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 * @return void Nessun valore di ritorno.
 */
static void eeprom_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task di espansione I/O.
 *
 * Questa funzione si occupa di gestire il task di espansione I/O, elaborando i dati ricevuti e aggiornando lo stato del sistema.
 *
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 * @return Nessun valore di ritorno.
 */
static void io_expander_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task per la comunicazione con il sensore SHT40.
 * 
 * Questa funzione si occupa di iniziare la misurazione del temperatura e
 * dell'umidità, di leggere i dati e di inviarli al server.
 * 
 * @param arg Puntatore agli argomenti del task.
 */
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


/** @brief Gestisce il task per la comunicazione RS232.
 *  
 *  Questa funzione si occupa di gestire il task per la comunicazione RS232. 
 *  Legge i dati ricevuti e li invia, gestendo eventuali errori.
 *  
 *  @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 *  @return Nessun valore di ritorno.
 */
static void rs232_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/** @brief Gestisce il task per la comunicazione RS485.
 *  
 *  Questa funzione gestisce il task dedicato alla comunicazione RS485. 
 *  Si occupa di inviare e ricevere dati tramite la linea RS485.
 *  
 *  @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 *  
 *  @return Nessun valore di ritorno.
 */
static void rs485_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    esp_err_t last_init_err = ESP_OK;
    esp_err_t last_poll_err = ESP_OK;

    while (true) {
        device_config_t *cfg = device_config_get();
        bool should_run_modbus = false;
        uint32_t delay_ms = (uint32_t)pdTICKS_TO_MS(param->period_ticks);

        if (cfg) {
            should_run_modbus = (cfg->sensors.rs485_enabled && cfg->modbus.enabled);
            if (cfg->modbus.poll_ms > 0U) {
                delay_ms = cfg->modbus.poll_ms;
            }
        }

        if (delay_ms < 20U) {
            delay_ms = 20U;
        }

        if (should_run_modbus) {
            esp_err_t init_err = modbus_relay_init();
            if (init_err != ESP_OK) {
                if (init_err != last_init_err) {
                    ESP_LOGW(TAG, "[M] Modbus init RS485 fallita: %s", esp_err_to_name(init_err));
                    last_init_err = init_err;
                }
            } else {
                last_init_err = ESP_OK;
                esp_err_t poll_err = modbus_relay_poll_once();
                if (poll_err != ESP_OK && poll_err != last_poll_err) {
                    ESP_LOGW(TAG, "[M] Modbus poll RS485 fallita: %s", esp_err_to_name(poll_err));
                    last_poll_err = poll_err;
                } else if (poll_err == ESP_OK) {
                    last_poll_err = ESP_OK;
                }
            }
        } else if (modbus_relay_is_running()) {
            (void)modbus_relay_deinit();
            last_init_err = ESP_OK;
            last_poll_err = ESP_OK;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay_ms));
    }
}


/**
 * @brief Gestisce il task per la gestione del database.
 *
 * Questa funzione viene eseguita come task e si occupa della gestione
 * dei dati del database, come la lettura, la scrittura e la sincronizzazione.
 *
 * @param arg Puntatore a dati aggiuntivi passati al task.
 */
static void mdb_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task PWM.
 * 
 * Questa funzione viene eseguita come task e si occupa di gestire il controllo PWM.
 * 
 * @param arg Puntatore a dati di input utilizzati dal task.
 * @return Nessun valore di ritorno.
 */
static void pwm_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task del Finite State Machine (FSM).
 *
 * Questa funzione è responsabile della gestione del task del FSM. Viene eseguita in un contesto di sistema operativo e gestisce lo stato corrente del sistema.
 *
 * @param arg Puntatore a dati aggiuntivi passati al task. In questo caso, non viene utilizzato.
 * @return Nessun valore di ritorno.
 */
static void fsm_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    fsm_ctx_t fsm;
    fsm_init(&fsm);
    fsm_apply_language_return_timeout(&fsm);
    TickType_t prev_tick = xTaskGetTickCount();

    if (!fsm_event_queue_init(0)) {
        ESP_LOGE(TAG, "[FSM] Event queue init failed");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "[FSM] Task started in state=%s", fsm_state_to_string(fsm.state));
    fsm_runtime_publish(&fsm);

    /* contatore per il log "alive" ogni 10 secondi */
    uint32_t alive_ms = 0;
    static const uint32_t ALIVE_INTERVAL_MS = 10000;

    while (true) {
        fsm_input_event_t event;
        bool event_received = false;
        bool changed = false;
        fsm_state_t state_before = fsm.state;

        fsm_apply_language_return_timeout(&fsm);

        /* #6 fix: receive first (blocca fino a period_ticks = 100ms), poi
         * misura l'elapsed così l'attesa è inclusa nel delta. In questo modo
         * fsm_tick() riceve il tempo realmente trascorso, non quello
         * dell'iterazione *precedente*. */
        if (fsm_event_receive(&event, AGN_ID_FSM, param->period_ticks)) {
            event_received = true;
            changed = fsm_handle_input_event(&fsm, &event);
        }

        TickType_t now_tick = xTaskGetTickCount();
        uint32_t elapsed_ms = (uint32_t)pdTICKS_TO_MS(now_tick - prev_tick);
        prev_tick = now_tick;

        changed = fsm_tick(&fsm, elapsed_ms) || changed;

        /* log alive ogni ALIVE_INTERVAL_MS */
        alive_ms += elapsed_ms;
        if (alive_ms >= ALIVE_INTERVAL_MS) {
            alive_ms = 0;
            char now_buf[24] = "--/--/---- --:--:--";
            time_t now = time(NULL);
            if (now != (time_t)-1) {
                struct tm ti;
                localtime_r(&now, &ti);
                strftime(now_buf, sizeof(now_buf), "%d/%m/%Y %H:%M:%S", &ti);
            }

            ESP_LOGI(TAG, "[FSM] Alive: now=%s state=%s credit=%ldc heap=%lu",
                     now_buf,
                     fsm_state_to_string(fsm.state),
                     (long)fsm.credit_cents,
                     (unsigned long)esp_get_free_heap_size());
        }

        if ((state_before == FSM_STATE_RUNNING || state_before == FSM_STATE_PAUSED) && fsm.state == FSM_STATE_CREDIT) {
            for (uint8_t relay = 1; relay <= WEB_UI_VIRTUAL_RELAY_MAX; ++relay) {
                (void)web_ui_virtual_relay_control(relay, false, 0);
            }
            fsm_append_message("Programma terminato: reset relay/schermata");
        }

        if (state_before == FSM_STATE_CREDIT && fsm.state == FSM_STATE_IDLE) {
            device_config_t *cfg = device_config_get();
            if (cfg && cfg->display.enabled) {
                lvgl_panel_show_language_select();
                ESP_LOGI(TAG, "[M] Timeout inattività: ritorno alla scelta lingua");
            } else {
                ESP_LOGI(TAG, "[M] Timeout inattività: display disabilitato, pagina lingua non mostrata");
            }
        }

        if (event_received &&
            event.type == FSM_INPUT_EVENT_PROGRAM_SELECTED &&
            state_before == FSM_STATE_CREDIT &&
            fsm.state == FSM_STATE_RUNNING) {
            publish_program_payment_event(&fsm, &event);
        }

        if (changed) {
            ESP_LOGI(TAG, "****************************************");
            ESP_LOGI(TAG, "[FSM] State=%s credit=%ldc", fsm_state_to_string(fsm.state), (long)fsm.credit_cents);
            ESP_LOGI(TAG, "****************************************");
        }

        fsm_runtime_publish(&fsm);

        /* give other tasks/idle a chance – prevent task watchdog trigger when
         * mailbox is busy or FSM has a burst of events. 100 ms period may not
         * be enough to break the CPU monopoly. */
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}


/**
 * @brief Gestisce il task per la gestione del touchscreen.
 *
 * Questa funzione si occupa di monitorare e interpretare gli eventi del touchscreen,
 * aggiornando lo stato dell'interfaccia utente di conseguenza.
 *
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 * @return Nessun valore di ritorno.
 */
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
                {
                    fsm_input_event_t ev = {
                        .from = AGN_ID_TOUCH,
                        .to = {AGN_ID_FSM},
                        .action = ACTION_ID_NONE,
                        .type = FSM_INPUT_EVENT_TOUCH,
                        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
                        .value_i32 = 0,
                        .value_u32 = 0,
                        .aux_u32 = 0,
                        .text = {0},
                    };
                    (void)fsm_event_publish(&ev, 0);
                }
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


/** @brief Gestisce la task LVGL.
 *  @param arg Argomento della task, non utilizzato.
 *  @return Nessun valore di ritorno. */
static void lvgl_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t last_wake = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}


/**
 * @brief Gestisce il task per l'aggiornamento del tempo NTP.
 *
 * Questa funzione viene eseguita in un task separato e si occupa di sincronizzare
 * l'orologio del sistema con un server NTP.
 *
 * @param arg Puntatore a dati di input (non utilizzato in questa funzione).
 * @return Nessun valore di ritorno.
 */
static void ntp_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;

    ESP_LOGI(TAG, "[NTP] NTP task started, period: %d seconds", 600);

    /* Attende che la rete sia pronta prima del primo tentativo.
     * L'Ethernet/Wi-Fi può impiegare alcuni secondi per ottenere un IP. */
    vTaskDelay(pdMS_TO_TICKS(30000));

    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        device_config_t *cfg = device_config_get();

        if (cfg->ntp_enabled) {
            ESP_LOGI(TAG, "[NTP] Checking NTP sync...");
            esp_err_t ret = init_sync_ntp();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "[NTP] NTP sync request sent (completamento asincrono)");
            } else {
                ESP_LOGW(TAG, "[NTP] NTP sync request failed");
            }
        } else {
            ESP_LOGD(TAG, "[NTP] NTP disabled in configuration");
        }

        vTaskDelayUntil(&last_wake, param->period_ticks);
    }
}

// wrapper per lo scanner USB: inizializza il driver e poi entra nella sua routine

/**
 * @brief Recupera il tempo di attesa scanner da configurazione.
 */
static uint32_t scanner_get_cooldown_ms(void)
{
    uint32_t cooldown_ms = SCANNER_QR_COOLDOWN_DEFAULT_MS;
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->scanner.cooldown_ms > 0) {
        cooldown_ms = cfg->scanner.cooldown_ms;
    }

    if (cooldown_ms < SCANNER_QR_COOLDOWN_MIN_MS) {
        cooldown_ms = SCANNER_QR_COOLDOWN_MIN_MS;
    }
    if (cooldown_ms > SCANNER_QR_COOLDOWN_MAX_MS) {
        cooldown_ms = SCANNER_QR_COOLDOWN_MAX_MS;
    }
    return cooldown_ms;
}

/**
 * @brief Verifica se il cooldown scanner è ancora attivo.
 */
static bool scanner_cooldown_is_active(TickType_t now)
{
    if (!s_scanner_cooldown_active) {
        return false;
    }
    return ((int32_t)(now - s_scanner_cooldown_until) < 0);
}


/**
 * @brief Riattiva lo scanner al termine del cooldown.
 */
static void scanner_cooldown_tick(void)
{
    if (!s_scanner_reenable_pending) {
        return;
    }

    TickType_t now = xTaskGetTickCount();
    if ((int32_t)(now - s_scanner_cooldown_until) < 0) {
        return;
    }

    esp_err_t on_err = usb_cdc_scanner_send_on_command();
    if (on_err == ESP_OK) {
        s_scanner_reenable_pending = false;
        s_scanner_cooldown_active = false;
        s_scanner_reenable_attempts = 0;
        ESP_LOGI("SCANNER", "[M] Scanner riattivato dopo cooldown di %lu ms",
                 (unsigned long)s_scanner_cooldown_active_ms);
    } else {
        s_scanner_reenable_attempts++;

        if ((s_scanner_reenable_attempts % 10U) == 1U) {
            ESP_LOGW("SCANNER", "[M] Riattivazione scanner fallita (tentativo=%lu): %s",
                     (unsigned long)s_scanner_reenable_attempts,
                     esp_err_to_name(on_err));
        }

        if (s_scanner_reenable_attempts >= 5U) {
            esp_err_t setup_err = usb_cdc_scanner_send_setup_command();
            if (setup_err == ESP_OK) {
                on_err = usb_cdc_scanner_send_on_command();
                if (on_err == ESP_OK) {
                    s_scanner_reenable_pending = false;
                    s_scanner_cooldown_active = false;
                    s_scanner_reenable_attempts = 0;
                    ESP_LOGI("SCANNER", "[M] Scanner riattivato con fallback setup+on");
                    return;
                }
            }

            if ((s_scanner_reenable_attempts % 10U) == 1U) {
                ESP_LOGW("SCANNER", "[M] Fallback setup+on fallito (setup=%s on=%s)",
                         esp_err_to_name(setup_err),
                         esp_err_to_name(on_err));
            }
        }

        s_scanner_cooldown_until = now + pdMS_TO_TICKS(500);
    }
}

/**
 * @brief Task dedicato alla riattivazione scanner dopo cooldown.
 */
static void scanner_cooldown_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;
    TickType_t period = (param && param->period_ticks > 0) ? param->period_ticks : pdMS_TO_TICKS(100);

    ESP_LOGI("SCANNER", "[M] scanner_cooldown_task avviato (period=%lu ms)",
             (unsigned long)pdTICKS_TO_MS(period));

    while (true) {
        scanner_cooldown_tick();
        vTaskDelay(period);
    }
}

/**
 * @brief Callback chiamata quando viene rilevato un barcode.
 *
 * Questa funzione viene invocata dal sistema quando viene rilevato un barcode.
 * 
 * @param barcode [in] Il codice barcode rilevato.
 * @return void Non restituisce alcun valore.
 */
static bool scanner_extract_clean_barcode(const char *raw_barcode, char *clean_barcode, size_t clean_len)
{
    if (!raw_barcode || !clean_barcode || clean_len == 0) {
        return false;
    }

    clean_barcode[0] = '\0';

    char printable[FSM_EVENT_TEXT_MAX_LEN] = {0};
    size_t printable_idx = 0;
    for (const unsigned char *p = (const unsigned char *)raw_barcode;
         *p != '\0' && printable_idx < sizeof(printable) - 1;
         ++p) {
        if (*p >= 32 && *p <= 126) {
            printable[printable_idx++] = (char)*p;
        }
    }
    printable[printable_idx] = '\0';

    const char *start = printable;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    size_t len = strlen(start);
    while (len > 0 && isspace((unsigned char)start[len - 1])) {
        len--;
    }
    if (len == 0) {
        return false;
    }

    char trimmed[FSM_EVENT_TEXT_MAX_LEN] = {0};
    if (len >= sizeof(trimmed)) {
        len = sizeof(trimmed) - 1;
    }
    memcpy(trimmed, start, len);
    trimmed[len] = '\0';

    bool looks_like_scanner_frame = (strstr(trimmed, "SCNENA") != NULL) ||
                                    (strstr(trimmed, "SCNMOD") != NULL) ||
                                    (strstr(trimmed, "RRDENA") != NULL) ||
                                    (strstr(trimmed, "CIDENA") != NULL) ||
                                    (strstr(trimmed, "RRDDUR") != NULL) ||
                                    (strchr(trimmed, '#') != NULL && strchr(trimmed, ';') != NULL);

    if (!looks_like_scanner_frame) {
        if (len >= clean_len) {
            len = clean_len - 1;
        }
        memcpy(clean_barcode, trimmed, len);
        clean_barcode[len] = '\0';
        return true;
    }

    size_t best_start = 0;
    size_t best_len = 0;
    size_t run_start = 0;
    size_t run_len = 0;
    for (size_t i = 0;; ++i) {
        char c = trimmed[i];
        if (isdigit((unsigned char)c)) {
            if (run_len == 0) {
                run_start = i;
            }
            run_len++;
        } else {
            if (run_len > 0) {
                if (run_len > best_len || (run_len == best_len && run_start >= best_start)) {
                    best_start = run_start;
                    best_len = run_len;
                }
                run_len = 0;
            }
            if (c == '\0') {
                break;
            }
        }
    }

    if (best_len < 6) {
        return false;
    }

    if (best_len >= clean_len) {
        best_len = clean_len - 1;
    }
    memcpy(clean_barcode, &trimmed[best_start], best_len);
    clean_barcode[best_len] = '\0';
    return true;
}


/**
 * @brief Callback chiamata quando viene rilevato un barcode.
 * 
 * Questa funzione viene invocata quando il sistema rileva un barcode.
 * 
 * @param barcode [in] Il codice barcode rilevato.
 * @return void Nessun valore di ritorno.
 */
static void scanner_on_barcode_cb(const char *barcode)
{
    if (!barcode || barcode[0] == '\0') {
        return;
    }

    char clean_barcode[FSM_EVENT_TEXT_MAX_LEN] = {0};
    if (!scanner_extract_clean_barcode(barcode, clean_barcode, sizeof(clean_barcode))) {
        ESP_LOGW("SCANNER", "[M] Barcode scanner non valido/scartato");
        return;
    }

    TickType_t now = xTaskGetTickCount();
    if (scanner_cooldown_is_active(now)) {
        ESP_LOGI("SCANNER", "[M] Barcode ignorato: scanner in cooldown");
        return;
    }

    uint32_t cooldown_ms = scanner_get_cooldown_ms();
    s_scanner_cooldown_active = true;
    s_scanner_reenable_pending = true;
    s_scanner_reenable_attempts = 0;
    s_scanner_cooldown_active_ms = cooldown_ms;
    s_scanner_cooldown_until = now + pdMS_TO_TICKS(cooldown_ms);

    esp_err_t off_err = usb_cdc_scanner_send_off_command();
    if (off_err == ESP_OK) {
        ESP_LOGI("SCANNER", "[M] Scanner spento per %lu ms dopo lettura QR",
                 (unsigned long)cooldown_ms);
    } else {
        ESP_LOGW("SCANNER", "[M] Spegnimento scanner fallito: %s",
                 esp_err_to_name(off_err));
    }

    ESP_LOGI("SCANNER", "[M] Barcode clean: %s", clean_barcode);
    /* Barcode readings: use distinct tag so UI can display readings separately */
    web_ui_add_log("INFO", "SCANNER_DATA", clean_barcode);
    {
        fsm_input_event_t ev = {
            .from = AGN_ID_USB_CDC_SCANNER,
            .to = {AGN_ID_HTTP_SERVICES},
            .action = ACTION_ID_USB_CDC_SCANNER_READ,
            .type = FSM_INPUT_EVENT_QR_SCANNED,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = 0,
            .value_u32 = 0,
            .aux_u32 = 0,
            .text = {0},
        };
        strncpy(ev.text, clean_barcode, sizeof(ev.text)-1);
        (void)fsm_event_publish(&ev, 0);
    }
}


/**
 * @brief Normalizza il testo barcode rimuovendo spazi iniziali/finali.
 */
static void normalize_barcode_text(const char *input, char *output, size_t output_len)
{
    if (!output || output_len == 0) {
        return;
    }
    output[0] = '\0';
    if (!input) {
        return;
    }

    while (*input && isspace((unsigned char)*input)) {
        input++;
    }

    size_t len = strlen(input);
    while (len > 0 && isspace((unsigned char)input[len - 1])) {
        len--;
    }

    if (len >= output_len) {
        len = output_len - 1;
    }

    memcpy(output, input, len);
    output[len] = '\0';
}


/**
 * @brief Pubblica verso FSM il credito ECD ricavato da QR.
 */
static void publish_qr_credit_event(const char *barcode, int32_t ecd_amount)
{
    if (ecd_amount <= 0) {
        return;
    }

    fsm_input_event_t credit_ev = {
        .from = AGN_ID_HTTP_SERVICES,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PAYMENT_ACCEPTED,
        .type = FSM_INPUT_EVENT_QR_CREDIT,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = ecd_amount,
        .value_u32 = 0,
        .aux_u32 = 0,
        .text = {0},
    };
    if (barcode && barcode[0] != '\0') {
        strncpy(credit_ev.text, barcode, sizeof(credit_ev.text) - 1);
    }

    if (!fsm_event_publish(&credit_ev, pdMS_TO_TICKS(50))) {
        ESP_LOGE(TAG, "[M] Publish QR_CREDIT fallito (barcode=%s ecd=%ld)",
                 barcode ? barcode : "",
                 (long)ecd_amount);
        return;
    }

    ESP_LOGI(TAG, "[M] QR_CREDIT pubblicato (barcode=%s ecd=%ld)",
             barcode ? barcode : "",
             (long)ecd_amount);
}


/**
 * @brief Pubblica verso HTTP_SERVICES l'evento di pagamento all'attivazione programma.
 */
static void publish_program_payment_event(const fsm_ctx_t *ctx, const fsm_input_event_t *source_event)
{
    if (!ctx) {
        return;
    }

    int32_t payment_amount = (ctx->running_price_units > 0) ? ctx->running_price_units : 0;
    const char *service_code = (ctx->running_program_name[0] != '\0')
                                   ? ctx->running_program_name
                                   : ((source_event && source_event->text[0] != '\0') ? source_event->text : "SER1");

    fsm_input_event_t pay_ev = {
        .from = AGN_ID_FSM,
        .to = {AGN_ID_HTTP_SERVICES},
        .action = ACTION_ID_PROGRAM_SELECTED,
        .type = FSM_INPUT_EVENT_PROGRAM_SELECTED,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = payment_amount,
        .value_u32 = 0,
        .aux_u32 = 0,
        .text = {0},
    };
    strncpy(pay_ev.text, service_code, sizeof(pay_ev.text) - 1);

    if (!fsm_event_publish(&pay_ev, pdMS_TO_TICKS(50))) {
        ESP_LOGE(TAG, "[M] Publish PAYMENT_EVENT fallito (service=%s amount=%ld)",
                 service_code,
                 (long)payment_amount);
        return;
    }

    ESP_LOGI(TAG, "[M] PAYMENT_EVENT pubblicato (service=%s amount=%ld)",
             service_code,
             (long)payment_amount);
}


/**
 * @brief Task consumer eventi per AGN_ID_HTTP_SERVICES.
 *
 * Riceve barcode scanner da mailbox FSM, interroga il backend (`getcustomers`) e
 * traduce `amount` in credito ECD verso FSM.
 */
static void http_services_task(void *arg)
{
    task_param_t *param = (task_param_t *)arg;

    if (!fsm_event_queue_init(0)) {
        ESP_LOGE(TAG, "[M] HTTP_SERVICES task: mailbox init fallita");
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        fsm_input_event_t event;
        if (!fsm_event_receive(&event, AGN_ID_HTTP_SERVICES, param->period_ticks)) {
            continue;
        }

        if (event.type == FSM_INPUT_EVENT_PROGRAM_SELECTED && event.action == ACTION_ID_PROGRAM_SELECTED) {
            const char *service_code = (event.text[0] != '\0') ? event.text : "SER1";
            int32_t amount = (event.value_i32 > 0) ? event.value_i32 : 0;
            const http_services_customer_t *customer = s_last_customer_available ? &s_last_customer : NULL;

            if (!s_last_customer_available) {
                ESP_LOGW(TAG, "[M] payment: customer non disponibile, invio campi customer vuoti");
            }

            http_services_payment_response_t pay_resp;
            esp_err_t pay_err = http_services_payment(customer, amount, service_code, &pay_resp);
            if (pay_err != ESP_OK || pay_resp.common.iserror) {
                ESP_LOGE(TAG,
                         "[M] payment fallita service=%s amount=%ld err=%s code=%ld des=%s",
                         service_code,
                         (long)amount,
                         esp_err_to_name(pay_err),
                         (long)pay_resp.common.codeerror,
                         pay_resp.common.deserror);
            } else {
                ESP_LOGI(TAG,
                         "[M] payment OK service=%s amount=%ld paymentid=%ld",
                         service_code,
                         (long)amount,
                         (long)pay_resp.paymentid);
            }
            continue;
        }

        if (event.type != FSM_INPUT_EVENT_QR_SCANNED || event.action != ACTION_ID_USB_CDC_SCANNER_READ) {
            continue;
        }

        char barcode[FSM_EVENT_TEXT_MAX_LEN] = {0};
        normalize_barcode_text(event.text, barcode, sizeof(barcode));
        if (barcode[0] == '\0') {
            ESP_LOGW(TAG, "[M] QR scanner: barcode vuoto, evento ignorato");
            continue;
        }

        ESP_LOGI(TAG, "[M] QR scanner: lookup customer code=%s", barcode);

        http_services_getcustomers_response_t gc_resp;
        esp_err_t gc_err = http_services_getcustomers(barcode, "", &gc_resp);
        if (gc_err != ESP_OK) {
            ESP_LOGE(TAG, "[M] getcustomers fallita per barcode=%s (%s)",
                     barcode,
                     esp_err_to_name(gc_err));
            continue;
        }
        if (gc_resp.common.iserror) {
            ESP_LOGW(TAG, "[M] getcustomers errore server barcode=%s code=%ld des=%s",
                     barcode,
                     (long)gc_resp.common.codeerror,
                     gc_resp.common.deserror);
            continue;
        }

        const http_services_customer_t *selected = NULL;
        for (size_t i = 0; i < gc_resp.customer_count; ++i) {
            const http_services_customer_t *candidate = &gc_resp.customers[i];
            if (!candidate->valid) {
                continue;
            }
            if (candidate->code[0] != '\0' && strcmp(candidate->code, barcode) == 0) {
                selected = candidate;
                break;
            }
            if (!selected) {
                selected = candidate;
            }
        }

        if (!selected) {
            ESP_LOGW(TAG, "[M] QR scanner: nessun customer valido per barcode=%s", barcode);
            continue;
        }

        memset(&s_last_customer, 0, sizeof(s_last_customer));
        memcpy(&s_last_customer, selected, sizeof(*selected));
        if (s_last_customer.code[0] == '\0') {
            strncpy(s_last_customer.code, barcode, sizeof(s_last_customer.code) - 1);
        }
        s_last_customer_available = true;

        if (selected->amount <= 0) {
            ESP_LOGI(TAG, "[M] QR scanner: credito non disponibile per code=%s (amount=%ld)",
                     selected->code,
                     (long)selected->amount);
            continue;
        }

        publish_qr_credit_event(barcode, selected->amount);
    }
}


/**
 * @brief Wrapper per la gestione del task del scanner USB.
 *
 * Questa funzione viene utilizzata per avviare e gestire il task del scanner USB.
 * È responsabile della creazione e del lancio del task che si occupa della scansione dei dispositivi USB.
 *
 * @param arg Puntatore a dati aggiuntivi che possono essere passati al task.
 * @return Nessun valore di ritorno.
 */
static void usb_scanner_task_wrapper(void *arg)
{
    usb_cdc_scanner_config_t cfg = {.on_barcode = scanner_on_barcode_cb};
    usb_cdc_scanner_init(&cfg);
    // usa la task routine del componente scanner che gestisce sia la modalità reale (CDC-ACM) che simulata
    usb_cdc_scanner_task(NULL);
}

/* Wrapper CCtalk: l'hardware UART è già inizializzato da init.c (cctalk_driver_init).
 * Questo wrapper entra direttamente nel loop di ricezione. */

/**
 * @brief Wrapper per l'engine CCTalk.
 *
 * Questa funzione agisce come un wrapper per l'engine CCTalk, gestendo
 * le operazioni di comunicazione e processamento dei dati.
 *
 * @param arg Puntatore a dati aggiuntivi necessari per l'operazione.
 * @return Nessun valore di ritorno.
 */
static void cctalk_engine_wrapper(void *arg)
{
    cctalk_task_run(NULL);
}

/* Wrapper MDB engine: l'hardware UART è già inizializzato da init.c (mdb_init).
 * Questo wrapper entra direttamente nel loop di polling. */

/**
 * @brief Wrapper per l'engine di gestione del database.
 *
 * Questa funzione agisce come un wrapper per l'engine di gestione del database,
 * permettendo di eseguire operazioni di lettura e scrittura su un database.
 *
 * @param arg Puntatore a un argomento generico che può essere utilizzato per passare
 *            informazioni aggiuntive alla funzione.
 * @return Nessun valore di ritorno.
 */
static void mdb_engine_wrapper(void *arg)
{
    mdb_engine_run(NULL);
}

/* Wrapper SD monitor: non richiede init hardware preventivo (il task configura
 * autonomamente il GPIO di card-detect). */

/**
 * @brief Wrapper per la funzione di monitoraggio.
 *
 * Questa funzione agisce come un wrapper per la funzione di monitoraggio,
 * permettendo di passare un argomento generico a una funzione di monitoraggio.
 *
 * @param arg Puntatore a un argomento generico da passare alla funzione di monitoraggio.
 * @return Nessun valore di ritorno.
 */
static void sd_monitor_wrapper(void *arg)
{
    sd_card_monitor_run(NULL);
}

static task_param_t s_tasks[] = {
    {
        .name = "ws2812",
        .state = TASK_STATE_RUN,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = ws2812_task,
        .stack_words = 4096,                  /* RISC-V: 4KB; ESP_LOGW in retry loop */
        .stack_caps = MALLOC_CAP_INTERNAL,
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
        .stack_words = 2048,                  /* RISC-V: StackType_t=1B; ~2KB reali */
        .stack_caps = MALLOC_CAP_SPIRAM,
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
        .stack_words = 2048,                  /* RISC-V: StackType_t=1B; ~2KB reali */
        .stack_caps = MALLOC_CAP_SPIRAM,
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
        .stack_words = 8192,                  /* RISC-V: 8KB; driver I2C + float + ESP_LOG */
        .stack_caps = MALLOC_CAP_SPIRAM,
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
        .stack_words = 4096,                  /* RISC-V: 4KB; skeleton con margine */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "rs485",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = rs485_task,
        .stack_words = 4096,                  /* RISC-V: 4KB; skeleton con margine */
        .stack_caps = MALLOC_CAP_SPIRAM,
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
        .stack_words = 4096,                  /* RISC-V: 4KB; skeleton con margine */
        .stack_caps = MALLOC_CAP_SPIRAM,
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
        .stack_words = 2048,                  /* RISC-V: StackType_t=1B; ~2KB reali */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "fsm",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = fsm_task,
        .stack_words = 32768,                 /* RISC-V: 32KB; logica FSM + ESP_LOG + cJSON */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "http_services",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = http_services_task,
        .stack_words = 32768,                 /* RISC-V: 32KB; esp_http_client + JSON parsing */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "scanner_cooldown",
        .state = TASK_STATE_RUN,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = scanner_cooldown_task,
        .stack_words = 4096,                  /* RISC-V: 4KB; polling cooldown + comando ON scanner */
        .stack_caps = MALLOC_CAP_SPIRAM,
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
        .stack_words = 8192,                  /* RISC-V: 8KB; polling touch + event publish */
        .stack_caps = MALLOC_CAP_SPIRAM,
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
        .stack_words = 65536,                 /* RISC-V: 256KB; LVGL stack frame profondo - aumentato per evitare overflow durante rendering */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "ntp",
        .state = TASK_STATE_RUN,
        .priority = 3,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(600000),
        .task_fn = ntp_task,
        .stack_words = 32768,                 /* RISC-V: 32KB; esp_http_client + TLS frame profondi */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        .name = "usb_scanner",
        .state = TASK_STATE_IDLE,
        .priority = 4,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(1000),
        .task_fn = usb_scanner_task_wrapper,
        .stack_words = 32768,                 /* RISC-V: 32KB; USB host stack profondo */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        /* Task di ricezione CCtalk. Richiede che cctalk_driver_init() sia già
         * stato chiamato da init.c (avviene quando sensors.rs232_enabled=true).
         * Default: IDLE — abilitare via tasks.csv se la periferica è presente. */
        .name = "cctalk_task",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(100),
        .task_fn = cctalk_engine_wrapper,
        .stack_words = 4096,                  /* RISC-V: 4KB; UART rx + snprintf + serial monitor */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        /* Polling engine MDB. Richiede che mdb_init() sia già stato chiamato da
         * init.c (avviene quando sensors.mdb_enabled=true).
         * Default: IDLE — abilitare via tasks.csv se la periferica è presente. */
        .name = "mdb_engine",
        .state = TASK_STATE_IDLE,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = mdb_engine_wrapper,
        .stack_words = 4096,                  /* RISC-V: 4KB; polling state-machine MDB */
        .stack_caps = MALLOC_CAP_SPIRAM,
        .arg = NULL,
        .handle = NULL,
    },
    {
        /* Monitor hot-plug SD. Configura autonomamente il GPIO di card-detect e
         * cicla indefinitamente. Avviato sempre (default RUN). */
        .name = "sd_monitor",
        .state = TASK_STATE_RUN,
        .priority = 5,
        .core_id = 0,
        .period_ticks = pdMS_TO_TICKS(500),
        .task_fn = sd_monitor_wrapper,
        .stack_words = 4096,                  /* RISC-V: 4KB; gpio_config + poll + ESP_LOG */
        .stack_caps = MALLOC_CAP_SPIRAM,
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

static const char *s_lvgl_test_task_names[] = {
    "ws2812",
    "io_expander",
    "sht40",
    "rs232",
    "rs485",
    "mdb",
    "pwm",
    "usb_scanner",
    "cctalk_task",
    "mdb_engine",
};

static bool s_lvgl_test_tasks_suspended = false;

void tasks_suspend_peripherals_for_lvgl_test(void)
{
    if (s_lvgl_test_tasks_suspended) {
        return;
    }

    for (size_t i = 0; i < sizeof(s_lvgl_test_task_names) / sizeof(s_lvgl_test_task_names[0]); ++i) {
        task_param_t *t = find_task_by_name(s_lvgl_test_task_names[i]);
        if (!t || !t->handle) {
            continue;
        }
        vTaskSuspend(t->handle);
        ESP_LOGI(TAG, "[M] LVGL test: sospesa task %s", t->name);
    }

    s_lvgl_test_tasks_suspended = true;
}

void tasks_resume_peripherals_after_lvgl_test(void)
{
    if (!s_lvgl_test_tasks_suspended) {
        return;
    }

    for (size_t i = 0; i < sizeof(s_lvgl_test_task_names) / sizeof(s_lvgl_test_task_names[0]); ++i) {
        task_param_t *t = find_task_by_name(s_lvgl_test_task_names[i]);
        if (!t || !t->handle) {
            continue;
        }
        vTaskResume(t->handle);
        ESP_LOGI(TAG, "[M] LVGL test: ripresa task %s", t->name);
    }

    s_lvgl_test_tasks_suspended = false;
}


/**
 * @brief Analizza lo stato di una task.
 *
 * Questa funzione prende una stringa che rappresenta lo stato di una task e lo converte in un valore di tipo task_state_t.
 * Se la stringa non è valida, viene restituito lo stato di default.
 *
 * @param [in] s Puntatore alla stringa che rappresenta lo stato della task.
 * @param [in] def Valore di default da restituire se la stringa non è valida.
 * @return Lo stato di tipo task_state_t corrispondente alla stringa fornita, o lo stato di default se la stringa non è valida.
 */
static task_state_t parse_state(const char *s, task_state_t def)
{
    if (!s) return def;
    if (strcasecmp(s, "run") == 0) return TASK_STATE_RUN;
    if (strcasecmp(s, "idle") == 0) return TASK_STATE_IDLE;
    if (strcasecmp(s, "pause") == 0) return TASK_STATE_PAUSE;
    return def;
}


/** @brief Imposta il gestore del touchscreen.
 *  
 *  @param [in] handle Puntatore al gestore del touchscreen.
 *  
 *  @return Nessun valore di ritorno.
 */
void tasks_set_touchscreen_handle(void *handle)
{
    task_param_t *t = find_task_by_name("touchscreen");
    if (t) {
        t->arg = handle;
    }
}


/**
 * @brief Carica la configurazione delle attività da un file.
 *
 * Questa funzione carica la configurazione delle attività dal file specificato
 * dal percorso passato come parametro.
 *
 * @param [in] path Percorso del file da cui caricare la configurazione.
 * @return Nessun valore di ritorno.
 */
void tasks_load_config(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGW(TAG, "[M] Impossibile aprire %s; uso configurazioni predefinite", path);
        return;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size <= 0 || file_size > 32768) {
        ESP_LOGE(TAG, "[M] tasks.json: dimensione non valida (%ld)", file_size);
        fclose(f);
        return;
    }

    char *buf = malloc((size_t)file_size + 1);
    if (!buf) {
        ESP_LOGE(TAG, "[M] tasks.json: out of memory");
        fclose(f);
        return;
    }
    fread(buf, 1, (size_t)file_size, f);
    fclose(f);
    buf[file_size] = '\0';

    cJSON *arr = cJSON_Parse(buf);
    free(buf);
    if (!arr || !cJSON_IsArray(arr)) {
        ESP_LOGE(TAG, "[M] tasks.json: JSON non valido");
        if (arr) cJSON_Delete(arr);
        return;
    }

    cJSON *obj;
    cJSON_ArrayForEach(obj, arr) {
        cJSON *jname = cJSON_GetObjectItem(obj, "n");
        if (!jname || !cJSON_IsString(jname)) continue;
        const char *name = jname->valuestring;

        task_param_t *t = find_task_by_name(name);
        if (!t) {
            if (strcmp(name, "http_server") == 0) continue;
            ESP_LOGD(TAG, "[M] Task '%s' in JSON non in s_tasks[] — ignorato", name);
            continue;
        }

        cJSON *jstate  = cJSON_GetObjectItem(obj, "s");
        cJSON *jprio   = cJSON_GetObjectItem(obj, "p");
        cJSON *jcore   = cJSON_GetObjectItem(obj, "c");
        cJSON *jperiod = cJSON_GetObjectItem(obj, "m");
        cJSON *jstack  = cJSON_GetObjectItem(obj, "w");
        cJSON *jcaps   = cJSON_GetObjectItem(obj, "k");

        if (jstate && cJSON_IsNumber(jstate)) {
            switch (jstate->valueint) {
                case 1:  t->state = TASK_STATE_RUN;   break;
                case 2:  t->state = TASK_STATE_PAUSE; break;
                default: t->state = TASK_STATE_IDLE;  break;
            }
        }
        if (jprio   && cJSON_IsNumber(jprio))   t->priority     = (UBaseType_t)jprio->valueint;
        if (jcore   && cJSON_IsNumber(jcore))   t->core_id      = (BaseType_t)jcore->valueint;
        if (jperiod && cJSON_IsNumber(jperiod)) t->period_ticks = pdMS_TO_TICKS(jperiod->valueint);
        if (jstack  && cJSON_IsNumber(jstack)) {
            uint32_t val = (uint32_t)jstack->valueint;
            if (val < 512) val = 512;
            t->stack_words = val;
        }
        if (jcaps && cJSON_IsNumber(jcaps)) {
            t->stack_caps = (jcaps->valueint == 1) ? MALLOC_CAP_INTERNAL : MALLOC_CAP_SPIRAM;
        }
    }
    cJSON_Delete(arr);
    ESP_LOGI(TAG, "[M] tasks.json caricato da %s", path);
}

/* ============================================================
 * tasks_set_state_idle_for_mocks
 * Imposta IDLE le task dei moduli con mock DNA_* attivo (= 1).
 * Va chiamata dopo tasks_load_config() e prima di tasks_start_all()
 * in modo che il CSV non possa sovrascrivere la decisione.
 * Aggiungere qui una riga per ogni nuovo mock DNA_*.
 * ============================================================ */

/**
 * @brief Imposta lo stato delle attività su "idle" per i mock.
 *
 * Questa funzione imposta lo stato delle attività su "idle" per i mock.
 * Non ha parametri di input o output.
 * Non restituisce alcun valore.
 */
static void tasks_set_state_idle_for_mocks(void)
{
#if DNA_LED_STRIP == 1
    task_param_t *t = find_task_by_name("ws2812");
    if (t) { t->state = TASK_STATE_IDLE; }
#endif
    /* Nota: sht40_task e io_expander_task hanno mock passthrough
     * (la funzione chiama le API del componente che risponde correttamente),
     * quindi non vanno messi IDLE: la task gira e il mock risponde.
     * Aggiungere qui solo i task che non hanno senso con il mock attivo.
     */
}


/** @brief Avvia tutte le attività del sistema.
 *  
 *  Questa funzione avvia tutte le attività del sistema, preparando il sistema per l'esecuzione.
 *  
 *  @return Nessun valore di ritorno.
 */
void tasks_start_all(void)
{
    tasks_set_state_idle_for_mocks();
    for (size_t i = 0; i < sizeof(s_tasks) / sizeof(s_tasks[0]); ++i) {
        task_param_t *t = &s_tasks[i];
        // Rispetta la configurazione di display: se headless salta lvgl/touchscreen
        if ((strcmp(t->name, "lvgl") == 0 || strcmp(t->name, "touchscreen") == 0) && !device_config_get()->display.enabled) {
            ESP_LOGI(TAG, "[M] Task saltato %s (display disabilitato da config)", t->name);
            continue;
        }
        // Salta il task io_expander se il dispositivo non è abilitato in config
        if (strcmp(t->name, "io_expander") == 0 && !device_config_get()->sensors.io_expander_enabled) {
            ESP_LOGI(TAG, "[M] Task saltato %s (I/O expander disabilitato)", t->name);
            continue;
        }
        if (t->state != TASK_STATE_RUN) {
            ESP_LOGI(TAG, "[M] Task saltato %s (stato=%d)", t->name, (int)t->state);
            continue;
        }
        if (strcmp(t->name, "touchscreen") == 0 && t->arg == NULL) {
            ESP_LOGW(TAG, "[M] Task saltato %s (touch handle non disponibile)", t->name);
            continue;
        }
        ESP_LOGI(TAG, "[M] Avvio task %s (stack=%lu words)...", t->name, (unsigned long)t->stack_words);
        // Non sovrascrivere arg se già impostato (es. touchscreen handle)
        // e non impostare fallback per task che richiedono handle specifici.
        if (t->arg == NULL && strcmp(t->name, "touchscreen") != 0) {
            t->arg = t;
        }
        BaseType_t res = task_create(t);
        if (res != pdPASS) {
            ESP_LOGE(TAG, "[M] Fallimento avvio task %s (stack=%lu caps=0x%lx)",
                     t->name, (unsigned long)t->stack_words, (unsigned long)t->stack_caps);
        }
    }
}


/** @brief Esegue tutte le attività pianificate e le esegue.
 * 
 * @param [in/out] Nessun parametro specifico.
 * 
 * @return Nessun valore di ritorno.
 * 
 * Questa funzione si occupa di eseguire tutte le attività pianificate in anticipo.
 * Non richiede parametri di input o output specifici e non restituisce alcun valore.
 */
void tasks_apply_n_run(void)
{
    ESP_LOGI(TAG, "Applicazione nuovi stati task... (display.enabled=%d)", (int)device_config_get()->display.enabled);
    device_config_t *cfg = device_config_get();
    for (size_t i = 0; i < sizeof(s_tasks) / sizeof(s_tasks[0]); ++i) {
        task_param_t *t = &s_tasks[i];

        // Forza IDLE sui task display solo se headless; altrimenti rispetta il CSV
        if ((strcmp(t->name, "lvgl") == 0 || strcmp(t->name, "touchscreen") == 0)) {
            if (!cfg->display.enabled) {
                t->state = TASK_STATE_IDLE; // headless: garantiamo che siano disabilitati
                ESP_LOGI(TAG, "[M] Task %s forzato IDLE (display.enabled=false)", t->name);
            }
            // display abilitato: stato gestito dal CSV, nessun override
        }
        // Forza stato idle su io_expander se il sensore è disabilitato
        if (strcmp(t->name, "io_expander") == 0 && !cfg->sensors.io_expander_enabled) {
            t->state = TASK_STATE_IDLE;
        }

        if (t->state == TASK_STATE_RUN) {
            if (t->handle == NULL) {
                if (strcmp(t->name, "touchscreen") == 0 && t->arg == NULL) {
                    ESP_LOGW(TAG, "[M] Task saltato %s (touch handle non disponibile)", t->name);
                    continue;
                }
                ESP_LOGI(TAG, "Avvio task %s (stack=%lu words)...", t->name, (unsigned long)t->stack_words);
                // Non sovrascrivere arg se già impostato (es. touchscreen handle)
                if (t->arg == NULL && strcmp(t->name, "touchscreen") != 0) {
                    t->arg = t;
                }
                xTaskCreatePinnedToCoreWithCaps(t->task_fn, t->name, t->stack_words, t->arg, t->priority, &t->handle, t->core_id, t->stack_caps);
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
