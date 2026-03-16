#include "lvgl_panel_pages.h"

#include "device_config.h"
#include "language_flags.h"
#include "lvgl_page_chrome.h"
#include "usb_cdc_scanner.h"

#include "lvgl.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "lvgl_page_lang";

extern const lv_font_t GoogleSans35;

#define COL_BG        lv_color_make(0x1a, 0x1a, 0x2e)
#define COL_PROG      lv_color_make(0x6c, 0x34, 0x83)
#define COL_PROG_ACT  lv_color_make(0x1e, 0x8b, 0x45)
#define COL_WHITE     lv_color_make(0xEE, 0xEE, 0xEE)

#define FONT_TITLE    (&GoogleSans35)
#define FONT_LANG     (&GoogleSans35)

typedef struct {
    const char *code;
    const char *label;
} panel_language_option_t;

static const panel_language_option_t s_panel_lang_opts[] = {
    {.code = "it", .label = "Italiano"},
    {.code = "en", .label = "English"},
    {.code = "de", .label = "Deutsch"},
    {.code = "fr", .label = "Français"},
    {.code = "es", .label = "Español"},
};

static char s_selected_user_lang[8] = "it";


/**
 * @brief Imposta lo stile di un pulsante LVGL.
 *
 * @param btn Puntatore all'oggetto pulsante da stилиizzare.
 * @param bg Colore di sfondo del pulsante.
 * @return void Nessun valore di ritorno.
 */
static void btn_style(lv_obj_t *btn, lv_color_t bg)
{
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 4, LV_PART_MAIN);
}


/**
 * @brief Riabilita lo scanner del pannello.
 *
 * Questa funzione riabilita lo scanner del pannello utilizzando il contesto fornito.
 *
 * @param [in] context Il contesto utilizzato per riabilitare lo scanner.
 * @return Nessun valore di ritorno.
 */
static void panel_reenable_scanner(const char *context)
{
    esp_err_t on_err = usb_cdc_scanner_send_on_command();
    if (on_err == ESP_OK) {
        ESP_LOGI(TAG, "[C] Riattivazione scanner %s: ON inviato", context ? context : "");
        return;
    }
    ESP_LOGI(TAG, "[C] Riattivazione scanner FALLITA");
    esp_err_t setup_err = usb_cdc_scanner_send_setup_command();
    esp_err_t retry_on_err = ESP_FAIL;
    if (setup_err == ESP_OK) {
        retry_on_err = usb_cdc_scanner_send_on_command();
    }

    if (setup_err == ESP_OK && retry_on_err == ESP_OK) {
        ESP_LOGI(TAG, "[C] Riattivazione scanner %s: fallback setup+on riuscito", context ? context : "");
    } else {
        ESP_LOGW(TAG,
                 "[C] Riattivazione scanner %s fallita (on=%s setup=%s retry_on=%s)",
                 context ? context : "",
                 esp_err_to_name(on_err),
                 esp_err_to_name(setup_err),
                 esp_err_to_name(retry_on_err));
    }
}


/**
 * @brief Applica la lingua selezionata in modo asincrono.
 *
 * Questa funzione si occupa dell'applicazione della lingua selezionata in modo asincrono.
 * Il parametro 'arg' è un puntatore generico che può essere utilizzato per passare informazioni aggiuntive.
 *
 * @param [in] arg Puntatore generico contenente informazioni aggiuntive.
 * @return Nessun valore di ritorno.
 */
static void panel_apply_selected_language_async(void *arg)
{
    (void)arg;

    device_config_t *cfg = device_config_get();
    if (cfg) {
        strncpy(cfg->ui.user_language, s_selected_user_lang, sizeof(cfg->ui.user_language) - 1);
        cfg->ui.user_language[sizeof(cfg->ui.user_language) - 1] = '\0';
        cfg->updated = true;

        if (device_config_save(cfg) != ESP_OK) {
            ESP_LOGW(TAG, "[C] Salvataggio lingua pannello fallito (%s)", s_selected_user_lang);
        }
    }

    panel_reenable_scanner("dopo scelta lingua");

    lvgl_page_main_show();
}


