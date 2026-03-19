/**
 * @file lvgl_i18n.c
 * @brief Implementazione sistema di internazionalizzazione LVGL ottimizzato
 * 
 * Implementazione completa del sistema i18n per LVGL con:
 * - Caricamento efficiente da i18n_v2.json
 * - Cache in PSRAM per tutte le lingue
 * - Lookup veloce via ID numerici
 * - Supporto multilingua completo
 * 
 * @author Test Wave Team
 * @date 26/03/2026
 */

#include "lvgl_i18n.h"
#include "esp_log.h"
#include "cJSON.h"
#include "device_config.h"
#include "esp_heap_caps.h"

// Tag per log
static const char *TAG = "LVGL_I18N";

// Tabella globale delle traduzioni LVGL
static lvgl_i18n_table_t s_i18n_table = {0};

// Mappatura lingue stringa → enum
static const struct {
    const char *code;
    lvgl_i18n_lang_t lang;
} s_lang_map[] = {
    {"it", LVGL_LANG_IT},
    {"en", LVGL_LANG_EN},
    {"fr", LVGL_LANG_FR},
    {"de", LVGL_LANG_DE},
    {"es", LVGL_LANG_ES},
    {NULL, LVGL_LANG_NONE}
};

// Mappatura lingua enum → stringa
static const char* s_lang_strings[] = {
    "it", "en", "fr", "de", "es"
};

// Forward declarations
static uint16_t lvgl_i18n_hash_key(const char *key);
static lvgl_i18n_entry_t* lvgl_i18n_find_entry(uint16_t key_id);

/**
 * @brief Carica le traduzioni LVGL da i18n_v2.json
 * 
 * Questa funzione legge il file i18n_v2.json, estrae la sezione "lvgl"
 * e crea una tabella ottimizzata in PSRAM con tutte le lingue.
 * La struttura LVGL nel JSON è: {"lvgl": {"key": {"text": {"it": "...", "en": "..."}}}}
 * 
 * @return ESP_OK se caricamento ha successo, altrimenti codice errore
 */
static esp_err_t lvgl_i18n_load_from_json(void)
{
    ESP_LOGI(TAG, "[C] Inizio caricamento traduzioni LVGL da i18n_v2.json");
    
    // Ottieni il JSON root completo dal device_config
    cJSON *root = device_config_get_i18n_v2_root();
    if (!root) {
        ESP_LOGE(TAG, "[C] Errore: impossibile ottenere root JSON da device_config");
        return ESP_FAIL;
    }
    
    // Estrai la sezione lvgl
    cJSON *lvgl_section = cJSON_GetObjectItem(root, "lvgl");
    if (!cJSON_IsObject(lvgl_section)) {
        ESP_LOGE(TAG, "[C] Errore: sezione 'lvgl' non trovata nel JSON");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Conta le entry LVGL
    size_t lvgl_count = cJSON_GetArraySize(lvgl_section);
    ESP_LOGI(TAG, "[C] Trovate %zu traduzioni LVGL nella sezione JSON", lvgl_count);
    
    if (lvgl_count == 0) {
        ESP_LOGE(TAG, "[C] Errore: nessuna traduzione LVGL trovata nella sezione");
        return ESP_ERR_NOT_FOUND;
    }
    
    if (lvgl_count > LVGL_I18N_MAX_ENTRIES) {
        ESP_LOGW(TAG, "[C] Trovate %zu traduzioni, ma il limite è %d. Verranno caricate solo le prime %d", 
                 lvgl_count, LVGL_I18N_MAX_ENTRIES, LVGL_I18N_MAX_ENTRIES);
        lvgl_count = LVGL_I18N_MAX_ENTRIES;
    }
    
    // Alloca tabella in PSRAM
    s_i18n_table.entries = (lvgl_i18n_entry_t*)heap_caps_malloc(
        lvgl_count * sizeof(lvgl_i18n_entry_t), MALLOC_CAP_SPIRAM);
    
    if (!s_i18n_table.entries) {
        ESP_LOGE(TAG, "[C] Errore: impossibile allocare %zu byte in PSRAM per la tabella i18n", 
                 lvgl_count * sizeof(lvgl_i18n_entry_t));
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "[C] Tabella LVGL i18n allocata in PSRAM: %zu byte", 
             lvgl_count * sizeof(lvgl_i18n_entry_t));
    
    // Popola la tabella
    size_t loaded_count = 0;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, lvgl_section) {
        if (loaded_count >= lvgl_count) break;
        
        if (!cJSON_IsObject(entry)) continue;
        
        // Estrai il testo per tutte le lingue
        cJSON *text_obj = cJSON_GetObjectItem(entry, "text");
        if (!cJSON_IsObject(text_obj)) continue;
        
        // Calcola key_id dal nome della chiave
        const char *key_name = entry->string;
        uint16_t key_id = lvgl_i18n_hash_key(key_name);
        
        // Popola entry
        lvgl_i18n_entry_t *lvgl_entry = &s_i18n_table.entries[loaded_count];
        lvgl_entry->key_id = key_id;
        lvgl_entry->scope_id = 2; // LVGL scope
        
        // Carica testi per tutte le lingue
        const char *lang_codes[] = {"it", "en", "fr", "de", "es"};
        for (int i = 0; i < 5; i++) {
            cJSON *lang_text = cJSON_GetObjectItem(text_obj, lang_codes[i]);
            if (cJSON_IsString(lang_text) && lang_text->valuestring) {
                strncpy(lvgl_entry->text[i], lang_text->valuestring, LVGL_I18N_MAX_TEXT_LEN - 1);
                lvgl_entry->text[i][LVGL_I18N_MAX_TEXT_LEN - 1] = '\0';
            } else {
                // Fallback all'italiano se disponibile
                if (i != 0) {
                    cJSON *fallback_text = cJSON_GetObjectItem(text_obj, "it");
                    if (cJSON_IsString(fallback_text) && fallback_text->valuestring) {
                        strncpy(lvgl_entry->text[i], fallback_text->valuestring, LVGL_I18N_MAX_TEXT_LEN - 1);
                        lvgl_entry->text[i][LVGL_I18N_MAX_TEXT_LEN - 1] = '\0';
                    } else {
                        lvgl_entry->text[i][0] = '\0';
                    }
                } else {
                    lvgl_entry->text[i][0] = '\0';
                }
            }
        }
        
        ESP_LOGD(TAG, "[C] Caricata entry: key='%s' -> id=%d, it='%s', en='%s'", 
                 key_name, key_id, lvgl_entry->text[0], lvgl_entry->text[1]);
        
        loaded_count++;
    }
    
    if (loaded_count == 0) {
        ESP_LOGE(TAG, "[C] Errore: nessuna traduzione LVGL caricata");
        heap_caps_free(s_i18n_table.entries);
        s_i18n_table.entries = NULL;
        return ESP_ERR_NOT_FOUND;
    }
    
    s_i18n_table.count = loaded_count;
    s_i18n_table.current_lang = LVGL_LANG_IT; // Default italiano
    s_i18n_table.initialized = true;
    
    ESP_LOGI(TAG, "[C] ✅ Sistema i18n LVGL inizializzato: %zu traduzioni caricate", loaded_count);
    return ESP_OK;
}

