#include <stdbool.h>
#include "sd_card.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static const char *TAG = "SD_CARD";
static bool s_mounted = false;
static sdmmc_card_t *s_card = NULL;

#define MOUNT_POINT "/sdcard"

// Pin basati sullo SCHEMA fornito:
#define SD_PWR_GPIO 45 // Gate P-MOS: LOW = Alimentazione ON
#define SD_CMD_GPIO 44
#define SD_CLK_GPIO 43
#define SD_D0_GPIO  39
#define SD_D1_GPIO  40
#define SD_D2_GPIO  41
#define SD_D3_GPIO  42

esp_err_t sd_card_mount(void) {
    if (s_mounted) return ESP_OK;

    ESP_LOGI(TAG, "Esecuzione power cycle SD (GPIO %d)...", SD_PWR_GPIO);
    
    gpio_reset_pin(SD_PWR_GPIO);
    gpio_set_direction(SD_PWR_GPIO, GPIO_MODE_OUTPUT);
    
    // Ciclo di spegnimento/accensione per resettare la scheda
    gpio_set_level(SD_PWR_GPIO, 1); // OFF
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(SD_PWR_GPIO, 0); // ON
    vTaskDelay(pdMS_TO_TICKS(400)); // Attesa stabilizzazione alimentazione (più lunga)

    ESP_LOGI(TAG, "Inizializzazione scheda SD (4-bit mode)...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    // Riduciamo la frequenza iniziale per maggiore tolleranza ai disturbi
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20MHz (o SDMMC_FREQ_13M per test)

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    
    // Configurazione pin secondo schema MicroHard MH1001
    slot_config.clk = SD_CLK_GPIO;
    slot_config.cmd = SD_CMD_GPIO;
    slot_config.d0 = SD_D0_GPIO;
    slot_config.d1 = SD_D1_GPIO;
    slot_config.d2 = SD_D2_GPIO;
    slot_config.d3 = SD_D3_GPIO;
    
    // Abilitiamo esplicitamente i pull-up interni
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
    // Debug pin
    ESP_LOGD(TAG, "Pin SD: CLK=%d, CMD=%d, D0=%d, D1=%d, D2=%d, D3=%d", 
             slot_config.clk, slot_config.cmd, slot_config.d0, slot_config.d1, slot_config.d2, slot_config.d3);

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Fallito il montaggio del filesystem. "
                     "Se la scheda è nuova, va formattata in FAT32.");
        } else {
            ESP_LOGE(TAG, "Errore inizializzazione SD (%s). "
                     "Controllare collegamenti e pull-up.", esp_err_to_name(ret));
        }
        return ret;
    }

    s_mounted = true;
    ESP_LOGI(TAG, "SD Card montata con successo!");
    sdmmc_card_print_info(stdout, s_card);

    return ESP_OK;
}

esp_err_t sd_card_unmount(void) {
    if (!s_mounted) return ESP_OK;

    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    if (ret == ESP_OK) {
        s_mounted = false;
        s_card = NULL;
        ESP_LOGI(TAG, "SD Card smontata.");
    }
    return ret;
}

bool sd_card_is_mounted(void) {
    return s_mounted;
}

uint64_t sd_card_get_total_size(void) {
    if (!s_mounted || !s_card) return 0;
    return (uint64_t)s_card->csd.capacity * s_card->csd.sector_size / 1024;
}

uint64_t sd_card_get_used_size(void) {
    if (!s_mounted) return 0;
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    FRESULT res = f_getfree("0:", &fre_clust, &fs);
    if (res != FR_OK) return 0;
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    return (uint64_t)(tot_sect - fre_sect) * fs->ssize / 1024;
}

esp_err_t sd_card_format(void) {
    if (!s_mounted) return ESP_ERR_INVALID_STATE;
    ESP_LOGW(TAG, "Formattazione SD in corso...");
    esp_err_t err = esp_vfs_fat_sdcard_format(MOUNT_POINT, s_card);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Formattazione completata.");
    } else {
        ESP_LOGE(TAG, "Formattazione fallita: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t sd_card_list_dir(const char *path, char *out_buf, size_t out_size) {
    if (!s_mounted) return ESP_ERR_INVALID_STATE;
    DIR *dir = opendir(path);
    if (!dir) return ESP_FAIL;

    struct dirent *entry;
    size_t pos = 0;
    out_buf[0] = '\0';
    
    pos += snprintf(out_buf + pos, out_size - pos, "Elenco file in %s:\n", path);

    while ((entry = readdir(dir)) != NULL && pos < out_size - 100) {
        char full_path[300];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0) {
            pos += snprintf(out_buf + pos, out_size - pos, " - %s (%ld byte)\n", entry->d_name, st.st_size);
        } else {
            pos += snprintf(out_buf + pos, out_size - pos, " - %s\n", entry->d_name);
        }
    }
    closedir(dir);
    return ESP_OK;
}

esp_err_t sd_card_write_file(const char *path, const char *data) {
    if (!s_mounted) return ESP_ERR_INVALID_STATE;
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Impossibile aprire il file per la scrittura: %s", path);
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    ESP_LOGI(TAG, "File scritto con successo: %s", path);
    return ESP_OK;
}
