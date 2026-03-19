#include "lvgl_panel_pages.h"
#include "lvgl_page_chrome.h"
#include "lvgl_i18n.h"
#include "lvgl.h"
#include "esp_log.h"
#include "device_config.h"
#include "sd_card.h"
#include "lvgl_panel.h"
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bsp/esp32_p4_nano.h"
#include "esp_heap_caps.h"
#include "tjpgd.h"    /* header privato LVGL: managed_components/lvgl__lvgl/src/libs/tjpgd/ */
#include "fsm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Font GoogleSans per pagina ads */
extern const lv_font_t GoogleSans70;
extern const lv_font_t GoogleSans50;
extern const lv_font_t GoogleSans40;

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

/* -----------------------------------------------------------------------
 * PRE-DECODIFICA JPEG → RGB565 IN PSRAM
 *
 * Al boot (fuori dal lock LVGL) ogni img*.jpg viene letta da SPIFFS e
 * decodificata interamente con TJPGD in un buffer PSRAM separato.
 * Il buffer contiene pixel raw RGB565, già pronti per il blit.
 *
 * Al cambio slide: lv_image_set_src(img, &s_images[idx].dsc)
 *   → LVGL vede LV_IMAGE_SRC_VARIABLE con cf=RGB565 → blit diretto
 *   → ZERO decode JPEG a run-time → transizione < 10 ms
 *
 * Memoria: ~692 × 904 × 2 B ≈ 1.2 MB per immagine (PSRAM 200 MHz)
 * ----------------------------------------------------------------------- */

#define TJPGD_WORK_SZ   4096
#define AD_IMG_MAX      16

typedef struct {
    lv_image_dsc_t  dsc;        /**< Descrittore LVGL (punta a pixels) */
    uint8_t        *pixels;     /**< Buffer RGB565 PSRAM (heap_caps_malloc) */
    char            name[256];  /**< Nome file originale (per log) */
} ad_image_t;

/* Contesto di output decodifica (accesso statico: single-threaded init) */
static struct {
    uint16_t *buf;
    uint32_t  stride;   /* pixel per riga */
} s_dec_out;

/* Input function TJPGD: legge dal FILE* passato come jd->device */
static size_t tjpg_infunc(JDEC *jd, uint8_t *buf, size_t n)
{
    FILE *fp = jd->device;
    if (buf) return fread(buf, 1, n, fp);
    return (fseek(fp, (long)n, SEEK_CUR) == 0) ? n : 0;
}

/* Output function TJPGD: converte MCU (RGB888) → RGB565 in PSRAM */
static int tjpg_outfunc(JDEC *jd, void *bitmap, JRECT *rect)
{
    (void)jd;
    const uint8_t *src = bitmap;
    uint32_t w = (uint32_t)(rect->right  - rect->left + 1);
    uint32_t h = (uint32_t)(rect->bottom - rect->top  + 1);
    for (uint32_t row = 0; row < h; row++) {
        uint16_t *dst = s_dec_out.buf
                        + (rect->top + row) * s_dec_out.stride
                        + rect->left;
        for (uint32_t col = 0; col < w; col++) {
            uint8_t r = *src++, g = *src++, b = *src++;
            // Correzione: LVGL usa formato RGB565 little-endian (BGR565)
            *dst++ = (uint16_t)(((uint16_t)(b & 0xF8) << 8)
                              | ((uint16_t)(g & 0xFC) << 3)
                              | (r >> 3));
        }
    }
    return 1;  /* continua decodifica */
}

/**
 * @brief Decodifica un JPEG da SPIFFS/SD in un buffer RGB565 allocato in PSRAM.
 * @param use_sd  true = apre con sd_card_fopen, false = fopen standard
 * @return true se successo, false in caso di errore
 */