/**
 * @brief Funzione di lookup binario ottimizzato
 * 
 * Cerca una entry per key_id usando ricerca binaria (se la tabella è ordinata)
 * o lineare come fallback.
 * 
 * @param key_id ID numerico della chiave cercata
 * @return Puntatore all'entry trovata o NULL
 */
static lvgl_i18n_entry_t* lvgl_i18n_find_entry(uint16_t key_id)
{
    if (!s_i18n_table.initialized || !s_i18n_table.entries) {
        return NULL;
    }
    
    // Per ora ricerca lineare - potremmo ottimizzare con binaria se ordiniamo per key_id
    for (size_t i = 0; i < s_i18n_table.count; i++) {
        if (s_i18n_table.entries[i].key_id == key_id) {
            return &s_i18n_table.entries[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Calcola hash di una stringa chiave per lookup veloce
 * 
 * @param key Stringa chiave
 * @return Hash numerico della chiave
 */
static uint16_t lvgl_i18n_hash_key(const char *key)
{
    uint16_t hash = 0;
    if (strncmp(key, "lvgl.", 6) == 0) {
        // Formato "lvgl.xxx" - estrai xxx come ID
        return (uint16_t)atoi(key + 6);
    }
    
    // Hash semplice per altri formati
    for (const char *p = key; *p; p++) {
        hash = (hash * 31) + *p;
    }
    return hash;
}

// ============================================================================
// IMPLEMENTAZIONE API PUBBLICA
// ============================================================================

esp_err_t lvgl_i18n_init(void)
{
    ESP_LOGI(TAG, "[C] Inizializzazione sistema i18n LVGL");
    
    // Resetta stato
    memset(&s_i18n_table, 0, sizeof(s_i18n_table));
    
    // Carica traduzioni da JSON
    esp_err_t ret = lvgl_i18n_load_from_json();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[C] Errore caricamento traduzioni: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "[C] ✅ Sistema i18n LVGL inizializzato con successo");
    return ESP_OK;
}

esp_err_t lvgl_i18n_set_language(lvgl_i18n_lang_t lang)
{
    if (!s_i18n_table.initialized) {
        ESP_LOGE(TAG, "[C] Errore: sistema i18n non inizializzato");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (lang >= 5) {
        ESP_LOGE(TAG, "[C] Errore: lingua %d non valida", lang);
        return ESP_ERR_INVALID_ARG;
    }
    
    lvgl_i18n_lang_t old_lang = s_i18n_table.current_lang;
    s_i18n_table.current_lang = lang;
    
    ESP_LOGI(TAG, "[C] Lingua LVGL cambiata: %s -> %s", 
             s_lang_strings[old_lang], s_lang_strings[lang]);
    
    return ESP_OK;
}

lvgl_i18n_lang_t lvgl_i18n_get_language(void)
{
    return s_i18n_table.initialized ? s_i18n_table.current_lang : LVGL_LANG_IT;
}

lvgl_i18n_lang_t lvgl_i18n_lang_from_string(const char *lang_str)
{
    if (!lang_str) return LVGL_LANG_NONE;
    
    for (int i = 0; s_lang_map[i].code; i++) {
        if (strcmp(s_lang_map[i].code, lang_str) == 0) {
            return s_lang_map[i].lang;
        }
    }
    
    return LVGL_LANG_NONE;
}

const char* lvgl_i18n_lang_to_string(lvgl_i18n_lang_t lang)
{
    if (lang >= 5) return NULL;
    return s_lang_strings[lang];
}

esp_err_t lvgl_i18n_get_text(const char *key, const char *fallback, char *out, size_t out_len)
{
    if (!key || !out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_i18n_table.initialized) {
        ESP_LOGW(TAG, "[C] Sistema i18n non inizializzato, uso fallback");
        if (fallback) {
            strncpy(out, fallback, out_len - 1);
            out[out_len - 1] = '\0';
        } else {
            out[0] = '\0';
        }
        return ESP_ERR_INVALID_STATE;
    }
    
    // Calcola hash della chiave
    uint16_t key_id = lvgl_i18n_hash_key(key);
    
    // Usa versione ottimizzata
    return lvgl_i18n_get_text_by_id(key_id, fallback, out, out_len);
}

esp_err_t lvgl_i18n_get_text_by_id(uint16_t key_id, const char *fallback, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!s_i18n_table.initialized) {
        ESP_LOGW(TAG, "[C] Sistema i18n non inizializzato, uso fallback");
        if (fallback) {
            strncpy(out, fallback, out_len - 1);
            out[out_len - 1] = '\0';
        } else {
            out[0] = '\0';
        }
        return ESP_ERR_INVALID_STATE;
    }
    
    // Cerca entry
    lvgl_i18n_entry_t *entry = lvgl_i18n_find_entry(key_id);
    
    if (entry && s_i18n_table.current_lang < 5) {
        // Testo trovato nella lingua corrente
        const char *text = entry->text[s_i18n_table.current_lang];
        if (text && text[0]) {
            strncpy(out, text, out_len - 1);
            out[out_len - 1] = '\0';
            ESP_LOGD(TAG, "[C] Hit i18n: key=%d -> '%s'", key_id, out);
            return ESP_OK;
        }
    }
    
    // Fallback all'italiano se disponibile
    if (entry && entry->text[LVGL_LANG_IT][0]) {
        strncpy(out, entry->text[LVGL_LANG_IT], out_len - 1);
        out[out_len - 1] = '\0';
        ESP_LOGD(TAG, "[C] Fallback i18n IT: key=%d -> '%s'", key_id, out);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Fallback finale al parametro
    if (fallback) {
        strncpy(out, fallback, out_len - 1);
        out[out_len - 1] = '\0';
        ESP_LOGD(TAG, "[C] Fallback parametro: key=%d -> '%s'", key_id, out);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Nessun fallback disponibile
    out[0] = '\0';
    ESP_LOGW(TAG, "[C] Nessun testo trovato per key=%d", key_id);
    return ESP_ERR_NOT_FOUND;
}

bool lvgl_i18n_has_key(const char *key)
{
    if (!key || !s_i18n_table.initialized) {
        return false;
    }
    
    uint16_t key_id = lvgl_i18n_hash_key(key);
    return lvgl_i18n_find_entry(key_id) != NULL;
}

size_t lvgl_i18n_get_count(void)
{
    return s_i18n_table.initialized ? s_i18n_table.count : 0;
}

void lvgl_i18n_cleanup(void)
{
    if (s_i18n_table.entries) {
        heap_caps_free(s_i18n_table.entries);
        s_i18n_table.entries = NULL;
    }
    
    memset(&s_i18n_table, 0, sizeof(s_i18n_table));
    ESP_LOGI(TAG, "[C] Sistema i18n LVGL deallocato");
}

esp_err_t lvgl_i18n_reload(void)
{
    ESP_LOGI(TAG, "[C] Ricaricamento sistema i18n LVGL");
    
    // Cleanup esistente
    lvgl_i18n_cleanup();
    
    // Re-inizializza
    return lvgl_i18n_init();
}
