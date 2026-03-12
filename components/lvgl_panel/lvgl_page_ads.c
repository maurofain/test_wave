#include "lvgl_panel_pages.h"
#include "lvgl_page_chrome.h"
#include "lvgl.h"
#include "esp_log.h"
#include "device_config.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bsp/esp32_p4_nano.h"
#include "esp_heap_caps.h"

static const char *TAG = "lvgl_page_ads";

/* -----------------------------------------------------------------------
 * Layout schermo (720 × 1280 px):
 *   chrome        : top 0..80  (ora + puntini + bandiera, da lvgl_page_chrome_add)
 *   titolo locale : y=84  h=62
 *   [ad container]: y=154 w=692 h=904  ← slideshow con angoli arrotondati
 *   [btn seleziona]: y=1070 w=640 h=110  (pill verde acqua)
 *   [barra errore]: y=1192 w=692 h=68   (berry scuro)
 * -----------------------------------------------------------------------
 * DIMENSIONI IMMAGINI PUBBLICITARIE
 *   Risoluzione consigliata : 692 × 904 px  (proporzione ≈ 3:4)
 *   Formato                 : JPEG
 *   Nomi file               : img01.jpg … img99.jpg  (cartella SPIFFS)
 *   Le immagini vengono scalate in modalità COVER: riempiono l'area
 *   mantenendo le proporzioni, ritagliando i margini eccedenti.
 * ----------------------------------------------------------------------- */

#define PANEL_W         720
#define PANEL_H         1280
#define PANEL_PAD_X     14
#define PANEL_PAD_Y     14

/* Ad image container */
#define AD_X            PANEL_PAD_X
#define AD_Y            154
#define AD_W            (PANEL_W - 2 * PANEL_PAD_X)   /* 692 */
#define AD_H            904
#define AD_RADIUS       36

/* "Seleziona lavaggio" button */
#define BTN_SEL_W       640
#define BTN_SEL_H       110
#define BTN_SEL_Y       1070

/* Barra errore */
#define ERR_Y           1192
#define ERR_H           68
#define ERR_W           (PANEL_W - 2 * PANEL_PAD_X)

/* Colori */
#define COL_BG          lv_color_make(0x00, 0x00, 0x00)
#define COL_WHITE       lv_color_make(0xEE, 0xEE, 0xEE)
#define COL_SEL_BTN     lv_color_make(0x2A, 0x9D, 0x8F)   /* Verde acqua */
#define COL_ERR_BG      lv_color_make(0x7B, 0x1F, 0x52)   /* Berry scuro */

#define ADS_SPIFFS_DIR   "/spiffs"
#define ADS_SPIFFS_PATH  "/spiffs/"   /* percorso POSIX per fopen */
#define ADS_MEM_PREFIX   "M:/"        /* driver LVGL in-memory (lettera 'M') */

/* -----------------------------------------------------------------------
 * PSRAM image buffer + custom LVGL FS driver lettera 'M'
 *
 * Tutte le immagini img*.jpg vengono lette da SPIFFS in PSRAM una-volta
 * sola durante il preload (fuori dal lock LVGL).
 * Lo slideshow legge poi direttamente dalla PSRAM: zero accessi SPIFFS.
 * ----------------------------------------------------------------------- */

/** Buffer PSRAM per una singola immagine JPEG. */
typedef struct {
    uint8_t *data;   /**< byte JPEG in PSRAM (liberare con heap_caps_free) */
    size_t   size;   /**< dimensione in byte */
} ad_psram_buf_t;

/** Contesto di un file aperto tramite il driver MemDrv 'M'. */
typedef struct {
    uint32_t img_idx; /**< indice in s_ad_psram[] */
    uint32_t pos;     /**< posizione di lettura corrente */
} memdrv_file_t;

/* ----------------------------------------------------------------------- */

/* Stato pagina */
static lv_obj_t      *s_ad_scr           = NULL;
static lv_obj_t      *s_ad_img           = NULL;
static lv_obj_t      *s_img_container    = NULL;
static lv_obj_t      *s_error_lbl        = NULL;
static lv_timer_t    *s_ad_carousel_timer = NULL;
static bool           s_ad_exit_pending  = false;

