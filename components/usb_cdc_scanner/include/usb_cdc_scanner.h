#pragma once
#include <stdint.h>
#include <stdbool.h>

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

#ifdef __cplusplus
}
#endif
