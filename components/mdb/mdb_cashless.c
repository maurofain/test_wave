#include "mdb_cashless.h"

#include "esp_log.h"

#include <string.h>

static const char *TAG_CASH_EVT = "MDB_CEVT";
static const char *TAG_CASH_CTL = "MDB_CCTL";

static mdb_cashless_device_t s_cashless_devices[MDB_CASHLESS_DEVICE_COUNT];

static void mdb_cashless_log_tag_event(size_t device_index, const char *event, const mdb_cashless_device_t *device)
{
    if (!event || !device) {
        return;
    }

    ESP_LOGI(TAG_CASH_EVT, "****************************************");
    ESP_LOGI(TAG_CASH_EVT,
             "[C] NFC TAG %s: dev=%u addr=0x%02X present=%d session_open=%d credit=%u",
             event,
             (unsigned)device_index,
             device->address,
             device->present ? 1 : 0,
             device->session_open ? 1 : 0,
             (unsigned)device->credit_cents);
    ESP_LOGI(TAG_CASH_EVT, "****************************************");
}

static const char *mdb_cashless_key_type_to_string(mdb_cashless_key_type_t key_type)
{
    switch (key_type) {
        case MDB_CASHLESS_KEY_NONE: return "none";
        case MDB_CASHLESS_KEY_CREDIT_CARD: return "credit_card";
        case MDB_CASHLESS_KEY_LOYALTY_CARD: return "loyalty_card";
        case MDB_CASHLESS_KEY_USER_CARD: return "user_card";
        case MDB_CASHLESS_KEY_FREE_VEND: return "free_vend";
        default: return "unknown";
    }
}

static const char *mdb_cashless_vend_status_to_string(MDB_VEND_STATUS status)
{
    switch (status) {
        case MDB_VEND_IDLE: return "idle";
        case MDB_VEND_PENDING: return "pending";
        case MDB_VEND_WORKING: return "working";
        case MDB_VEND_APPROVED: return "approved";
        case MDB_VEND_DENIED: return "denied";
        default: return "unknown";
    }
}

static const char *mdb_cashless_revalue_status_to_string(MDB_REVALUE_STATUS status)
{
    switch (status) {
        case MDB_REVALUE_IDLE: return "idle";
        case MDB_REVALUE_REQUEST_PENDING: return "request_pending";
        case MDB_REVALUE_IN_PROGRESS: return "in_progress";
        case MDB_REVALUE_APPROVED: return "approved";
        case MDB_REVALUE_DENIED: return "denied";
        default: return "unknown";
    }
}

static uint8_t mdb_cashless_address_for_index(size_t device_index)
{
    (void)device_index;
    return MDB_CASHLESS_DEVICE_ADDR_1;
}

static bool mdb_cashless_valid_index(size_t device_index)
{
    return device_index < MDB_CASHLESS_DEVICE_COUNT;
}

static void mdb_cashless_classify_device(mdb_cashless_device_t *device)
{
    if (!device) {
        return;
    }

    device->device_type = MDB_CASHLESS_DEVICE_USER_CARD;

    if (strcmp(device->manufacturer_code, "GLO") == 0 ||
        strcmp(device->manufacturer_code, "ING") == 0 ||
        strcmp(device->manufacturer_code, "MEI") == 0 ||
        strcmp(device->manufacturer_code, "WIZ") == 0 ||
        strcmp(device->manufacturer_code, "oti") == 0 ||
        strcmp(device->manufacturer_code, "NYX") == 0 ||
        strcmp(device->manufacturer_code, "FEI") == 0 ||
        strcmp(device->manufacturer_code, "HOE") == 0 ||
        strcmp(device->manufacturer_code, "TBD") == 0 ||
        strcmp(device->manufacturer_code, "PTR") == 0 ||
        strcmp(device->manufacturer_code, "INF") == 0 ||
        strcmp(device->manufacturer_code, "PAX") == 0) {
        device->device_type = MDB_CASHLESS_DEVICE_CREDIT_CARD;
    }
}

