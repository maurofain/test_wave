#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

/* DNA_SD_CARD: passa come -DDNA_SD_CARD=1 nel CMakeLists del componente
 * per attivare il mockup senza hardware reale. Default: 0 (driver reale). */
#ifndef DNA_SD_CARD
#define DNA_SD_CARD 0
#endif


static const char *TAG = "SD_CARD";
static bool s_mounted = false;
static sdmmc_card_t *s_card = NULL;
static sd_pwr_ctrl_handle_t s_pwr_ctrl_handle = NULL;
static char s_last_error[128] = "Nessuno";
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

#if DNA_SD_CARD == 0  /* implementazioni reali — escluse se mockup attivo */

/**
 * @brief Task di monitoraggio hot‑plug della scheda SD
 *
 * Viene eseguito a background e controlla il pin CARD_DETECT. Quando rileva
 * un inserimento o una rimozione genera un log e aggiorna lo stato interno.
 * Il task non termina mai.
 */
void sd_card_monitor_run(void *pvParameters) {
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

/**
 * @brief Inizializza e avvia il monitor hot‑plug
 *
 * Questa routine crea il task sd_monitor_task una sola volta, anche se
 * richiamata più volte. Il monitor si occupa di rilevare inserimenti e
 * rimozioni fisiche della scheda e tenerne traccia internamente.
 */
void sd_card_init_monitor(void) {
    /* Il task sd_monitor è ora gestito da tasks.c tramite sd_card_monitor_run().
     * Funzione mantenuta per retrocompatibilità. */
    ESP_LOGD(TAG, "sd_card_init_monitor: task gestito da tasks.c (sd_card_monitor_run)");
}

/**
 * @brief Restituisce l'ultimo messaggio di errore del modulo SD
 *
 * Il messaggio descrive l'ultimo fallimento (mount, write, format, ecc.)
 * e può essere usato per diagnostica. La stringa è statica.
 *
 * @return puntatore all'ultima stringa d'errore.
 */
const char* sd_card_get_last_error(void) {
    return s_last_error;
}

/**
 * @brief Tenta di montare il filesystem FAT sulla scheda SD
 *
 * Se la scheda è già montata questa funzione ritorna ESP_OK immediatamente.
 * Viene effettuato un power‑cycle dell'alimentazione, configurato il
 * controller SDMMC e infine eseguito il mount tramite esp_vfs_fat.
 * In caso di errore il messaggio viene salvato in s_last_error.
 *
 * @return ESP_OK se il mount è andato a buon fine, altrimenti codice
 *         di errore esp_err_t restituito dal driver.
 */
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
        /* usa un'unità di allocazione ridotta per diminuire la richiesta di
           memoria DMA.  valori grandi (es. 16 kB) possono causare errori
           "not enough mem" durante la scrittura se l'heap DMA è frammentato. */
        .allocation_unit_size = 4 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    // Riduciamo la frequenza per maggiore stabilità (20MHz -> 10MHz)
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // Tentiamo il default, ma...
    /*MF - orig. 10000*/
    host.max_freq_khz = 5000; // Forziamo 1MHz per stabilità durante il debug

    // Configurazione LDO per alimentazione SD (ID 4 trovato nel reference WT99P4C5-S1)
    if (s_pwr_ctrl_handle == NULL) {
        ESP_LOGI(TAG, "Inizializzazione controller LDO (Canale 4) per alimentazione SD...");
        sd_pwr_ctrl_ldo_config_t ldo_config = {
            .ldo_chan_id = 4,
        };
        esp_err_t pwr_ret = sd_pwr_ctrl_new_on_chip_ldo(&ldo_config, &s_pwr_ctrl_handle);
        if (pwr_ret != ESP_OK) {
            ESP_LOGE(TAG, "Errore inizializzazione LDO SD: %s", esp_err_to_name(pwr_ret));
        } else {
            // Impostiamo il voltaggio a 3.3V (3300 mV) per la SD
            sd_pwr_ctrl_set_io_voltage(s_pwr_ctrl_handle, 3300);
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

/**
 * @brief Smonta il filesystem SD e libera le risorse
 *
 * Se la scheda non è montata la chiamata viene ignorata e viene
 * restituito ESP_OK. In caso di errore la variabile s_mounted rimane false
 * e viene registrato un messaggio in s_last_error.
 *
 * @return ESP_OK o errore esp_err_t.
 */
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

/**
 * @brief Indica se il filesystem SD è attualmente montato
 *
 * Questa funzione controlla solo la flag interna s_mounted e non effettua
 * alcuna operazione sul hardware.
 *
 * @return true se montata.
 */
bool sd_card_is_mounted(void) {
    return s_mounted;
}

/**
 * @brief Legge il pin Card Detect per sapere se una scheda è inserita
 *
 * Restituisce true se il pin indica la presenza della scheda (livello basso).
 * Non garantisce che il filesystem sia montato; serve solo per rilevazioni
 * hardware.
 *
 * @return true se la scheda è fisicamente presente.
 */
bool sd_card_is_present(void) {
    return (gpio_get_level(SD_CD_GPIO) == 0);
}


/**
 * @brief Ottiene la dimensione totale della scheda SD.
 * 
 * @return uint64_t La dimensione totale della scheda SD in byte.
 */
uint64_t sd_card_get_total_size(void) {
    if (!s_mounted || !s_card) return 0;
    return (uint64_t)s_card->csd.capacity * s_card->csd.sector_size / 1024;
}


/**
 * @brief Ottiene la dimensione utilizzata della scheda SD.
 * 
 * @return uint64_t La dimensione in byte della parte della scheda SD che è stata utilizzata.
 */
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


/**
 * @brief Gestisce il timer per la formattazione del sistema.
 * 
 * Questa funzione viene eseguita come task in un sistema operativo RTOS.
 * Si occupa della formattazione del sistema in base al timer configurato.
 * 
 * @param pvParameters Puntatore ai parametri del task, non utilizzato in questa funzione.
 * @return void Nessun valore di ritorno.
 */
static void sd_format_timer_task(void *pvParameters) {
    int elapsed = 0;
    while (s_is_formatting) {
        if (elapsed > 120) break; // Timeout sicurezza 2 min
        snprintf(s_last_error, sizeof(s_last_error), "Formattazione in corso... (%ds) : attendi il messaggio di completamento", elapsed);
        vTaskDelay(pdMS_TO_TICKS(1000));
        elapsed++;
    }
    vTaskDelete(NULL);
}


/**
 * @brief Gestisce il task di formattazione del dispositivo SD.
 * 
 * Questa funzione viene eseguita come task in un sistema operativo RTOS e si occupa della formattazione del dispositivo SD.
 * 
 * @param pvParameters Puntatore ai parametri passati al task. In questo caso, non viene utilizzato.
 * @return void Non restituisce alcun valore.
 */
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

/**
 * @brief Avvia la formattazione FAT della scheda in background
 *
 * Se la scheda non è montata ritorna ESP_ERR_INVALID_STATE. La formattazione
 * viene eseguita in un task separato per non bloccare il chiamante.
 *
 * @return ESP_OK se il processo è stato avviato correttamente.
 */
esp_err_t sd_card_format(void) {
    if (!s_mounted || !s_card) {
        snprintf(s_last_error, sizeof(s_last_error), "Errore: SD non montata");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_is_formatting) return ESP_ERR_INVALID_STATE;

    s_is_formatting = true;
    xTaskCreate(sd_format_worker_task, "sd_fmt_work", 4096, NULL, 5, NULL);
    /* the formatting timer may call printf; enlarge stack accordingly */
    xTaskCreate(sd_format_timer_task, "sd_fmt_timer", 4096, NULL, 4, NULL);
    
    return ESP_OK;
}

/**
 * @brief Legge l'intero contenuto di un file SD in memoria heap
 *
 * Allocates a buffer that must be freed by the caller.  The returned block
 * is null‑terminated; the actual byte count is stored in *out_size if non-
 * NULL.  Return NULL on any error (not mounted, cannot open, malloc failure).
 */
/*
 * @brief Legge l'intero contenuto di un file SD in memoria heap
 *
 * Allocates a buffer that must be freed by the caller.  The returned block
 * is null‑terminated; the actual byte count is stored in *out_size if non-
 * NULL.  Return NULL on any error (not mounted, cannot open, malloc failure).
 */
void *sd_card_read_file(const char *path, size_t *out_size) {
    if (!s_mounted) return NULL;
    ESP_LOGI(TAG, "*** READ SDCARD **"); 
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_size) *out_size = rd;
    return buf;
}

/**
 * @brief Elenca i file in una directory sulla scheda
 *
 * Il risultato viene scritto nel buffer fornito. Il filesystem deve essere
 * montato; altrimenti la funzione ritorna ESP_ERR_INVALID_STATE.
 *
 * @param path directory da esplorare (es. "/sdcard")
 * @param out_buf buffer di destinazione
 * @param out_size dimensione del buffer
 * @return ESP_OK se l'operazione è riuscita, altrimenti errore
 */
esp_err_t sd_card_list_dir(const char *path, char *out_buf, size_t out_size) {
    if (!s_mounted) return ESP_ERR_INVALID_STATE;
    ESP_LOGI(TAG, "*** READ SDCARDDIR **"); 
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

/**
 * @brief Scrive una stringa in un file sulla scheda
 *
 * Viene usata fopen/fprintf; in caso di fallimento con errno==ENOMEM la
 * scheda viene smontata automaticamente per risparmiare heap DMA.
 *
 * @param path percorso file (es. "/sdcard/test.txt")
 * @param data stringa da scrivere
 * @return ESP_OK se riuscito, ESP_FAIL altrimenti
 */
esp_err_t sd_card_write_file(const char *path, const char *data) {
    if (!s_mounted) return ESP_ERR_INVALID_STATE;
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        snprintf(s_last_error, sizeof(s_last_error), "Write Error: %d", errno);
        ESP_LOGE(TAG, "Impossibile aprire il file per la scrittura: %s (errno: %d, %s)", 
                 path, errno, strerror(errno));
        if (errno == ENOMEM) {
            ESP_LOGW(TAG, "Heap DMA esaurito durante fopen, smonto la SD per evitare ulteriori tentativi");
            sd_card_unmount();
        }
        return ESP_FAIL;
    }
    if (fprintf(f, "%s", data) < 0) {
        int ferr = ferror(f);
        ESP_LOGE(TAG, "Errore durante la scrittura su SD: %d", ferr);
        if (ferr == ENOMEM) {
            ESP_LOGW(TAG, "Heap DMA esaurito durante fprintf, smonto la SD per evitare ulteriori tentativi");
            fclose(f);
            sd_card_unmount();
            return ESP_FAIL;
        }
    }
    fflush(f);
    if (ferror(f)) {
        ESP_LOGE(TAG, "Errore post-flush su SD (errno=%d)", errno);
    }
    fclose(f);
    snprintf(s_last_error, sizeof(s_last_error), "Scrittura OK");
    ESP_LOGI(TAG, "File scritto con successo: %s", path);
    return ESP_OK;
}


/**
 * @brief Apre un file sulla scheda SD
 *
 * Controlla che la scheda sia montata prima di invocare fopen. In caso di
 * errore ENOMEM la scheda viene smontata per liberare heap DMA.
 *
 * @param path percorso file
 * @param mode modalit\u00e0 apertura (es. "rb", "wb", "a")
 * @return FILE* valido o NULL
 */
FILE *sd_card_fopen(const char *path, const char *mode)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "[C] sd_card_fopen: SD non montata (%s)", path);
        return NULL;
    }
    FILE *f = fopen(path, mode);
    if (!f) {
        ESP_LOGE(TAG, "[C] sd_card_fopen: impossibile aprire %s (mode=%s, errno=%d %s)",
                 path, mode, errno, strerror(errno));
        if (errno == ENOMEM) {
            ESP_LOGW(TAG, "[C] Heap DMA esaurito durante fopen, smonto la SD");
            sd_card_unmount();
        }
    }
    return f;
}

