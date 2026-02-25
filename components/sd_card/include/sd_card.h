#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_err.h"

/**
 * @brief Inizializza e monta la scheda SD (FAT)
 *
 * La funzione esegue il power‑cycle della linea di alimentazione, configura
 * l'interfaccia SDMMC e monta il filesystem. Per migliorare l'affidabilità
 * su sistemi con heap DMA frammentato viene utilizzata una unità di
 * allocazione ridotta (4 KiB). Se il montaggio o la formattazione falliscono
 * la routine mantiene il flag `s_mounted` a false e ritorna l'errore.
 *
 * Nota: in caso di scritture successive che falliscono con ENOMEM il driver
 * smonterà automaticamente la scheda per evitare tentativi ripetuti.
 *
 * @return ESP_OK se riuscito, altrimenti codice esp_err_t
 */
esp_err_t sd_card_mount(void);

/**
 * @brief Restituisce l'ultimo errore registrato dal modulo SD
 *
 * Questa stringa è aggiornata dalle varie routine interne (mount, write,
 * format, ecc.) e può contenere il nome dell'errore esp_err_t o il valore di
 * errno in caso di operazioni su file. Utile per diagnosticare fallimenti
 * successivi al montaggio.
 *
 * @return puntatore a stringa statica; non va liberato.
 */
const char* sd_card_get_last_error(void);

/**
 * @brief Avvia il task di monitoraggio hot‑plug della SD
 *
 * Il task configura un GPIO per la rilevazione della presenza della scheda
 * e segnala inserimenti/rimozioni. Viene creato solo una volta anche se la
 * funzione è richiamata più volte.
 *
 * Nota: il monitor è separato dal montaggio; chiamare comunque
 * sd_card_mount() per accedere al filesystem.
 */
void sd_card_init_monitor(void);

/**
 * @brief Smonta la scheda SD e libera le risorse
 *
 * Svuota eventuali cache e chiude il filesystem FAT. In caso di errori non
 * critici viene comunque aggiornato lo stato interno a "non montata" per
 * evitare ulteriori operazioni invalidanti.
 *
 * @return ESP_OK se la smontatura è stata completata, altrimenti un codice
 *         esp_err_t di errore.
 */
esp_err_t sd_card_unmount(void);

/**
 * @brief Verifica se il filesystem SD è attualmente montato
 *
 * Questa funzione può essere utilizzata prima di qualsiasi operazione su file
 * per evitare errori. Lo stato viene aggiornato da sd_card_mount() e
 * sd_card_unmount() (o internamente da errori critici).
 *
 * @return true se montata, false altrimenti
 */
bool sd_card_is_mounted(void);

/**
 * @brief Controlla il pin "card detect" per sapere se c'è una SD inserita
 *
 * Non indica lo stato del filesystem; anche se la funzione restituisce true
 * potrebbe essere necessario un montaggio manuale per accedere ai file.
 *
 * @return true se il pin di card-detect indica presenza (livello basso)
 */
bool sd_card_is_present(void);

/**
 * @brief Calcola la capacità totale della scheda (in kilobyte)
 *
 * Deve essere invocata solo a scheda montata; altrimenti restituisce 0.
 * Viene determinata leggendo i parametri della struttura sdmmc_card_t
 * fornita da esp_vfs_fat_sdmmc_mount.
 *
 * @return quantità totale di spazio disponibile (KB) oppure 0 se non montata
 */
uint64_t sd_card_get_total_size(void);

/**
 * @brief Restituisce lo spazio occupato sulla scheda (in kilobyte)
 *
 * La funzione è valida solo quando il filesystem è montato; ritorna 0 se
 * non è presente una scheda o se non è montata.
 *
 * @return spazio utilizzato (KB) oppure 0 in caso di errore
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
 *
 * Questa routine utilizza fopen/fprintf sull'albero montato. Se l'apertura
 * o la scrittura falliscono con errno==ENOMEM significa che non è più
 * disponibile heap DMA sufficiente per sostenere l'operazione; in quel caso
 * la scheda viene smontata automaticamente e la funzione ritorna ESP_FAIL.
 *
 * @param path Percorso file (es. /sdcard/test.txt)
 * @param data Stringa da scrivere
 * @return ESP_OK se riuscito, ESP_FAIL altrimenti
 */
void *sd_card_read_file(const char *path, size_t *out_size);

/**
 * @brief Scrive una stringa in un file sulla scheda SD
 *
 * Viene usata fopen/fprintf; in caso di fallimento con errno==ENOMEM la
 * scheda viene smontata automaticamente per risparmiare heap DMA.
 *
 * @param path percorso file (es. "/sdcard/test.txt")
 * @param data stringa da scrivere
 * @return ESP_OK se riuscito, ESP_FAIL altrimenti
 */
esp_err_t sd_card_write_file(const char *path, const char *data);

/**
 * @brief Apre un file sulla scheda SD
 *
 * Wrapper di fopen limitato ai path SD. In modalità mockup (DNA_SD_CARD=1)
 * restituisce un handle fittizio senza toccare hardware.
 *
 * @param path percorso file (es. "/sdcard/log.txt")
 * @param mode modalità ("r", "w", "wb", "a", ...)
 * @return FILE* valido o NULL in caso di errore / SD non montata
 */
FILE *sd_card_fopen(const char *path, const char *mode);

/**
 * @brief Chiude un file aperto con sd_card_fopen
 * @param f handle restituito da sd_card_fopen
 */
void sd_card_fclose(FILE *f);

/**
 * @brief Scrive dati binari su file SD
 * @param f handle restituito da sd_card_fopen
 * @param buf dati da scrivere
 * @param size numero di byte
 * @return byte scritti effettivamente
 */
size_t sd_card_fwrite(FILE *f, const void *buf, size_t size);

/**
 * @brief Legge dati da file SD
 * @param f handle restituito da sd_card_fopen
 * @param buf buffer destinazione
 * @param size byte da leggere
 * @return byte letti (0 = EOF o errore)
 */
size_t sd_card_fread(FILE *f, void *buf, size_t size);

/**
 * @brief Forza flush + fsync su file SD
 *
 * Garantisce che i dati siano scritti sul supporto fisico.
 * In modalità mockup è un no-op.
 *
 * @param f handle restituito da sd_card_fopen
 */
void sd_card_fflush(FILE *f);

/**
 * @brief Apre una directory sulla scheda SD
 *
 * Wrapper di opendir limitato ai path SD. In modalità mockup (DNA_SD_CARD=1)
 * restituisce un iteratore fittizio che cicla su 4 file di esempio.
 *
 * @param path percorso directory (es. "/sdcard")
 * @return DIR* valido o NULL in caso di errore
 */
DIR *sd_card_opendir(const char *path);

/**
 * @brief Legge la prossima entry dalla directory SD
 * @param dir handle restituito da sd_card_opendir
 * @return puntatore a struct dirent o NULL a fine lista
 */
struct dirent *sd_card_readdir(DIR *dir);

/**
 * @brief Chiude una directory aperta con sd_card_opendir
 * @param dir handle restituito da sd_card_opendir
 */
void sd_card_closedir(DIR *dir);

/**
 * @brief Ottiene informazioni su un file SD (dimensione, tipo, ...)
 *
 * Wrapper di stat() per path SD. In modalità mockup restituisce metadati
 * fittizi per i 4 file di esempio; per qualunque altro nome ritorna -1.
 *
 * @param path percorso file completo (es. "/sdcard/foo.txt")
 * @param st struttura stat da riempire
 * @return 0 se riuscito, -1 altrimenti
 */
int sd_card_stat(const char *path, struct stat *st);
