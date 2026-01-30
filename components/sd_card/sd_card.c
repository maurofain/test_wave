#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "sd_card.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

static const char *TAG = "SD_CARD";
static bool s_mounted = false;
static sdmmc_card_t *s_card = NULL;
static sd_pwr_ctrl_handle_t s_pwr_ctrl_handle = NULL;
static char s_last_error[64] = "Nessuno";
static bool s_is_formatting = false;

#define MOUNT_POINT "/sdcard"

// Pin basati sullo SCHEMA fornito:
#define SD_PWR_GPIO 45 // Gate P-MOS: LOW = Alimentazione ON
#define SD_CMD_GPIO 44
#define SD_CLK_GPIO 43
#define SD_D0_GPIO  39
#define SD_D1_GPIO  40
#define SD_D2_GPIO  41
#define SD_D3_GPIO  42
#define SD_CD_GPIO  0   // Card Detect

static bool s_last_cd_state = true; // True = Estratta (Pull-up), False = Inserita (GND)

static void sd_monitor_task(void *pvParameters) {
    ESP_LOGI(TAG, "Avvio monitor hot-plug SD (GPIO %d)...", SD_CD_GPIO);
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << SD_CD_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    s_last_cd_state = gpio_get_level(SD_CD_GPIO);
    
    while(1) {
        bool current_state = gpio_get_level(SD_CD_GPIO);
        if (current_state != s_last_cd_state) {
            if (current_state == 0) {
                ESP_LOGI(TAG, ">>> EVENTO: MicroSD INSERITA");
            } else {
                ESP_LOGI(TAG, ">>> EVENTO: MicroSD RIMOSSA");
                if (s_mounted) {
                    ESP_LOGW(TAG, "Scheda rimossa mentre era montata! Smontaggio forzato...");
                    s_mounted = false;
                    s_card = NULL;
                }
            }
            s_last_cd_state = current_state;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void sd_card_init_monitor(void) {
    static bool s_task_started = false;
    if (s_task_started) return;
    
    xTaskCreate(sd_monitor_task, "sd_monitor", 4096, NULL, 5, NULL);
    s_task_started = true;
}

const char* sd_card_get_last_error(void) {
    return s_last_error;
}

esp_err_t sd_card_mount(void) {
    if (s_mounted) {
        ESP_LOGI(TAG, "Tentativo di montaggio ignorato: scheda già montata.");
        return ESP_OK;
    }

    snprintf(s_last_error, sizeof(s_last_error), "Inizializzazione...");
    ESP_LOGI(TAG, "Inizio procedura di montaggio SD...");
    ESP_LOGI(TAG, "Esecuzione power cycle SD (GPIO %d)...", SD_PWR_GPIO);
    
    gpio_reset_pin(SD_PWR_GPIO);
    gpio_set_direction(SD_PWR_GPIO, GPIO_MODE_OUTPUT);
    
    // Ciclo di spegnimento/accensione per resettare la scheda
    gpio_set_level(SD_PWR_GPIO, 1); // OFF
    ESP_LOGD(TAG, "Alimentazione SD: OFF");
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(SD_PWR_GPIO, 0); // ON
    ESP_LOGD(TAG, "Alimentazione SD: ON, attesa stabilizzazione...");
    vTaskDelay(pdMS_TO_TICKS(400)); // Attesa stabilizzazione alimentazione (più lunga)

    ESP_LOGI(TAG, "Configurazione driver SDMMC (4-bit mode)...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true, // Abilitiamo la formattazione automatica se il mount fallisce
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    // Riduciamo la frequenza per maggiore stabilità (20MHz -> 10MHz)
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // Tentiamo il default, ma...
    host.max_freq_khz = 10000; // Forziamo 10MHz per debug stability

    // Configurazione LDO per alimentazione SD (ID 4 trovato nel reference WT99P4C5-S1)
    if (s_pwr_ctrl_handle == NULL) {
        ESP_LOGI(TAG, "Inizializzazione controller LDO (Canale 4) per alimentazione SD...");
        sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = 4,
        };
        esp_err_t pwr_ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &s_pwr_ctrl_handle);
        if (pwr_ret != ESP_OK) {
            ESP_LOGE(TAG, "Errore inizializzazione LDO SD: %s", esp_err_to_name(pwr_ret));
        }
    }
    host.pwr_ctrl_handle = s_pwr_ctrl_handle;

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
        snprintf(s_last_error, sizeof(s_last_error), "%s (0x%x)", esp_err_to_name(ret), ret);
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "ERRORE: Fallito il montaggio del filesystem FAT (Card non formattata?).");
        } else {
            ESP_LOGE(TAG, "ERRORE: Inizializzazione fisica SD fallita (%s - 0x%x).", esp_err_to_name(ret), ret);
        }
        s_mounted = false;
        return ret;
    }

    s_mounted = true;
    snprintf(s_last_error, sizeof(s_last_error), "SD Montata");
    ESP_LOGI(TAG, "SUCCESSO: SD Card montata correttamente in %s", MOUNT_POINT);
    sdmmc_card_print_info(stdout, s_card);
    
    // Test di scrittura rapido subito dopo il mount
    ESP_LOGI(TAG, "Esecuzione test di scrittura post-mount...");
    sd_card_write_file(MOUNT_POINT "/test.txt", "Test di montaggio OK");

    return ESP_OK;
}

