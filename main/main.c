#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "init.h"
#include "tasks.h"
#include "app_version.h"
#include "device_config.h"
#include "error_log.h"

static const char *TAG = "APP";
#define LOG_CTX_PREFIX "[" COMPILE_LOG_PREFIX "]"

/*
 * Policy azzeramento contatore reboot consecutivi.
 * 0 = reset immediato dopo tasks_start_all()
 * 1 = reset posticipato dopo finestra di stabilità
 */
#define BOOT_COUNTER_RESET_DELAYED 1
#define BOOT_COUNTER_STABLE_WINDOW_MS (30000)

/*
 * Debug: forza un crash subito al boot per validare il flusso crash-log/boot-guard.
 * 0 = disattivo, 1 = attivo
 */
#define DEBUG_FORCE_CRASH_AT_BOOT 0

/*
 * Policy log runtime iniziale: minima verbosità al boot.
 *
 * Regola: livello globale = ERROR all'avvio.
 * In seguito il livello può essere rialzato dinamicamente via codice/API.
 */
static void apply_boot_log_policy(void)
{
#if COMPILE_APP
    esp_log_level_set("*", ESP_LOG_INFO);
#else
    esp_log_level_set("*", ESP_LOG_ERROR);
#endif

    /* Stack USB/CDC: forza ERROR al boot per evitare flood */
    esp_log_level_set("usb", ESP_LOG_ERROR);
    esp_log_level_set("USB", ESP_LOG_ERROR);
    esp_log_level_set("usb_host", ESP_LOG_ERROR);
    esp_log_level_set("cdc_acm", ESP_LOG_ERROR);
}

/*
 * Dopo il boot, abilitiamo INFO per vedere lo stato operativo,
 * mantenendo USB/CDC più silenziosi per evitare flood.
 */
static void apply_post_boot_log_policy(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("usb", ESP_LOG_WARN);
    esp_log_level_set("USB", ESP_LOG_WARN);
    esp_log_level_set("usb_host", ESP_LOG_WARN);
    esp_log_level_set("cdc_acm", ESP_LOG_WARN);
}

static void maybe_force_crash_at_boot(void)
{
#if DEBUG_FORCE_CRASH_AT_BOOT
    char debug_msg[192];
    snprintf(debug_msg,
             sizeof(debug_msg),
             "[DEBUG_FORCE_CRASH_AT_BOOT] trigger requested | mode=%s version=%s date=%s\n",
             COMPILE_MODE_LABEL,
             APP_VERSION,
             APP_DATE);
    error_log_write_msg(debug_msg);

    ESP_LOGE(TAG,
             LOG_CTX_PREFIX " DEBUG_FORCE_CRASH_AT_BOOT attivo: crash intenzionale in corso | mode=%s version=%s date=%s",
             COMPILE_MODE_LABEL,
             APP_VERSION,
             APP_DATE);

    snprintf(debug_msg,
             sizeof(debug_msg),
             "[DEBUG_FORCE_CRASH_AT_BOOT] forcing panic fault now | mode=%s version=%s date=%s\n",
             COMPILE_MODE_LABEL,
             APP_VERSION,
             APP_DATE);
    error_log_write_msg(debug_msg);

    error_log_write_msg("[DEBUG_FORCE_CRASH_AT_BOOT] stack/backtrace completo non viene scritto in error_*.log; verra' salvato nel coredump su SD al boot successivo\n");

    esp_err_t mark_ret = init_mark_forced_crash_request();
    if (mark_ret != ESP_OK)
    {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " impossibile registrare marker force_crash: %s", esp_err_to_name(mark_ret));
    }

    vTaskDelay(pdMS_TO_TICKS(150));
    volatile uint32_t *bad_ptr = (volatile uint32_t *)0x0;
    *bad_ptr = 0xDEADCAFE;
#endif
}

void app_main(void)
{
    apply_boot_log_policy();
    esp_log_level_set("INIT", ESP_LOG_INFO);
    // inizializza logging errori su SD (dopo montare la SD)
    error_log_init();

    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "  MODO COMPILE: %s", COMPILE_MODE_LABEL);
    ESP_LOGI(TAG, "  MODO RUNNING: %s", device_config_get_running_app_name());
    ESP_LOGI(TAG, "  App Test Wave - Versione: %s", APP_VERSION);
    ESP_LOGI(TAG, "  Data Compilazione: %s", APP_DATE);
    ESP_LOGI(TAG, "==========================================");

    // Inizializzazione I2C e I/O Expander
    ESP_LOGI(TAG, "Inizializzazione I2C e I/O Expander");
    init_i2c_and_io_expander();

    esp_err_t init_ret = init_run_factory();
    if (init_ret == ESP_ERR_INVALID_STATE && init_is_error_lock_active())
    {
        ESP_LOGE(TAG, LOG_CTX_PREFIX " ERROR_LOCK attivo: avvio task inibito (reboot consecutivi=%lu)",
                 (unsigned long)init_get_consecutive_reboots());
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_ERROR_CHECK(init_ret);

    maybe_force_crash_at_boot();

    /*
     * Riapplica la policy dopo le init: alcuni componenti possono alterare
     * i livelli durante l'avvio.
     */
    apply_post_boot_log_policy();

    tasks_load_config("/spiffs/tasks.csv");
    tasks_start_all();
    if (BOOT_COUNTER_RESET_DELAYED)
    {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " reset contatore reboot posticipato di %d ms", BOOT_COUNTER_STABLE_WINDOW_MS);
        const uint32_t step_ms = 5000;
        uint32_t remaining_ms = BOOT_COUNTER_STABLE_WINDOW_MS;
        while (remaining_ms > 0) {
            uint32_t this_step = (remaining_ms > step_ms) ? step_ms : remaining_ms;
            ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] finestra stabilita': attendo ancora %lu ms (heap=%lu dram=%lu)",
                     (unsigned long)remaining_ms,
                     (unsigned long)esp_get_free_heap_size(),
                     (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            vTaskDelay(pdMS_TO_TICKS(this_step));
            remaining_ms -= this_step;
        }
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] finestra stabilita' completata (%d ms)", BOOT_COUNTER_STABLE_WINDOW_MS);
    }

    esp_err_t boot_done_ret = init_mark_boot_completed();
    if (boot_done_ret != ESP_OK)
    {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " impossibile azzerare contatore reboot consecutivi: %s", esp_err_to_name(boot_done_ret));
    }
    ESP_LOGI(TAG, LOG_CTX_PREFIX " App pronta: endpoint HTTP /status e /ota disponibili");

    // Loop principale: segnala attività periodicamente
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Attesa
        ESP_LOGI(TAG, LOG_CTX_PREFIX " Device in funzione ✓");
    }
}
