#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Inizializza la comunicazione cctalk su una porta seriale
void cctalk_init(int uart_num, int tx_pin, int rx_pin, int baudrate);

// Invia un pacchetto cctalk
bool cctalk_send(uint8_t dest_addr, const uint8_t *data, uint8_t len);

// Riceve un pacchetto cctalk (bloccante, timeout in ms)
bool cctalk_receive(uint8_t *src_addr, uint8_t *data, uint8_t *len, uint32_t timeout_ms);

// Calcola il checksum cctalk
uint8_t cctalk_checksum(const uint8_t *packet, uint8_t len);

// Inizializza il driver CCtalk (installa UART).
// Il task di ricezione va avviato separatamente chiamando cctalk_task_run().
esp_err_t cctalk_driver_init(void);

// Entry point del task di ricezione CCtalk. Da chiamare dopo cctalk_driver_init().
void cctalk_task_run(void *arg);

#ifdef __cplusplus
}
#endif