/**
 * @brief Chiude un file aperto con sd_card_fopen
 * @param f handle restituito da sd_card_fopen
 */
void sd_card_fclose(FILE *f)
{
    if (!f) return;
    fclose(f);
}

/**
 * @brief Scrive un blocco di byte su file SD
 * @param f handle file
 * @param buf dati da scrivere
 * @param size byte da scrivere
 * @return byte scritti effettivamente
 */
size_t sd_card_fwrite(FILE *f, const void *buf, size_t size)
{
    if (!f || !buf || size == 0) return 0;
    return fwrite(buf, 1, size, f);
}

/**
 * @brief Legge un blocco di byte da file SD
 * @param f handle file
 * @param buf buffer destinazione
 * @param size byte da leggere
 * @return byte letti (0 = EOF o errore)
 */
size_t sd_card_fread(FILE *f, void *buf, size_t size)
{
    if (!f || !buf || size == 0) return 0;
    return fread(buf, 1, size, f);
}

/**
 * @brief Flush + fsync su file SD
 *
 * Garantisce che i dati siano effettivamente scritti sul supporto fisico.
 * @param f handle file
 */
void sd_card_fflush(FILE *f)
{
    if (!f) return;
    fflush(f);
    int fd = fileno(f);
    if (fd >= 0) {
        fsync(fd);
    }
}

