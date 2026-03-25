#include "keepalive_task.h"
#include "http_services.h"
#include "digital_io.h"
#include "sht40.h"
#include "fsm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "keepalive_task";

static TaskHandle_t s_keepalive_task_handle = NULL;
static TimerHandle_t s_keepalive_timer_handle = NULL;
static bool s_keepalive_running = false;
static uint32_t s_keepalive_count = 0;

// Keepalive statistics
static keepalive_stats_t s_stats = {0};

static void keepalive_build_bit_string(uint16_t mask, uint8_t count, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }

    if (out_len <= count) {
        out[0] = '\0';
        return;
    }

    for (uint8_t i = 0; i < count; ++i) {
        out[i] = ((mask >> i) & 0x1U) ? '1' : '0';
    }
    out[count] = '\0';
}

/**
 * @brief Send keepalive message without requiring token request first
 */
static esp_err_t keepalive_send_message(void)
{
    digital_io_snapshot_t snapshot = {0};
    fsm_ctx_t fsm_snapshot = {0};
    bool has_fsm_snapshot = fsm_runtime_snapshot(&fsm_snapshot);
    char inputstates[DIGITAL_IO_LOCAL_INPUT_COUNT + 1] = {0};
    char outputstates[DIGITAL_IO_LOCAL_OUTPUT_COUNT + 1] = {0};
    float temp_float = 0.0f;
    float hum_float = 0.0f;
    http_services_keepalive_response_t *response = calloc(1, sizeof(http_services_keepalive_response_t));
    if (response == NULL) {
        ESP_LOGE(TAG, "[C] Keepalive: allocazione risposta fallita");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t snapshot_err = digital_io_get_snapshot(&snapshot);
    if (snapshot_err != ESP_OK) {
        ESP_LOGW(TAG, "[C] Snapshot digital_io non disponibile per keepalive: %s",
                 esp_err_to_name(snapshot_err));
        snapshot.inputs_mask = 0;
        snapshot.outputs_mask = 0;
    }

    keepalive_build_bit_string(snapshot.inputs_mask,
                               DIGITAL_IO_LOCAL_INPUT_COUNT,
                               inputstates,
                               sizeof(inputstates));
    keepalive_build_bit_string(snapshot.outputs_mask,
                               DIGITAL_IO_LOCAL_OUTPUT_COUNT,
                               outputstates,
                               sizeof(outputstates));

    esp_err_t sht_err = sht40_read(&temp_float, &hum_float);
    if (sht_err != ESP_OK) {
        ESP_LOGW(TAG, "[C] Lettura SHT40 fallita per keepalive: %s",
                 esp_err_to_name(sht_err));
        temp_float = 0.0f;
        hum_float = 0.0f;
    }

    int32_t temp_hundredths = (int32_t)(temp_float * 100.0f);
    int32_t hum_hundredths = (int32_t)(hum_float * 100.0f);
    s_stats.total_sent++;

    const char *status_text = "OK";
    if (has_fsm_snapshot && fsm_snapshot.state == FSM_STATE_OUT_OF_SERVICE) {
        status_text = "OUT_OF_SERVICE";
    }

    esp_err_t err = http_services_keepalive(status_text,
                                            inputstates,
                                            outputstates,
                                            temp_hundredths,
                                            hum_hundredths,
                                            response);
    if (err != ESP_OK) {
        s_stats.failed++;
        s_stats.last_failure_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
        const char *err_text = response->common.deserror[0] ? response->common.deserror : esp_err_to_name(err);
        strncpy(s_stats.last_error, err_text, sizeof(s_stats.last_error) - 1);
        s_stats.last_error[sizeof(s_stats.last_error) - 1] = '\0';
        ESP_LOGW(TAG,
                 "[C] Keepalive remoto fallito: %s (inputs=%s outputs=%s)",
                 s_stats.last_error,
                 inputstates,
                 outputstates);
        free(response);
        return err;
    }

    s_stats.successful++;
    s_stats.last_success_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    s_stats.last_error[0] = '\0';
    s_keepalive_count++;

    ESP_LOGI(TAG,
             "[C] Keepalive remoto inviato: inputs=%s outputs=%s temp=%ld hum=%ld activities=%u count=%u ecd=%ld vcd=%ld credit=%ld ecd_res=%ld vcd_res=%ld",
             inputstates,
             outputstates,
             (long)temp_hundredths,
             (long)hum_hundredths,
             (unsigned)response->activity_count,
             (unsigned)s_keepalive_count,
             has_fsm_snapshot ? (long)fsm_snapshot.ecd_coins : -1L,
             has_fsm_snapshot ? (long)fsm_snapshot.vcd_coins : -1L,
             has_fsm_snapshot ? (long)fsm_snapshot.credit_cents : -1L,
             has_fsm_snapshot ? (long)fsm_snapshot.ecd_cents_residual : -1L,
             has_fsm_snapshot ? (long)fsm_snapshot.vcd_cents_residual : -1L);

    free(response);

    return ESP_OK;
}

/**
 * @brief Keepalive timer callback
 */
static void keepalive_timer_callback(TimerHandle_t xTimer)
{
    (void)xTimer;
    
    if (!s_keepalive_running) {
        return;
    }

    if (s_keepalive_task_handle != NULL) {
        xTaskNotifyGive(s_keepalive_task_handle);
    }
}

/**
 * @brief Keepalive task main function
 */
static void keepalive_task(void *parameter)
{
    (void)parameter;
    
    ESP_LOGI(TAG, "[C] Keepalive task avviato");
    
    // Create timer for 10-second intervals
    s_keepalive_timer_handle = xTimerCreate(
        "keepalive_timer",
        pdMS_TO_TICKS(10000),  // 10 seconds
        pdTRUE,                // Auto-reload
        NULL,
        keepalive_timer_callback
    );
    
    if (s_keepalive_timer_handle == NULL) {
        ESP_LOGE(TAG, "[C] Failed to create keepalive timer");
        vTaskDelete(NULL);
        return;
    }
    
    // Start the timer
    if (xTimerStart(s_keepalive_timer_handle, 0) != pdPASS) {
        ESP_LOGE(TAG, "[C] Failed to start keepalive timer");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "[C] Keepalive timer started (10-second intervals)");
    
    TickType_t last_stats_log = xTaskGetTickCount();

    // Task main loop - attende eventi timer ed esegue keepalive nel contesto task
    while (s_keepalive_running) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) > 0) {
            esp_err_t err = keepalive_send_message();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "[C] Keepalive fallito, nuovo tentativo al prossimo ciclo");
            }
        }

        TickType_t now = xTaskGetTickCount();
        if ((now - last_stats_log) >= pdMS_TO_TICKS(30000)) {
            ESP_LOGI(TAG, "[C] Keepalive stats - Sent: %u, Success: %u, Failed: %u, Refresh: %u",
                     s_stats.total_sent, s_stats.successful, s_stats.failed, s_stats.token_refreshes);
            last_stats_log = now;
        }
    }
    
    // Cleanup
    if (s_keepalive_timer_handle) {
        xTimerDelete(s_keepalive_timer_handle, 0);
        s_keepalive_timer_handle = NULL;
    }
    
    ESP_LOGI(TAG, "[C] Keepalive task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Start the keepalive task
 */
esp_err_t keepalive_task_start(void)
{
    if (s_keepalive_running) {
        ESP_LOGW(TAG, "[C] Keepalive task already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    s_keepalive_running = true;
    s_keepalive_count = 0;
    memset(&s_stats, 0, sizeof(s_stats));
    
    BaseType_t result = xTaskCreate(
        keepalive_task,
        "keepalive_task",
        8192,  // Stack size
        NULL,
        5,    // Priority (medium)
        &s_keepalive_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "[C] Failed to create keepalive task");
        s_keepalive_running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "[C] Keepalive task created successfully");
    return ESP_OK;
}

/**
 * @brief Stop the keepalive task
 */
esp_err_t keepalive_task_stop(void)
{
    if (!s_keepalive_running) {
        ESP_LOGW(TAG, "[C] Keepalive task not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    s_keepalive_running = false;
    
    if (s_keepalive_task_handle) {
        vTaskDelete(s_keepalive_task_handle);
        s_keepalive_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "[C] Keepalive task stopped");
    return ESP_OK;
}

/**
 * @brief Get keepalive task statistics
 */
esp_err_t keepalive_task_get_stats(keepalive_stats_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(stats, &s_stats, sizeof(keepalive_stats_t));
    return ESP_OK;
}

/**
 * @brief Check if keepalive task is running
 */
bool keepalive_task_is_running(void)
{
    return s_keepalive_running;
}