/* Gestione immagini */
static char         **s_ad_images        = NULL;   /**< nomi file originali (solo per log) */
static ad_psram_buf_t *s_ad_psram        = NULL;   /**< buffer JPEG in PSRAM */
static uint32_t       s_ad_image_count   = 0;
static uint32_t       s_ad_current_image = 0;
static uint32_t       s_ad_rotation_ms   = 30000;  /* Default 30 secondi */

/* Driver FS in-memory (lettera 'M') */
static lv_fs_drv_t    s_memdrv;
static bool           s_memdrv_registered = false;

/* Messaggio errore corrente (aggiornabile dall'esterno) */
static char           s_error_msg[128]   = "";

/* ----------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * Driver FS in-memory  –  lettera 'M'
 *
 * Il driver mappa i path "M:/N.jpg" (N = indice decimale 0…count-1)
 * sui buffer PSRAM in s_ad_psram[].
 * Viene registrato una volta sola (s_memdrv_registered) e deve essere
 * chiamato col lock LVGL acquisito.
 * ----------------------------------------------------------------------- */

static void *memdrv_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    (void)drv; (void)mode;
    /* path = "/N.jpg"  (LVGL toglie la lettera e ':') */
    int idx = atoi(path + 1);   /* "/0.jpg" → 0 */
    if (idx < 0 || (uint32_t)idx >= s_ad_image_count) return NULL;
    if (!s_ad_psram || !s_ad_psram[idx].data)         return NULL;

    memdrv_file_t *f = malloc(sizeof(memdrv_file_t));
    if (!f) return NULL;
    f->img_idx = (uint32_t)idx;
    f->pos     = 0;
    return f;
}

static lv_fs_res_t memdrv_close(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    free(file_p);
    return LV_FS_RES_OK;
}

static lv_fs_res_t memdrv_read(lv_fs_drv_t *drv, void *file_p,
                                void *buf, uint32_t btr, uint32_t *br)
{
    (void)drv;
    memdrv_file_t       *f   = file_p;
    const ad_psram_buf_t *pb = &s_ad_psram[f->img_idx];
    size_t avail = (f->pos < pb->size) ? (pb->size - f->pos) : 0;
    *br = (uint32_t)((btr < avail) ? btr : avail);
    if (*br) {
        memcpy(buf, pb->data + f->pos, *br);
        f->pos += *br;
    }
    return LV_FS_RES_OK;
}

static lv_fs_res_t memdrv_seek(lv_fs_drv_t *drv, void *file_p,
                                uint32_t pos, lv_fs_whence_t whence)
{
    (void)drv;
    memdrv_file_t       *f   = file_p;
    const ad_psram_buf_t *pb = &s_ad_psram[f->img_idx];
    switch (whence) {
        case LV_FS_SEEK_SET: f->pos = pos; break;
        case LV_FS_SEEK_CUR: f->pos += pos; break;
        case LV_FS_SEEK_END:
            f->pos = (pos <= pb->size) ? (uint32_t)(pb->size - pos) : 0;
            break;
        default: break;
    }
    if (f->pos > (uint32_t)pb->size) f->pos = (uint32_t)pb->size;
    return LV_FS_RES_OK;
}

static lv_fs_res_t memdrv_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    (void)drv;
    *pos_p = ((memdrv_file_t *)file_p)->pos;
    return LV_FS_RES_OK;
}

/**
 * @brief Registra il driver FS 'M' in LVGL. Idempotente.
 *        Deve essere chiamato con il lock LVGL acquisito.
 */
static void register_psram_fs(void)
{
    if (s_memdrv_registered) return;
    lv_fs_drv_init(&s_memdrv);
    s_memdrv.letter   = 'M';
    s_memdrv.open_cb  = memdrv_open;
    s_memdrv.close_cb = memdrv_close;
    s_memdrv.read_cb  = memdrv_read;
    s_memdrv.seek_cb  = memdrv_seek;
    s_memdrv.tell_cb  = memdrv_tell;
    lv_fs_drv_register(&s_memdrv);
    s_memdrv_registered = true;
    ESP_LOGI(TAG, "[M] Driver FS PSRAM 'M' registrato");
}

