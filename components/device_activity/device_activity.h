#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;
    char description[64];
} device_activity_entry_t;

/**
 * @brief Initialize activity table by loading JSON from SPIFFS.
 *        If file missing creates default table.
 */
esp_err_t device_activity_init(void);

/**
 * @brief Lookup entry by id. Returns NULL if not found.
 */
const device_activity_entry_t *device_activity_find(uint32_t id);

#ifdef __cplusplus
}
#endif