static void mdb_cashless_zero_dynamic_state(mdb_cashless_device_t *device)
{
    if (!device) {
        return;
    }

    device->enabled_status = false;
    device->poll_state = MDB_STATE_INACTIVE;
    device->present = false;
    device->config_read = false;
    device->vmc_setup_done = false;
    device->expansion_read = false;
    device->session_open = false;
    device->cash_sale_support = false;
    device->session_complete_requested = false;
    device->vend_failure_requested = false;
    device->vend_abort_requested = false;
    device->vend_success_requested = false;
    device->revalue_limit_requested = false;
    device->credit_sync_pending = false;
    device->vend_result_pending = false;
    device->vend_result_approved = false;
    device->credit_cents = 0;
    device->request_price_cents = 0;
    device->approved_price_cents = 0;
    device->vend_result_cents = 0;
    device->revalue_price_cents = 0;
    device->approved_revalue_cents = 0;
    device->revalue_limit_cents = 0;
    device->last_synced_credit_cents = 0;
    device->feature_level = 0;
    device->key_number = 0xFFFFFFFFUL;
    device->key_type = MDB_CASHLESS_KEY_NONE;
    device->key_price_group = 0;
    device->device_type = MDB_CASHLESS_DEVICE_NONE;
    device->manufacturer_version = 0;
    device->session_begin_ms = 0;
    device->vend_status = MDB_VEND_IDLE;
    device->revalue_status = MDB_REVALUE_IDLE;
    device->session_state = MDB_CASHLESS_SESSION_IDLE;
    memset(device->manufacturer_code, 0, sizeof(device->manufacturer_code));
    memset(device->model_number, 0, sizeof(device->model_number));
    memset(device->totalerg_version, 0, sizeof(device->totalerg_version));
    memset(device->ingcb2_version, 0, sizeof(device->ingcb2_version));
    memset(device->last_display_message, 0, sizeof(device->last_display_message));
}

static void mdb_cashless_init_device(mdb_cashless_device_t *device, size_t device_index)
{
    memset(device, 0, sizeof(*device));
    device->address = mdb_cashless_address_for_index(device_index);
    device->cash_sale_item_number = 0xFFFFU;
    mdb_cashless_zero_dynamic_state(device);
}

void mdb_cashless_init_state(void)
{
    for (size_t device_index = 0; device_index < MDB_CASHLESS_DEVICE_COUNT; ++device_index) {
        mdb_cashless_init_device(&s_cashless_devices[device_index], device_index);
    }
}

void mdb_cashless_reset_device(size_t device_index)
{
    if (!mdb_cashless_valid_index(device_index)) {
        return;
    }

    mdb_cashless_init_device(&s_cashless_devices[device_index], device_index);
}

size_t mdb_cashless_get_device_count(void)
{
    return MDB_CASHLESS_DEVICE_COUNT;
}

const mdb_cashless_device_t *mdb_cashless_get_device(size_t device_index)
{
    if (!mdb_cashless_valid_index(device_index)) {
        return NULL;
    }

    return &s_cashless_devices[device_index];
}

const char *mdb_cashless_response_to_string(uint8_t response_code)
{
    switch ((mdb_cashless_response_t)response_code) {
        case MDB_CASHLESS_RESP_JUST_RESET: return "just_reset";
        case MDB_CASHLESS_RESP_READ_CONFIG: return "read_config";
        case MDB_CASHLESS_RESP_DISPLAY_REQUEST: return "display_request";
        case MDB_CASHLESS_RESP_BEGIN_SESSION: return "begin_session";
        case MDB_CASHLESS_RESP_SESSION_CANCEL: return "session_cancel";
        case MDB_CASHLESS_RESP_VEND_APPROVED: return "vend_approved";
        case MDB_CASHLESS_RESP_VEND_DENIED: return "vend_denied";
        case MDB_CASHLESS_RESP_END_SESSION: return "end_session";
        case MDB_CASHLESS_RESP_REQUEST_ID: return "request_id";
        case MDB_CASHLESS_RESP_MALFUNCTION: return "malfunction";
        case MDB_CASHLESS_RESP_OUT_OF_SEQUENCE: return "out_of_sequence";
        case MDB_CASHLESS_RESP_REVALUE_APPROVED: return "revalue_approved";
        case MDB_CASHLESS_RESP_REVALUE_DENIED: return "revalue_denied";
        case MDB_CASHLESS_RESP_REVALUE_LIMIT: return "revalue_limit";
        default: return "unknown";
    }
}

