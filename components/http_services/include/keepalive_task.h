#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Keepalive task statistics
 */
typedef struct {
    uint32_t total_sent;        ///< Total keepalive messages sent
    uint32_t successful;        ///< Successful keepalive messages
    uint32_t failed;             ///< Failed keepalive messages
    uint32_t token_refreshes;    ///< Number of token refreshes
    uint32_t last_success_ms;    ///< Timestamp of last success
    uint32_t last_failure_ms;    ///< Timestamp of last failure
    char last_error[64];         ///< Last error message
} keepalive_stats_t;

/**
 * @brief Start the keepalive task
 * 
 * The task will send keepalive messages every 10 seconds.
 * If a token expires, it will automatically refresh the token.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t keepalive_task_start(void);

/**
 * @brief Stop the keepalive task
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t keepalive_task_stop(void);

/**
 * @brief Get keepalive task statistics
 * 
 * @param stats Pointer to statistics structure to fill
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t keepalive_task_get_stats(keepalive_stats_t *stats);

/**
 * @brief Check if keepalive task is running
 * 
 * @return true if running, false otherwise
 */
bool keepalive_task_is_running(void);

#ifdef __cplusplus
}
#endif
