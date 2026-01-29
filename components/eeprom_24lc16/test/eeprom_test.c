#include "eeprom_test.h"
#include "eeprom_24lc16.h"
#include "device_config.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "EEPROM_TEST";

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
        if (len > 256) len = 256;

        uint8_t *data = malloc(len);
        if (eeprom_24lc16_read(addr, data, len) == ESP_OK) {
            cJSON *arr = cJSON_CreateArray();
            for (int i = 0; i < len; i++) {
                cJSON_AddItemToArray(arr, cJSON_CreateNumber(data[i]));
            }
            cJSON_AddItemToObject(resp, "data", arr);
            cJSON_AddStringToObject(resp, "status", "ok");
        } else {
            cJSON_AddStringToObject(resp, "status", "error");
        }
        free(data);
    } 
    else if (strcmp(op->valuestring, "read_json") == 0) {
        char *json = device_config_to_json(device_config_get());
        if (json) {
            cJSON_AddStringToObject(resp, "json", json);
            cJSON_AddStringToObject(resp, "status", "ok");
            free(json);
        } else {
            cJSON_AddStringToObject(resp, "status", "error");
        }
    }
    else if (strcmp(op->valuestring, "status") == 0) {
        cJSON_AddNumberToObject(resp, "crc", device_config_get_crc());
        cJSON_AddBoolToObject(resp, "updated", device_config_is_updated());
        cJSON_AddStringToObject(resp, "status", "ok");
    }
    else if (strcmp(op->valuestring, "write") == 0) {
        int addr = cJSON_GetObjectItem(root, "addr")->valueint;
        cJSON *data_arr = cJSON_GetObjectItem(root, "data");
        int len = cJSON_GetArraySize(data_arr);
        
        uint8_t *data = malloc(len);
        for (int i = 0; i < len; i++) {
            data[i] = (uint8_t)cJSON_GetArrayItem(data_arr, i)->valueint;
        }

        if (eeprom_24lc16_write(addr, data, len) == ESP_OK) {
            cJSON_AddStringToObject(resp, "status", "ok");
        } else {
            cJSON_AddStringToObject(resp, "status", "error");
        }
        free(data);
    }

    char *json_resp = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_resp);
    
    free(json_resp);
    cJSON_Delete(resp);
    cJSON_Delete(root);
    return ESP_OK;
}