const char *mdb_cashless_session_state_to_string(mdb_cashless_session_state_t state)
{
    switch (state) {
        case MDB_CASHLESS_SESSION_IDLE: return "idle";
        case MDB_CASHLESS_SESSION_OPEN: return "open";
        case MDB_CASHLESS_SESSION_VEND_REQUESTED: return "vend_requested";
        case MDB_CASHLESS_SESSION_VEND_APPROVED: return "vend_approved";
        case MDB_CASHLESS_SESSION_REVALUE_REQUESTED: return "revalue_requested";
        case MDB_CASHLESS_SESSION_ENDING: return "ending";
        default: return "unknown";
    }
}

static void mdb_cashless_parse_request_id(mdb_cashless_device_t *device, const uint8_t *data, size_t len)
{
    if (!device || !data || len < 30U) {
        return;
    }

    memcpy(device->manufacturer_code, data + 1, 3);
    device->manufacturer_code[3] = '\0';
    memcpy(device->model_number, data + 4, 12);
    device->model_number[12] = '\0';
    device->manufacturer_version = ((uint16_t)data[28] << 8) | data[29];
    device->expansion_read = true;
    device->present = true;
    mdb_cashless_classify_device(device);
    ESP_LOGI(TAG_CASH_EVT,
             "[C] [mdb_cashless_parse_request_id] addr=0x%02X manuf=%s model=%s version=0x%04X type=%d",
             device->address,
             device->manufacturer_code,
             device->model_number,
             device->manufacturer_version,
             (int)device->device_type);
}

static void mdb_cashless_parse_begin_session(mdb_cashless_device_t *device, const uint8_t *data, size_t len)
{
    bool was_session_open = false;
    uint16_t new_credit_cents = 0;

    if (!device || !data || len < 3U) {
        return;
    }

    was_session_open = device->session_open;
    new_credit_cents = ((uint16_t)data[1] << 8) | data[2];

    device->session_open = true;
    device->session_state = MDB_CASHLESS_SESSION_OPEN;
    device->credit_cents = new_credit_cents;
    device->key_number = 0xFFFFFFFFUL;
    device->key_type = MDB_CASHLESS_KEY_CREDIT_CARD;
    device->key_price_group = 0;

    if (!was_session_open) {
        mdb_cashless_log_tag_event(device->address == MDB_CASHLESS_DEVICE_ADDR_1 ? 0 : 0,
                                   "INSERITO",
                                   device);
    }
    if (!was_session_open || device->last_synced_credit_cents != new_credit_cents) {
        device->credit_sync_pending = (new_credit_cents > 0U);
    }

    if (device->feature_level > 1U && len >= 9U) {
        device->key_number = ((unsigned long)data[3] << 24) |
                             ((unsigned long)data[4] << 16) |
                             ((unsigned long)data[5] << 8) |
                             (unsigned long)data[6];
        if (device->key_number == 0xFFFFFFFFUL) {
            device->key_number = 0UL;
        }

        if ((data[7] & 0x80U) != 0U) {
            device->key_type = MDB_CASHLESS_KEY_FREE_VEND;
        } else if (data[7] == 1U && len >= 9U) {
            device->key_type = MDB_CASHLESS_KEY_USER_CARD;
            device->key_price_group = data[8];
        } else {
            device->key_type = MDB_CASHLESS_KEY_CREDIT_CARD;
        }
    }

    ESP_LOGI(TAG_CASH_EVT,
             "[C] [mdb_cashless_parse_begin_session] addr=0x%02X credit=%u key_type=%s key=%lu price_group=%u was_open=%d",
             device->address,
             (unsigned)device->credit_cents,
             mdb_cashless_key_type_to_string(device->key_type),
             device->key_number,
             (unsigned)device->key_price_group,
             was_session_open ? 1 : 0);
}

