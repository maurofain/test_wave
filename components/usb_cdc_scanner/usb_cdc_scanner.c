#include "usb_cdc_scanner.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

#if CONFIG_USB_OTG_SUPPORTED
#include "usb/usb_host.h"
/* Note: the CDC-ACM host driver is a separate managed component (usb_host_cdc_acm).
   It may not be present in all ESP-IDF setups. If you enable that component later,
   enable CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST in Kconfig and ensure the managed
   component is available in the project. */
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST
#if defined(__has_include)
  #if __has_include("usb/cdc_acm_host.h")
    #include "usb/cdc_acm_host.h"
    #define USB_CDC_ACM_AVAILABLE 1
  #else
    #warning "usb/cdc_acm_host.h not available: CDC-ACM features will be disabled until the component is added"
    #define USB_CDC_ACM_AVAILABLE 0
  #endif
#else
  /* Fallback: attempt to include and hope for the best */
  #include "usb/cdc_acm_host.h"
  #define USB_CDC_ACM_AVAILABLE 1
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#endif

#define BARCODE_BUF_SIZE 128
static const char *TAG = "USB_CDC_SCANNER";
static usb_cdc_scanner_callback_t s_on_barcode = NULL;

#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
static TaskHandle_t s_usb_host_lib_task = NULL;
static TaskHandle_t s_usb_open_task = NULL;
static cdc_acm_dev_hdl_t s_cdc_dev = NULL;

static void usb_host_lib_task(void *arg)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
    }
}

static bool cdc_data_cb(const uint8_t *data, size_t data_len, void *arg)
{
    static char buf[BARCODE_BUF_SIZE];
    static size_t idx = 0;
    for (size_t i = 0; i < data_len; ++i) {
        char c = (char)data[i];
        if (c == '\r' || c == '\n') {
            if (idx > 0) {
                buf[idx] = '\0';
                if (s_on_barcode) s_on_barcode(buf);
                idx = 0;
            }
        } else if (idx < BARCODE_BUF_SIZE - 1) {
            buf[idx++] = c;
        }
    }
    return true;
}

static void cdc_event_cb(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "CDC device disconnected");
        if (event->data.cdc_hdl) {
            cdc_acm_host_close(event->data.cdc_hdl);
        }
        break;
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC error %d", event->data.error);
        break;
    default:
        break;
    }
}

static void usb_cdc_scanner_open_task(void *arg)
{
    const uint16_t vid = (uint16_t)CONFIG_USB_CDC_SCANNER_VID;
    const uint16_t pid = (uint16_t)CONFIG_USB_CDC_SCANNER_PID;
    const uint16_t dual_pid = (uint16_t)CONFIG_USB_CDC_SCANNER_DUAL_PID;

    cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .user_arg = NULL,
        .event_cb = cdc_event_cb,
        .data_cb = cdc_data_cb
    };

    while (1) {
        cdc_acm_dev_hdl_t cdc_dev = NULL;
        ESP_LOGI(TAG, "Trying to open CDC device %04X:%04X", vid, pid);
        esp_err_t err = cdc_acm_host_open(vid, pid, 0, &dev_config, &cdc_dev);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Try dual PID %04X:%04X", vid, dual_pid);
            err = cdc_acm_host_open(vid, dual_pid, 0, &dev_config, &cdc_dev);
        }
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "CDC device opened");
            // wait until disconnect; the event callback will handle closing
            // simply block here waiting a bit and allow callbacks to run
            while (cdc_dev != NULL) {
                vTaskDelay(pdMS_TO_TICKS(500));
                // In event of disconnect, event_cb will close and we break next loop
                // There is no explicit handle update from this thread; keep trying on failures
            }
        } else {
            ESP_LOGI(TAG, "CDC device not found, retrying in 2s");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#endif

void usb_cdc_scanner_init(const usb_cdc_scanner_config_t *config) {
    s_on_barcode = config ? config->on_barcode : NULL;
#if CONFIG_USB_OTG_SUPPORTED
    // Inizializza USB Host e CDC-ACM (semplificato, dettagli da completare secondo ESP-IDF)
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    ESP_LOGI(TAG, "USB Host installato");

#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    // Install CDC-ACM host driver (if available as component)
    ESP_LOGI(TAG, "CDC-ACM host enabled by config: installing CDC-ACM host component (if available)");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    // Create task to handle USB host library events
    if (xTaskCreate(usb_host_lib_task, "usb_host_lib_task", 4096, NULL, 20, &s_usb_host_lib_task) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to create usb_host_lib_task");
    }

    // Create background task to try opening the configured device
    if (xTaskCreate(usb_cdc_scanner_open_task, "usb_cdc_open", 4096, NULL, 18, &s_usb_open_task) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to create usb_cdc_open task");
    }

#else
    ESP_LOGW(TAG, "CDC-ACM integration disabled in Kconfig: scanner will use simulated input");
#endif
#else
    ESP_LOGW(TAG, "USB Host non abilitato in sdkconfig: il driver scanner rimane inattivo");
#endif
}

void usb_cdc_scanner_task(void *param) {
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    // If using real CDC-ACM host, the data callback handles incoming bytes.
    // This task can sleep indefinitely.
    while (1) vTaskDelay(pdMS_TO_TICKS(1000));
#else
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
#endif
}
