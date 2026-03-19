#include "lvgl_panel_pages.h"
#include "lvgl_page_chrome.h"

#include "lvgl_i18n.h"
#include "lvgl.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "lvgl_page_oos";

extern const lv_font_t GoogleSans35;
extern const lv_font_t GoogleSans50;
extern const lv_font_t GoogleSans140;


/**
 * @brief Mostra la pagina di servizio non disponibile.
 *
 * Questa funzione visualizza la pagina di servizio non disponibile
 * quando il sistema ha superato il numero massimo di riavvii consentiti.
 *
 * @param reboots Numero di riavvii effettuati.
 * @return void
 */
void lvgl_page_out_of_service_show(uint32_t reboots)
{
    lv_obj_t *scr = lv_scr_act();

    lvgl_page_main_deactivate();

    /* [C] Ferma il timer chrome e resetta indev prima di distruggere gli oggetti */
    lvgl_page_chrome_remove();
    lv_indev_t *indev_o = lv_indev_get_next(NULL);
    while (indev_o) {
        lv_indev_reset(indev_o, NULL);
        indev_o = lv_indev_get_next(indev_o);
    }

    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, lv_color_make(0x6b, 0x00, 0x00), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    // lvgl_page_chrome_add(scr);

    lv_obj_t *ico = lv_label_create(scr);
    lv_label_set_text(ico, "!");
    lv_obj_set_style_text_font(ico, &GoogleSans140, LV_PART_MAIN);
    lv_obj_set_style_text_color(ico, lv_color_make(0xFF, 0xCC, 0x00), LV_PART_MAIN);
    lv_obj_align(ico, LV_ALIGN_CENTER, 0, -120);

    lv_obj_t *lbl = lv_label_create(scr);
    char out_of_service_title[64] = {0};
    (void)lvgl_i18n_get_text("out_of_service_title",
                             "Fuori servizio",
                             out_of_service_title,
                             sizeof(out_of_service_title));
    lv_label_set_text(lbl, out_of_service_title);
    lv_obj_set_style_text_font(lbl, &GoogleSans50, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -40);

    char sub[96] = {0};
    char out_of_service_body[96] = {0};
    (void)lvgl_i18n_get_text("out_of_service_body",
                             "Reboot consecutivi: %lu\nContattare l'assistenza",
                             out_of_service_body,
                             sizeof(out_of_service_body));
    snprintf(sub, sizeof(sub), out_of_service_body, (unsigned long)reboots);

    lv_obj_t *sub_lbl = lv_label_create(scr);
    lv_label_set_text(sub_lbl, sub);
    lv_obj_set_style_text_font(sub_lbl, &GoogleSans35, LV_PART_MAIN);
    lv_obj_set_style_text_color(sub_lbl, lv_color_make(0xFF, 0xAA, 0xAA), LV_PART_MAIN);
    lv_obj_set_style_text_align(sub_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(sub_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(sub_lbl, 700);
    lv_obj_align(sub_lbl, LV_ALIGN_CENTER, 0, 60);

    ESP_LOGI(TAG, "[C] Pagina fuori servizio visualizzata (reboots=%lu)", (unsigned long)reboots);
}