bool mdb_cashless_handle_poll_response(size_t device_index, const uint8_t *data, size_t len)
{
    if (!mdb_cashless_valid_index(device_index) || !data || len == 0U) {
        return false;
    }

    mdb_cashless_device_t *device = &s_cashless_devices[device_index];
    uint8_t previous_response_code = device->last_response_code;
    device->last_response_code = data[0];
    device->present = true;

    if (((mdb_cashless_response_t)data[0] == MDB_CASHLESS_RESP_SESSION_CANCEL &&
         previous_response_code == MDB_CASHLESS_RESP_SESSION_CANCEL) ||
        ((mdb_cashless_response_t)data[0] == MDB_CASHLESS_RESP_END_SESSION &&
         previous_response_code == MDB_CASHLESS_RESP_END_SESSION)) {
        /* Ignora ripetuti Rx di chiusura sessione già gestiti. */
        return true;
    }

    ESP_LOGI(TAG_CASH_EVT,
             "[C] [mdb_cashless_handle_poll_response] dev=%u addr=0x%02X resp=%s(0x%02X) len=%u",
             (unsigned)device_index,
             device->address,
             mdb_cashless_response_to_string(data[0]),
             data[0],
             (unsigned)len);

    switch ((mdb_cashless_response_t)data[0]) {
        case MDB_CASHLESS_RESP_JUST_RESET:
            mdb_cashless_zero_dynamic_state(device);
            device->address = mdb_cashless_address_for_index(device_index);
            device->cash_sale_item_number = 0xFFFFU;
            ESP_LOGW(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u lettore appena resettato",
                     (unsigned)device_index);
            return true;

        case MDB_CASHLESS_RESP_READ_CONFIG:
            device->config_read = true;
            if (len >= 8U) {
                device->feature_level = data[1];
                device->cash_sale_support = (data[7] & 0x08U) != 0U;
            }
            ESP_LOGI(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u read_config feature_level=%u cash_sale_support=%d",
                     (unsigned)device_index,
                     (unsigned)device->feature_level,
                     device->cash_sale_support ? 1 : 0);
            return true;

        case MDB_CASHLESS_RESP_DISPLAY_REQUEST:
            if (len > 3U) {
                size_t copy_len = len - 3U;
                if (copy_len >= sizeof(device->last_display_message)) {
                    copy_len = sizeof(device->last_display_message) - 1U;
                }
                memcpy(device->last_display_message, data + 3, copy_len);
                device->last_display_message[copy_len] = '\0';
            }
            ESP_LOGI(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u display='%s'",
                     (unsigned)device_index,
                     device->last_display_message);
            return true;

        case MDB_CASHLESS_RESP_BEGIN_SESSION:
            mdb_cashless_parse_begin_session(device, data, len);
            return true;

        case MDB_CASHLESS_RESP_SESSION_CANCEL:
        {
            MDB_VEND_STATUS previous_vend_status = device->vend_status;
            mdb_cashless_session_state_t previous_session_state = device->session_state;

            device->session_open = false;
            device->session_state = MDB_CASHLESS_SESSION_IDLE;
            device->session_complete_requested = false;
            device->vend_success_requested = false;
            device->revalue_limit_requested = false;
            device->revalue_status = MDB_REVALUE_IDLE;
            device->revalue_price_cents = 0U;
            device->approved_revalue_cents = 0U;

            mdb_cashless_log_tag_event(device_index, "RIMOSSO", device);

            if (previous_vend_status == MDB_VEND_PENDING ||
                previous_vend_status == MDB_VEND_WORKING ||
                previous_session_state == MDB_CASHLESS_SESSION_VEND_REQUESTED ||
                previous_session_state == MDB_CASHLESS_SESSION_VEND_APPROVED) {
                device->vend_status = MDB_VEND_DENIED;
                device->vend_result_pending = true;
                device->vend_result_approved = false;
                device->vend_result_cents = 0U;
                ESP_LOGW(TAG_CASH_EVT,
                         "[C] [mdb_cashless_handle_poll_response] dev=%u session_cancel vend_status=%s",
                         (unsigned)device_index,
                         mdb_cashless_vend_status_to_string(device->vend_status));
            } else {
                device->vend_status = MDB_VEND_IDLE;
                device->vend_result_pending = false;
                device->vend_result_approved = false;
                device->vend_result_cents = 0U;
                ESP_LOGI(TAG_CASH_EVT,
                         "[C] [mdb_cashless_handle_poll_response] dev=%u session_cancel senza vendita attiva",
                         (unsigned)device_index);
            }
            return true;
            }

        case MDB_CASHLESS_RESP_VEND_APPROVED:
            device->vend_status = MDB_VEND_APPROVED;
            device->session_state = MDB_CASHLESS_SESSION_VEND_APPROVED;
            if (len >= 3U) {
                device->approved_price_cents = ((uint16_t)data[1] << 8) | data[2];
            }
            device->vend_result_pending = true;
            device->vend_result_approved = true;
            device->vend_result_cents = device->approved_price_cents;
            ESP_LOGI(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u vend approved amount=%u",
                     (unsigned)device_index,
                     (unsigned)device->approved_price_cents);
            return true;

        case MDB_CASHLESS_RESP_VEND_DENIED:
            device->vend_status = MDB_VEND_DENIED;
            device->session_state = MDB_CASHLESS_SESSION_ENDING;
            device->vend_result_pending = true;
            device->vend_result_approved = false;
            device->vend_result_cents = 0U;
            ESP_LOGW(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u vend denied",
                     (unsigned)device_index);
            return true;

        case MDB_CASHLESS_RESP_END_SESSION:
            device->session_open = false;
            device->session_state = MDB_CASHLESS_SESSION_IDLE;
            device->vend_status = MDB_VEND_IDLE;
            device->revalue_status = MDB_REVALUE_IDLE;
            device->revalue_limit_requested = false;
            device->request_price_cents = 0U;
            device->approved_price_cents = 0U;
            device->vend_result_cents = 0U;
            device->revalue_price_cents = 0U;
            device->approved_revalue_cents = 0U;
            device->vend_success_requested = false;
            device->session_complete_requested = false;
            mdb_cashless_log_tag_event(device_index, "RIMOSSO", device);
            ESP_LOGI(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u end_session session_open=%d",
                     (unsigned)device_index,
                     device->session_open ? 1 : 0);
            return true;

        case MDB_CASHLESS_RESP_REQUEST_ID:
            mdb_cashless_parse_request_id(device, data, len);
            return true;

        case MDB_CASHLESS_RESP_MALFUNCTION:
            device->error_count++;
            ESP_LOGW(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u malfunction error_count=%lu",
                     (unsigned)device_index,
                     (unsigned long)device->error_count);
            return true;

        case MDB_CASHLESS_RESP_OUT_OF_SEQUENCE:
            device->session_open = false;
            device->session_state = MDB_CASHLESS_SESSION_IDLE;
            device->vend_status = MDB_VEND_IDLE;
            device->revalue_status = MDB_REVALUE_IDLE;
            device->revalue_limit_requested = false;
            device->revalue_price_cents = 0U;
            device->approved_revalue_cents = 0U;
            ESP_LOGW(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u out_of_sequence, sessione invalidata",
                     (unsigned)device_index);
            return true;

        case MDB_CASHLESS_RESP_REVALUE_APPROVED:
            device->revalue_limit_requested = false;
            device->revalue_status = MDB_REVALUE_APPROVED;
            device->approved_revalue_cents = device->revalue_price_cents;
            device->revalue_price_cents = 0U;
            device->session_state = MDB_CASHLESS_SESSION_OPEN;
            ESP_LOGI(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u revalue approved amount=%u",
                     (unsigned)device_index,
                     (unsigned)device->approved_revalue_cents);
            return true;

        case MDB_CASHLESS_RESP_REVALUE_DENIED:
            device->revalue_limit_requested = false;
            device->revalue_status = MDB_REVALUE_DENIED;
            device->approved_revalue_cents = 0U;
            device->revalue_price_cents = 0U;
            device->session_state = MDB_CASHLESS_SESSION_OPEN;
            ESP_LOGW(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u revalue denied",
                     (unsigned)device_index);
            return true;

        case MDB_CASHLESS_RESP_REVALUE_LIMIT:
            if (len >= 3U) {
                device->revalue_limit_cents = ((uint16_t)data[1] << 8) | data[2];
            }
            if (device->revalue_limit_requested) {
                device->revalue_limit_requested = false;
                if (device->revalue_price_cents == 0U &&
                    device->revalue_status == MDB_REVALUE_REQUEST_PENDING) {
                    device->revalue_status = MDB_REVALUE_IDLE;
                }
            }
            ESP_LOGI(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u revalue_limit=%u status=%s",
                     (unsigned)device_index,
                     (unsigned)device->revalue_limit_cents,
                     mdb_cashless_revalue_status_to_string(device->revalue_status));
            return true;

        default:
            ESP_LOGW(TAG_CASH_EVT,
                     "[C] [mdb_cashless_handle_poll_response] dev=%u risposta sconosciuta 0x%02X",
                     (unsigned)device_index,
                     data[0]);
            return false;
    }
}

