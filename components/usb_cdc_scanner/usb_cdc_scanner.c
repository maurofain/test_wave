#include "usb_cdc_scanner.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "device_config.h"
#include "bsp/esp32_p4_nano.h"
#include "serial_test.h"
/* Sperimentali: invio log alla Web UI per diagnostica remota */
#include "web_ui.h" /* Sperimentali */
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#ifndef DNA_USB_SCANNER
#define DNA_USB_SCANNER 0
#endif

/* Codice reale — escluso se mockup attivo */
#if DNA_USB_SCANNER == 0

#if CONFIG_USB_OTG_SUPPORTED
#include "usb/usb_host.h"
/* Nota: il driver host CDC-ACM è un componente gestito separato (usb_host_cdc_acm).
   Potrebbe non essere presente in tutte le distribuzioni ESP-IDF. Se abiliti quel componente in seguito,
   attiva CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST in Kconfig e assicurati che il componente gestito
   sia disponibile nel progetto. */
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
  /* Fallback: tentativo di inclusione sperando funzioni */
  #include "usb/cdc_acm_host.h"
  #define USB_CDC_ACM_AVAILABLE 1
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#endif

#define BARCODE_BUF_SIZE 128
/* Experimental diagnostics may allocate additional USB handles/endpoints and
 * increase host resource pressure. Disable by default for production stability.
 * Enable only when actively debugging USB enumeration or device descriptors. */
#define USB_SCANNER_EXPERIMENTAL_DIAG 0

/* Default scanner identifiers (auto-aligned / authoritative defaults) */
#define SCANNER_DEFAULT_VID  0x1EAB  /* VID: 1EAB */
#define SCANNER_DEFAULT_PID  0x0006  /* PID: 0006 */
#define SCANNER_DEFAULT_CLASS 0x02   /* Device class */

/* EPAPER_USB module (ESP32-S2 CDC ACM) defaults from host logs */
#define EPAPER_USB_DEFAULT_VID 0x303A
#define EPAPER_USB_DEFAULT_PID 0x0002

/* EPAPER_USB timing rules */
#define EPAPER_USB_DELAY_AFTER_WRITE_MS        (500)
#define EPAPER_USB_DELAY_AFTER_FULL_REFRESH_MS (2000)

static const char *TAG = "USB_CDC_SCANNER";
static usb_cdc_scanner_callback_t s_on_barcode = NULL;
static volatile bool s_scanner_connected = false;
static TaskHandle_t s_usb_open_task = NULL;

// Stato logico del dispositivo scanner (ACTIVE / SUSPENDED / OOS)
static volatile usb_cdc_scanner_state_t s_scanner_logical_state = USB_CDC_SCANNER_STATE_ACTIVE;
static volatile bool s_is_epaper_mode = false;
/* EPAPER_USB: interfaccia CDC rilevata
 * - ifnum: bInterfaceNumber (valore nel descriptor)
 * - idx:   indice 0..N-1 nell'ordine dei descriptor di interfaccia
 *
 * Alcune versioni/varianti del driver CDC-ACM host interpretano il terzo parametro
 * di open in modo diverso; logghiamo e teniamo entrambi per fallback robusto. */
static volatile int s_epaper_cdc_ifnum = -1;
static volatile int s_epaper_cdc_idx = -1;

/* EPAPER keepalive: dopo INIT (0x00 0xAA) invia 0x00 0xAF ogni 10s.
 * Nota: timer attivo solo in modalità runtime EPAPER_USB e solo se CDC è aperto. */
static esp_timer_handle_t s_epaper_keepalive_timer = NULL;
#define EPAPER_KEEPALIVE_PERIOD_US (10LL * 1000000LL)

static esp_err_t usb_cdc_scanner_epaper_send_raw_internal(const uint8_t *data, size_t len);
static void usb_cdc_scanner_epaper_keepalive_tick(void *arg);
static void usb_cdc_scanner_epaper_keepalive_start(void);
static void usb_cdc_scanner_epaper_keepalive_stop(void);

__attribute__((weak)) bool usb_cdc_scanner_runtime_allowed(void)
{
    return true;
}

static void usb_cdc_scanner_notify_open_task(void)
{
    if (s_usb_open_task == NULL) {
        return;
    }
    xTaskNotifyGive(s_usb_open_task);
}

static bool s_usb_host_initialized = false;
static bool s_usb_bsp_started = false;

device_component_status_t usb_cdc_scanner_get_component_status(void)
{
    const device_config_t *cfg = device_config_get();

    if (!cfg || !cfg->scanner.enabled) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    // Stato OOS: icona errore
    if (s_scanner_logical_state == USB_CDC_SCANNER_STATE_OOS) {
        return DEVICE_COMPONENT_STATUS_OFFLINE;
    }

    if (!s_usb_host_initialized) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    // Stato SUSPENDED: scanner operativo ma temporaneamente disabilitato (icona ok)
    if (s_scanner_logical_state == USB_CDC_SCANNER_STATE_SUSPENDED) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    // Stato ACTIVE: riflette connessione fisica
    return s_scanner_connected ? DEVICE_COMPONENT_STATUS_ONLINE
                               : DEVICE_COMPONENT_STATUS_OFFLINE;
}

#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
static cdc_acm_dev_hdl_t s_cdc_dev = NULL;
static QueueHandle_t s_cdc_data_queue = NULL;
static bool s_cdc_acm_installed = false;
#define USB_CDC_SCANNER_RX_QUEUE_LEN 1024
#define USB_CDC_SCANNER_RETRY_DELAY_MS 5000U
static const char *SCN_CMD_SETUP = "0000#SCNMOD3;RRDENA1;CIDENA1;SCNENA0;RRDDUR3000;";
static const char *SCN_CMD_STATE = "0000#SCNENA*;";
static const char *SCN_CMD_ON = "0000#SCNENA1;";
static const char *SCN_CMD_OFF = "0000#SCNENA0;";


/**
 * @brief Callback per la gestione dei dati CDC.
 *
 * Questa funzione viene chiamata quando ci sono dati disponibili per essere letti.
 *
 * @param [in] data Puntatore ai dati ricevuti.
 * @param [in] data_len Lunghezza dei dati ricevuti.
 * @param [in] arg Puntatore agli argomenti aggiuntivi.
 *
 * @return true se la gestione dei dati è stata completata con successo, false altrimenti.
 */