static bool decode_jpeg_to_psram(const char *fpath, ad_image_t *img, bool use_sd)
{
    FILE *fp = use_sd ? sd_card_fopen(fpath, "rb") : fopen(fpath, "rb");
    if (!fp) { ESP_LOGW(TAG, "[L] fopen: %s", fpath); return false; }

    uint8_t *work = malloc(TJPGD_WORK_SZ);
    if (!work) { fclose(fp); return false; }

    JDEC jd;
    JRESULT rc = jd_prepare(&jd, tjpg_infunc, work, TJPGD_WORK_SZ, fp);
    if (rc != JDR_OK) {
        ESP_LOGW(TAG, "[L] jd_prepare %d: %s", rc, fpath);
        free(work); fclose(fp); return false;
    }

    uint32_t w = jd.width, h = jd.height;
    size_t bytes = (size_t)w * h * sizeof(uint16_t);
    img->pixels = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!img->pixels) {
        ESP_LOGE(TAG, "[L] OOM PSRAM %u B: %s", (unsigned)bytes, fpath);
        free(work); fclose(fp); return false;
    }

    s_dec_out.buf    = (uint16_t *)img->pixels;
    s_dec_out.stride = w;
    rc = jd_decomp(&jd, tjpg_outfunc, 0 /* scala 1:1 */);
    free(work);
    fclose(fp);

    if (rc != JDR_OK) {
        ESP_LOGW(TAG, "[L] jd_decomp %d: %s", rc, fpath);
        heap_caps_free(img->pixels);
        img->pixels = NULL;
        return false;
    }

    img->dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    img->dsc.header.cf     = LV_COLOR_FORMAT_RGB565;
    img->dsc.header.flags  = 0;
    img->dsc.header.w      = (uint32_t)w;
    img->dsc.header.h      = (uint32_t)h;
    img->dsc.header.stride = (uint32_t)(w * sizeof(uint16_t));
    img->dsc.data_size     = bytes;
    img->dsc.data          = img->pixels;

    ESP_LOGI(TAG, "[L] Decoded %s → %ux%u RGB565 (%u B PSRAM)",
             fpath, w, h, (unsigned)bytes);
    return true;
}

/* Stato pagina */
static lv_obj_t      *s_ad_scr           = NULL;
static lv_obj_t      *s_ad_img           = NULL;
static lv_obj_t      *s_img_container    = NULL;
static lv_obj_t      *s_error_lbl        = NULL;
static lv_timer_t    *s_ad_carousel_timer = NULL;
static bool           s_ad_exit_pending  = false;

/* Immagini pre-decodificate in PSRAM */
static ad_image_t     s_images[AD_IMG_MAX];
static uint32_t       s_image_count      = 0;
static uint32_t       s_current_image    = 0;
static uint32_t       s_ad_rotation_ms   = 30000;

/* Messaggio errore corrente */
static char           s_error_msg[128]   = "";

/* ----------------------------------------------------------------------- */

/**
 * @brief Legge tutti i img*.jpg da SPIFFS e SDCARD (priorità SPIFFS) e li decodifica RGB565 in PSRAM.
 *        Priorità: 1) SPIFFS, 2) SDCARD, 3) nessuna immagine → skip pagina ads.
 *        Chiamare SENZA lock LVGL (operazioni I/O e CPU intensive).
 */
