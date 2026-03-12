#include "lvgl_panel.h"
#include "lvgl_panel_pages.h"
#include "lvgl_page_chrome.h"
#include "init.h"

#include "lvgl.h"
#include "bsp/esp32_p4_nano.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <stdio.h>

extern const lv_font_t GoogleSans70;
extern const lv_font_t GoogleSans35;

static const char *TAG = "lvgl_panel";
static lv_obj_t *s_init_status_label = NULL;

#define COL_BG    lv_color_make(0x1a, 0x1a, 0x2e)
#define COL_BOOT_BG lv_color_make(0x00, 0x00, 0x00)
#define COL_WHITE lv_color_make(0xEE, 0xEE, 0xEE)
#define BOOT_LOGO_SPIFFS_PATH "/spiffs/logo.jpg"
#define BOOT_LOGO_LVGL_ALT_PATH "S:/spiffs/logo.jpg"


/**
 * @brief Controlla se il file del logo del pannello esiste.
 *
 * Questa funzione verifica la presenza del file del logo del pannello.
 *
 * @return true se il file esiste, false altrimenti.
 */
static bool panel_boot_logo_file_exists(void)
{
    FILE *f = fopen(BOOT_LOGO_SPIFFS_PATH, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

static bool panel_try_set_logo_src(lv_obj_t *img, const char *src)
{
    if (!img || !src || src[0] == '\0') {
        return false;
    }
    lv_image_set_src(img, src);
    lv_obj_update_layout(img);
    if (lv_obj_get_width(img) > 0 && lv_obj_get_height(img) > 0) {
        return true;
    }
    ESP_LOGW(TAG, "[C] Caricamento logo fallito o dimensioni nulle (src=%s)", src);
    return false;
}


/**
 * @brief Mostra lo schermo del logo di avvio del pannello.
 *
 * Questa funzione visualizza lo schermo del logo di avvio del pannello.
 *
 * @return Niente.
 */
static void panel_show_boot_logo_screen(void)
{
    lv_obj_t *scr = lv_scr_act();

    lvgl_page_main_deactivate();

    /* [C] Ferma chrome timer e resetta indev prima di lv_obj_clean */
    lvgl_page_chrome_remove();
    lv_indev_t *indev_b = lv_indev_get_next(NULL);
    while (indev_b) {
        lv_indev_reset(indev_b, NULL);
        indev_b = lv_indev_get_next(indev_b);
    }

    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, COL_BOOT_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    s_init_status_label = NULL;

    bool logo_drawn = false;
    if (panel_boot_logo_file_exists()) {
        lv_obj_t *logo_img = lv_image_create(scr);
        logo_drawn = panel_try_set_logo_src(logo_img, BOOT_LOGO_LVGL_ALT_PATH);
        if (!logo_drawn) {
            logo_drawn = panel_try_set_logo_src(logo_img, BOOT_LOGO_SPIFFS_PATH);
        }
        if (logo_drawn) {
            lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, 0);
            ESP_LOGI(TAG, "[C] Pagina logo boot visualizzata (src=%s)",
                     lv_obj_get_width(logo_img) > 0 ? (const char *)lv_image_get_src(logo_img) : BOOT_LOGO_SPIFFS_PATH);
        } else {
            lv_obj_del(logo_img);
        }
    }

    if (!logo_drawn) {
        ESP_LOGW(TAG, "[C] Logo non visualizzato, uso fallback testuale");
        lv_obj_t *logo_lbl = lv_label_create(scr);
        lv_label_set_text(logo_lbl, "MicroHard");
        lv_obj_set_style_text_color(logo_lbl, COL_WHITE, LV_PART_MAIN);
        lv_obj_set_style_text_font(logo_lbl, &GoogleSans70, LV_PART_MAIN);
        lv_obj_align(logo_lbl, LV_ALIGN_CENTER, 0, 0);
    }

    s_init_status_label = lv_label_create(scr);
    lv_label_set_long_mode(s_init_status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_init_status_label, "Init: avvio...");
    lv_obj_set_width(s_init_status_label, LV_PCT(100));
    lv_obj_set_style_text_align(s_init_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_init_status_label, &GoogleSans35, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_init_status_label, COL_WHITE, LV_PART_MAIN);
    lv_obj_align(s_init_status_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}


/** @brief Mostra il pannello LVGL.
 *  
 *  Questa funzione mostra il pannello LVGL sullo schermo.
 *  
 *  @return Nessun valore di ritorno.
 */
void lvgl_panel_show(void)
{
    lvgl_panel_show_boot_logo();
}


/**
 * @brief Mostra il logo di avvio sul pannello.
 * 
 * @param [in] Nessun parametro.
 * @return void Nessun valore di ritorno.
 */
void lvgl_panel_show_boot_logo(void)
{
    if (!bsp_display_lock(pdMS_TO_TICKS(500))) {
        ESP_LOGW(TAG, "[C] LVGL lock fallito in lvgl_panel_show_boot_logo, provo re-init display");
        if (init_run_display_only() != ESP_OK) {
            ESP_LOGE(TAG, "[C] Re-init display fallita in lvgl_panel_show_boot_logo");
            return;
        }
        if (!bsp_display_lock(pdMS_TO_TICKS(500))) {
            ESP_LOGE(TAG, "[C] LVGL lock ancora indisponibile dopo re-init in lvgl_panel_show_boot_logo");
            return;
        }
    }

    panel_show_boot_logo_screen();

    bsp_display_unlock();
}


/**
 * @brief Mostra la finestra di selezione della lingua.
 * 
 * Questa funzione mostra la finestra di selezione della lingua sul display.
 * 
 * @param [in] Nessun parametro di input.
 * @return void Nessun valore di ritorno.
 */
void lvgl_panel_show_language_select(void)
{
    if (!bsp_display_lock(pdMS_TO_TICKS(1500))) {
        ESP_LOGW(TAG, "[C] LVGL lock fallito in lvgl_panel_show_language_select, provo re-init display");
        if (init_run_display_only() != ESP_OK) {
            ESP_LOGE(TAG, "[C] Re-init display fallita in lvgl_panel_show_language_select");
            return;
        }
        if (!bsp_display_lock(pdMS_TO_TICKS(500))) {
            ESP_LOGE(TAG, "[C] LVGL lock ancora fallito dopo re-init in lvgl_panel_show_language_select");
            return;
        }
    }

    lvgl_page_language_2_show(lvgl_page_main_show);  /* [C] Fallback: ritorna a programmi */

    bsp_display_unlock();

    ESP_LOGI(TAG, "[C] Pagina selezione lingua visualizzata");
}


/**
 * @brief Aggiorna i testi del pannello LVGL.
 * 
 * Questa funzione si occupa di aggiornare i testi visualizzati sul pannello LVGL.
 * Effettua un tentativo di acquisire un blocco di esclusione per 100 millisecondi.
 * Se il blocco non può essere acquisito entro questo periodo, la funzione termina.
 * 
 * @param Nessun parametro.
 * @return Nessun valore di ritorno.
 */
void lvgl_panel_refresh_texts(void)
{
    if (!bsp_display_lock(100)) {
        return;
    }

    lvgl_page_main_refresh_texts();

    bsp_display_unlock();
}


/**
 * @brief Mostra il pannello di servizio non disponibile.
 * 
 * @param reboots Numero di riavvii effettuati.
 * @return void Nessun valore di ritorno.
 */
void lvgl_panel_show_out_of_service(uint32_t reboots)
{
    if (!bsp_display_lock(pdMS_TO_TICKS(200))) {
        ESP_LOGW(TAG, "[C] Display non attivo, init forzata per schermata errore");
        if (init_run_display_only() != ESP_OK) {
            ESP_LOGE(TAG, "[C] Init display forzata fallita, schermata non disponibile");
            return;
        }
    } else {
        bsp_display_unlock();
    }

    if (!bsp_display_lock(0)) {
        ESP_LOGW(TAG, "[C] LVGL lock fallito in lvgl_panel_show_out_of_service");
        return;
    }

    lvgl_page_out_of_service_show(reboots);

    bsp_display_unlock();
}

void lvgl_panel_show_main_page(void)
{
    if (!bsp_display_lock(0)) {
        ESP_LOGW(TAG, "[C] LVGL lock fallito in lvgl_panel_show_main_page");
        return;
    }

    lvgl_page_main_show();

    bsp_display_unlock();
}

void lvgl_panel_show_ads_page(void)
{
    /* [C] Pre-carica le immagini SPIFFS fuori dal lock LVGL per non bloccare il rendering */
    lvgl_page_ads_preload_images();

    if (!bsp_display_lock(0)) {
        ESP_LOGW(TAG, "[C] LVGL lock fallito in lvgl_panel_show_ads_page");
        return;
    }

    lvgl_page_ads_show();

    bsp_display_unlock();
}

void lvgl_panel_set_init_status(const char *text)
{
    if (!text || text[0] == '\0') {
        return;
    }
    if (!bsp_display_lock(pdMS_TO_TICKS(30))) {
        return;
    }
    if (s_init_status_label && lv_obj_is_valid(s_init_status_label)) {
        lv_label_set_text(s_init_status_label, text);
    }
    bsp_display_unlock();
}
