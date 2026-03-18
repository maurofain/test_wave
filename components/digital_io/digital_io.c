#include "digital_io.h"

#include "device_config.h"
#include "io_expander.h"
#include "modbus_relay.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "DIGITAL_IO"

#define DIGITAL_IO_MIN_ID 1U

static SemaphoreHandle_t s_lock = NULL;
static bool s_initialized = false;

static bool is_valid_output_id(uint8_t output_id)
{
    return (output_id >= DIGITAL_IO_MIN_ID && output_id <= DIGITAL_IO_OUTPUT_COUNT);
}

static bool is_valid_input_id(uint8_t input_id)
{
    return (input_id >= DIGITAL_IO_MIN_ID && input_id <= DIGITAL_IO_INPUT_COUNT);
}

static bool is_local_output(uint8_t output_id)
{
    return output_id <= DIGITAL_IO_LOCAL_OUTPUT_COUNT;
}

static bool is_local_input(uint8_t input_id)
{
    return input_id <= DIGITAL_IO_LOCAL_INPUT_COUNT;
}

static uint8_t local_index_from_output(uint8_t output_id)
{
    return (uint8_t)(output_id - 1U);
}

static uint8_t local_index_from_input(uint8_t input_id)
{
    return (uint8_t)(input_id - 1U);
}

static uint16_t modbus_output_offset(uint8_t output_id)
{
    return (uint16_t)(output_id - DIGITAL_IO_FIRST_MODBUS_OUTPUT);
}

static uint16_t modbus_input_offset(uint8_t input_id)
{
    return (uint16_t)(input_id - DIGITAL_IO_FIRST_MODBUS_INPUT);
}

