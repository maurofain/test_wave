#include "include/usb_cdc_epaper.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if defined(__has_include)
  #if __has_include("usb/cdc_acm_host.h")
    #include "usb/cdc_acm_host.h"
    #define USB_CDC_ACM_AVAILABLE 1
  #else
    #define USB_CDC_ACM_AVAILABLE 0
  #endif
#else
  #include "usb/cdc_acm_host.h"
  #define USB_CDC_ACM_AVAILABLE 1
#endif

#if USB_CDC_ACM_AVAILABLE
static cdc_acm_dev_hdl_t s_cdc = NULL;
#endif

static esp_timer_handle_t s_keepalive_timer = NULL;
static volatile uint32_t s_keepalive_seq = 0;
#define EPAPER_KEEPALIVE_PERIOD_US (1LL * 1000000LL)
/* Small pacing between EPAPER writes (mirrors existing EPAPER_USB tuning). */
#define EPAPER_USB_DELAY_AFTER_WRITE_MS (500)

void usb_cdc_epaper_keepalive_stop(const char *reason);

static void keepalive_tick(void *arg)
{
    (void)arg;
#if USB_CDC_ACM_AVAILABLE
    if (s_cdc == NULL) {
        return;
    }
    const uint8_t ka[] = {0x00, 0xAF};
    uint32_t seq = ++s_keepalive_seq;
    ESP_LOGI("INIT", "#### EPAPER_USB TX: KEEPALIVE seq=%lu (0x00 0xAF)", (unsigned long)seq);
    (void)cdc_acm_host_data_tx_blocking(s_cdc, ka, sizeof(ka), 1000);
#endif
}

esp_err_t usb_cdc_epaper_attach(void *cdc_acm_dev_hdl)
{
#if !USB_CDC_ACM_AVAILABLE
    (void)cdc_acm_dev_hdl;
    return ESP_ERR_NOT_SUPPORTED;
#else
    s_cdc = (cdc_acm_dev_hdl_t)cdc_acm_dev_hdl;
    s_keepalive_seq = 0;
    ESP_LOGI("INIT", "#### EPAPER_USB attached to CDC handle=%p", s_cdc);
    return ESP_OK;
#endif
}

void usb_cdc_epaper_detach(const char *reason)
{
    usb_cdc_epaper_keepalive_stop(reason ? reason : "detach");
#if USB_CDC_ACM_AVAILABLE
    s_cdc = NULL;
#endif
    ESP_LOGI("INIT", "#### EPAPER_USB detached (%s)", reason ? reason : "n/a");
}

esp_err_t usb_cdc_epaper_send_raw(const uint8_t *data, size_t len)
{
#if !USB_CDC_ACM_AVAILABLE
    (void)data;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_cdc == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Log minimale “chiaro” con prefisso #### */
    if (data[0] == 0x00 && len >= 2) {
        ESP_LOGI("INIT", "#### EPAPER_USB TX: CMD 0x%02X (len=%u)", data[1], (unsigned)len);
    }
    return cdc_acm_host_data_tx_blocking(s_cdc, data, len, 1000);
#endif
}

esp_err_t usb_cdc_epaper_send_init(void)
{
    static const uint8_t init_pkt[] = {0x00, 0xAA};
    ESP_LOGI("INIT", "#### EPAPER_USB TX: INIT (0x00 0xAA)");
    return usb_cdc_epaper_send_raw(init_pkt, sizeof(init_pkt));
}

esp_err_t usb_cdc_epaper_keepalive_start_1hz(void)
{
    if (s_keepalive_timer == NULL) {
        const esp_timer_create_args_t args = {
            .callback = &keepalive_tick,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "epd_ka",
            .skip_unhandled_events = true,
        };
        esp_err_t err = esp_timer_create(&args, &s_keepalive_timer);
        if (err != ESP_OK) {
            ESP_LOGW("INIT", "#### EPAPER_USB keepalive timer create failed: %s", esp_err_to_name(err));
            s_keepalive_timer = NULL;
            return err;
        }
    }

    (void)esp_timer_stop(s_keepalive_timer);
    s_keepalive_seq = 0;
    esp_err_t err = esp_timer_start_periodic(s_keepalive_timer, EPAPER_KEEPALIVE_PERIOD_US);
    if (err == ESP_OK) {
        ESP_LOGI("INIT", "#### EPAPER_USB keepalive enabled: 0x00 0xAF every 1s");
    } else {
        ESP_LOGW("INIT", "#### EPAPER_USB keepalive start failed: %s", esp_err_to_name(err));
    }
    return err;
}

void usb_cdc_epaper_keepalive_stop(const char *reason)
{
    if (s_keepalive_timer != NULL) {
        (void)esp_timer_stop(s_keepalive_timer);
    }
    ESP_LOGI("INIT", "#### EPAPER_USB keepalive stopped (%s)", reason ? reason : "n/a");
}

esp_err_t usb_cdc_epaper_demo_send_scan_code(void)
{
    /* Testi font 60px (GoogleSans 60pt → font#9): "SCAN" e "CODE" */
    const uint8_t pkt1[] = {0x01, 0xFF, 9, 100, 50,  'S','C','A','N', 0x00};
    const uint8_t pkt2[] = {0x01, 0xFF, 9, 100, 150, 'C','O','D','E', 0x00};

    esp_err_t err = usb_cdc_epaper_send_raw(pkt1, sizeof(pkt1));
    vTaskDelay(pdMS_TO_TICKS(EPAPER_USB_DELAY_AFTER_WRITE_MS));
    if (err != ESP_OK) {
        return err;
    }

    err = usb_cdc_epaper_send_raw(pkt2, sizeof(pkt2));
    vTaskDelay(pdMS_TO_TICKS(EPAPER_USB_DELAY_AFTER_WRITE_MS));
    return err;
}

