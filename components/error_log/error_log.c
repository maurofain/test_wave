#include "error_log.h"
#include "sd_card.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>

static const char *TAG = "ERROR_LOG";
static FILE *s_error_file = NULL;
static bool s_sd_available = false; // indica se la SD è montata con successo


/**
 * @brief Svuota il buffer del file di log.
 *
 * @param [in/out] f Puntatore al file di log da svuotare.
 * @return Nessun valore di ritorno.
 */
static void flush_log_file(FILE *f)
{
    sd_card_fflush(f);
}


// generatore nome file basato su timestamp (UTC/boot se non disponibile)

/**
 * @brief Genera un nome di file nella stringa fornita.
 *
 * @param [out] buf Puntatore alla stringa in cui memorizzare il nome del file.
 * @param len Lunghezza massima della stringa di destinazione.
 * @return void
 */
static void generate_file_name(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm tm;
    if (now == ((time_t)-1)) {
        // fallback: usa up-time
        uint64_t us = esp_timer_get_time();
        uint32_t sec = (uint32_t)(us / 1000000ULL);
        /* sec is uint32_t which may be larger than unsigned int on this platform */
        snprintf(buf, len, "/sdcard/error_%lu.log", (unsigned long)sec);
        return;
    }
    localtime_r(&now, &tm);
    snprintf(buf, len, "/sdcard/error_%04d%02d%02d-%02d%02d%02d.log",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}


/**
 * @brief Wrapper per la funzione vprintf.
 *
 * Questa funzione è un wrapper per la funzione standard vprintf, permettendo di
 * personalizzare o estendere il suo comportamento senza modificare il codice
 * originale.
 *
 * @param fmt La stringa di formato, come per la funzione vprintf.
 * @param args La lista di argomenti variabili, come per la funzione vprintf.
 * @return Il numero di caratteri stampati, o un valore negativo in caso di errore.
 */
static int vprintf_wrapper(const char *fmt, va_list args)
{
    char msg[1024];
    va_list ap;
    va_copy(ap, args);
    int len = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    int safe_len = len;
    if (safe_len < 0) {
        safe_len = 0;
    } else if ((size_t)safe_len >= sizeof(msg)) {
        safe_len = (int)sizeof(msg) - 1;
    }

    // forward to original output
    int res = vprintf(fmt, args);

    if (safe_len > 0) {
        // se l'output è un errore che inizia con "E (" crea nuovo file
        if (strncmp(msg, "E (", 3) == 0) {
            if (s_error_file) {
                flush_log_file(s_error_file);
                sd_card_fclose(s_error_file);
                s_error_file = NULL;
            }
            char fname[64];
            generate_file_name(fname, sizeof(fname));
            s_error_file = sd_card_fopen(fname, "w");
            if (s_error_file) {
                const char *header = "--- error log start ---\n";
                sd_card_fwrite(s_error_file, header, strlen(header));
                flush_log_file(s_error_file);
            }
        }
        if (s_error_file) {
            sd_card_fwrite(s_error_file, msg, (size_t)safe_len);
            flush_log_file(s_error_file);
        }
    }
    return res;
}


/**
 * @brief Inizializza il sistema di logging degli errori.
 *
 * Questa funzione inizializza il sistema di logging degli errori, preparando
 * l'ambiente per la registrazione di eventuali errori che potrebbero verificarsi
 * durante l'esecuzione del programma.
 *
 * @return Niente.
 */
void error_log_init(void)
{
#if defined(DNA_ERROR_DUMP) && (DNA_ERROR_DUMP == 1)
    ESP_LOGI(TAG, "[MOCK] error_log_init: disabilitato (DNA_ERROR_DUMP=1)");
    return;
#endif
    // provare a montare la SD; la funzione sd_card_mount gestisce se è già
    // montata o se non presente
    if (sd_card_mount() != ESP_OK) {
        ESP_LOGW(TAG, "Impossibile montare SD: nessuna operazione di log su file, userò solo remote logging");
        s_sd_available = false;
    } else {
        s_sd_available = true;
    }
    // sostituisci la funzione vprintf del logger ESP
    // il wrapper verificherà s_error_file; se SD non disponibile non verrà scritto nulla
    esp_log_set_vprintf(vprintf_wrapper);
    ESP_LOGI(TAG, "Modulo error_log inizializzato");
}


/**
 * @brief Scrive un messaggio di errore nel registro degli errori.
 *
 * @param [in] msg Il messaggio di errore da scrivere nel registro.
 * @return void Non restituisce alcun valore.
 */
void error_log_write_msg(const char *msg)
{
    if (!msg) return;
    if (!s_error_file) {
        char fname[64];
        generate_file_name(fname, sizeof(fname));
        s_error_file = sd_card_fopen(fname, "w");
        if (s_error_file) {
            const char *header = "--- error log start ---\n";
            sd_card_fwrite(s_error_file, header, strlen(header));
            flush_log_file(s_error_file);
        }
    }
    if (s_error_file) {
        sd_card_fwrite(s_error_file, msg, strlen(msg));
        flush_log_file(s_error_file);
    }
}