/* ----------------------------------------------------------------------- */

/**
 * @brief Carica dall'SPIFFS la lista di immagini img01.jpg … img99.jpg
 *        e le copia interamente in buffer PSRAM.
 *
 * Deve essere chiamata SENZA il lock LVGL (operazioni I/O bloccanti).
 */
static void load_ad_images(void)
{
    /* --- Cleanup eventuale caricamento precedente --- */
    if (s_ad_images) {
        for (uint32_t i = 0; i < s_ad_image_count; i++) free(s_ad_images[i]);
        free(s_ad_images);
        s_ad_images = NULL;
    }
    if (s_ad_psram) {
        for (uint32_t i = 0; i < s_ad_image_count; i++) {
            heap_caps_free(s_ad_psram[i].data);
        }
        free(s_ad_psram);
        s_ad_psram = NULL;
    }
    s_ad_image_count   = 0;
    s_ad_current_image = 0;

    /* --- Prima passata: conta img*.jpg --- */
    DIR *dir = opendir(ADS_SPIFFS_DIR);
    if (!dir) {
        ESP_LOGW(TAG, "[L] Impossibile aprire " ADS_SPIFFS_DIR);
        return;
    }
    struct dirent *ent;
    uint32_t count = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_type == DT_REG &&
            strncmp(ent->d_name, "img", 3) == 0 &&
            strlen(ent->d_name) >= strlen("img01.jpg") &&
            strcmp(ent->d_name + strlen(ent->d_name) - 4, ".jpg") == 0) {
            count++;
        }
    }
    if (count == 0) {
        ESP_LOGW(TAG, "[L] Nessuna immagine img*.jpg in " ADS_SPIFFS_DIR);
        closedir(dir);
        return;
    }

    /* --- Alloca array indice (nomi file + buffer PSRAM) --- */
    s_ad_images = malloc(count * sizeof(char *));
    s_ad_psram  = malloc(count * sizeof(ad_psram_buf_t));
    if (!s_ad_images || !s_ad_psram) {
        ESP_LOGE(TAG, "[L] OOM per array immagini");
        free(s_ad_images); s_ad_images = NULL;
        free(s_ad_psram);  s_ad_psram  = NULL;
        closedir(dir);
        return;
    }
    memset(s_ad_psram, 0, count * sizeof(ad_psram_buf_t));

    /* --- Seconda passata: carica ogni file in PSRAM --- */
    rewinddir(dir);
    uint32_t idx = 0;
    while ((ent = readdir(dir)) != NULL && idx < count) {
        if (ent->d_type != DT_REG ||
            strncmp(ent->d_name, "img", 3) != 0 ||
            strlen(ent->d_name) < strlen("img01.jpg") ||
            strcmp(ent->d_name + strlen(ent->d_name) - 4, ".jpg") != 0) {
            continue;
        }

        /* Salva nome file per log */
        s_ad_images[idx] = strdup(ent->d_name);

        /* Apri file, determina dimensione, leggi in PSRAM */
        char fpath[64];
        snprintf(fpath, sizeof(fpath), ADS_SPIFFS_PATH "%s", ent->d_name);
        FILE *fp = fopen(fpath, "rb");
        if (!fp) {
            ESP_LOGW(TAG, "[L] fopen fallito: %s", fpath);
            idx++; continue;
        }
        fseek(fp, 0, SEEK_END);
        long fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (fsize <= 0) {
            ESP_LOGW(TAG, "[L] File vuoto: %s", fpath);
            fclose(fp); idx++; continue;
        }

        uint8_t *buf = heap_caps_malloc((size_t)fsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!buf) {
            ESP_LOGE(TAG, "[L] OOM PSRAM per %s (%ld B)", ent->d_name, fsize);
            fclose(fp); idx++; continue;
        }
        size_t rd = fread(buf, 1, (size_t)fsize, fp);
        fclose(fp);
        if (rd != (size_t)fsize) {
            ESP_LOGW(TAG, "[L] Lettura parziale %s: %u/%ld B", ent->d_name, (unsigned)rd, fsize);
            heap_caps_free(buf);
            idx++; continue;
        }

        s_ad_psram[idx].data = buf;
        s_ad_psram[idx].size = (size_t)fsize;
        ESP_LOGI(TAG, "[L] Caricata in PSRAM: %s (%ld B)", ent->d_name, fsize);
        idx++;
    }
    closedir(dir);
    s_ad_image_count = idx;

    ESP_LOGI(TAG, "[L] %u immagini caricate in PSRAM", s_ad_image_count);
}