/**
 * @brief Apre una directory sulla scheda SD
 * @param path percorso directory (es. "/sdcard")
 * @return DIR* valido o NULL se SD non montata o apertura fallita
 */
DIR *sd_card_opendir(const char *path)
{
    if (!s_mounted) {
        ESP_LOGE(TAG, "[C] sd_card_opendir: SD non montata (%s)", path);
        return NULL;
    }
    return opendir(path);
}

/**
 * @brief Legge la prossima entry dalla directory SD
 * @param dir handle restituito da sd_card_opendir
 * @return puntatore a struct dirent o NULL a fine lista
 */
struct dirent *sd_card_readdir(DIR *dir)
{
    if (!dir) return NULL;
    return readdir(dir);
}

/**
 * @brief Chiude una directory aperta con sd_card_opendir
 * @param dir handle restituito da sd_card_opendir
 */
void sd_card_closedir(DIR *dir)
{
    if (!dir) return;
    closedir(dir);
}

/**
 * @brief Wrapper di stat() per path SD
 * @param path percorso file
 * @param st struttura stat da riempire
 * @return 0 se riuscito, -1 altrimenti
 */
int sd_card_stat(const char *path, struct stat *st)
{
    if (!path || !st) return -1;
    return stat(path, st);
}

#endif /* DNA_SD_CARD == 0 */

