#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "component_status.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback per barcode letto
typedef void (*usb_cdc_scanner_callback_t)(const char *barcode);

// Inizializza il driver scanner USB CDC
typedef struct {
    usb_cdc_scanner_callback_t on_barcode;
} usb_cdc_scanner_config_t;

// Stato logico dello scanner (indipendente dalla connessione fisica)
typedef enum {
    USB_CDC_SCANNER_STATE_ACTIVE = 0,   // scanner abilitato e operativo
    USB_CDC_SCANNER_STATE_SUSPENDED,    // scanner sospeso (es. durante programma attivo)
    USB_CDC_SCANNER_STATE_OOS,          // scanner fuori servizio
} usb_cdc_scanner_state_t;

void usb_cdc_scanner_init(const usb_cdc_scanner_config_t *config);
void usb_cdc_scanner_task(void *param);
esp_err_t usb_cdc_scanner_send_setup_command(void);
esp_err_t usb_cdc_scanner_send_state_command(void);
esp_err_t usb_cdc_scanner_send_on_command(void);
esp_err_t usb_cdc_scanner_send_off_command(void);
bool usb_cdc_scanner_is_connected(void);
device_component_status_t usb_cdc_scanner_get_component_status(void);

// Gestione stato logico con aggiornamento hw integrato
esp_err_t usb_cdc_scanner_set_state(usb_cdc_scanner_state_t state);
usb_cdc_scanner_state_t usb_cdc_scanner_get_state(void);

#ifdef __cplusplus
}
#endif
