#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// MDB Addresses
#define MDB_ADDR_COIN_CHANGER   0x08
#define MDB_ADDR_CASHLESS       0x10
#define MDB_ADDR_BILL_VALIDATOR 0x30

// Common MDB Commands (Base + command offset)
#define MDB_CMD_RESET           0x00
#define MDB_CMD_SETUP           0x01
#define MDB_CMD_POLL            0x03
#define MDB_CMD_ENABLE          0x05

// Coin Changer specific
#define MDB_COIN_CMD_TUBE_STATUS 0x02
#define MDB_COIN_CMD_COIN_TYPE   0x04

// MDB Responses
#define MDB_ACK                 0x00
#define MDB_NAK                 0xFF
#define MDB_RET                 0xAA

/**
 * @brief Stato di una periferica MDB nel Polling Engine
 */
typedef enum {
    MDB_STATE_INACTIVE = 0,
    MDB_STATE_INIT_RESET,
    MDB_STATE_INIT_SETUP,
    MDB_STATE_INIT_EXPANSION,
    MDB_STATE_IDLE_POLLING,
    MDB_STATE_ERROR
} mdb_device_state_t;

/**
 * @brief Struttura globale per lo stato del bus MDB
 */
typedef struct {
    struct {
        mdb_device_state_t state;
        uint32_t last_poll_ms;
        uint32_t credit_cents;
        bool is_online;
    } coin;
    struct {
        mdb_device_state_t state;
        bool is_online;
    } bill;
} mdb_bus_status_t;

/**
 * @brief Inizializza l'interfaccia MDB (UART a 9 bit)
 * @return ESP_OK se riuscito
 */
esp_err_t mdb_bus_init(void);

/**
 * @brief Avvia il Polling Engine MDB in un task FreeRTOS
 * @return ESP_OK se avviato
 */
esp_err_t mdb_bus_start_engine(void);

/**
 * @brief Ottiene lo stato attuale del bus MDB
 * @return Puntatore alla struttura di stato (sola lettura)
 */
const mdb_bus_status_t* mdb_bus_get_status(void);

/**
 * @brief Invia un pacchetto MDB (Indirizzo + Dati + Checksum)
 * 
 * @param address Indirizzo della periferica (bits 7-3) + comando (bits 2-0)
 * @param data Buffer dei dati (optional)
 * @param len Lunghezza del buffer dati
 * @return ESP_OK se inviato correttamente
 */
esp_err_t mdb_bus_send_packet(uint8_t address, const uint8_t *data, size_t len);

/**
 * @brief Riceve una risposta MDB
 * 
 * @param out_data Buffer dove scrivere i dati ricevuti
 * @param max_len Dimensione massima del buffer
 * @param out_len Lunghezza effettiva ricevuta
 * @param timeout_ms Timeout in millisecondi
 * @return ESP_OK se ricevuto pacchetto valido (compreso Checksum)
 */
esp_err_t mdb_bus_receive_packet(uint8_t *out_data, size_t max_len, size_t *out_len, uint32_t timeout_ms);