/*
 * Mockup section: if DNA_SD_CARD is defined to 1, provide fake versions of all
 * public SD APIs so the module can be linked without real hardware. The
 * replacements behave as if the card were successfully mounted, returning
 * ESP_OK and reasonable placeholder data; this allows higher‑level code to
 * exercise logic without special casing errors.
 */
#if defined(DNA_SD_CARD) && (DNA_SD_CARD == 1)

static bool s_mock_mounted = true;


/**
 * @brief Monta la scheda SD.
 * 
 * @return esp_err_t 
 * - ESP_OK: Operazione riuscita.
 * - ESP_FAIL: Operazione non riuscita.
 */
esp_err_t sd_card_mount(void) {
    s_mock_mounted = true;
    snprintf(s_last_error, sizeof(s_last_error), "mock mount");
    return ESP_OK;
}


/**
 * @brief Sfalia la scheda SD.
 * 
 * Questa funzione si occupa di smontare la scheda SD collegata al sistema.
 * 
 * @return esp_err_t
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t sd_card_unmount(void) {
    s_mock_mounted = false;
    snprintf(s_last_error, sizeof(s_last_error), "mock unmount");
    return ESP_OK;
}


/** @brief Inizializza il monitor della scheda SD.
 *  
 *  Questa funzione inizializza il monitor della scheda SD, ma non esegue alcuna operazione hardware.
 *  
 *  @return Niente.
 */
 
/** @brief Avvia il monitor della scheda SD.
 *  
 *  Questa funzione avvia il monitor della scheda SD in un ciclo infinito.
 *  
 *  @param pvParameters Puntatore ai parametri passati alla funzione.
 *  @return Niente.
 */
void sd_card_init_monitor(void) { /* no hardware, nothing to watch */ }
void sd_card_monitor_run(void *pvParameters) { while (1) { vTaskDelay(pdMS_TO_TICKS(5000)); } }


/**
 * @brief Controlla se la scheda SD è montata.
 * 
 * Questa funzione verifica lo stato della scheda SD e restituisce true se la scheda è montata, altrimenti false.
 * 
 * @return true se la scheda SD è montata, false altrimenti.
 */
bool sd_card_is_mounted(void) { return s_mock_mounted; }