static bool cdc_data_cb(const uint8_t *data, size_t data_len, void *arg)
{
    /* Diagnostica minimale RX: logga ogni ~2s se arrivano dati (non stampa payload). */
    static uint32_t s_last_rx_log_ms = 0;
    uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
    if ((int32_t)(now_ms - s_last_rx_log_ms) > 2000) {
        s_last_rx_log_ms = now_ms;
        ESP_LOGI(TAG, "CDC RX data_len=%u (queue=%s)", (unsigned)data_len,
                 (s_cdc_data_queue != NULL) ? "ok" : "null");
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    for (size_t i = 0; i < data_len; ++i) {
        uint8_t byte = data[i];
        if (s_cdc_data_queue != NULL) {
            xQueueSendFromISR(s_cdc_data_queue, &byte, &xHigherPriorityTaskWoken);
        }
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    return true;
}


/**
 * @brief Callback di gestione degli eventi del dispositivo CDC-ACM.
 * 
 * Questa funzione viene chiamata quando si verifica un evento sul dispositivo CDC-ACM.
 * 
 * @param [in] event Puntatore ai dati dell'evento del dispositivo CDC-ACM.
 * @param [in] user_ctx Puntatore al contesto utente passato alla funzione di callback.
 * 
 * @return Nessun valore di ritorno.
 */
static void cdc_event_cb(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type) {
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "CDC device disconnected");
        s_scanner_connected = false;
        usb_cdc_scanner_epaper_keepalive_stop();
        if (s_is_epaper_mode) {
            ESP_LOGI("INIT", "#### EPAPER_USB CDC disconnected (keepalive stopped)");
        }
        serial_test_push_monitor_action("SCANNER", "CDC device disconnected");
        if (event->data.cdc_hdl) {
            cdc_acm_host_close(event->data.cdc_hdl);
            if (s_cdc_dev == event->data.cdc_hdl) {
                s_cdc_dev = NULL;
            }
        }
        if (s_usb_open_task != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xTaskNotifyFromISR(s_usb_open_task, 0, eNoAction, &xHigherPriorityTaskWoken);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        break;
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC error %d", event->data.error);
        break;
    default:
        break;
    }
}

static void usb_cdc_scanner_epaper_keepalive_tick(void *arg)
{
    (void)arg;
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    if (!s_is_epaper_mode) {
        return;
    }
    if (!s_scanner_connected || s_cdc_dev == NULL) {
        return;
    }
    const uint8_t ka[] = {0x00, 0xAF};
    (void)usb_cdc_scanner_epaper_send_raw_internal(ka, sizeof(ka));
#endif
}

static void usb_cdc_scanner_epaper_keepalive_start(void)
{
    if (!s_is_epaper_mode) {
        return;
    }
    if (s_epaper_keepalive_timer == NULL) {
        const esp_timer_create_args_t args = {
            .callback = &usb_cdc_scanner_epaper_keepalive_tick,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "epd_ka",
            .skip_unhandled_events = true,
        };
        if (esp_timer_create(&args, &s_epaper_keepalive_timer) != ESP_OK) {
            s_epaper_keepalive_timer = NULL;
            ESP_LOGW("INIT", "#### EPAPER_USB keepalive timer create failed");
            return;
        }
    }
    (void)esp_timer_stop(s_epaper_keepalive_timer);
    if (esp_timer_start_periodic(s_epaper_keepalive_timer, EPAPER_KEEPALIVE_PERIOD_US) == ESP_OK) {
        ESP_LOGI("INIT", "#### EPAPER_USB keepalive enabled (0x00 0xAF every 10s)");
    } else {
        ESP_LOGW("INIT", "#### EPAPER_USB keepalive start failed");
    }
}

static void usb_cdc_scanner_epaper_keepalive_stop(void)
{
    if (s_epaper_keepalive_timer != NULL) {
        (void)esp_timer_stop(s_epaper_keepalive_timer);
    }
}

/* New device callback: called from USB Host context when any device is connected.
 * We log the basic device descriptor (VID/PID, device class) for diagnostics. */
static void cdc_new_device_cb(usb_device_handle_t usb_dev)
{
    static uint16_t s_last_vid = 0;
    static uint16_t s_last_pid = 0;
    const usb_device_desc_t *device_desc = NULL;
    if (usb_host_get_device_descriptor(usb_dev, &device_desc) == ESP_OK && device_desc) {
        ESP_LOGI(TAG, "New USB device connected: VID:%04X PID:%04X class:%02X",
                 device_desc->idVendor, device_desc->idProduct, device_desc->bDeviceClass);

        /* TODO.md punto 5: log in init USB con VID/PID device rilevato */
        if (device_desc->idVendor != s_last_vid || device_desc->idProduct != s_last_pid) {
            s_last_vid = device_desc->idVendor;
            s_last_pid = device_desc->idProduct;
            ESP_LOGI("INIT", "[M] [USB] *****************************************");
            ESP_LOGI("INIT", "[M] [USB] Device rilevato su porta USB: VID=%04X PID=%04X class=%02X",
                     device_desc->idVendor, device_desc->idProduct, device_desc->bDeviceClass);
            ESP_LOGI("INIT", "[M] [USB] *****************************************");
        }

        /* EPAPER_USB: determina l'INTERFACE INDEX (0..N-1) della CDC dal configuration descriptor.
         * Nota: teniamo sia idx che bInterfaceNumber per compatibilità. */
        if (device_desc->idVendor == EPAPER_USB_DEFAULT_VID && device_desc->idProduct == EPAPER_USB_DEFAULT_PID) {
            const usb_config_desc_t *cfg_desc = NULL;
            if (usb_host_get_active_config_descriptor(usb_dev, &cfg_desc) == ESP_OK && cfg_desc) {
                const uint8_t *p = (const uint8_t *)cfg_desc;
                int len = (int)cfg_desc->wTotalLength;
                int iface_idx = -1;
                int found_comm_idx = -1;
                int found_data_idx = -1;
                uint8_t found_comm_ifnum = 0;
                uint8_t found_data_ifnum = 0;
                while (len >= 2) {
                    uint8_t desc_len = p[0];
                    uint8_t desc_type = p[1];
                    if (desc_len == 0 || desc_len > len) break;
                    if (desc_type == 0x04 && desc_len >= 9) { /* interface descriptor */
                        iface_idx++;
                        uint8_t ifnum = p[2];
                        uint8_t cls = p[5];
                        uint8_t sub = p[6];
                        uint8_t proto = p[7];
                        ESP_LOGI("INIT", "#### EPAPER_USB IFACE idx=%d ifnum=%u class=%02X sub=%02X proto=%02X",
                                 iface_idx, (unsigned)ifnum, cls, sub, proto);
                        /* Prefer COMM (0x02), fallback DATA (0x0A) */
                        if (cls == 0x02 && found_comm_idx < 0) {
                            found_comm_idx = iface_idx;
                            found_comm_ifnum = ifnum;
                        } else if (cls == 0x0A && found_data_idx < 0) {
                            found_data_idx = iface_idx;
                            found_data_ifnum = ifnum;
                        }
                    }
                    p += desc_len;
                    len -= desc_len;
                }
                int sel_idx = found_comm_idx;
                uint8_t sel_ifnum = found_comm_ifnum;
                if (sel_idx < 0) {
                    sel_idx = found_data_idx;
                    sel_ifnum = found_data_ifnum;
                }
                if (sel_idx >= 0) {
                    s_epaper_cdc_idx = sel_idx;
                    s_epaper_cdc_ifnum = (int)sel_ifnum;
                    ESP_LOGI("INIT", "#### EPAPER_USB detected CDC: idx=%d bInterfaceNumber=%u",
                             sel_idx, (unsigned)sel_ifnum);
                } else {
                    s_epaper_cdc_idx = -1;
                    s_epaper_cdc_ifnum = -1;
                    ESP_LOGW("INIT", "#### EPAPER_USB could not detect CDC interface (no class 02/0A)");
                }
            } else {
                ESP_LOGW("INIT", "#### EPAPER_USB could not read active config descriptor");
            }
        }
    } else {
        ESP_LOGI(TAG, "New USB device connected: failed to read device descriptor");
    }

    if (s_usb_open_task != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(s_usb_open_task, 0, eNoAction, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

#if CONFIG_USB_OTG_SUPPORTED && CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
static void usb_cdc_scanner_force_host_reenumeration(void)
{
    /* In alcuni casi (device già collegato a boot) lo stack host può non riportare device presenti.
     * ATTENZIONE: NON fermiamo/riavviamo l'USB Host BSP qui: in alcune versioni BSP/IDF lo stop può
     * andare in abort con ESP_ERR_INVALID_STATE (hub_uninstall). Facciamo un recovery “soft”:
     * close handle + reinstall del driver CDC-ACM. */
    ESP_LOGW("INIT", "#### EPAPER_USB forcing CDC-ACM soft reinit (no usb_host stop)");

    if (s_cdc_dev != NULL) {
        (void)cdc_acm_host_close(s_cdc_dev);
        s_cdc_dev = NULL;
    }

    if (s_cdc_acm_installed) {
        (void)cdc_acm_host_uninstall();
        s_cdc_acm_installed = false;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    esp_err_t acm_err = cdc_acm_host_install(NULL);
    ESP_LOGW("INIT", "#### EPAPER_USB cdc_acm_host_install err=%s", esp_err_to_name(acm_err));
    if (acm_err == ESP_OK) {
        s_cdc_acm_installed = true;
        if (cdc_acm_host_register_new_dev_callback(cdc_new_device_cb) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register new-device callback for CDC-ACM host");
        }
    }
}
#endif

/*
 * Persistent USB Host client for diagnostics
 * - registers a client that receives attach/detach events
 * - on attach, opens the device and dumps basic descriptors
 */
static usb_host_client_handle_t s_usb_client = NULL;
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
static TaskHandle_t s_usb_client_evt_task = NULL;
#endif

#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
/* Client callback stile “usb_host”: logga VID/PID per ogni addr visto.
 * Serve soprattutto per capire se stiamo vedendo l'hub o il device EPAPER dietro hub. */
static void usb_host_vidpid_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    (void)arg;
    usb_host_client_handle_t client = s_usb_client;
    if (!event_msg) return;

    if (event_msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        int addr = event_msg->new_dev.address;
        usb_device_handle_t dev_hdl = NULL;
        if (client && usb_host_device_open(client, addr, &dev_hdl) == ESP_OK && dev_hdl) {
            const usb_device_desc_t *dev_desc = NULL;
            if (usb_host_get_device_descriptor(dev_hdl, &dev_desc) == ESP_OK && dev_desc) {
                ESP_LOGI("INIT", "#### USB_ENUM NEW addr=%d VID=%04X PID=%04X class=%02X",
                         addr, dev_desc->idVendor, dev_desc->idProduct, dev_desc->bDeviceClass);
            } else {
                ESP_LOGW("INIT", "#### USB_ENUM NEW addr=%d (no descriptor)", addr);
            }
            usb_host_device_close(client, dev_hdl);
        } else {
            ESP_LOGW("INIT", "#### USB_ENUM NEW addr=%d (open failed)", addr);
        }

        /* sveglia il task open: potrebbe ora essere disponibile EPAPER */
        usb_cdc_scanner_notify_open_task();
    } else if (event_msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        ESP_LOGI("INIT", "#### USB_ENUM GONE");
        /* reset interfaccia EPAPER: verrà rideterminata al prossimo attach */
        s_epaper_cdc_ifnum = -1;
        usb_cdc_scanner_notify_open_task();
    }
}

static void usb_host_client_events_task(void *arg)
{
    (void)arg;
    while (1) {
        usb_host_client_handle_t client = s_usb_client;
        if (!client) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        /* Questo call dispatcha gli eventi al callback usb_host_vidpid_event_cb().
         * Non deve essere chiamato da più task contemporaneamente. */
        (void)usb_host_client_handle_events(client, pdMS_TO_TICKS(200));
    }
}
#endif

#if USB_SCANNER_EXPERIMENTAL_DIAG
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
                /* Esegue il dump dei primi byte del descriptor in esadecimale */
                snprintf(info, sizeof(info), "[Sperimentali] - Config: interfaces=%u, total_length=%u",
                         cfg_desc->bNumInterfaces, cfg_desc->wTotalLength);
                ESP_LOGI(TAG, "%s", info);
                web_ui_add_log("INFO", "USB_DBG", info); /* Sperimentali */

                /* Esegue il dump dei primi byte del descriptor in esadecimale (Sperimentali) */
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
#endif

#if USB_SCANNER_EXPERIMENTAL_DIAG
/* Sperimentali: task che monitora periodicamente la lista dispositivi e notifica cambiamenti */
static TaskHandle_t s_usb_monitor_task = NULL;

/**
 * @brief Gestisce la monitorizzazione del dispositivo USB.
 *
 * Questa funzione esegue il monitoraggio continuo del dispositivo USB e gestisce gli eventi relativi.
 *
 * @param arg Puntatore a dati aggiuntivi (non utilizzato in questa implementazione).
 * @return Nessun valore di ritorno.
 */
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
#endif

/**
 * @brief Verifica se due coppie di VID (Vendor ID) e PID (Product ID) sono uguali.
 *
 * @param [in] vid_a Primo VID da confrontare.
 * @param [in] pid_a Primo PID da confrontare.
 * @param [in] vid_b Secondo VID da confrontare.
 * @param [in] pid_b Secondo PID da confrontare.
 * @return true Se le coppie di VID e PID sono uguali.
 * @return false Se le coppie di VID e PID sono diverse.
 */
static bool is_same_vid_pid(uint16_t vid_a, uint16_t pid_a, uint16_t vid_b, uint16_t pid_b)
{
    return (vid_a == vid_b) && (pid_a == pid_b);
}


/**
 * @brief Invia un comando in formato framinizzato tramite USB CDC.
 * 
 * @param [in] payload Puntatore al buffer contenente il payload del comando.
 * @return esp_err_t Codice di errore.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_ERR_INVALID_ARG: Payload non valido (NULL).
 */
static esp_err_t usb_cdc_scanner_send_framed_command(const char *payload)
{
    if (payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    cdc_acm_dev_hdl_t cdc_hdl = s_cdc_dev;
    if (cdc_hdl == NULL) {
        usb_cdc_scanner_notify_open_task();
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t tx_buf[160] = {0};
    int written = snprintf((char *)tx_buf, sizeof(tx_buf), "%c%c%s%c", 0x7E, 0x01, payload, 0x03);
    if (written <= 0 || written >= (int)sizeof(tx_buf)) {
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = cdc_acm_host_data_tx_blocking(cdc_hdl, tx_buf, (size_t)written, 1000);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Scanner command sent: %s", payload);
    } else {
        ESP_LOGW(TAG, "Scanner command send failed (%s): %s", esp_err_to_name(err), payload);
    }
    return err;
}

static esp_err_t usb_cdc_scanner_epaper_send_raw_internal(const uint8_t *data, size_t len)
{
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    cdc_acm_dev_hdl_t cdc_hdl = s_cdc_dev;
    if (cdc_hdl == NULL) {
        usb_cdc_scanner_notify_open_task();
        return ESP_ERR_INVALID_STATE;
    }

    /* #### Log “in evidenza” per ogni comando EPAPER inviato */
    {
        char hex[3 * 64 + 1] = {0};
        size_t dump_len = (len > 64) ? 64 : len;
        size_t o = 0;
        for (size_t i = 0; i < dump_len && (o + 3) < sizeof(hex); ++i) {
            o += (size_t)snprintf(&hex[o], sizeof(hex) - o, "%02X%s", data[i], (i + 1 < dump_len) ? " " : "");
        }

        if (data[0] == 0x00 && len >= 2) {
            const uint8_t cmd = data[1];
            const char *name = "CMD";
            if (cmd == 0xAA) name = "INIT";
            else if (cmd == 0x00) name = "FULL_REFRESH";
            ESP_LOGI("INIT", "#### EPAPER_TX %s len=%u hex=%s%s", name, (unsigned)len, hex, (len > dump_len) ? " ..." : "");
        } else if (data[0] == 0x01 && len >= 6) {
            const uint8_t color = data[1];
            const uint8_t font = data[2];
            const uint8_t x = data[3];
            const uint8_t y = data[4];
            char text[48] = {0};
            size_t ti = 0;
            for (size_t i = 5; i < len && data[i] != 0x00 && ti + 1 < sizeof(text); ++i) {
                char c = (char)data[i];
                text[ti++] = (c >= 32 && c <= 126) ? c : '.';
            }
            text[ti] = 0;
            ESP_LOGI("INIT", "#### EPAPER_TX TEXT len=%u color=%u font=%u x=%u y=%u txt=\"%s\" hex=%s%s",
                     (unsigned)len, (unsigned)color, (unsigned)font, (unsigned)x, (unsigned)y,
                     text, hex, (len > dump_len) ? " ..." : "");
        } else if (data[0] == 0x02 && len >= 3) {
            ESP_LOGI("INIT", "#### EPAPER_TX LED len=%u mask=0x%02X on=0x%02X hex=%s%s",
                     (unsigned)len, data[1], data[2], hex, (len > dump_len) ? " ..." : "");
        } else {
            ESP_LOGI("INIT", "#### EPAPER_TX RAW len=%u hex=%s%s", (unsigned)len, hex, (len > dump_len) ? " ..." : "");
        }
    }

    esp_err_t err = cdc_acm_host_data_tx_blocking(cdc_hdl, data, len, 1000);
    if (err != ESP_OK) {
        ESP_LOGW("INIT", "#### EPAPER_TX FAILED (%s) len=%u", esp_err_to_name(err), (unsigned)len);
    }
    return err;
#else
    (void)data;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t usb_cdc_scanner_epaper_send_raw(const uint8_t *data, size_t len)
{
    return usb_cdc_scanner_epaper_send_raw_internal(data, len);
}


/**
 * @brief Gestisce la funzione di apertura del task scanner USB CDC.
 *
 * Questa funzione viene eseguita come task e si occupa di aprire e inizializzare
 * il dispositivo USB CDC per la scansione.
 *
 * @param arg Puntatore a dati di input passati al task.
 * @return Nessun valore di ritorno.
 */
static void usb_cdc_scanner_open_task(void *arg)
{
    /* Attendi che l'USB host completi l'enumerazione dei dispositivi già collegati */
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Read runtime configuration (NVS/device_config)
    device_config_t *d_cfg = device_config_get();
    uint16_t vid = (uint16_t)(CONFIG_USB_CDC_SCANNER_VID ? CONFIG_USB_CDC_SCANNER_VID : SCANNER_DEFAULT_VID);
    uint16_t pid = (uint16_t)(CONFIG_USB_CDC_SCANNER_PID ? CONFIG_USB_CDC_SCANNER_PID : SCANNER_DEFAULT_PID);
    /* dual_pid not used: scanner ha un solo PID */
    uint16_t dual_pid = 0; // (uint16_t)CONFIG_USB_CDC_SCANNER_DUAL_PID;
    bool runtime_enabled = true;
    if (d_cfg) {
        /* If NVS contains zero/invalid values, fall back to our SCANNER_DEFAULT_* constants */
        vid = (uint16_t)(d_cfg->scanner.vid ? d_cfg->scanner.vid : SCANNER_DEFAULT_VID);
        pid = (uint16_t)(d_cfg->scanner.pid ? d_cfg->scanner.pid : SCANNER_DEFAULT_PID);
        dual_pid = (uint16_t)d_cfg->scanner.dual_pid;
        runtime_enabled = d_cfg->scanner.enabled;
    }

    /* EPAPER_USB: il modulo ePaper è un device CDC diverso dallo scanner.
     * In questa modalità forziamo sempre VID/PID EPAPER, evitando che vecchie config “scanner” (1EAB:0006)
     * impediscano l’apertura della CDC. */
    if (d_cfg && d_cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_USB) {
        s_is_epaper_mode = true;
        vid = EPAPER_USB_DEFAULT_VID;
        pid = EPAPER_USB_DEFAULT_PID;
        runtime_enabled = true; /* anche se scanner.enabled=0, EPAPER deve poter aprire la CDC */
    } else {
        s_is_epaper_mode = false;
    }

    ESP_LOGI(TAG, "Configured scanner VID:PID %04X:%04X (dual %04X), runtime enabled=%d", vid, pid, dual_pid, runtime_enabled);
    /* Log “in evidenza” richiesto: sempre visibile a boot */
    ESP_LOGI("INIT", "[M] [USB] Scanner config runtime: VID=%04X PID=%04X (enabled=%d)",
             vid, pid, runtime_enabled ? 1 : 0);

    cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .user_arg = NULL,
        .event_cb = cdc_event_cb,
        .data_cb = cdc_data_cb
    };

    bool runtime_block_logged = false;
    uint32_t epaper_zero_dev_cycles = 0;

    while (1) {
        if (!usb_cdc_scanner_runtime_allowed()) {
            if (!runtime_block_logged) {
                ESP_LOGI(TAG, "Scanner runtime blocked: opening/enumeration paused");
                runtime_block_logged = true;
            }

            if (s_cdc_dev != NULL) {
                cdc_acm_host_close(s_cdc_dev);
                s_cdc_dev = NULL;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        runtime_block_logged = false;

        // Refresh runtime config each iteration to allow dynamic changes
        device_config_t *cur = device_config_get();
        if (cur) {
            if (cur->display.type == DEVICE_DISPLAY_TYPE_EPAPER_USB) {
                s_is_epaper_mode = true;
                vid = EPAPER_USB_DEFAULT_VID;
                pid = EPAPER_USB_DEFAULT_PID;
                dual_pid = 0;
                runtime_enabled = true;
            } else {
                s_is_epaper_mode = false;
                vid = (uint16_t)cur->scanner.vid;
                pid = (uint16_t)cur->scanner.pid;
                dual_pid = (uint16_t)cur->scanner.dual_pid;
                runtime_enabled = cur->scanner.enabled;
            }
        }

        if (!runtime_enabled) {
            ESP_LOGI(TAG, "Scanner disabled by runtime config, sleeping 2s");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        cdc_acm_dev_hdl_t cdc_dev = NULL;
        ESP_LOGI(TAG, "Trying to open CDC device %04X:%04X", vid, pid);
        ESP_LOGI("INIT", "[M] [USB] Provo apertura CDC: VID=%04X PID=%04X", vid, pid);
        ESP_LOGI("INIT", "#### USB_CDC_OPEN mode=%s VID=%04X PID=%04X",
                 s_is_epaper_mode ? "EPAPER_USB" : "SCANNER",
                 vid, pid);
        /* EPAPER_USB open strategy:
         * - prova prima con bInterfaceNumber rilevato (compat con commit 3874752)
         * - poi con interface index rilevato
         * - poi brute force su un range più ampio (0..7) */
        uint8_t open_param = 0;
        esp_err_t err = ESP_FAIL;
        if (s_is_epaper_mode) {
            if (s_epaper_cdc_ifnum >= 0 && s_epaper_cdc_ifnum <= 255) {
                open_param = (uint8_t)s_epaper_cdc_ifnum;
                ESP_LOGI("INIT", "#### EPAPER_USB open try bInterfaceNumber=%u", (unsigned)open_param);
                err = cdc_acm_host_open(vid, pid, open_param, &dev_config, &cdc_dev);
            }
            if (err != ESP_OK && s_epaper_cdc_idx >= 0 && s_epaper_cdc_idx <= 255) {
                open_param = (uint8_t)s_epaper_cdc_idx;
                ESP_LOGI("INIT", "#### EPAPER_USB open try interface_idx=%u", (unsigned)open_param);
                err = cdc_acm_host_open(vid, pid, open_param, &dev_config, &cdc_dev);
            }
            if (err != ESP_OK) {
                for (uint8_t ifx = 0; ifx < 8 && err != ESP_OK; ++ifx) {
                    ESP_LOGI("INIT", "#### EPAPER_USB open brute_force=%u", (unsigned)ifx);
                    err = cdc_acm_host_open(vid, pid, ifx, &dev_config, &cdc_dev);
                }
            }
        } else {
            err = cdc_acm_host_open(vid, pid, 0, &dev_config, &cdc_dev);
        }
        if (err != ESP_OK && s_is_epaper_mode) {
            /* EPAPER_USB (ESP32-S2) può presentarsi come composite (class EF) e l'interfaccia CDC non
             * è necessariamente la #0. Prova alcune interfacce tipiche. */
            for (uint8_t ifx = 1; ifx < 4 && err != ESP_OK; ++ifx) {
                ESP_LOGI("INIT", "#### EPAPER_USB open retry interface_idx=%u", (unsigned)ifx);
                err = cdc_acm_host_open(vid, pid, ifx, &dev_config, &cdc_dev);
            }
        }
        /* dual PID logic removed – scanner usa un solo PID */
        /*
        if (err != ESP_OK && dual_pid != 0 && dual_pid != pid) {
            ESP_LOGI(TAG, "Try dual PID %04X:%04X", vid, dual_pid);
            err = cdc_acm_host_open(vid, dual_pid, 0, &dev_config, &cdc_dev);
        }
        */
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "CDC device opened");
            ESP_LOGI("INIT", "[M] [USB] CDC aperto: VID=%04X PID=%04X", vid, pid);
            if (s_is_epaper_mode) {
                ESP_LOGI("INIT", "#### EPAPER_USB CDC opened OK (starting EPAPER protocol)");
            }
            s_cdc_dev = cdc_dev;
            s_scanner_connected = true;
            serial_test_push_monitor_action("SCANNER", "CDC device connected");

            vTaskDelay(pdMS_TO_TICKS(100));
            if (!s_is_epaper_mode) {
                esp_err_t on_err = usb_cdc_scanner_send_on_command();
                if (on_err == ESP_OK) {
                    ESP_LOGI(TAG, "Scanner ON command sent after CDC open");
                } else {
                    ESP_LOGW(TAG, "Scanner ON command after open failed: %s", esp_err_to_name(on_err));
                }
            } else {
                ESP_LOGI(TAG, "[EPAPER_USB] CDC aperto: nessun comando Newland inviato");

                /* Allinea la “seriale” CDC ai parametri del tester Electron: 9600 8N1, nessun RTS/DTR */
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
                {
                    const cdc_acm_line_coding_t lc = {
                        .dwDTERate = 9600,
                        .bCharFormat = 0, /* 1 stop bit */
                        .bParityType = 0, /* none */
                        .bDataBits = 8,
                    };
                    esp_err_t lc_err = cdc_acm_host_line_coding_set(s_cdc_dev, &lc);
                    ESP_LOGI("INIT", "#### EPAPER_USB LINE_CODING set 9600 8N1 err=%s", esp_err_to_name(lc_err));

                    esp_err_t cls_err = cdc_acm_host_set_control_line_state(s_cdc_dev, false, false);
                    ESP_LOGI("INIT", "#### EPAPER_USB CTRL_LINE dtr=0 rts=0 err=%s", esp_err_to_name(cls_err));
                }
#endif

                /* Avvio protocollo EPAPER_USB: init + testi di test */
                ESP_LOGI("INIT", "#### EPAPER_USB TX: INIT (0x00 0xAA)");
                const uint8_t ep_init[] = {0x00, 0xAA};
                (void)usb_cdc_scanner_epaper_send_raw_internal(ep_init, sizeof(ep_init));
                vTaskDelay(pdMS_TO_TICKS(EPAPER_USB_DELAY_AFTER_WRITE_MS));

                /* Keepalive: dopo INIT invia 0x00 0xAF ogni 10 secondi */
                usb_cdc_scanner_epaper_keepalive_start();

                /* Refresh totale dopo init (regola) */
                ESP_LOGI("INIT", "#### EPAPER_USB TX: FULL_REFRESH (0x00 0x00)");
                const uint8_t ep_full_refresh[] = {0x00, 0x00};
                (void)usb_cdc_scanner_epaper_send_raw_internal(ep_full_refresh, sizeof(ep_full_refresh));
                vTaskDelay(pdMS_TO_TICKS(EPAPER_USB_DELAY_AFTER_FULL_REFRESH_MS));

                /* “rotazione 0”: non è definita nel documento del protocollo;
                 * assumiamo default 0 (nessun comando). */

                /* Testi font 60px (GoogleSans 60pt → font#9): "SCAN" @ (50,100), "CODE" @ (150,100) */
                ESP_LOGI("INIT", "#### EPAPER_USB TX: TEXT boot (\"SCAN\"/\"CODE\")");
                uint8_t pkt1[] = {0x01, 0xFF, 9, 100, 50, 'S','C','A','N', 0x00};
                uint8_t pkt2[] = {0x01, 0xFF, 9, 100, 150, 'C','O','D','E', 0x00};
                (void)usb_cdc_scanner_epaper_send_raw_internal(pkt1, sizeof(pkt1));
                vTaskDelay(pdMS_TO_TICKS(EPAPER_USB_DELAY_AFTER_WRITE_MS));
                (void)usb_cdc_scanner_epaper_send_raw_internal(pkt2, sizeof(pkt2));
                vTaskDelay(pdMS_TO_TICKS(EPAPER_USB_DELAY_AFTER_WRITE_MS));
            }

            // wait until disconnect; the event callback will handle closing
            // simply block here waiting a bit and allow callbacks to run
            while (s_cdc_dev != NULL) {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            cdc_dev = NULL;
        } else if (err == ESP_ERR_NO_MEM) {
            ESP_LOGW(TAG, "CDC open failed due to low USB host resources: %s; retrying in 5s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        } else {
            if (s_is_epaper_mode) {
                ESP_LOGW("INIT", "#### EPAPER_USB CDC open failed (%s): EPAPER init NOT sent", esp_err_to_name(err));
            }
            ESP_LOGI(TAG, "CDC device not found (err=%s), enumerating connected devices...", esp_err_to_name(err));

            uint8_t addr_list[16];
            int num_devs = 0;
            esp_err_t lerr = usb_host_device_addr_list_fill(sizeof(addr_list), addr_list, &num_devs);
            if (lerr == ESP_OK) {
                ESP_LOGI(TAG, "USB host reports %d device(s) connected", num_devs);
                if (s_is_epaper_mode) {
                    if (num_devs == 0) {
                        epaper_zero_dev_cycles++;
                    } else {
                        epaper_zero_dev_cycles = 0;
                    }
                }
                for (int i = 0; i < num_devs; ++i) {
                    ESP_LOGI(TAG, " - device address: %d", addr_list[i]);
                }

                /* EPAPER_USB: dump VID/PID/class di tutti i device enumerati (hub incluso) */
                if (s_is_epaper_mode && s_usb_client != NULL) {
                    for (int i = 0; i < num_devs; ++i) {
                        usb_device_handle_t dev_hdl = NULL;
                        if (usb_host_device_open(s_usb_client, addr_list[i], &dev_hdl) != ESP_OK || dev_hdl == NULL) {
                            ESP_LOGW("INIT", "#### EPAPER_USB enum addr=%d: open failed", addr_list[i]);
                            continue;
                        }
                        const usb_device_desc_t *device_desc = NULL;
                        if (usb_host_get_device_descriptor(dev_hdl, &device_desc) == ESP_OK && device_desc) {
                            ESP_LOGI("INIT", "#### EPAPER_USB enum addr=%d VID=%04X PID=%04X class=%02X",
                                     addr_list[i],
                                     device_desc->idVendor,
                                     device_desc->idProduct,
                                     device_desc->bDeviceClass);
                        } else {
                            ESP_LOGW("INIT", "#### EPAPER_USB enum addr=%d: no device descriptor", addr_list[i]);
                        }
                        usb_host_device_close(s_usb_client, dev_hdl);
                    }
                }

                /*
                 * Fallback robusto:
                 * se VID/PID configurati non funzionano (es. config persistita vecchia),
                 * prova ad aprire i device effettivamente enumerati sul bus.
                 */
                if (s_usb_client != NULL) {
                    for (int i = 0; i < num_devs && cdc_dev == NULL; ++i) {
                        usb_device_handle_t dev_hdl = NULL;
                        if (usb_host_device_open(s_usb_client, addr_list[i], &dev_hdl) != ESP_OK || dev_hdl == NULL) {
                            continue;
                        }

                        const usb_device_desc_t *device_desc = NULL;
                        if (usb_host_get_device_descriptor(dev_hdl, &device_desc) == ESP_OK && device_desc) {
                            uint16_t enum_vid = device_desc->idVendor;
                            uint16_t enum_pid = device_desc->idProduct;
                            uint8_t dev_class = device_desc->bDeviceClass;
                            ESP_LOGI(TAG, "Enumerated device %d VID:%04X PID:%04X class:%02X",
                                     addr_list[i], enum_vid, enum_pid, dev_class);

                            /* Decide whether to attempt a CDC-ACM open.
                             * - If the device class is CDC (0x02) or zero, try.
                             * - If the device class is something else, inspect interfaces for CDC class.
                             */
                            bool has_cdc_iface = false;
                            if (dev_class != SCANNER_DEFAULT_CLASS && dev_class != 0) {
                                const usb_config_desc_t *cfg_desc = NULL;
                                if (usb_host_get_active_config_descriptor(dev_hdl, &cfg_desc) == ESP_OK && cfg_desc) {
                                    const uint8_t *p = (const uint8_t *)cfg_desc + cfg_desc->bLength;
                                    int len = cfg_desc->wTotalLength - cfg_desc->bLength;
                                    while (len >= 2) {
                                        uint8_t desc_len = p[0];
                                        uint8_t desc_type = p[1];
                                        if (desc_len == 0) break;
                                        if (desc_type == 0x04 && desc_len >= 9) { /* interface descriptor */
                                            uint8_t iface_class = p[5];
                                            if (iface_class == SCANNER_DEFAULT_CLASS) {
                                                has_cdc_iface = true;
                                                break;
                                            }
                                        }
                                        p += desc_len;
                                        len -= desc_len;
                                    }
                                }
                            }

                            if (!has_cdc_iface && dev_class != SCANNER_DEFAULT_CLASS && dev_class != 0) {
                                ESP_LOGI(TAG, "Skipping device %04X:%04X with class %02X (no CDC interface)",
                                         enum_vid, enum_pid, dev_class);
                            } else if (!is_same_vid_pid(enum_vid, enum_pid, vid, pid) &&
                                       !is_same_vid_pid(enum_vid, enum_pid, vid, dual_pid)) {
                                ESP_LOGI(TAG, "Trying fallback open on enumerated device %04X:%04X", enum_vid, enum_pid);
                                err = cdc_acm_host_open(enum_vid, enum_pid, 0, &dev_config, &cdc_dev);
                                if (err == ESP_OK) {
                                    ESP_LOGI(TAG, "CDC device opened via fallback VID:PID %04X:%04X", enum_vid, enum_pid);
                                    s_cdc_dev = cdc_dev;
                                    s_scanner_connected = true;
                                    serial_test_push_monitor_action("SCANNER", "CDC device connected");

                                    vTaskDelay(pdMS_TO_TICKS(100));
                                    esp_err_t on_err = usb_cdc_scanner_send_on_command();
                                    if (on_err == ESP_OK) {
                                        ESP_LOGI(TAG, "Scanner ON command sent after fallback open");
                                    } else {
                                        ESP_LOGW(TAG, "Scanner ON command after fallback open failed: %s", esp_err_to_name(on_err));
                                    }

                                    /* Persist detected VID/PID into runtime config (NVS) */
                                    device_config_t *cfg = device_config_get();
                                    if (cfg && (cfg->scanner.vid != enum_vid || cfg->scanner.pid != enum_pid)) {
                                        cfg->scanner.vid = enum_vid;
                                        cfg->scanner.pid = enum_pid;
                                        cfg->scanner.dual_pid = 0;
                                        if (device_config_save(cfg) == ESP_OK) {
                                            ESP_LOGI(TAG, "Auto-updated stored scanner VID:PID -> %04X:%04X", enum_vid, enum_pid);
                                            char msgbuf[96];
                                            snprintf(msgbuf, sizeof(msgbuf), "Auto-align scanner to %04X:%04X", enum_vid, enum_pid);
                                            web_ui_add_log("INFO", "USB_DBG", msgbuf);
                                        } else {
                                            ESP_LOGW(TAG, "Failed to persist scanner VID:PID to NVS");
                                        }
                                    }
                                }
                            }
                        }

                        usb_host_device_close(s_usb_client, dev_hdl);
                    }
                }
            } else {
                ESP_LOGI(TAG, "usb_host_device_addr_list_fill failed: %s", esp_err_to_name(lerr));
            }

            if (err == ESP_OK) {
                while (s_cdc_dev != NULL) {
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                cdc_dev = NULL;
            } else {
                ESP_LOGI(TAG, "Retrying in 5s");
                if (s_is_epaper_mode && epaper_zero_dev_cycles >= 3) {
                    epaper_zero_dev_cycles = 0;
#if CONFIG_USB_OTG_SUPPORTED && CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
                    usb_cdc_scanner_force_host_reenumeration();
#endif
                }
            }
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(USB_CDC_SCANNER_RETRY_DELAY_MS));
    }
}

#endif /* CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && USB_CDC_ACM_AVAILABLE */

void usb_cdc_scanner_init(const usb_cdc_scanner_config_t *config) {
    s_on_barcode = config ? config->on_barcode : NULL;
    if (s_cdc_data_queue == NULL) {
        s_cdc_data_queue = xQueueCreate(USB_CDC_SCANNER_RX_QUEUE_LEN, sizeof(uint8_t));
        if (s_cdc_data_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create USB CDC scanner RX queue");
        }
    }

    if (s_usb_host_initialized) {
        ESP_LOGI(TAG, "USB CDC scanner already initialized, skipping repeated init");
        return;
    }
    s_usb_host_initialized = true;

#if CONFIG_USB_OTG_SUPPORTED
    // Start USB Host using the board BSP which will also enable VBUS/power mgmt if needed
    if (!s_usb_bsp_started) {
        ESP_LOGI(TAG, "Starting USB Host via BSP");
        ESP_ERROR_CHECK(bsp_usb_host_start(BSP_USB_HOST_POWER_MODE_USB_DEV, true));
        s_usb_bsp_started = true;
        ESP_LOGI(TAG, "USB Host started via BSP");
    } else {
        ESP_LOGI(TAG, "USB Host BSP already started");
    }

#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    // Install CDC-ACM host driver (if available as component)
    if (!s_cdc_acm_installed) {
        ESP_LOGI(TAG, "CDC-ACM host enabled by config: installing CDC-ACM host component (if available)");
        ESP_ERROR_CHECK(cdc_acm_host_install(NULL));
        s_cdc_acm_installed = true;
    } else {
        ESP_LOGI(TAG, "CDC-ACM host already installed");
    }

    /* Register new-device callback to get notified on connect and log descriptors for diagnostics */
    if (cdc_acm_host_register_new_dev_callback(cdc_new_device_cb) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register new-device callback for CDC-ACM host");
    }

    /*
     * Modalità operativa normale: niente forcing DEBUG sullo stack USB,
     * per evitare flood di log che può impattare CPU/UART.
     */
    esp_log_level_set("cdc_acm", ESP_LOG_WARN);
    esp_log_level_set("USB", ESP_LOG_WARN);
    esp_log_level_set("usb_host", ESP_LOG_WARN);
    esp_log_level_set("usb", ESP_LOG_WARN);

    /* Client usb_host per log VID/PID (sempre utile in EPAPER_USB; leggero) */
    if (s_usb_client == NULL) {
        const usb_host_client_config_t client_config = {
            .is_synchronous = false,
            .max_num_event_msg = 5,
            .async = {
                .client_event_callback = usb_host_vidpid_event_cb,
                .callback_arg = NULL,
            }
        };
        if (usb_host_client_register(&client_config, &s_usb_client) == ESP_OK) {
            ESP_LOGI(TAG, "Registered USB host client (VID/PID dump)");
            if (s_usb_client_evt_task == NULL) {
                if (xTaskCreate(usb_host_client_events_task, "usb_client_evt", 4096, NULL, 9, &s_usb_client_evt_task) != pdTRUE) {
                    ESP_LOGW(TAG, "Failed to create usb_client_evt task");
                    s_usb_client_evt_task = NULL;
                }
            }
        } else {
            ESP_LOGW(TAG, "Failed to register USB host client (VID/PID dump)");
        }
    }

    /* Il loop usb_host_lib è già gestito dal BSP (bsp_usb_host_start). */

    /* Monitor USB diagnostico opzionale (disabilitato di default). */
#if USB_SCANNER_EXPERIMENTAL_DIAG
    if (s_usb_monitor_task == NULL) {
        if (xTaskCreate(usb_host_monitor_task, "usb_monitor", 4096, NULL, 16, &s_usb_monitor_task) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to create usb_monitor task (Sperimentali)");
        }
    }
#endif

    // Create background task to try opening the configured device
    if (s_usb_open_task == NULL) {
        if (xTaskCreate(usb_cdc_scanner_open_task, "usb_cdc_open", 4096, NULL, 18, &s_usb_open_task) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to create usb_cdc_open task");
            s_usb_open_task = NULL;
        }
    } else {
        ESP_LOGI(TAG, "usb_cdc_open task already running");
    }

#else
    ESP_LOGW(TAG, "CDC-ACM integration disabled in Kconfig: scanner will use simulated input");
#endif
#else
    ESP_LOGW(TAG, "USB Host non abilitato in sdkconfig: il driver scanner rimane inattivo");
#endif
}


/**
 * @brief Task per lo scanner USB CDC.
 * 
 * Questa funzione gestisce il task per lo scanner USB CDC. 
 * Si occupa di leggere i dati inviati dal dispositivo USB CDC e di elaborarli.
 * 
 * @param param Puntatore a parametri di input per il task.
 * @return void Nessun valore di ritorno.
 */
void usb_cdc_scanner_task(void *param) {
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    char barcode[BARCODE_BUF_SIZE];
    int idx = 0;
    TickType_t last_rx_tick = 0;
    const TickType_t flush_timeout = pdMS_TO_TICKS(300); /* evita concatenazione di frame incompleti */
    if (s_cdc_data_queue == NULL) {
        s_cdc_data_queue = xQueueCreate(USB_CDC_SCANNER_RX_QUEUE_LEN, sizeof(uint8_t));
        if (s_cdc_data_queue == NULL) {
            ESP_LOGE(TAG, "Failed to create USB CDC scanner RX queue");
        }
    }

    while (1) {
        uint8_t byte = 0;
        if (s_cdc_data_queue != NULL && xQueueReceive(s_cdc_data_queue, &byte, pdMS_TO_TICKS(500)) == pdTRUE) {
            char c = (char)byte;
            TickType_t now = xTaskGetTickCount();
            if (idx > 0 && last_rx_tick != 0 && (int32_t)(now - last_rx_tick) > (int32_t)flush_timeout) {
                /* Se passa troppo tempo tra byte, considera la riga precedente corrotta/incompleta. */
                idx = 0;
            }
            last_rx_tick = now;

            if (c == '\r' || c == '\n') {
                if (idx > 0) {
                    barcode[idx] = '\0';
                    if (s_on_barcode) s_on_barcode(barcode);
                    idx = 0;
                }
            } else if ((unsigned char)c < 32 || (unsigned char)c > 126) {
                /* Separatore robusto: byte non stampabile → chiudi/flush riga */
                if (idx > 0) {
                    barcode[idx] = '\0';
                    if (s_on_barcode) s_on_barcode(barcode);
                    idx = 0;
                }
            } else if (idx < BARCODE_BUF_SIZE - 1) {
                barcode[idx++] = c;
            }
        }
    }
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


/**
 * @brief Invia un comando di configurazione all'USB CDC Scanner.
 *
 * Questa funzione invia un comando di configurazione all'USB CDC Scanner per iniziare la scansione.
 *
 * @param [in] Nessun parametro di input.
 * @return esp_err_t Codice di errore che indica il successo o la fallita dell'operazione.
 */
esp_err_t usb_cdc_scanner_send_setup_command(void)
{
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    if (s_is_epaper_mode) {
        return ESP_OK;
    }
    return usb_cdc_scanner_send_framed_command(SCN_CMD_SETUP);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}


/**
 * @brief Invia un comando di stato al dispositivo USB CDC scanner.
 *
 * Questa funzione invia un comando di stato al dispositivo USB CDC scanner per ottenere
 * l'attuale stato del dispositivo.
 *
 * @param [in/out] None
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t usb_cdc_scanner_send_state_command(void)
{
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    return usb_cdc_scanner_send_framed_command(SCN_CMD_STATE);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}


/**
 * @brief Invia dati tramite USB CDC in risposta a un comando.
 *
 * Questa funzione invia dati tramite l'interfaccia USB CDC in risposta a un comando specifico.
 *
 * @param [in] data Puntatore ai dati da inviare.
 * @param [in] length Lunghezza dei dati da inviare.
 * @return esp_err_t Codice di errore.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione fallita.
 */
esp_err_t usb_cdc_scanner_send_on_command(void)
{
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    if (s_is_epaper_mode) {
        return ESP_OK;
    }
    return usb_cdc_scanner_send_framed_command(SCN_CMD_ON);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}


/**
 * @brief Invia il comando di spegnimento alla scanner USB CDC.
 *
 * Questa funzione invia un comando di spegnimento alla scanner USB CDC.
 *
 * @param [in] Nessun parametro di input.
 * @return esp_err_t Codice di errore.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione fallita.
 */
esp_err_t usb_cdc_scanner_send_off_command(void)
{
#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
    if (s_is_epaper_mode) {
        return ESP_OK;
    }
    return usb_cdc_scanner_send_framed_command(SCN_CMD_OFF);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

bool usb_cdc_scanner_is_connected(void)
{
    return s_scanner_connected;
}

usb_cdc_scanner_state_t usb_cdc_scanner_get_state(void)
{
    return s_scanner_logical_state;
}

/**
 * @brief Imposta lo stato logico dello scanner e invia i comandi HW corrispondenti.
 *
 * ACTIVE     → setup + on (scanner abilitato e pronto)
 * SUSPENDED  → off (scanner disabilitato temporaneamente, es. durante un programma)
 * OOS        → off (scanner fuori servizio)
 *
 * @param state Nuovo stato logico desiderato.
 * @return esp_err_t ESP_OK se il comando HW è andato a buon fine o se non necessario.
 */
esp_err_t usb_cdc_scanner_set_state(usb_cdc_scanner_state_t state)
{
    const usb_cdc_scanner_state_t prev_state = s_scanner_logical_state;
    s_scanner_logical_state = state;

    /* EPAPER_USB: lo stato logico viene usato solo internamente (UI/health),
     * ma NON dobbiamo inviare né loggare comandi Newland (setup/on/off).
     * Manteniamo un log molto ridotto solo sul cambio stato. */
    if (s_is_epaper_mode) {
        if (prev_state != state) {
            ESP_LOGI(TAG, "[EPAPER_USB] Scanner state -> %s",
                     (state == USB_CDC_SCANNER_STATE_ACTIVE)    ? "ACTIVE" :
                     (state == USB_CDC_SCANNER_STATE_SUSPENDED) ? "SUSPENDED" :
                     (state == USB_CDC_SCANNER_STATE_OOS)       ? "OOS" : "UNKNOWN");
        }
        return ESP_OK;
    }

    switch (state) {
    case USB_CDC_SCANNER_STATE_ACTIVE: {
        esp_err_t err_setup = usb_cdc_scanner_send_setup_command();
        esp_err_t err_on    = usb_cdc_scanner_send_on_command();
        if (err_setup == ESP_OK && err_on == ESP_OK) {
            ESP_LOGI(TAG, "[C] Scanner → ACTIVE (setup+on OK)");
        } else {
            /* Riduci spam se il CDC non è ancora aperto */
            if (err_setup == ESP_ERR_INVALID_STATE && err_on == ESP_ERR_INVALID_STATE) {
                static uint32_t s_last_log_ms = 0;
                uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
                if ((int32_t)(now_ms - s_last_log_ms) > 2000) {
                    s_last_log_ms = now_ms;
                    ESP_LOGW(TAG, "[C] Scanner → ACTIVE non disponibile (CDC non aperto ancora)");
                }
            } else {
                ESP_LOGW(TAG, "[C] Scanner → ACTIVE fallito (setup=%s on=%s)",
                         esp_err_to_name(err_setup), esp_err_to_name(err_on));
            }
            return (err_on != ESP_OK) ? err_on : err_setup;
        }
        return ESP_OK;
    }
    case USB_CDC_SCANNER_STATE_SUSPENDED: {
        esp_err_t err = usb_cdc_scanner_send_off_command();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[C] Scanner → SUSPENDED (off OK)");
        } else {
            ESP_LOGW(TAG, "[C] Scanner → SUSPENDED fallito (off=%s)", esp_err_to_name(err));
        }
        return err;
    }
    case USB_CDC_SCANNER_STATE_OOS: {
        esp_err_t err = usb_cdc_scanner_send_off_command();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[C] Scanner → OOS (off OK)");
        } else {
            ESP_LOGW(TAG, "[C] Scanner → OOS fallito (off=%s)", esp_err_to_name(err));
        }
        return err;
    }
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

#endif /* DNA_USB_SCANNER == 0 */

/*
 * Mockup — nessun USB OTG reale, nessuna periferica scanner.
 * Attiva quando DNA_USB_SCANNER == 1
 */
#if defined(DNA_USB_SCANNER) && (DNA_USB_SCANNER == 1)

static const char *TAG_MOCK = "USB_SCN";
static bool s_mock_scanner_initialized = false;

device_component_status_t usb_cdc_scanner_get_component_status(void)
{
    const device_config_t *cfg = device_config_get();

    if (!cfg || !cfg->scanner.enabled) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (!s_mock_scanner_initialized) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    return DEVICE_COMPONENT_STATUS_OFFLINE;
}


/**
 * @brief Inizializza il scanner USB CDC.
 *
 * Questa funzione inizializza il scanner USB CDC utilizzando la configurazione fornita.
 *
 * @param [in] config Puntatore alla struttura di configurazione del scanner USB CDC.
 * @return Nessun valore di ritorno.
 */
void usb_cdc_scanner_init(const usb_cdc_scanner_config_t *config)
{
    (void)config;
    s_mock_scanner_initialized = true;
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] usb_cdc_scanner_init: scanner USB simulato");
}


/**
 * @brief Gestisce il task per lo scanner USB CDC.
 *
 * Questa funzione si occupa di gestire il task per lo scanner USB CDC, che si occupa di leggere i dati
 * inviati dal dispositivo USB CDC e di elaborarli.
 *
 * @param param Puntatore a un parametro di input utilizzato dal task.
 * @return Nessun valore di ritorno.
 */
void usb_cdc_scanner_task(void *param)
{
    (void)param;
    /* MOCK: task fittizio, esce subito */
    vTaskDelete(NULL);
}


/**
 * @brief Invia un comando di configurazione all'USB CDC scanner.
 *
 * Questa funzione invia un comando di configurazione all'USB CDC scanner per iniziare la scansione.
 *
 * @param [in] Nessun parametro di input.
 * @return esp_err_t Errore di ritorno.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione non riuscita.
 */
esp_err_t usb_cdc_scanner_send_setup_command(void)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] usb_cdc_scanner_send_setup_command");
    return ESP_OK;
}


/**
 * @brief Invia un comando di stato al dispositivo USB CDC scanner.
 *
 * Questa funzione invia un comando di stato al dispositivo USB CDC scanner per ottenere
 * l'attuale stato del dispositivo.
 *
 * @param [in/out] None
 * @return esp_err_t Errore generato dalla funzione.
 */
esp_err_t usb_cdc_scanner_send_state_command(void)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] usb_cdc_scanner_send_state_command");
    return ESP_OK;
}


/**
 * @brief Invia dati tramite USB CDC in risposta a un comando.
 *
 * Questa funzione invia dati tramite l'interfaccia USB CDC in risposta a un comando specifico.
 *
 * @param [in] data Puntatore ai dati da inviare.
 * @param [in] length Lunghezza dei dati da inviare.
 * @return esp_err_t Codice di errore.
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione fallita.
 */
esp_err_t usb_cdc_scanner_send_on_command(void)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] usb_cdc_scanner_send_on_command");
    return ESP_OK;
}


/**
 * @brief Invia il comando di spegnimento alla scanner USB CDC.
 *
 * Questa funzione invia un comando di spegnimento alla scanner USB CDC.
 *
 * @return esp_err_t
 *         - ESP_OK: il comando è stato inviato con successo.
 *         - ESP_FAIL: si è verificato un errore durante l'invio del comando.
 */
esp_err_t usb_cdc_scanner_send_off_command(void)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] usb_cdc_scanner_send_off_command");
    return ESP_OK;
}

bool usb_cdc_scanner_is_connected(void)
{
    return false;
}

usb_cdc_scanner_state_t usb_cdc_scanner_get_state(void)
{
    return USB_CDC_SCANNER_STATE_ACTIVE;
}

esp_err_t usb_cdc_scanner_set_state(usb_cdc_scanner_state_t state)
{
    ESP_LOGI(TAG_MOCK, "[C] [MOCK] usb_cdc_scanner_set_state(%d)", (int)state);
    return ESP_OK;
}

#endif /* DNA_USB_SCANNER == 1 */