/* ----------------------------------------------------------------------- */

/**
 * @brief Mostra l'immagine corrente nel container dal buffer PSRAM.
 *        Usa il path "M:/N.jpg" servito dal driver FS in-memory.
 */
static void display_current_ad_image(void)
{
    if (s_ad_image_count == 0 || s_ad_current_image >= s_ad_image_count) {
        return;
    }
    if (!(s_ad_img && lv_obj_is_valid(s_ad_img))) return;
    if (!(s_ad_psram && s_ad_psram[s_ad_current_image].data)) {
        ESP_LOGW(TAG, "[D] PSRAM buf NULL per img %u", s_ad_current_image);
        return;
    }

    /* Path numerico: LVGL chiama open_cb con "/N.jpg" → driver PSRAM */
    char path[20];
    snprintf(path, sizeof(path), ADS_MEM_PREFIX "%u.jpg", s_ad_current_image);
    lv_image_set_src(s_ad_img, path);

    const char *name = s_ad_images ? s_ad_images[s_ad_current_image] : "?";
    ESP_LOGI(TAG, "[D] Img %u/%u: %s (PSRAM %u B)",
             s_ad_current_image + 1, s_ad_image_count,
             name, (unsigned)s_ad_psram[s_ad_current_image].size);
}

/**
 * @brief Timer callback: avanza al prossimo slide.
 */
static void ad_carousel_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!bsp_display_lock(0)) {
        return;
    }
    if (s_ad_image_count > 0) {
        s_ad_current_image = (s_ad_current_image + 1) % s_ad_image_count;
        display_current_ad_image();
    }
    bsp_display_unlock();
}

/* ----------------------------------------------------------------------- */

static void switch_from_ads_async(void *arg)
{
    (void)arg;
    lvgl_page_main_show();  /* [C] "Seleziona lavaggio" → pagina programmi */
}

static void ads_flag_async(void *arg)
{
    (void)arg;
    /* [C] Bandiera chrome → selezione lingua con ritorno agli ads */
    lvgl_page_ads_deactivate();
    lvgl_page_language_2_show(lvgl_page_ads_show);
}

static void on_ads_flag_btn(lv_event_t *e)
{
    (void)e;
    lv_async_call(ads_flag_async, NULL);
}

static void on_select_btn(lv_event_t *e)
{
    (void)e;
    if (s_ad_exit_pending) {
        return;
    }
    s_ad_exit_pending = true;
    ESP_LOGI(TAG, "[C] Pulsante 'Seleziona lavaggio' premuto");
    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
        s_ad_carousel_timer = NULL;
    }
    lv_async_call(switch_from_ads_async, NULL);
}

static void on_ad_touch(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) {
        return;
    }
    /* Tocco sull'immagine: va alla selezione lingua (stessa destinazione del btn) */
    on_select_btn(e);
}

/* ----------------------------------------------------------------------- */

/**
 * @brief Mostra la pagina pubblicitaria con slideshow e pulsante selezione.
 *
 * Layout:
 *  - Chrome (ora + dots + bandiera) in cima
 *  - Nome locale (device_name) sotto il chrome
 *  - Slideshow con angoli arrotondati al centro
 *  - Pulsante "Seleziona lavaggio" verde acqua
 *  - Barra errore berry in fondo
 */
