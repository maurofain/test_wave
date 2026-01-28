#include "test_mdb.h"
#include "mdb_bus.h"
#include "esp_log.h"
#include "test_serial.h" // Per il parser escape
#include <string.h>

static const char *TAG = "TEST_MDB";

esp_err_t test_mdb_send_custom(const char* hex_or_str) {
    uint8_t buf[256];
    size_t len = test_serial_parse_escapes(hex_or_str, buf, sizeof(buf));
    if (len > 0) {
        uint8_t address = buf[0];
        const uint8_t* data = (len > 1) ? &buf[1] : NULL;
        size_t data_len = (len > 1) ? len - 1 : 0;
        
        ESP_LOGI(TAG, "Sending custom MDB: Addr 0x%02X, Data len %d", address, (int)data_len);
        return mdb_bus_send_packet(address, data, data_len);
    }
    return ESP_ERR_INVALID_ARG;
}
