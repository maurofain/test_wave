#include "eeprom_test.h"
#include "eeprom_24lc16.h"
#include "device_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

#define EEPROM_TOTAL_SIZE 2048

static const char *TAG = "EEPROM_TEST";


/**
 * @brief Gestisce la richiesta HTTP per il test dell'EEPROM.
 * 
 * @param req Puntatore alla richiesta HTTP.
 * @return esp_err_t Codice di errore.
 */
esp_err_t eeprom_test_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Ricevuta richiesta test EEPROM");
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_OK;
    }

    cJSON *op = cJSON_GetObjectItem(root, "op");
    if (!op || !op->valuestring) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'op'");
        return ESP_OK;
    }

    cJSON *resp = cJSON_CreateObject();
    
    if (strcmp(op->valuestring, "read") == 0) {
        cJSON *addr_obj = cJSON_GetObjectItem(root, "addr");
        cJSON *len_obj = cJSON_GetObjectItem(root, "len");
        int addr = addr_obj ? addr_obj->valueint : 0;
        int len = len_obj ? len_obj->valueint : 16;
        esp_err_t read_ret = ESP_OK;

        if (addr < 0) addr = 0;
        if (len <= 0) len = 1;
        if (len > 256) len = 256;

        if ((addr + len) > EEPROM_TOTAL_SIZE) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "range_out_of_bounds");
            cJSON_AddNumberToObject(resp, "addr", addr);
            cJSON_AddNumberToObject(resp, "len", len);
            cJSON_AddNumberToObject(resp, "max_size", EEPROM_TOTAL_SIZE);
            goto send_response;
        }

        if (!eeprom_24lc16_is_available()) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "eeprom_not_available");
            goto send_response;
        }

        uint8_t *data = malloc(len);
        if (!data) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "no_memory");
            goto send_response;
        }

        read_ret = eeprom_24lc16_read(addr, data, len);
        if (read_ret == ESP_OK) {
            cJSON *arr = cJSON_CreateArray();
            for (int i = 0; i < len; i++) {
                cJSON_AddItemToArray(arr, cJSON_CreateNumber(data[i]));
            }
            cJSON_AddItemToObject(resp, "data", arr);
            cJSON_AddStringToObject(resp, "status", "ok");
        } else {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", esp_err_to_name(read_ret));
            cJSON_AddNumberToObject(resp, "addr", addr);
            cJSON_AddNumberToObject(resp, "len", len);
        }
        free(data);
    } 
    else if (strcmp(op->valuestring, "read_json") == 0) {
        char *json = device_config_read_json_from_eeprom();
        if (json) {
            cJSON_AddStringToObject(resp, "json", json);
            cJSON_AddStringToObject(resp, "source", "eeprom");
            cJSON_AddStringToObject(resp, "status", "ok");
            free(json);
        } else {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "eeprom_not_valid_or_unavailable");
        }
    }
    else if (strcmp(op->valuestring, "status") == 0) {
        cJSON_AddNumberToObject(resp, "crc", device_config_get_crc());
        cJSON_AddBoolToObject(resp, "updated", device_config_is_updated());
        cJSON_AddStringToObject(resp, "status", "ok");
    }
    else if (strcmp(op->valuestring, "dump_ascii") == 0) {
        /* return an ASCII‑clean dump of requested EEPROM range */
        cJSON *addr_obj = cJSON_GetObjectItem(root, "addr");
        cJSON *len_obj = cJSON_GetObjectItem(root, "len");
        int addr = addr_obj ? addr_obj->valueint : 0;
        int len = len_obj ? len_obj->valueint : EEPROM_TOTAL_SIZE;
        if (addr < 0) addr = 0;
        if (len <= 0) len = EEPROM_TOTAL_SIZE;
        if ((addr + len) > EEPROM_TOTAL_SIZE) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "range_out_of_bounds");
            cJSON_AddNumberToObject(resp, "addr", addr);
            cJSON_AddNumberToObject(resp, "len", len);
            cJSON_AddNumberToObject(resp, "max_size", EEPROM_TOTAL_SIZE);
            goto send_response;
        }
        if (!eeprom_24lc16_is_available()) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "eeprom_not_available");
            goto send_response;
        }
        uint8_t *data = malloc(len);
        if (!data) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "no_memory");
            goto send_response;
        }
        esp_err_t read_ret = eeprom_24lc16_read(addr, data, len);
        if (read_ret == ESP_OK) {
            char *ascii = malloc(len + 1);
            if (ascii) {
                for (int i = 0; i < len; i++) {
                    uint8_t b = data[i];
                    ascii[i] = (b >= 32 && b <= 126) ? (char)b : '.';
                }
                ascii[len] = '\0';
                cJSON_AddStringToObject(resp, "ascii", ascii);
                free(ascii);
            }
            cJSON_AddStringToObject(resp, "status", "ok");
        } else {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", esp_err_to_name(read_ret));
            cJSON_AddNumberToObject(resp, "addr", addr);
            cJSON_AddNumberToObject(resp, "len", len);
        }
        free(data);
    }
    else if (strcmp(op->valuestring, "dump_hex") == 0) {
        /* return raw bytes for hex rendering */
        cJSON *addr_obj = cJSON_GetObjectItem(root, "addr");
        cJSON *len_obj = cJSON_GetObjectItem(root, "len");
        int addr = addr_obj ? addr_obj->valueint : 0;
        int len = len_obj ? len_obj->valueint : EEPROM_TOTAL_SIZE;
        if (addr < 0) addr = 0;
        if (len <= 0) len = EEPROM_TOTAL_SIZE;
        if ((addr + len) > EEPROM_TOTAL_SIZE) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "range_out_of_bounds");
            cJSON_AddNumberToObject(resp, "addr", addr);
            cJSON_AddNumberToObject(resp, "len", len);
            cJSON_AddNumberToObject(resp, "max_size", EEPROM_TOTAL_SIZE);
            goto send_response;
        }
        if (!eeprom_24lc16_is_available()) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "eeprom_not_available");
            goto send_response;
        }
        uint8_t *data = malloc(len);
        if (!data) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "no_memory");
            goto send_response;
        }
        esp_err_t read_ret = eeprom_24lc16_read(addr, data, len);
        if (read_ret == ESP_OK) {
            cJSON *arr = cJSON_CreateArray();
            for (int i = 0; i < len; i++) {
                cJSON_AddItemToArray(arr, cJSON_CreateNumber(data[i]));
            }
            cJSON_AddItemToObject(resp, "data", arr);
            cJSON_AddStringToObject(resp, "status", "ok");
        } else {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", esp_err_to_name(read_ret));
            cJSON_AddNumberToObject(resp, "addr", addr);
            cJSON_AddNumberToObject(resp, "len", len);
        }
        free(data);
    }
    else if (strcmp(op->valuestring, "write") == 0) {
        cJSON *addr_item = cJSON_GetObjectItem(root, "addr");
        cJSON *data_arr = cJSON_GetObjectItem(root, "data");
        if (!addr_item || !cJSON_IsNumber(addr_item) || !data_arr || !cJSON_IsArray(data_arr)) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "invalid_payload");
            goto send_response;
        }

        int addr = addr_item->valueint;
        if (addr < 0) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "invalid_addr");
            goto send_response;
        }

        int len = cJSON_GetArraySize(data_arr);
        if (len <= 0 || (addr + len) > EEPROM_TOTAL_SIZE) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "range_out_of_bounds");
            cJSON_AddNumberToObject(resp, "addr", addr);
            cJSON_AddNumberToObject(resp, "len", len);
            cJSON_AddNumberToObject(resp, "max_size", EEPROM_TOTAL_SIZE);
            goto send_response;
        }

        if (!eeprom_24lc16_is_available()) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "eeprom_not_available");
            goto send_response;
        }

        uint8_t *data = malloc(len);
        if (!data) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "no_memory");
            goto send_response;
        }
        for (int i = 0; i < len; i++) {
            data[i] = (uint8_t)cJSON_GetArrayItem(data_arr, i)->valueint;
        }

        esp_err_t write_ret = eeprom_24lc16_write(addr, data, len);
        if (write_ret == ESP_OK) {
            cJSON_AddStringToObject(resp, "status", "ok");
        } else {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", esp_err_to_name(write_ret));
            cJSON_AddNumberToObject(resp, "addr", addr);
            cJSON_AddNumberToObject(resp, "len", len);
        }
        free(data);
    }

send_response:

    char *json_resp = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_resp);
    
    free(json_resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return ESP_OK;
}
