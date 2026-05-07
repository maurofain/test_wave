#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* EPAPER_USB protocol (Serial_protocol.md):
 * - 0x00 0xAA init handshake
 * - 0x00 0xAF keepalive (reset OOS timer)
 */

/** Attach EPAPER transport to an opened CDC-ACM handle (EPAPER_USB mode). */
esp_err_t usb_cdc_epaper_attach(void *cdc_acm_dev_hdl);

/** Detach/stop EPAPER keepalive (call on disconnect). */
void usb_cdc_epaper_detach(const char *reason);

/** Send raw EPAPER packet on CDC (0x00/0x01/0x02 payloads). */
esp_err_t usb_cdc_epaper_send_raw(const uint8_t *data, size_t len);

/** Convenience: send INIT (0x00 0xAA). */
esp_err_t usb_cdc_epaper_send_init(void);

/** Start 1Hz keepalive (0x00 0xAF). Safe to call multiple times. */
esp_err_t usb_cdc_epaper_keepalive_start_1hz(void);

/** Stop keepalive. */
void usb_cdc_epaper_keepalive_stop(const char *reason);

/** Demo boot texts: send "SCAN" / "CODE" using font 60px (font#9). */
esp_err_t usb_cdc_epaper_demo_send_scan_code(void);

#if defined(__cplusplus)
}
#endif