/**
 * @brief Controlla se la scheda SD è presente.
 * 
 * Questa funzione verifica se la scheda SD è attualmente collegata e accessibile.
 * 
 * @return true se la scheda SD è presente, false altrimenti.
 */
bool sd_card_is_present(void) { return s_mock_mounted; }


/**
 * @brief Ottiene la dimensione totale della scheda SD.
 * 
 * @return uint64_t La dimensione totale della scheda SD in byte.
 */
uint64_t sd_card_get_total_size(void) { return s_mock_mounted ? (32UL * 1024UL) : 0; }


/**
 * @brief Ottiene la dimensione utilizzata della scheda SD.
 * 
 * @return uint64_t La dimensione utilizzata della scheda SD in byte.
 */
uint64_t sd_card_get_used_size(void) { return s_mock_mounted ? (1UL * 1024UL) : 0; }

const char* sd_card_get_last_error(void) { return "OK"; }


/**
 * @brief Formatta la scheda SD.
 * 
 * Questa funzione formatta la scheda SD collegata al dispositivo.
 * 
 * @return esp_err_t 
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
esp_err_t sd_card_format(void) {
    /* pretend format succeeds and leaves card mounted */
    s_mock_mounted = true;
    return ESP_OK;
}


/**
 * @brief Lista i file e directory nella directory specificata.
 *
 * @param [in] path Percorso della directory da elencare.
 * @param [out] out_buf Buffer in cui memorizzare i risultati della lista.
 * @param [in] out_size Dimensione del buffer di output.
 *
 * @return
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 * - ESP_ERR_INVALID_ARG: Argomento non valido.
 * - ESP_ERR_NO_MEM: Memoria insufficiente.
 * - ESP_ERR_NOT_FOUND: Directory non trovata.
 */
esp_err_t sd_card_list_dir(const char *path, char *out_buf, size_t out_size) {
    /* mockup operation:
     * pretend that the directory contains four files; this allows higher-
     * level code to see realistic output without touching real storage.
     * The caller is responsible for providing a sufficiently large buffer.
     */
    if (!s_mock_mounted) return ESP_ERR_INVALID_STATE;
    if (!out_buf || out_size == 0) return ESP_ERR_INVALID_ARG;

    int written = snprintf(out_buf, out_size,
        "Elenco file in %s:\n"
        " - mock1.txt (1234 byte)\n"
        " - mock2.log (5678 byte)\n"
        " - mock3.bin (0 byte)\n"
        " - mock4.cfg (42 byte)\n",
        path);
    return (written < 0 || (size_t)written >= out_size) ? ESP_FAIL : ESP_OK;
}

/* mock version of read_file placed at end of mock section */
void *sd_card_read_file(const char *path, size_t *out_size) {
    if (!s_mock_mounted) return NULL;
    const char *data = "MockupData\n";
    size_t len = strlen(data);
    char *buf = malloc(len + 1);
    if (buf) {
        memcpy(buf, data, len + 1);
        if (out_size) *out_size = len;
    }
    return buf;
}


/**
 * @brief Scrive i dati in un file sulla scheda SD.
 * 
 * @param [in] path Percorso del file su cui scrivere i dati.
 * @param [in] data Puntatore ai dati da scrivere.
 * @return esp_err_t Codice di errore.
 */
esp_err_t sd_card_write_file(const char *path, const char *data) {
    if (!s_mock_mounted) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}

/* Indirizzo sentinella usato come FILE* fittizio nelle mock di streaming.
 * Le funzioni sd_card_fwrite/fread/fclose/fflush riconoscono questo valore
 * e non invocano mai file I/O reale; in questo modo il compilatore non
 * genera chiamate a fclose(NULL) o accessi a strutture FILE invalide.
 */
static uint8_t s_mock_file_sentinel;

FILE *sd_card_fopen(const char *path, const char *mode)
{
    if (!s_mock_mounted) return NULL;
    ESP_LOGI(TAG, "[C] [MOCK] sd_card_fopen: %s (mode=%s) -> handle fittizio", path, mode);
    return (FILE *)&s_mock_file_sentinel;
}


/** Chiude il file aperto in modalità SD card.
 * @param [in] f Puntatore al file da chiudere.
 * @return 0 in caso di successo, -1 in caso di errore. */
