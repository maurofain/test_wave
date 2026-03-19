/**
 * @file lvgl_i18n.h
 * @brief Sistema di internazionalizzazione LVGL ottimizzato
 * 
 * Questo modulo gestisce il caricamento e lookup delle traduzioni per LVGL
 * in modo efficiente con cache in PSRAM e accesso rapido via ID numerici.
 * 
 * @author Test Wave Team
 * @date 26/03/2026
 */

#ifndef LVGL_I18N_H
#define LVGL_I18N_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Massimo numero di traduzioni LVGL supportate
 */
#define LVGL_I18N_MAX_ENTRIES 200

/**
 * @brief Massimo lunghezza testo tradotto (incluso terminatore)
 */
#define LVGL_I18N_MAX_TEXT_LEN 128

/**
 * @brief Numero di lingue supportate
 */
#define LVGL_I18N_LANG_COUNT 5

/**
 * @brief Codici lingue supportati
 */
typedef enum {
    LVGL_LANG_IT = 0,  ///< Italiano (default)
    LVGL_LANG_EN = 1,  ///< Inglese
    LVGL_LANG_FR = 2,  ///< Francese
    LVGL_LANG_DE = 3,  ///< Tedesco
    LVGL_LANG_ES = 4,  ///< Spagnolo
    LVGL_LANG_NONE = 255 ///< Lingua non valida
} lvgl_i18n_lang_t;

/**
 * @brief Struttura per una singola traduzione LVGL
 */
typedef struct {
    uint16_t key_id;                              ///< ID numerico della chiave
    uint8_t scope_id;                             ///< ID numerico dello scope (sempre 1 per lvgl)
    char text[LVGL_I18N_LANG_COUNT][LVGL_I18N_MAX_TEXT_LEN]; ///< Testi per tutte le lingue
} lvgl_i18n_entry_t;

/**
 * @brief Tabella delle traduzioni LVGL in PSRAM
 */
typedef struct {
    lvgl_i18n_entry_t *entries;                   ///< Array delle traduzioni
    size_t count;                                 ///< Numero di traduzioni caricate
    lvgl_i18n_lang_t current_lang;                ///< Lingua corrente attiva
    bool initialized;                             ///< Flag di inizializzazione
} lvgl_i18n_table_t;

/**
 * @brief Inizializza il sistema i18n LVGL
 * 
 * Carica tutte le traduzioni LVGL da i18n_v2.json e le memorizza
 * in PSRAM per accesso rapido.
 * 
 * @return ESP_OK se l'inizializzazione ha successo, altrimenti codice errore
 */
esp_err_t lvgl_i18n_init(void);

/**
 * @brief Imposta la lingua corrente per LVGL
 * 
 * @param lang Codice lingua da impostare
 * @return ESP_OK se la lingua è valida e impostata correttamente
 */
esp_err_t lvgl_i18n_set_language(lvgl_i18n_lang_t lang);

/**
 * @brief Ottiene la lingua corrente di LVGL
 * 
 * @return Codice lingua corrente
 */
lvgl_i18n_lang_t lvgl_i18n_get_language(void);

/**
 * @brief Converte codice lingua da stringa a enum
 * 
 * @param lang_str Stringa lingua ("it", "en", "fr", "de", "es")
 * @return Codice lingua corrispondente o LVGL_LANG_NONE se non valida
 */
lvgl_i18n_lang_t lvgl_i18n_lang_from_string(const char *lang_str);

/**
 * @brief Converte codice lingua da enum a stringa
 * 
 * @param lang Codice lingua
 * @return Stringa lingua corrispondente o NULL se non valida
 */
const char* lvgl_i18n_lang_to_string(lvgl_i18n_lang_t lang);

/**
 * @brief Ottiene testo tradotto per chiave e lingua corrente
 * 
 * Funzione principale di lookup per LVGL. Cerca il testo nella lingua
 * corrente attiva, con fallback automatico all'italiano se non disponibile.
 * 
 * @param key Stringa chiave (es: "credit_label", "program_name_01")
 * @param fallback Testo fallback da usare se chiave non trovata
 * @param out Buffer output dove scrivere il testo tradotto
 * @param out_len Dimensione buffer output
 * @return ESP_OK se testo trovato, ESP_ERR_NOT_FOUND se usa fallback
 */
esp_err_t lvgl_i18n_get_text(const char *key, const char *fallback, char *out, size_t out_len);

/**
 * @brief Ottiene testo tradotto per ID numerico (versione veloce)
 * 
 * Versione ottimizzata che usa ID numerici pre-calcolati per lookup
 * ultra-veloce. Da usare in contesti performance-critical.
 * 
 * @param key_id ID numerico chiave
 * @param fallback Testo fallback da usare se chiave non trovata
 * @param out Buffer output dove scrivere il testo tradotto
 * @param out_len Dimensione buffer output
 * @return ESP_OK se testo trovato, ESP_ERR_NOT_FOUND se usa fallback
 */
esp_err_t lvgl_i18n_get_text_by_id(uint16_t key_id, const char *fallback, char *out, size_t out_len);

/**
 * @brief Verifica se una chiave esiste nel dizionario
 * 
 * @param key Stringa chiave da verificare
 * @return true se la chiave esiste, false altrimenti
 */
bool lvgl_i18n_has_key(const char *key);

/**
 * @brief Ottiene il numero totale di traduzioni caricate
 * 
 * @return Numero di entry nella tabella i18n
 */
size_t lvgl_i18n_get_count(void);

/**
 * @brief Libera la memoria occupata dalla tabella i18n
 */
void lvgl_i18n_cleanup(void);

/**
 * @brief Ricarica le traduzioni da file (per debug/testing)
 * 
 * @return ESP_OK se ricarica ha successo
 */
esp_err_t lvgl_i18n_reload(void);

#ifdef __cplusplus
}
#endif

#endif // LVGL_I18N_H
