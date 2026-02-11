#include "usb_cdc_scanner.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "device_config.h"
#include "bsp/esp32_p4_nano.h"
/* Sperimentali: invio log alla Web UI per diagnostica remota */
#include "web_ui.h" /* Sperimentali */
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

/* New device callback: called from USB Host context when any device is connected.
 * We log the basic device descriptor (VID/PID, device class) for diagnostics. */
static void cdc_new_device_cb(usb_device_handle_t usb_dev)
{
    const usb_device_desc_t *device_desc = NULL;
    if (usb_host_get_device_descriptor(usb_dev, &device_desc) == ESP_OK && device_desc) {
        ESP_LOGI(TAG, "New USB device connected: VID:%04X PID:%04X class:%02X",
                 device_desc->idVendor, device_desc->idProduct, device_desc->bDeviceClass);
    } else {
        ESP_LOGI(TAG, "New USB device connected: failed to read device descriptor");
    }
}

/*
 * Persistent USB Host client for diagnostics
 * - registers a client that receives attach/detach events
 * - on attach, opens the device and dumps basic descriptors
 */
static usb_host_client_handle_t s_usb_client = NULL;

/* Sperimentali: evento client host - dump più dettagliato e invio log alla Web UI */
static void usb_host_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV: {
        ESP_LOGI(TAG, "[Sperimentali] USB host: device attached, addr=%d", event_msg->new_dev.address);
        web_ui_add_log("INFO", "USB_DBG", "[Sperimentali] device attached"); /* Sperimentali */

        usb_device_handle_t dev_hdl;
        if (usb_host_device_open(s_usb_client, event_msg->new_dev.address, &dev_hdl) == ESP_OK) {
            const usb_device_desc_t *device_desc = NULL;
            if (usb_host_get_device_descriptor(dev_hdl, &device_desc) == ESP_OK && device_desc) {
                char buf[128];
                snprintf(buf, sizeof(buf), "[Sperimentali] - Device VID:%04X PID:%04X class:%02X",
                         device_desc->idVendor, device_desc->idProduct, device_desc->bDeviceClass);
                ESP_LOGI(TAG, "%s", buf);
                web_ui_add_log("INFO", "USB_DBG", buf); /* Sperimentali */
            }
            const usb_config_desc_t *cfg_desc = NULL;
            if (usb_host_get_active_config_descriptor(dev_hdl, &cfg_desc) == ESP_OK && cfg_desc) {
                char info[128];
                snprintf(info, sizeof(info), "[Sperimentali] - Config: interfaces=%u, total_length=%u",
                         cfg_desc->bNumInterfaces, cfg_desc->wTotalLength);
                ESP_LOGI(TAG, "%s", info);
                web_ui_add_log("INFO", "USB_DBG", info); /* Sperimentali */

                /* Dump first bytes of the descriptor as hex (Sperimentali) */
                size_t dump_len = cfg_desc->wTotalLength > 64 ? 64 : cfg_desc->wTotalLength;
                const uint8_t *raw = (const uint8_t *)cfg_desc;
                char hexbuf[256];
                int pos = 0;
                pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, "[Sperimentali] - Config hex:");
                for (size_t i = 0; i < dump_len && pos < (int)(sizeof(hexbuf) - 3); ++i) {
                    pos += snprintf(hexbuf + pos, sizeof(hexbuf) - pos, " %02X", raw[i]);
                }
                ESP_LOGI(TAG, "%s", hexbuf);
                web_ui_add_log("DEBUG", "USB_DBG", hexbuf); /* Sperimentali */
            }
            usb_host_device_close(s_usb_client, dev_hdl);
        } else {
            ESP_LOGI(TAG, "[Sperimentali] - Failed to open device for descriptor dump");
            web_ui_add_log("WARN", "USB_DBG", "[Sperimentali] Failed to open device for descriptor dump");
        }
        break;
    }
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
        ESP_LOGI(TAG, "[Sperimentali] USB host: device removed");
        web_ui_add_log("INFO", "USB_DBG", "[Sperimentali] device removed");
        break;
    default:
        break;
    }
}

