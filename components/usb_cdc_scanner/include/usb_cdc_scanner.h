#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback per barcode letto
typedef void (*usb_cdc_scanner_callback_t)(const char *barcode);

// Inizializza il driver scanner USB CDC
typedef struct {
    usb_cdc_scanner_callback_t on_barcode;
} usb_cdc_scanner_config_t;

void usb_cdc_scanner_init(const usb_cdc_scanner_config_t *config);
void usb_cdc_scanner_task(void *param);
esp_err_t usb_cdc_scanner_send_setup_command(void);
esp_err_t usb_cdc_scanner_send_state_command(void);
esp_err_t usb_cdc_scanner_send_on_command(void);
esp_err_t usb_cdc_scanner_send_off_command(void);
bool usb_cdc_scanner_is_connected(void);

#ifdef __cplusplus
}
#endif
