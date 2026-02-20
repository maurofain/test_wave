#include "device_activity.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"   // for heap_caps_malloc and MALLOC_CAP_*
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "DEVICE_ACTIVITY";
static device_activity_entry_t *s_table = NULL;
static size_t s_count = 0;

static const char *ACTIVITY_FILE = "/spiffs/activity.json";

static esp_err_t create_default_table(void)
{
    // create a small default JSON with few entries
    cJSON *root = cJSON_CreateArray();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON *item = cJSON_CreateObject();
    if (item) {
        cJSON_AddNumberToObject(item, "id", 1);
        cJSON_AddStringToObject(item, "description", "Start program");
        cJSON_AddItemToArray(root, item);
    }
    item = cJSON_CreateObject();
    if (item) {
        cJSON_AddNumberToObject(item, "id", 2);
        cJSON_AddStringToObject(item, "description", "Stop program");
        cJSON_AddItemToArray(root, item);
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return ESP_ERR_NO_MEM;
    FILE *f = fopen(ACTIVITY_FILE, "w");
    if (!f) {
        free(json);
        return ESP_FAIL;
    }
    fwrite(json, 1, strlen(json), f);
    fclose(f);
    free(json);
    return ESP_OK;
}

static esp_err_t load_table_from_file(void)
{
    FILE *f = fopen(ACTIVITY_FILE, "r");
    if (!f) {
        ESP_LOGW(TAG, "Activity file not found, creating default");
        if (create_default_table() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create default activity file");
            return ESP_FAIL;
        }
        f = fopen(ACTIVITY_FILE, "r");
        if (!f) return ESP_FAIL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        return ESP_FAIL;
    }
    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    int cnt = cJSON_GetArraySize(root);
    s_table = heap_caps_malloc(cnt * sizeof(device_activity_entry_t), MALLOC_CAP_SPIRAM);
    if (!s_table) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    s_count = cnt;
    for (int i = 0; i < cnt; ++i) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (!item) continue;
        cJSON *jid = cJSON_GetObjectItem(item, "id");
        cJSON *desc = cJSON_GetObjectItem(item, "description");
        if (cJSON_IsNumber(jid) && cJSON_IsString(desc)) {
            s_table[i].id = (uint32_t)jid->valuedouble;
            strncpy(s_table[i].description, desc->valuestring, sizeof(s_table[i].description) - 1);
            s_table[i].description[sizeof(s_table[i].description)-1] = '\0';
        }
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d activity entries", (int)s_count);
    return ESP_OK;
}

esp_err_t device_activity_init(void)
{
    if (s_table) {
        return ESP_OK; // already initialized
    }
    // ensure SPIFFS is mounted (assume done elsewhere)
    esp_err_t err = load_table_from_file();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "loading activity table failed");
        return err;
    }
    return ESP_OK;
}

const device_activity_entry_t *device_activity_find(uint32_t id)
{
    for (size_t i = 0; i < s_count; ++i) {
        if (s_table[i].id == id) return &s_table[i];
    }
    return NULL;
}