esp_err_t sd_card_unmount(void) {
    if (!s_mounted) {
        ESP_LOGW(TAG, "Tentativo di smontaggio ignorato: scheda non montata.");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Inizio smontaggio SD...");
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
    if (ret == ESP_OK) {
        s_mounted = false;
        s_card = NULL;
        snprintf(s_last_error, sizeof(s_last_error), "Smontata");
        ESP_LOGI(TAG, "SUCCESSO: SD Card smontata correttamente.");
    } else {
        snprintf(s_last_error, sizeof(s_last_error), "Unmount Error: %d", ret);
        ESP_LOGE(TAG, "ERRORE: Smontaggio fallito (%s)", esp_err_to_name(ret));
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

static void sd_format_timer_task(void *pvParameters) {
    int elapsed = 0;
    while (s_is_formatting) {
        if (elapsed > 120) break; // Timeout sicurezza 2 min
        snprintf(s_last_error, sizeof(s_last_error), "Formattazione... [%ds]", elapsed);
        vTaskDelay(pdMS_TO_TICKS(1000));
        elapsed++;
    }
    vTaskDelete(NULL);
}

static void sd_format_worker_task(void *pvParameters) {
    ESP_LOGW(TAG, "Lavoro di formattazione iniziato (bloccante)...");
    esp_err_t err = esp_vfs_fat_sdcard_format(MOUNT_POINT, s_card);
    
    s_is_formatting = false; 
    vTaskDelay(pdMS_TO_TICKS(100)); 
    
    if (err == ESP_OK) {
        snprintf(s_last_error, sizeof(s_last_error), "Formattazione OK");
        ESP_LOGI(TAG, "Formattazione completata con successo.");
        sd_card_write_file(MOUNT_POINT "/form_ok.txt", "Scheda pulita e formattata");
    } else {
        snprintf(s_last_error, sizeof(s_last_error), "Errore: 0x%x", err);
        ESP_LOGE(TAG, "Formattazione fallita: 0x%x", err);
    }
    vTaskDelete(NULL);
}

esp_err_t sd_card_format(void) {
    if (!s_mounted || !s_card) {
        snprintf(s_last_error, sizeof(s_last_error), "Errore: SD non montata");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_is_formatting) return ESP_ERR_INVALID_STATE;

    s_is_formatting = true;
    xTaskCreate(sd_format_worker_task, "sd_fmt_work", 4096, NULL, 5, NULL);
    xTaskCreate(sd_format_timer_task, "sd_fmt_timer", 2048, NULL, 4, NULL);
    
    return ESP_OK;
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
        snprintf(s_last_error, sizeof(s_last_error), "Write Error: %d", errno);
        ESP_LOGE(TAG, "Impossibile aprire il file per la scrittura: %s (errno: %d, %s)", 
                 path, errno, strerror(errno));
        return ESP_FAIL;
    }
    fprintf(f, "%s", data);
    fclose(f);
    snprintf(s_last_error, sizeof(s_last_error), "Scrittura OK");
    ESP_LOGI(TAG, "File scritto con successo: %s", path);
    return ESP_OK;
}