static void load_ad_images(void)
{
    /* Cleanup caricamento precedente */
    for (uint32_t i = 0; i < s_image_count; i++) {
        if (s_images[i].pixels) {
            heap_caps_free(s_images[i].pixels);
            s_images[i].pixels = NULL;
        }
    }
    s_image_count   = 0;
    s_current_image = 0;

    ESP_LOGI(TAG, "[L] ===== load_ad_images START =====");
    
    /* 1) Prima prova a caricare da SPIFFS */
    ESP_LOGI(TAG, "[L] Tentativo caricamento immagini da SPIFFS...");
    
    DIR *dir = opendir("/spiffs");
    if (dir) {
        struct dirent *ent;
        uint32_t idx = 0;
        while ((ent = readdir(dir)) != NULL && idx < AD_IMG_MAX) {
            if (ent->d_type != DT_REG ||
                strncmp(ent->d_name, "img", 3) != 0 ||
                strlen(ent->d_name) < strlen("img01.jpg") ||
                strcmp(ent->d_name + strlen(ent->d_name) - 4, ".jpg") != 0) {
                continue;
            }
            char fpath[280];
            snprintf(fpath, sizeof(fpath), "/spiffs/%s", ent->d_name);
            strncpy(s_images[idx].name, ent->d_name, sizeof(s_images[idx].name) - 1);
            s_images[idx].name[sizeof(s_images[idx].name) - 1] = '\0';
            memset(&s_images[idx].dsc, 0, sizeof(lv_image_dsc_t));
            s_images[idx].pixels = NULL;
            ESP_LOGI(TAG, "[L] SPIFFS: tentativo decode %s", ent->d_name);
            if (decode_jpeg_to_psram(fpath, &s_images[idx], false)) {
                ESP_LOGI(TAG, "[L] SPIFFS: %s decodificato con successo", ent->d_name);
                idx++;
            } else {
                ESP_LOGW(TAG, "[L] SPIFFS: fallito decode %s", ent->d_name);
            }
        }
        closedir(dir);
        s_image_count = idx;
        
        if (s_image_count > 0) {
            ESP_LOGI(TAG, "[L] Caricate %u immagini da SPIFFS", s_image_count);
        } else {
            ESP_LOGW(TAG, "[L] Nessuna immagine trovata in SPIFFS");
        }
    } else {
        ESP_LOGW(TAG, "[L] Impossibile aprire directory SPIFFS");
    }

    /* 2) Se non ci sono immagini in SPIFFS, prova da SDCARD */
    if (s_image_count == 0) {
        ESP_LOGI(TAG, "[L] Tentativo caricamento immagini da SDCARD...");
        
        if (sd_card_is_mounted()) {
            DIR *sd_dir = sd_card_opendir("/sdcard");
            if (sd_dir) {
                struct dirent *ent;
                uint32_t idx = 0;
                while ((ent = readdir(sd_dir)) != NULL && idx < AD_IMG_MAX) {
                    if (ent->d_type != DT_REG ||
                        strncmp(ent->d_name, "img", 3) != 0 ||
                        strlen(ent->d_name) < strlen("img01.jpg") ||
                        strcmp(ent->d_name + strlen(ent->d_name) - 4, ".jpg") != 0) {
                        continue;
                    }
                    char fpath[280];
                    snprintf(fpath, sizeof(fpath), "/sdcard/%s", ent->d_name);
                    strncpy(s_images[idx].name, ent->d_name, sizeof(s_images[idx].name) - 1);
                    s_images[idx].name[sizeof(s_images[idx].name) - 1] = '\0';
                    memset(&s_images[idx].dsc, 0, sizeof(lv_image_dsc_t));
                    s_images[idx].pixels = NULL;
                    ESP_LOGI(TAG, "[L] SDCARD: tentativo decode %s", ent->d_name);
                    if (decode_jpeg_to_psram(fpath, &s_images[idx], true)) {
                        ESP_LOGI(TAG, "[L] SDCARD: %s decodificato con successo", ent->d_name);
                        idx++;
                    } else {
                        ESP_LOGW(TAG, "[L] SDCARD: fallito decode %s", ent->d_name);
                    }
                }
                sd_card_closedir(sd_dir);
                s_image_count = idx;
                
                if (s_image_count > 0) {
                    ESP_LOGI(TAG, "[L] Caricate %u immagini da SDCARD", s_image_count);
                } else {
                    ESP_LOGW(TAG, "[L] Nessuna immagine trovata in SDCARD");
                }
            } else {
                ESP_LOGW(TAG, "[L] Impossibile aprire directory SDCARD");
            }
        } else {
            ESP_LOGW(TAG, "[L] SDCARD non montata");
        }
    }

    ESP_LOGI(TAG, "[L] ===== load_ad_images END: %u immagini caricate =====", s_image_count);
}

/* ----------------------------------------------------------------------- */

/**
 * @brief Mostra l'immagine corrente: blit diretto dal buffer PSRAM pre-decodificato.
 *        Nessun decode JPEG a run-time — transizione <10 ms.
 */
static void display_current_ad_image(void)
{
    if (s_image_count == 0 || s_current_image >= s_image_count) return;
    if (!s_images[s_current_image].pixels) return;
    if (!(s_ad_img && lv_obj_is_valid(s_ad_img))) return;

    lv_image_set_src(s_ad_img, &s_images[s_current_image].dsc);
    ESP_LOGI(TAG, "[D] Img %u/%u: %s (blit PSRAM RGB565)",
             s_current_image + 1, s_image_count,
             s_images[s_current_image].name);
}

/**
 * @brief Timer callback: avanza al prossimo slide.
 */
