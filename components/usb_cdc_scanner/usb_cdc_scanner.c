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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
#define USB_SCANNER_EXPERIMENTAL_DIAG 0

/* Default scanner identifiers (auto-aligned / authoritative defaults) */
#define SCANNER_DEFAULT_VID  0x1EAB  /* VID: 1EAB */
#define SCANNER_DEFAULT_PID  0x0006  /* PID: 0006 */
#define SCANNER_DEFAULT_CLASS 0x02   /* Device class */

static const char *TAG = "USB_CDC_SCANNER";
static usb_cdc_scanner_callback_t s_on_barcode = NULL;

__attribute__((weak)) bool usb_cdc_scanner_runtime_allowed(void)
{
    return true;
}

#if CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && (defined(USB_CDC_ACM_AVAILABLE) && USB_CDC_ACM_AVAILABLE)
static TaskHandle_t s_usb_open_task = NULL;
static cdc_acm_dev_hdl_t s_cdc_dev = NULL;
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
        if (event->data.cdc_hdl) {
            cdc_acm_host_close(event->data.cdc_hdl);
            if (s_cdc_dev == event->data.cdc_hdl) {
                s_cdc_dev = NULL;
            }
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

    ESP_LOGI(TAG, "Configured scanner VID:PID %04X:%04X (dual %04X), runtime enabled=%d", vid, pid, dual_pid, runtime_enabled);

    cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .user_arg = NULL,
        .event_cb = cdc_event_cb,
        .data_cb = cdc_data_cb
    };

    bool runtime_block_logged = false;

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
        /* dual PID logic removed – scanner usa un solo PID */
        /*
        if (err != ESP_OK && dual_pid != 0 && dual_pid != pid) {
            ESP_LOGI(TAG, "Try dual PID %04X:%04X", vid, dual_pid);
            err = cdc_acm_host_open(vid, dual_pid, 0, &dev_config, &cdc_dev);
        }
        */
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "CDC device opened");
            s_cdc_dev = cdc_dev;

            vTaskDelay(pdMS_TO_TICKS(100));
            esp_err_t on_err = usb_cdc_scanner_send_on_command();
            if (on_err == ESP_OK) {
                ESP_LOGI(TAG, "Scanner ON command sent after CDC open");
            } else {
                ESP_LOGW(TAG, "Scanner ON command after open failed: %s", esp_err_to_name(on_err));
            }

            // wait until disconnect; the event callback will handle closing
            // simply block here waiting a bit and allow callbacks to run
            while (s_cdc_dev != NULL) {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            cdc_dev = NULL;
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
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#endif /* CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST && USB_CDC_ACM_AVAILABLE */

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

    /*
     * Modalità operativa normale: niente forcing DEBUG sullo stack USB,
     * per evitare flood di log che può impattare CPU/UART.
     */
    esp_log_level_set("cdc_acm", ESP_LOG_WARN);
    esp_log_level_set("USB", ESP_LOG_WARN);
    esp_log_level_set("usb_host", ESP_LOG_WARN);
    esp_log_level_set("usb", ESP_LOG_WARN);

    /* Client diagnostico opzionale (disabilitato di default per ridurre uso RAM USB). */
#if USB_SCANNER_EXPERIMENTAL_DIAG
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
#else
    s_usb_client = NULL;
#endif

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
    return usb_cdc_scanner_send_framed_command(SCN_CMD_OFF);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

#endif /* DNA_USB_SCANNER == 0 */

/*
 * Mockup — nessun USB OTG reale, nessuna periferica scanner.
 * Attiva quando DNA_USB_SCANNER == 1
 */
#if defined(DNA_USB_SCANNER) && (DNA_USB_SCANNER == 1)

static const char *TAG_MOCK = "USB_SCN";


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

#endif /* DNA_USB_SCANNER == 1 */