/**
 * @brief Gestisce l'evento del bottone di lingua.
 *
 * Questa funzione viene chiamata quando si verifica un evento sul bottone di lingua.
 * 
 * @param [in] e Puntatore all'evento generato dal bottone di lingua.
 * @return Nessun valore di ritorno.
 */
static void on_lang_btn(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const panel_language_option_t *opt = (const panel_language_option_t *)lv_obj_get_user_data(btn);
    if (!opt || !opt->code) {
        return;
    }

    strncpy(s_selected_user_lang, opt->code, sizeof(s_selected_user_lang) - 1);
    s_selected_user_lang[sizeof(s_selected_user_lang) - 1] = '\0';

    lv_async_call(panel_apply_selected_language_async, NULL);
}


/** @brief Mostra la pagina di selezione della lingua.
 *  
 *  Questa funzione visualizza la pagina che consente all'utente di selezionare la lingua dell'applicazione.
 *  
 *  @return Nessun valore di ritorno.
 */
void lvgl_page_language_show(void)
{
    lv_obj_t *scr = lv_scr_act();

    lvgl_page_main_deactivate();

    /* [C] Ferma il timer chrome e resetta indev prima di distruggere gli oggetti */
    lvgl_page_chrome_remove();
    lv_indev_t *indev_lg = lv_indev_get_next(NULL);
    while (indev_lg) {
        lv_indev_reset(indev_lg, NULL);
        indev_lg = lv_indev_get_next(indev_lg);
    }

    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lvgl_page_chrome_add(scr);

    panel_reenable_scanner("all'apertura scelta lingua");

    const char *current_lang = device_config_get_ui_user_language();
    if (current_lang && strlen(current_lang) == 2) {
        strncpy(s_selected_user_lang, current_lang, sizeof(s_selected_user_lang) - 1);
        s_selected_user_lang[sizeof(s_selected_user_lang) - 1] = '\0';
    }

    lv_obj_t *title = lv_label_create(scr);
    char select_language[64] = {0};
    device_config_get_ui_text_scoped("lvgl", "language_select_title", "Seleziona lingua", select_language, sizeof(select_language));
    lv_label_set_text(title, select_language);
    lv_obj_set_style_text_color(title, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(title, FONT_TITLE, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    const int32_t btn_w = 560;
    const int32_t btn_h = 150;
    const int32_t gap = 14;
    const int32_t start_y = 120;
    const uint32_t flag_scale = 256;
    const int32_t flag_x = 30;
    const int32_t label_x = 136;

    for (size_t i = 0; i < (sizeof(s_panel_lang_opts) / sizeof(s_panel_lang_opts[0])); i++) {
        const panel_language_option_t *opt = &s_panel_lang_opts[i];
        bool selected = (strcmp(s_selected_user_lang, opt->code) == 0);

        lv_obj_t *btn = lv_button_create(scr);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, start_y + (int32_t)i * (btn_h + gap));
        btn_style(btn, selected ? COL_PROG_ACT : COL_PROG);
        lv_obj_set_style_border_width(btn, selected ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, COL_WHITE, LV_PART_MAIN);
        lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t *img = NULL;
        if (opt->code) {
            img = lv_image_create(btn);
            lv_image_set_src(img, get_flag_src_for_language(opt->code));
            lv_image_set_scale(img, flag_scale);
            lv_obj_align(img, LV_ALIGN_LEFT_MID, flag_x, 0);
        }

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, opt->label);
        lv_obj_set_style_text_color(lbl, COL_WHITE, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, FONT_LANG, LV_PART_MAIN);

        if (img) {
            lv_obj_align(lbl, LV_ALIGN_LEFT_MID, label_x, 0);
        } else {
            lv_obj_center(lbl);
        }

        lv_obj_set_user_data(btn, (void *)opt);
        lv_obj_add_event_cb(btn, on_lang_btn, LV_EVENT_CLICKED, NULL);
    }

    ESP_LOGI(TAG, "[C] Pagina selezione lingua visualizzata");
}
