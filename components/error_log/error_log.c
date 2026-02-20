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

static void flush_log_file(FILE *f)
{
    if (!f) {
        return;
    }
    fflush(f);
    int fd = fileno(f);
    if (fd >= 0) {
        fsync(fd);
    }
}


// generatore nome file basato su timestamp (UTC/boot se non disponibile)
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
                fclose(s_error_file);
                s_error_file = NULL;
            }
            char fname[64];
            generate_file_name(fname, sizeof(fname));
            s_error_file = fopen(fname, "w");
            if (s_error_file) {
                fprintf(s_error_file, "--- error log start ---\n");
                flush_log_file(s_error_file);
            }
        }
        if (s_error_file) {
            fwrite(msg, 1, (size_t)safe_len, s_error_file);
            flush_log_file(s_error_file);
        }
    }
    return res;
}

void error_log_init(void)
{
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

void error_log_write_msg(const char *msg)
{
    if (!msg) return;
    if (!s_error_file) {
        char fname[64];
        generate_file_name(fname, sizeof(fname));
        s_error_file = fopen(fname, "w");
        if (s_error_file) {
            fprintf(s_error_file, "--- error log start ---\n");
            flush_log_file(s_error_file);
        }
    }
    if (s_error_file) {
        fputs(msg, s_error_file);
        flush_log_file(s_error_file);
    }
}