/* Sperimentali: task che monitora periodicamente la lista dispositivi e notifica cambiamenti */
static TaskHandle_t s_usb_monitor_task = NULL;
static void usb_host_monitor_task(void *arg)
{
    uint8_t prev_list[16] = {0};
    int prev_num = 0;
    while (1) {
        uint8_t addr_list[16];
        int num_devs = 0;
        if (usb_host_device_addr_list_fill(sizeof(addr_list), addr_list, &num_devs) == ESP_OK) {
            if (num_devs != prev_num) {
                char msg[128];
                snprintf(msg, sizeof(msg), "[Sperimentali] Devices changed: %d -> %d", prev_num, num_devs);
                ESP_LOGI(TAG, "%s", msg);
                web_ui_add_log("INFO", "USB_DBG", msg);
            } else {
                /* detect address changes */
                for (int i = 0; i < num_devs; ++i) {
                    bool found = false;
                    for (int j = 0; j < prev_num; ++j) if (addr_list[i] == prev_list[j]) { found = true; break; }
                    if (!found) {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "[Sperimentali] New device address: %d", addr_list[i]);
                        ESP_LOGI(TAG, "%s", msg);
                        web_ui_add_log("INFO", "USB_DBG", msg);
                    }
                }
            }
            /* save current */
            memset(prev_list, 0, sizeof(prev_list));
            for (int i = 0; i < num_devs && i < (int)sizeof(prev_list); ++i) prev_list[i] = addr_list[i];
            prev_num = num_devs;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void usb_cdc_scanner_open_task(void *arg)
{
    // Read runtime configuration (NVS/device_config)
    device_config_t *d_cfg = device_config_get();
    uint16_t vid = (uint16_t)CONFIG_USB_CDC_SCANNER_VID;
    uint16_t pid = (uint16_t)CONFIG_USB_CDC_SCANNER_PID;
    uint16_t dual_pid = (uint16_t)CONFIG_USB_CDC_SCANNER_DUAL_PID;
    bool runtime_enabled = true;
    if (d_cfg) {
        vid = (uint16_t)d_cfg->scanner.vid;
        pid = (uint16_t)d_cfg->scanner.pid;
        dual_pid = (uint16_t)d_cfg->scanner.dual_pid;
        runtime_enabled = d_cfg->scanner.enabled;
    }

    ESP_LOGI(TAG, "Configured scanner VID:PID %04X:%04X (dual %04X), runtime enabled=%d", vid, pid, dual_pid, runtime_enabled);

    cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .user_arg = NULL,
        .event_cb = cdc_event_cb,
        .data_cb = cdc_data_cb
    };

    while (1) {
        // Refresh runtime config each iteration to allow dynamic changes
        device_config_t *cur = device_config_get();
        if (cur) {
            vid = (uint16_t)cur->scanner.vid;
            pid = (uint16_t)cur->scanner.pid;
            dual_pid = (uint16_t)cur->scanner.dual_pid;
            runtime_enabled = cur->scanner.enabled;
        }

        if (!runtime_enabled) {
            ESP_LOGI(TAG, "Scanner disabled by runtime config, sleeping 2s");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

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
            ESP_LOGI(TAG, "CDC device not found (err=%s), enumerating connected devices...", esp_err_to_name(err));

            uint8_t addr_list[16];
            int num_devs = 0;
            esp_err_t lerr = usb_host_device_addr_list_fill(sizeof(addr_list), addr_list, &num_devs);
            if (lerr == ESP_OK) {
                ESP_LOGI(TAG, "USB host reports %d device(s) connected", num_devs);
                for (int i = 0; i < num_devs; ++i) {
                    ESP_LOGI(TAG, " - device address: %d", addr_list[i]);
                }
            } else {
                ESP_LOGI(TAG, "usb_host_device_addr_list_fill failed: %s", esp_err_to_name(lerr));
            }

            ESP_LOGI(TAG, "Retrying in 2s");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
#endif

void usb_cdc_scanner_init(const usb_cdc_scanner_config_t *config) {
    s_on_barcode = config ? config->on_barcode : NULL;
#if CONFIG_USB_OTG_SUPPORTED
    // Start USB Host using the board BSP which will also enable VBUS/power mgmt if needed
    ESP_LOGI(TAG, "Starting USB Host via BSP");
    ESP_ERROR_CHECK(bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true));
    ESP_LOGI(TAG, "USB Host started via BSP");

#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    // Install CDC-ACM host driver (if available as component)
    ESP_LOGI(TAG, "CDC-ACM host enabled by config: installing CDC-ACM host component (if available)");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    /* Register new-device callback to get notified on connect and log descriptors for diagnostics */
    if (cdc_acm_host_register_new_dev_callback(cdc_new_device_cb) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register new-device callback for CDC-ACM host");
    }

    /* Temporarily raise USB-related log levels to DEBUG to aid diagnostics when device is not detected */
    esp_log_level_set("cdc_acm", ESP_LOG_DEBUG);
    esp_log_level_set("USB", ESP_LOG_DEBUG);
    esp_log_level_set("usb_host", ESP_LOG_DEBUG);
    esp_log_level_set("usb", ESP_LOG_DEBUG);

    /* Register a diagnostic USB Host client to receive attach/detach events and dump descriptors (Sperimentali) */
    const usb_host_client_config_t client_config = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = usb_host_event_cb,
            .callback_arg = NULL,
        }
    };
    if (usb_host_client_register(&client_config, &s_usb_client) == ESP_OK) {
        ESP_LOGI(TAG, "Registered USB host diagnostic client (Sperimentali)");
    } else {
        ESP_LOGW(TAG, "Failed to register USB host diagnostic client (Sperimentali)");
    }

    /* Ensure usb_host lib event loop task is running so callbacks are delivered */
    if (s_usb_host_lib_task == NULL) {
        if (xTaskCreate(usb_host_lib_task, "usb_host_lib", 4096, NULL, 17, &s_usb_host_lib_task) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to create usb_host_lib_task");
        }
    }

    /* Sperimentali: start monitor task that periodically polls device list */
    if (s_usb_monitor_task == NULL) {
        if (xTaskCreate(usb_host_monitor_task, "usb_monitor", 4096, NULL, 16, &s_usb_monitor_task) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to create usb_monitor task (Sperimentali)");
        }
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
