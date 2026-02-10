#include "usb_cdc_scanner.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"

#define BARCODE_BUF_SIZE 128
static const char *TAG = "USB_CDC_SCANNER";
static usb_cdc_scanner_callback_t s_on_barcode = NULL;

void usb_cdc_scanner_init(const usb_cdc_scanner_config_t *config) {
    s_on_barcode = config ? config->on_barcode : NULL;
    // Inizializza USB Host e CDC-ACM (semplificato, dettagli da completare secondo ESP-IDF)
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    ESP_LOGI(TAG, "USB Host installato");
    // Qui andrebbe la registrazione della callback per device CDC-ACM
}

void usb_cdc_scanner_task(void *param) {
    char barcode[BARCODE_BUF_SIZE];
    int idx = 0;
    while (1) {
        // Simulazione: in reale, leggere da CDC-ACM
        int c = getchar(); // Sostituire con lettura CDC-ACM
        if (c == '\r' || c == '\n') {
            if (idx > 0) {
                barcode[idx] = 0;
                if (s_on_barcode) s_on_barcode(barcode);
                idx = 0;
            }
        } else if (c > 0 && idx < BARCODE_BUF_SIZE - 1) {
            barcode[idx++] = (char)c;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