void lvgl_page_ads_show(void)
{
    lv_obj_t *scr = lv_scr_act();

    lvgl_page_main_deactivate();
    lvgl_page_language_2_deactivate();  /* [C] Sicurezza: ferma eventuali timer lingua */

    /* Intervallo rotazione da config */
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->timeouts.ad_rotation_ms > 0) {
        s_ad_rotation_ms = cfg->timeouts.ad_rotation_ms;
    }

    /* Carica immagini in PSRAM se non già caricate */
    if (s_ad_psram == NULL) {
        load_ad_images();
    }

    if (s_ad_image_count == 0) {
        ESP_LOGW(TAG, "[C] Nessuna immagine disponibile, ritorno alla pagina principale");
        lvgl_page_main_show();
        return;
    }

    /* [C] Ferma il timer chrome e resetta indev prima di distruggere gli oggetti */
    lvgl_page_chrome_remove();
    lv_indev_t *indev_a = lv_indev_get_next(NULL);
    while (indev_a) {
        lv_indev_reset(indev_a, NULL);
        indev_a = lv_indev_get_next(indev_a);
    }

    /* Pulisci schermo */
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Registra driver FS PSRAM (idempotente, richiede lock LVGL già acquisito) */
    register_psram_fs();

    /* Chrome (ora + dots + bandiera cliccabile → selezione lingua) */
    lvgl_page_chrome_set_flag_callback(on_ads_flag_btn, NULL);
    lvgl_page_chrome_add(scr);

    /* ── Titolo: nome del locale / dispositivo ─────────────────────────── */
    lv_obj_t *title_lbl = lv_label_create(scr);
    const char *location_name = (cfg && cfg->location_name[0] != '\0')
                               ? cfg->location_name
                               : "MicroHard";
    lv_label_set_text(title_lbl, location_name);
    lv_obj_set_style_text_color(title_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, PANEL_PAD_X, 88);

    /* ── Container slideshow con angoli arrotondati ─────────────────────── */
    s_img_container = lv_obj_create(scr);
    lv_obj_set_pos(s_img_container, AD_X, AD_Y);
    lv_obj_set_size(s_img_container, AD_W, AD_H);
    lv_obj_set_style_bg_color(s_img_container, lv_color_make(0x11, 0x11, 0x11), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_img_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_img_container, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_img_container, AD_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(s_img_container, true, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_img_container, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_img_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Immagine dentro il container */
    s_ad_img = lv_image_create(s_img_container);
    lv_obj_set_size(s_ad_img, AD_W, AD_H);
    lv_obj_align(s_ad_img, LV_ALIGN_CENTER, 0, 0);
    lv_image_set_inner_align(s_ad_img, LV_IMAGE_ALIGN_STRETCH);  /* scala per riempire il container */

    /* Tocco sull'immagine → selezione programma */
    lv_obj_add_flag(s_img_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_img_container, on_ad_touch, LV_EVENT_PRESSED, NULL);

    /* ── Pulsante "Seleziona lavaggio" ─────────────────────────────────── */
    lv_obj_t *sel_btn = lv_button_create(scr);
    lv_obj_set_size(sel_btn, BTN_SEL_W, BTN_SEL_H);
    lv_obj_align(sel_btn, LV_ALIGN_TOP_MID, 0, BTN_SEL_Y);
    lv_obj_set_style_bg_color(sel_btn, COL_SEL_BTN, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sel_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(sel_btn, BTN_SEL_H / 2, LV_PART_MAIN);   /* pill */
    lv_obj_set_style_border_width(sel_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(sel_btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sel_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(sel_btn, on_select_btn, LV_EVENT_CLICKED, NULL);

    lv_obj_t *sel_lbl = lv_label_create(sel_btn);
    lv_label_set_text(sel_lbl, "Seleziona lavaggio");
    lv_obj_set_style_text_color(sel_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(sel_lbl, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_center(sel_lbl);

    /* ── Barra messaggio errore ─────────────────────────────────────────── */
    lv_obj_t *err_box = lv_obj_create(scr);
    lv_obj_set_pos(err_box, PANEL_PAD_X, ERR_Y);
    lv_obj_set_size(err_box, ERR_W, ERR_H);
    lv_obj_set_style_bg_color(err_box, COL_ERR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(err_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(err_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(err_box, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_all(err_box, 8, LV_PART_MAIN);
    lv_obj_remove_flag(err_box, LV_OBJ_FLAG_SCROLLABLE);

    s_error_lbl = lv_label_create(err_box);
    lv_label_set_text(s_error_lbl, s_error_msg[0] ? s_error_msg : "Nessun errore");
    lv_obj_set_style_text_color(s_error_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_error_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_label_set_long_mode(s_error_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_error_lbl, ERR_W - 16);
    lv_obj_center(s_error_lbl);

    /* Avvio carousel */
    s_ad_current_image = 0;
    s_ad_exit_pending  = false;
    display_current_ad_image();

    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
    }
    s_ad_carousel_timer = lv_timer_create(ad_carousel_timer_cb, s_ad_rotation_ms, NULL);

    s_ad_scr = scr;
    ESP_LOGI(TAG, "[C] Schermata ads visualizzata (%u img, rot=%u ms)", s_ad_image_count, s_ad_rotation_ms);
}

/* ----------------------------------------------------------------------- */

/**
 * @brief Aggiorna il messaggio di errore nella barra in fondo.
 *        Può essere chiamata da qualsiasi task (solo strncpy, nessuna UI diretta).
 *        Il cambio diventa visibile al prossimo ads_show().
 *        Se la pagina è già visibile e s_error_lbl è valido, aggiorna subito.
 */
void lvgl_page_ads_set_error_message(const char *msg)
{
    strncpy(s_error_msg, msg ? msg : "", sizeof(s_error_msg) - 1);
    s_error_msg[sizeof(s_error_msg) - 1] = '\0';

    if (s_error_lbl && lv_obj_is_valid(s_error_lbl)) {
        lv_label_set_text(s_error_lbl, s_error_msg[0] ? s_error_msg : "Nessun errore");
    }
}

/* ----------------------------------------------------------------------- */

/**
 * @brief Deattiva e pulisce la pagina ads.
 */
void lvgl_page_ads_deactivate(void)
{
    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
        s_ad_carousel_timer = NULL;
    }
    s_ad_img        = NULL;
    s_img_container = NULL;
    s_error_lbl     = NULL;
    s_ad_scr        = NULL;
    s_ad_current_image = 0;
    s_ad_exit_pending  = false;
}

/**
 * @brief Libera la lista immagini dalla RAM (da chiamare se si vuole ricaricare).
 */
/**
 * @brief Libera la lista immagini dalla RAM (da chiamare se si vuole ricaricare).
 */
void lvgl_page_ads_preload_images(void)
{
    /* [C] Pre-carica le immagini da SPIFFS in PSRAM SENZA tenere il lock LVGL.
       Sicuro da chiamare più volte: se già caricati non fa nulla.
       Il driver FS 'M' viene registrato in ads_show() (dentro il lock). */
    if (s_ad_psram == NULL) {
        load_ad_images();
    }
}

void lvgl_page_ads_unload_images(void)
{
    /* Libera nomi file */
    if (s_ad_images) {
        for (uint32_t i = 0; i < s_ad_image_count; i++) free(s_ad_images[i]);
        free(s_ad_images);
        s_ad_images = NULL;
    }
    /* Libera buffer PSRAM */
    if (s_ad_psram) {
        for (uint32_t i = 0; i < s_ad_image_count; i++) {
            heap_caps_free(s_ad_psram[i].data);
        }
        free(s_ad_psram);
        s_ad_psram = NULL;
    }
    s_ad_image_count   = 0;
    s_ad_current_image = 0;
    ESP_LOGI(TAG, "[U] Immagini pubblicitarie scaricate (PSRAM + indice)");
}
