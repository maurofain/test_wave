#pragma once

#include "mdb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MDB_CASHLESS_DEVICE_COUNT 1U
#define MDB_CASHLESS_DEVICE_ADDR_1 0x10U
/* Hardware attuale: un solo lettore cashless/NFC su 0x10.
    Il secondo indirizzo 0x60 resta documentato ma non viene interrogato. */
/* #define MDB_CASHLESS_DEVICE_ADDR_2 0x60U */

typedef enum {
    MDB_CASHLESS_SESSION_IDLE = 0,
    MDB_CASHLESS_SESSION_OPEN,
    MDB_CASHLESS_SESSION_VEND_REQUESTED,
    MDB_CASHLESS_SESSION_VEND_APPROVED,
    MDB_CASHLESS_SESSION_REVALUE_REQUESTED,
    MDB_CASHLESS_SESSION_ENDING,
} mdb_cashless_session_state_t;

typedef enum {
    MDB_CASHLESS_KEY_NONE = 0,
    MDB_CASHLESS_KEY_CREDIT_CARD = 1,
    MDB_CASHLESS_KEY_LOYALTY_CARD = 2,
    MDB_CASHLESS_KEY_USER_CARD = 3,
    MDB_CASHLESS_KEY_FREE_VEND = 4,
} mdb_cashless_key_type_t;

typedef enum {
    MDB_CASHLESS_DEVICE_NONE = 0,
    MDB_CASHLESS_DEVICE_CREDIT_CARD = 1,
    MDB_CASHLESS_DEVICE_LOYALTY = 2,
    MDB_CASHLESS_DEVICE_USER_CARD = 3,
} mdb_cashless_device_type_t;

typedef enum {
    MDB_CASHLESS_RESP_JUST_RESET = 0x00,
    MDB_CASHLESS_RESP_READ_CONFIG = 0x01,
    MDB_CASHLESS_RESP_DISPLAY_REQUEST = 0x02,
    MDB_CASHLESS_RESP_BEGIN_SESSION = 0x03,
    MDB_CASHLESS_RESP_SESSION_CANCEL = 0x04,
    MDB_CASHLESS_RESP_VEND_APPROVED = 0x05,
    MDB_CASHLESS_RESP_VEND_DENIED = 0x06,
    MDB_CASHLESS_RESP_END_SESSION = 0x07,
    MDB_CASHLESS_RESP_REQUEST_ID = 0x09,
    MDB_CASHLESS_RESP_MALFUNCTION = 0x0A,
    MDB_CASHLESS_RESP_OUT_OF_SEQUENCE = 0x0B,
    MDB_CASHLESS_RESP_REVALUE_APPROVED = 0x0D,
    MDB_CASHLESS_RESP_REVALUE_DENIED = 0x0E,
    MDB_CASHLESS_RESP_REVALUE_LIMIT = 0x0F,
} mdb_cashless_response_t;

typedef struct {
    uint8_t address;
    mdb_device_state_t poll_state;
    bool enabled;
    bool enabled_status;
    bool present;
    bool config_read;
    bool vmc_setup_done;
    bool expansion_read;
    bool session_open;
    bool cash_sale_support;
    bool session_complete_requested;
    bool vend_failure_requested;
    bool vend_abort_requested;
    bool vend_success_requested;
    bool credit_sync_pending;
    bool vend_result_pending;
    bool vend_result_approved;
    uint32_t error_count;
    uint32_t start_tick_ms;
    uint32_t session_begin_ms;
    uint8_t feature_level;
    uint8_t last_response_code;
    uint8_t key_price_group;
    mdb_cashless_key_type_t key_type;
    mdb_cashless_device_type_t device_type;
    mdb_cashless_session_state_t session_state;
    MDB_VEND_STATUS vend_status;
    MDB_REVALUE_STATUS revalue_status;
    uint16_t credit_cents;
    uint16_t request_price_cents;
    uint16_t approved_price_cents;
    uint16_t vend_result_cents;
    uint16_t revalue_price_cents;
    uint16_t approved_revalue_cents;
    uint16_t revalue_limit_cents;
    uint16_t last_synced_credit_cents;
    uint16_t cash_sale_item_price_cents;
    uint16_t cash_sale_item_number;
    uint16_t manufacturer_version;
    unsigned long key_number;
    char manufacturer_code[4];
    char model_number[13];
    char totalerg_version[6];
    char ingcb2_version[6];
    char last_display_message[64];
} mdb_cashless_device_t;

void mdb_cashless_init_state(void);
void mdb_cashless_reset_device(size_t device_index);
size_t mdb_cashless_get_device_count(void);
const mdb_cashless_device_t *mdb_cashless_get_device(size_t device_index);
bool mdb_cashless_handle_poll_response(size_t device_index, const uint8_t *data, size_t len);
bool mdb_cashless_prepare_vend_request(size_t device_index, uint16_t amount_cents, uint16_t item_number);
bool mdb_cashless_prepare_vend_success(size_t device_index, uint16_t amount_cents);
bool mdb_cashless_prepare_revalue(size_t device_index, uint16_t amount_cents);
bool mdb_cashless_request_revalue_limit(size_t device_index);
bool mdb_cashless_request_session_complete(size_t device_index);
bool mdb_cashless_close_session_locally(size_t device_index);
bool mdb_cashless_get_pending_credit_event(size_t *device_index, uint16_t *credit_cents);
bool mdb_cashless_get_pending_vend_event(size_t *device_index, bool *approved, uint16_t *amount_cents);
void mdb_cashless_ack_pending_credit_event(size_t device_index);
void mdb_cashless_ack_pending_vend_event(size_t device_index);
const char *mdb_cashless_response_to_string(uint8_t response_code);
const char *mdb_cashless_session_state_to_string(mdb_cashless_session_state_t state);