void sd_card_fclose(FILE *f)
{
    (void)f; /* handle fittizio: no-op */
}


/**
 * @brief Scrive dati in un file SD.
 *
 * Questa funzione scrive un blocco di dati in un file SD aperto.
 *
 * @param [in] f Puntatore al file SD aperto in modalità scrittura.
 * @param [in] buf Puntatore ai dati da scrivere nel file.
 * @param [in] size Numero di byte da scrivere.
 * @return Numero di byte effettivamente scritti, o 0 in caso di errore.
 */
size_t sd_card_fwrite(FILE *f, const void *buf, size_t size)
{
    (void)buf;
    /* simula scrittura riuscita senza toccare hardware */
    return (f == (FILE *)&s_mock_file_sentinel) ? size : 0;
}


/**
 * @brief Legge dati dal file aperto in modalità lettura.
 *
 * @param [in] f Puntatore al file aperto in modalità lettura.
 * @param [out] buf Puntatore al buffer dove verranno memorizzati i dati letti.
 * @param [in] size Numero di byte da leggere.
 * @return Numero di byte effettivamente letti, o 0 se si verifica un errore.
 */
size_t sd_card_fread(FILE *f, void *buf, size_t size)
{
    (void)f; (void)buf; (void)size;
    return 0; /* EOF immediato in modalità mock */
}


/** @brief Svuota il buffer di scrittura del file specificato.
 *  
 *  @param [in] f Puntatore al file su cui eseguire il flush.
 *  
 *  @return Nessun valore di ritorno.
 */
void sd_card_fflush(FILE *f)
{
    (void)f; /* no-op */
}

/* --- Mock directory iterator --- */

/* Dati fittizi: stessi 4 file usati da sd_card_list_dir mockup */
static const char  *s_mock_dir_names[] = { "mock1.txt", "mock2.log", "mock3.bin", "mock4.cfg" };
static const off_t  s_mock_dir_sizes[] = { 1234,        5678,        0,            42          };
#define MOCK_DIR_COUNT 4

static int           s_mock_dir_idx = 0;
static struct dirent s_mock_dirent;

DIR *sd_card_opendir(const char *path)
{
    if (!s_mock_mounted) return NULL;
    ESP_LOGI(TAG, "[C] [MOCK] sd_card_opendir: %s", path);
    s_mock_dir_idx = 0;
    return (DIR *)&s_mock_dir_idx; /* indirizzo sentinella */
}

struct dirent *sd_card_readdir(DIR *dir)
{
    if (!dir || s_mock_dir_idx >= MOCK_DIR_COUNT) return NULL;
    memset(&s_mock_dirent, 0, sizeof(s_mock_dirent));
    s_mock_dirent.d_type = DT_REG;
    strncpy(s_mock_dirent.d_name, s_mock_dir_names[s_mock_dir_idx],
            sizeof(s_mock_dirent.d_name) - 1);
    s_mock_dir_idx++;
    return &s_mock_dirent;
}


/**
 * @brief Chiude il directory stream aperto per la lettura della directory SD card.
 *
 * @param [in] dir Puntatore al directory stream da chiudere.
 * @return void Nessun valore di ritorno.
 */
void sd_card_closedir(DIR *dir)
{
    (void)dir; /* no-op */
}


/**
 * @brief Ottiene lo stato del file system della scheda SD.
 *
 * Questa funzione restituisce informazioni sul file system della scheda SD specificata.
 *
 * @param [in] path Percorso del file system della scheda SD.
 * @param [out] st Struttura di output contenente le informazioni sul file system.
 * @return 0 in caso di successo, -1 in caso di errore.
 */
int sd_card_stat(const char *path, struct stat *st)
{
    if (!path || !st) return -1;
    /* cerca il nome file tra i mock */
    const char *fname = strrchr(path, '/');
    fname = fname ? fname + 1 : path;
    for (int i = 0; i < MOCK_DIR_COUNT; i++) {
        if (strcmp(fname, s_mock_dir_names[i]) == 0) {
            memset(st, 0, sizeof(*st));
            st->st_mode = S_IFREG | 0644;
            st->st_size = s_mock_dir_sizes[i];
            return 0;
        }
    }
    return -1; /* non trovato */
}

#endif // DNA_SD_CARD