bool mdb_cashless_prepare_vend_request(size_t device_index, uint16_t amount_cents, uint16_t item_number)
{
    if (!mdb_cashless_valid_index(device_index) || amount_cents == 0U) {
        ESP_LOGW(TAG_CASH_CTL,
                 "[C] [mdb_cashless_prepare_vend_request] parametri non validi dev=%u amount=%u",
                 (unsigned)device_index,
                 (unsigned)amount_cents);
        return false;
    }

    mdb_cashless_device_t *device = &s_cashless_devices[device_index];
    if (device->revalue_limit_requested ||
        device->revalue_status == MDB_REVALUE_REQUEST_PENDING ||
        device->revalue_status == MDB_REVALUE_IN_PROGRESS) {
        ESP_LOGW(TAG_CASH_CTL,
                 "[C] [mdb_cashless_prepare_vend_request] dev=%u vend rifiutata: ricarica in corso status=%s",
                 (unsigned)device_index,
                 mdb_cashless_revalue_status_to_string(device->revalue_status));
        return false;
    }

    device->request_price_cents = amount_cents;
    device->cash_sale_item_number = item_number;
    device->vend_status = MDB_VEND_PENDING;
    device->session_state = MDB_CASHLESS_SESSION_VEND_REQUESTED;
    ESP_LOGI(TAG_CASH_CTL,
             "[C] [mdb_cashless_prepare_vend_request] dev=%u amount=%u item=0x%04X vend_status=%s",
             (unsigned)device_index,
             (unsigned)amount_cents,
             (unsigned)item_number,
             mdb_cashless_vend_status_to_string(device->vend_status));
    return true;
}