static void ad_carousel_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!bsp_display_lock(0)) return;
    if (s_image_count > 0) {
        s_current_image = (s_current_image + 1) % s_image_count;
        display_current_ad_image();
    }
    bsp_display_unlock();
}

/* ----------------------------------------------------------------------- */

static void switch_from_ads_async(void *arg)
{
    (void)arg;

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap)) {
        if (snap.state == FSM_STATE_CREDIT ||
            snap.state == FSM_STATE_RUNNING ||
            snap.state == FSM_STATE_PAUSED) {
            ESP_LOGI(TAG, "[C] Ads touch con FSM gia' in stato %s: apro pagina principale",
                     fsm_state_to_string(snap.state));
            lvgl_panel_show_main_page();
            return;
        }
    }

    fsm_input_event_t ev = {
        .from = AGN_ID_LVGL,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_USER_ACTIVITY,
        .type = FSM_INPUT_EVENT_TOUCH,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
    };
    (void)fsm_event_publish(&ev, pdMS_TO_TICKS(20));
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

    if (s_image_count == 0) {
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
    lv_obj_set_style_text_font(title_lbl, &GoogleSans70, LV_PART_MAIN);
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
    char select_wash[64] = {0};
    (void)lvgl_i18n_get_text("ads_select_wash",
                             "Seleziona lavaggio",
                             select_wash,
                             sizeof(select_wash));
    lv_label_set_text(sel_lbl, select_wash);
    lv_obj_set_style_text_color(sel_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(sel_lbl, &GoogleSans50, LV_PART_MAIN);
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
    
    // Nascondi la barra errore se non ci sono messaggi di errore
    if (s_error_msg[0]) {
        // C'è un errore, mostra la barra e il messaggio
        lv_obj_clear_flag(err_box, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_error_lbl, s_error_msg);
    } else {
        // Nessun errore, nascondi completamente la barra
        lv_obj_add_flag(err_box, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_error_lbl, "");
    }
    
    lv_obj_set_style_text_color(s_error_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_error_lbl, &GoogleSans40, LV_PART_MAIN);
    lv_label_set_long_mode(s_error_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_error_lbl, ERR_W - 16);
    lv_obj_center(s_error_lbl);

    /* Avvio carousel */
    s_current_image    = 0;
    s_ad_exit_pending  = false;
    display_current_ad_image();

    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
    }
    s_ad_carousel_timer = lv_timer_create(ad_carousel_timer_cb, s_ad_rotation_ms, NULL);

    s_ad_scr = scr;
    ESP_LOGI(TAG, "[C] Schermata ads visualizzata (%u img, rot=%u ms)", s_image_count, s_ad_rotation_ms);
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
        // Trova il parent box dell'errore
        lv_obj_t *err_box = lv_obj_get_parent(s_error_lbl);
        if (err_box && lv_obj_is_valid(err_box)) {
            if (s_error_msg[0]) {
                // C'è un errore, mostra barra e messaggio
                lv_label_set_text(s_error_lbl, s_error_msg);
                lv_obj_clear_flag(err_box, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(s_error_lbl, LV_OBJ_FLAG_HIDDEN);
            } else {
                // Nessun errore, nascondi completamente barra e label
                lv_label_set_text(s_error_lbl, "");
                lv_obj_add_flag(err_box, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(s_error_lbl, LV_OBJ_FLAG_HIDDEN);
            }
        }
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
    s_current_image    = 0;
    s_ad_exit_pending  = false;
}

/**
 * @brief Libera la lista immagini dalla RAM (da chiamare se si vuole ricaricare).
 */
void lvgl_page_ads_preload_images(void)
{
    /* [C] Pre-carica e decodifica le immagini in PSRAM SENZA lock LVGL.
       Sicuro da chiamare più volte: idempotente se già caricate. */
    if (s_image_count == 0) {
        load_ad_images();
    }
}

void lvgl_page_ads_unload_images(void)
{
    for (uint32_t i = 0; i < s_image_count; i++) {
        if (s_images[i].pixels) {
            heap_caps_free(s_images[i].pixels);
            s_images[i].pixels = NULL;
        }
    }
    s_image_count   = 0;
    s_current_image = 0;
    ESP_LOGI(TAG, "[U] Immagini pubblicitarie liberate dalla PSRAM");
}