static esp_err_t ensure_lock(void)
{
    if (s_lock != NULL) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t take_lock(void)
{
    esp_err_t err = ensure_lock();
    if (err != ESP_OK) {
        return err;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static void give_lock(void)
{
    if (s_lock != NULL) {
        xSemaphoreGive(s_lock);
    }
}

static bool is_modbus_available(const device_config_t *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    return (cfg->sensors.rs485_enabled && cfg->modbus.enabled);
}

static esp_err_t ensure_local_io_ready(const device_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!cfg->sensors.io_expander_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    return io_expander_init();
}

static esp_err_t ensure_modbus_ready(const device_config_t *cfg)
{
    if (!is_modbus_available(cfg)) {
        return ESP_ERR_INVALID_STATE;
    }

    return modbus_relay_init();
}

static esp_err_t set_output_locked(const device_config_t *cfg, uint8_t output_id, bool value)
{
    if (is_local_output(output_id)) {
        esp_err_t local_err = ensure_local_io_ready(cfg);
        if (local_err != ESP_OK) {
            return local_err;
        }

        io_set_pin((int)local_index_from_output(output_id), value ? 1 : 0);
        return ESP_OK;
    }

    esp_err_t modbus_err = ensure_modbus_ready(cfg);
    if (modbus_err != ESP_OK) {
        return modbus_err;
    }

    uint16_t coil_addr = (uint16_t)(cfg->modbus.relay_start + modbus_output_offset(output_id));
    return modbus_relay_write_single_coil(cfg->modbus.slave_id, coil_addr, value);
}

static esp_err_t get_output_locked(const device_config_t *cfg, uint8_t output_id, bool *out_value)
{
    if (out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (is_local_output(output_id)) {
        esp_err_t local_err = ensure_local_io_ready(cfg);
        if (local_err != ESP_OK) {
            return local_err;
        }

        uint8_t bit = local_index_from_output(output_id);
        *out_value = ((io_output_state >> bit) & 0x01U) != 0U;
        return ESP_OK;
    }

    esp_err_t modbus_err = ensure_modbus_ready(cfg);
    if (modbus_err != ESP_OK) {
        return modbus_err;
    }

    uint16_t coil_addr = (uint16_t)(cfg->modbus.relay_start + modbus_output_offset(output_id));
    uint8_t bits[1] = {0};
    esp_err_t err = modbus_relay_read_coils(cfg->modbus.slave_id,
                                            coil_addr,
                                            1,
                                            bits,
                                            sizeof(bits));
    if (err != ESP_OK) {
        return err;
    }

    *out_value = (bits[0] & 0x01U) != 0U;
    return ESP_OK;
}

static esp_err_t get_input_locked(const device_config_t *cfg, uint8_t input_id, bool *out_value)
{
    if (out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (is_local_input(input_id)) {
        esp_err_t local_err = ensure_local_io_ready(cfg);
        if (local_err != ESP_OK) {
            return local_err;
        }

        *out_value = io_get_pin((int)local_index_from_input(input_id));
        return ESP_OK;
    }

    esp_err_t modbus_err = ensure_modbus_ready(cfg);
    if (modbus_err != ESP_OK) {
        return modbus_err;
    }

    uint16_t input_addr = (uint16_t)(cfg->modbus.input_start + modbus_input_offset(input_id));
    uint8_t bits[1] = {0};
    esp_err_t err = modbus_relay_read_discrete_inputs(cfg->modbus.slave_id,
                                                      input_addr,
                                                      1,
                                                      bits,
                                                      sizeof(bits));
    if (err != ESP_OK) {
        return err;
    }

    *out_value = (bits[0] & 0x01U) != 0U;
    return ESP_OK;
}

esp_err_t digital_io_init(void)
{
    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    const device_config_t *cfg = device_config_get();
    if (cfg == NULL) {
        give_lock();
        return ESP_ERR_INVALID_STATE;
    }

    if (cfg->sensors.io_expander_enabled) {
        esp_err_t local_err = io_expander_init();
        if (local_err != ESP_OK) {
            ESP_LOGW(TAG, "[C] Init I/O locali fallita: %s", esp_err_to_name(local_err));
        }
    }

    if (is_modbus_available(cfg)) {
        esp_err_t modbus_err = modbus_relay_init();
        if (modbus_err != ESP_OK) {
            ESP_LOGW(TAG, "[C] Init Modbus I/O fallita: %s", esp_err_to_name(modbus_err));
        }
    }

    s_initialized = true;
    give_lock();
    return ESP_OK;
}

esp_err_t digital_io_set_output(uint8_t output_id, bool value)
{
    if (!is_valid_output_id(output_id)) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    const device_config_t *cfg = device_config_get();
    if (cfg == NULL) {
        give_lock();
        return ESP_ERR_INVALID_STATE;
    }

    err = set_output_locked(cfg, output_id, value);
    if (err != ESP_OK) {
        ESP_LOGW(TAG,
                 "[C] Set output R%02u=%d fallita: %s",
                 (unsigned)output_id,
                 value ? 1 : 0,
                 esp_err_to_name(err));
    }

    give_lock();
    return err;
}

esp_err_t digital_io_get_output(uint8_t output_id, bool *out_value)
{
    if (!is_valid_output_id(output_id) || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    const device_config_t *cfg = device_config_get();
    if (cfg == NULL) {
        give_lock();
        return ESP_ERR_INVALID_STATE;
    }

    err = get_output_locked(cfg, output_id, out_value);
    give_lock();
    return err;
}

esp_err_t digital_io_get_input(uint8_t input_id, bool *out_value)
{
    if (!is_valid_input_id(input_id) || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    const device_config_t *cfg = device_config_get();
    if (cfg == NULL) {
        give_lock();
        return ESP_ERR_INVALID_STATE;
    }

    err = get_input_locked(cfg, input_id, out_value);
    give_lock();
    return err;
}

esp_err_t digital_io_set_outputs_mask(uint16_t outputs_mask)
{
    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    const device_config_t *cfg = device_config_get();
    if (cfg == NULL) {
        give_lock();
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t first_err = ESP_OK;
    for (uint8_t output_id = DIGITAL_IO_MIN_ID; output_id <= DIGITAL_IO_OUTPUT_COUNT; ++output_id) {
        bool value = ((outputs_mask >> (output_id - 1U)) & 0x01U) != 0U;
        esp_err_t set_err = set_output_locked(cfg, output_id, value);
        if (set_err != ESP_OK && first_err == ESP_OK) {
            first_err = set_err;
        }
    }

    if (first_err != ESP_OK) {
        ESP_LOGW(TAG, "[C] Set mask uscite fallita parzialmente: %s", esp_err_to_name(first_err));
    }

    give_lock();
    return first_err;
}

esp_err_t digital_io_get_snapshot(digital_io_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = take_lock();
    if (err != ESP_OK) {
        return err;
    }

    const device_config_t *cfg = device_config_get();
    if (cfg == NULL) {
        give_lock();
        return ESP_ERR_INVALID_STATE;
    }

    digital_io_snapshot_t snapshot = {0};
    esp_err_t first_err = ESP_OK;

    for (uint8_t output_id = DIGITAL_IO_MIN_ID; output_id <= DIGITAL_IO_OUTPUT_COUNT; ++output_id) {
        bool state = false;
        esp_err_t out_err = get_output_locked(cfg, output_id, &state);
        if (out_err == ESP_OK && state) {
            snapshot.outputs_mask |= (uint16_t)(1U << (output_id - 1U));
        } else if (out_err != ESP_OK && first_err == ESP_OK) {
            first_err = out_err;
        }
    }

    for (uint8_t input_id = DIGITAL_IO_MIN_ID; input_id <= DIGITAL_IO_INPUT_COUNT; ++input_id) {
        bool state = false;
        esp_err_t in_err = get_input_locked(cfg, input_id, &state);
        if (in_err == ESP_OK && state) {
            snapshot.inputs_mask |= (uint16_t)(1U << (input_id - 1U));
        } else if (in_err != ESP_OK && first_err == ESP_OK) {
            first_err = in_err;
        }
    }

    *out_snapshot = snapshot;

    give_lock();
    return first_err;
}

size_t digital_io_get_input_infos(digital_io_input_info_t *out_list, size_t max_items)
{
    const size_t total_inputs = (size_t)DIGITAL_IO_INPUT_COUNT;
    if (out_list == NULL || max_items == 0) {
        return total_inputs;
    }

    const device_config_t *cfg = device_config_get();
    bool local_available = (cfg != NULL && cfg->sensors.io_expander_enabled);
    bool modbus_available = (cfg != NULL && cfg->sensors.rs485_enabled && cfg->modbus.enabled);

    size_t count = (max_items < total_inputs) ? max_items : total_inputs;
    for (size_t index = 0; index < count; ++index) {
        uint8_t input_id = (uint8_t)(index + 1U);
        bool is_local = is_local_input(input_id);

        out_list[index].input_id = input_id;
        out_list[index].is_local = is_local;
        out_list[index].available = is_local ? local_available : modbus_available;
    }

    return count;
}