bool mdb_cashless_prepare_vend_success(size_t device_index, uint16_t amount_cents)
{
    if (!mdb_cashless_valid_index(device_index) || amount_cents == 0U) {
        ESP_LOGW(TAG_CASH_CTL,
                 "[C] [mdb_cashless_prepare_vend_success] parametri non validi dev=%u amount=%u",
                 (unsigned)device_index,
                 (unsigned)amount_cents);
        return false;
    }

    mdb_cashless_device_t *device = &s_cashless_devices[device_index];
    device->approved_price_cents = amount_cents;
    device->vend_success_requested = true;
    ESP_LOGI(TAG_CASH_CTL,
             "[C] [mdb_cashless_prepare_vend_success] dev=%u amount=%u",
             (unsigned)device_index,
             (unsigned)amount_cents);
    return true;
}

bool mdb_cashless_prepare_revalue(size_t device_index, uint16_t amount_cents)
{
    if (!mdb_cashless_valid_index(device_index) || amount_cents == 0U) {
        ESP_LOGW(TAG_CASH_CTL,
                 "[C] [mdb_cashless_prepare_revalue] parametri non validi dev=%u amount=%u",
                 (unsigned)device_index,
                 (unsigned)amount_cents);
        return false;
    }

    mdb_cashless_device_t *device = &s_cashless_devices[device_index];
    if (device->revalue_limit_requested ||
        device->revalue_status == MDB_REVALUE_REQUEST_PENDING ||
        device->revalue_status == MDB_REVALUE_IN_PROGRESS) {
        ESP_LOGW(TAG_CASH_CTL,
                 "[C] [mdb_cashless_prepare_revalue] dev=%u richiesta ignorata: operazione precedente ancora attiva status=%s",
                 (unsigned)device_index,
                 mdb_cashless_revalue_status_to_string(device->revalue_status));
        return false;
    }

    device->revalue_price_cents = amount_cents;
    device->approved_revalue_cents = 0U;
    device->revalue_limit_requested = false;
    device->revalue_status = MDB_REVALUE_REQUEST_PENDING;
    device->session_state = MDB_CASHLESS_SESSION_REVALUE_REQUESTED;
    ESP_LOGI(TAG_CASH_CTL,
             "[C] [mdb_cashless_prepare_revalue] dev=%u amount=%u revalue_status=%s",
             (unsigned)device_index,
             (unsigned)amount_cents,
             mdb_cashless_revalue_status_to_string(device->revalue_status));
    return true;
}

