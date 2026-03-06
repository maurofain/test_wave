#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CCTALK_MASTER_ADDRESS        (0x01U)
#define CCTALK_DEFAULT_DEVICE_ADDR   (0x02U)

typedef struct {
	uint8_t destination;
	uint8_t data_len;
	uint8_t source;
	uint8_t header;
	uint8_t data[255];
} cctalk_frame_t;

typedef struct {
	uint8_t coin_id;
	uint8_t error_code;
} cctalk_credit_event_t;

typedef struct {
	uint8_t event_counter;
	cctalk_credit_event_t events[5];
} cctalk_buffer_t;

// Inizializza la comunicazione cctalk su una porta seriale
void cctalk_init(int uart_num, int tx_pin, int rx_pin, int baudrate);

// Invia un pacchetto cctalk (compatibilità: data[0]=header, data[1..]=payload)
bool cctalk_send(uint8_t dest_addr, const uint8_t *data, uint8_t len);

// Riceve un pacchetto cctalk (compatibilità: data[0]=header, data[1..]=payload)
bool cctalk_receive(uint8_t *src_addr, uint8_t *data, uint8_t *len, uint32_t timeout_ms);

// Calcola il checksum cctalk
uint8_t cctalk_checksum(const uint8_t *packet, uint8_t len);

// API frame-level
bool cctalk_send_frame(uint8_t dest_addr,
					   uint8_t src_addr,
					   uint8_t header,
					   const uint8_t *data,
					   uint8_t len);
bool cctalk_receive_frame(cctalk_frame_t *frame, uint32_t timeout_ms);
bool cctalk_command(uint8_t dest_addr,
					uint8_t src_addr,
					uint8_t header,
					const uint8_t *data,
					uint8_t len,
					cctalk_frame_t *response,
					uint32_t timeout_ms);

// Comandi gettoniera (rif. docs/gettoniera.md)
bool cctalk_address_poll(uint8_t dest_addr, uint32_t timeout_ms);
bool cctalk_request_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms);
bool cctalk_request_manufacturer_id(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms);
bool cctalk_request_equipment_category(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms);
bool cctalk_request_product_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms);
bool cctalk_request_serial_number(uint8_t dest_addr, uint8_t serial[3], uint32_t timeout_ms);
bool cctalk_request_build_code(uint8_t dest_addr, char *out, size_t out_len, uint32_t timeout_ms);
bool cctalk_modify_master_inhibit(uint8_t dest_addr, bool accept_enabled, uint32_t timeout_ms);
bool cctalk_modify_inhibit_status(uint8_t dest_addr, uint8_t mask_low, uint8_t mask_high, uint32_t timeout_ms);
bool cctalk_request_inhibit_status(uint8_t dest_addr, uint8_t *mask_low, uint8_t *mask_high, uint32_t timeout_ms);
bool cctalk_request_coin_id(uint8_t dest_addr, uint8_t channel, char *out, size_t out_len, uint32_t timeout_ms);
bool cctalk_modify_sorter_paths(uint8_t dest_addr, const uint8_t *paths, uint8_t len, uint32_t timeout_ms);
bool cctalk_read_buffered_credit(uint8_t dest_addr, cctalk_buffer_t *buffer, uint32_t timeout_ms);
bool cctalk_request_insertion_status(uint8_t dest_addr, uint8_t *status, uint32_t timeout_ms);
bool cctalk_reset_device(uint8_t dest_addr, uint32_t timeout_ms);

// Inizializza il driver CCtalk (installa UART).
// Il task di ricezione va avviato separatamente chiamando cctalk_task_run().
esp_err_t cctalk_driver_init(void);

// Attiva/disattiva la gettoniera (master inhibit) e polling eventi monete.
esp_err_t cctalk_driver_start_acceptor(void);
esp_err_t cctalk_driver_stop_acceptor(void);
bool cctalk_driver_is_acceptor_enabled(void);

// Entry point del task di ricezione CCtalk. Da chiamare dopo cctalk_driver_init().
void cctalk_task_run(void *arg);

#ifdef __cplusplus
}
#endif
