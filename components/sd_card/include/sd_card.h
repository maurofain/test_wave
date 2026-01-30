#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Inizializza e monta la scheda SD
 * @return ESP_OK se riuscito
 */
esp_err_t sd_card_mount(void);

/**
 * @brief Restituisce l'ultimo errore registrato
 */
const char* sd_card_get_last_error(void);

/**
 * @brief Inizializza il monitor hot-plug della SD
 */
void sd_card_init_monitor(void);

/**
 * @brief Smonta la scheda SD
 * @return ESP_OK se riuscito
 */
esp_err_t sd_card_unmount(void);

/**
 * @brief Ritorna true se la scheda SD è montata
 * @return true se montata
 */
bool sd_card_is_mounted(void);

/**
 * @brief Ritorna true se la scheda SD è fisicamente presente
 * @return true se inserita
 */
bool sd_card_is_present(void);

/**
 * @brief Ritorna lo spazio totale in KB
 * @return Spazio totale
 */
uint64_t sd_card_get_total_size(void);

/**
 * @brief Ritorna lo spazio usato in KB
 * @return Spazio usato
 */
uint64_t sd_card_get_used_size(void);

/**
 * @brief Formatta la scheda SD in FAT32
 * @return ESP_OK se riuscito
 */
esp_err_t sd_card_format(void);

/**
 * @brief Elenca i file in una directory e scrive il risultato in un buffer
 * @param path Percorso directory
 * @param out_buf Buffer di destinazione
 * @param out_size Dimensione buffer
 * @return ESP_OK se riuscito
 */
esp_err_t sd_card_list_dir(const char *path, char *out_buf, size_t out_size);

/**
 * @brief Scrive una stringa in un file sulla scheda SD
 * @param path Percorso file (es. /sdcard/test.txt)
 * @param data Stringa da scrivere
 * @return ESP_OK se riuscito
 */
esp_err_t sd_card_write_file(const char *path, const char *data);
