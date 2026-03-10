#include "keepalive_task.h"
#include "http_services.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include <stdlib.h>
#include "tasks.h" // For tasks_get_temperature and tasks_get_humidity

static const char *TAG = "keepalive_task";

static TaskHandle_t s_keepalive_task_handle = NULL;
static TimerHandle_t s_keepalive_timer_handle = NULL;
static bool s_keepalive_running = false;
static uint32_t s_keepalive_count = 0;
static uint32_t s_keepalive_failures = 0;
static uint32_t s_token_refresh_count = 0;

// Keepalive statistics
static keepalive_stats_t s_stats = {0};

/**
 * @brief Send keepalive message without requiring token request first
 */
static esp_err_t keepalive_send_message(void)
{
    if (!http_services_has_auth_token()) {
        ESP_LOGW(TAG, "[K] No token available, skipping keepalive");
        snprintf(s_stats.last_error, sizeof(s_stats.last_error), "No token available");
        return ESP_ERR_INVALID_STATE;
    }

    // Get actual temperature and humidity
    float temp_float = tasks_get_temperature();
    float hum_float = tasks_get_humidity();
    
    // Convert to hundredths (e.g. 23.45 -> 2345)
    int32_t temp_hundredths = (int32_t)(temp_float * 100.0f);
    int32_t hum_hundredths = (int32_t)(hum_float * 100.0f);

    // Prepare keepalive payload with temperature and humidity
    char payload[256];
    int64_t timestamp = esp_timer_get_time() / 1000; // milliseconds
    snprintf(payload, sizeof(payload), 
             "{\"status\":\"OK\",\"timestamp\":%lld,\"temperature\":%ld,\"humidity\":%ld}", 
             timestamp, (long)temp_hundredths, (long)hum_hundredths);

    // For now, just simulate keepalive with logging
    // TODO: Implement actual HTTP POST call when API is available
    s_stats.total_sent++;
    s_stats.successful++;
    s_stats.last_success_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    s_keepalive_count++;
    
    ESP_LOGI(TAG, "[K] Keepalive sent: %s (count: %u)", payload, s_keepalive_count);
    
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

    esp_err_t err = keepalive_send_message();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[K] Keepalive failed, will retry next cycle");
    }
}

/**
 * @brief Keepalive task main function
 */
static void keepalive_task(void *parameter)
{
    (void)parameter;
    
    ESP_LOGI(TAG, "[K] Keepalive task started");
    
    // Create timer for 10-second intervals
    s_keepalive_timer_handle = xTimerCreate(
        "keepalive_timer",
        pdMS_TO_TICKS(10000),  // 10 seconds
        pdTRUE,                // Auto-reload
        NULL,
        keepalive_timer_callback
    );
    
    if (s_keepalive_timer_handle == NULL) {
        ESP_LOGE(TAG, "[K] Failed to create keepalive timer");
        vTaskDelete(NULL);
        return;
    }
    
    // Start the timer
    if (xTimerStart(s_keepalive_timer_handle, 0) != pdPASS) {
        ESP_LOGE(TAG, "[K] Failed to start keepalive timer");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "[K] Keepalive timer started (10-second intervals)");
    
    // Task main loop - just wait and monitor
    while (s_keepalive_running) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Check every 30 seconds
        
        // Log periodic statistics
        ESP_LOGI(TAG, "[K] Stats - Sent: %u, Success: %u, Failed: %u, Refresh: %u", 
                 s_stats.total_sent, s_stats.successful, s_stats.failed, s_stats.token_refreshes);
    }
    
    // Cleanup
    if (s_keepalive_timer_handle) {
        xTimerDelete(s_keepalive_timer_handle, 0);
        s_keepalive_timer_handle = NULL;
    }
    
    ESP_LOGI(TAG, "[K] Keepalive task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Start the keepalive task
 */
esp_err_t keepalive_task_start(void)
{
    if (s_keepalive_running) {
        ESP_LOGW(TAG, "[K] Keepalive task already running");
        return ESP_ERR_INVALID_STATE;
    }
    
    s_keepalive_running = true;
    s_keepalive_count = 0;
    s_keepalive_failures = 0;
    s_token_refresh_count = 0;
    memset(&s_stats, 0, sizeof(s_stats));
    
    BaseType_t result = xTaskCreate(
        keepalive_task,
        "keepalive_task",
        4096,  // Stack size
        NULL,
        5,    // Priority (medium)
        &s_keepalive_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "[K] Failed to create keepalive task");
        s_keepalive_running = false;
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "[K] Keepalive task created successfully");
    return ESP_OK;
}

/**
 * @brief Stop the keepalive task
 */
esp_err_t keepalive_task_stop(void)
{
    if (!s_keepalive_running) {
        ESP_LOGW(TAG, "[K] Keepalive task not running");
        return ESP_ERR_INVALID_STATE;
    }
    
    s_keepalive_running = false;
    
    if (s_keepalive_task_handle) {
        vTaskDelete(s_keepalive_task_handle);
        s_keepalive_task_handle = NULL;
    }
    
    ESP_LOGI(TAG, "[K] Keepalive task stopped");
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