bool mdb_cashless_request_revalue_limit(size_t device_index)
{
    if (!mdb_cashless_valid_index(device_index)) {
        ESP_LOGW(TAG_CASH_CTL,
                 "[C] [mdb_cashless_request_revalue_limit] device non valido dev=%u",
                 (unsigned)device_index);
        return false;
    }

    if (s_cashless_devices[device_index].revalue_limit_requested) {
        ESP_LOGI(TAG_CASH_CTL,
                 "[C] [mdb_cashless_request_revalue_limit] dev=%u richiesta limite gia' in corso",
                 (unsigned)device_index);
        return true;
    }

    if (s_cashless_devices[device_index].revalue_status == MDB_REVALUE_REQUEST_PENDING ||
        s_cashless_devices[device_index].revalue_status == MDB_REVALUE_IN_PROGRESS) {
        ESP_LOGW(TAG_CASH_CTL,
                 "[C] [mdb_cashless_request_revalue_limit] dev=%u richiesta rifiutata: ricarica attiva status=%s",
                 (unsigned)device_index,
                 mdb_cashless_revalue_status_to_string(s_cashless_devices[device_index].revalue_status));
        return false;
    }

    s_cashless_devices[device_index].revalue_price_cents = 0U;
    s_cashless_devices[device_index].approved_revalue_cents = 0U;
    s_cashless_devices[device_index].revalue_limit_requested = true;
    s_cashless_devices[device_index].revalue_status = MDB_REVALUE_REQUEST_PENDING;
    ESP_LOGI(TAG_CASH_CTL,
             "[C] [mdb_cashless_request_revalue_limit] dev=%u revalue_status=%s",
             (unsigned)device_index,
             mdb_cashless_revalue_status_to_string(s_cashless_devices[device_index].revalue_status));
    return true;
}

bool mdb_cashless_request_session_complete(size_t device_index)
{
    if (!mdb_cashless_valid_index(device_index)) {
        ESP_LOGW(TAG_CASH_CTL,
                 "[C] [mdb_cashless_request_session_complete] device non valido dev=%u",
                 (unsigned)device_index);
        return false;
    }

    s_cashless_devices[device_index].session_complete_requested = true;
    s_cashless_devices[device_index].session_state = MDB_CASHLESS_SESSION_ENDING;
    ESP_LOGI(TAG_CASH_CTL,
             "[C] [mdb_cashless_request_session_complete] dev=%u session_state=%s",
             (unsigned)device_index,
             mdb_cashless_session_state_to_string(s_cashless_devices[device_index].session_state));
    return true;
}

bool mdb_cashless_get_pending_credit_event(size_t *device_index, uint16_t *credit_cents)
{
    if (!device_index || !credit_cents) {
        return false;
    }

    for (size_t index = 0; index < MDB_CASHLESS_DEVICE_COUNT; ++index) {
        if (!s_cashless_devices[index].credit_sync_pending) {
            continue;
        }

        *device_index = index;
        *credit_cents = s_cashless_devices[index].credit_cents;
        return true;
    }

    return false;
}

bool mdb_cashless_get_pending_vend_event(size_t *device_index, bool *approved, uint16_t *amount_cents)
{
    if (!device_index || !approved || !amount_cents) {
        return false;
    }

    for (size_t index = 0; index < MDB_CASHLESS_DEVICE_COUNT; ++index) {
        if (!s_cashless_devices[index].vend_result_pending) {
            continue;
        }

        *device_index = index;
        *approved = s_cashless_devices[index].vend_result_approved;
        *amount_cents = s_cashless_devices[index].vend_result_cents;
        return true;
    }

    return false;
}

void mdb_cashless_ack_pending_credit_event(size_t device_index)
{
    if (!mdb_cashless_valid_index(device_index)) {
        return;
    }

    s_cashless_devices[device_index].credit_sync_pending = false;
    s_cashless_devices[device_index].last_synced_credit_cents = s_cashless_devices[device_index].credit_cents;
    ESP_LOGD(TAG_CASH_CTL,
             "[C] [mdb_cashless_ack_pending_credit_event] dev=%u credito_sync_ack=%u",
             (unsigned)device_index,
             (unsigned)s_cashless_devices[device_index].last_synced_credit_cents);
}

void mdb_cashless_ack_pending_vend_event(size_t device_index)
{
    if (!mdb_cashless_valid_index(device_index)) {
        return;
    }

    s_cashless_devices[device_index].vend_result_pending = false;
    ESP_LOGD(TAG_CASH_CTL,
             "[C] [mdb_cashless_ack_pending_vend_event] dev=%u esito vend ack",
             (unsigned)device_index);
}