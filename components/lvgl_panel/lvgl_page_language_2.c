#include "lvgl_panel_pages.h"

#include "device_config.h"
#include "language_flags.h"
#include "lvgl_page_chrome.h"
#include "usb_cdc_scanner.h"

#include "lvgl.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "lvgl_page_lang2";

extern const lv_font_t GoogleSans35;
extern const lv_font_t GoogleSans40;

#define COL_BG            lv_color_make(0x00, 0x00, 0x00)
#define COL_WHITE         lv_color_make(0xEE, 0xEE, 0xEE)
#define COL_ACCENT        lv_color_make(0x56, 0xE6, 0xBD)
#define COL_TEXT_DARK     lv_color_make(0x08, 0x10, 0x0F)

#define FONT_TITLE        (&GoogleSans40)
#define FONT_LANG         (&GoogleSans35)

typedef struct {
    const char *code;
    const char *label;
} panel_language_option_t;

static const panel_language_option_t s_panel_lang_opts[] = {
    {.code = "it", .label = "Italiano"},
    {.code = "en", .label = "English"},
    {.code = "es", .label = "Español"},
    {.code = "de", .label = "Deutsch"},
    {.code = "fr", .label = "Français"},
};

static char s_selected_user_lang[8] = "it";
static void (*s_return_cb)(void)       = NULL;  /* [C] Pagina da cui tornare dopo la selezione */
static lv_timer_t *s_lang_timeout_timer = NULL; /* [C] Timer 60s: ritorno automatico */
static lv_timer_t *s_lang_tick_timer    = NULL; /* [C] Ticker 1s per countdown */
static lv_obj_t   *s_timeout_lbl        = NULL; /* [C] Label contatore */
static uint32_t    s_lang_ticks_left    = 60;

#define LANG_TIMEOUT_S  60
#define LANG_TICK_MS    1000U

static void panel_reenable_scanner(const char *context);

/* [C] Ferma i timer lingua senza toccare le widget (sicuro da chiamare sempre). */
void lvgl_page_language_2_deactivate(void)
{
    if (s_lang_timeout_timer) {
        lv_timer_delete(s_lang_timeout_timer);
        s_lang_timeout_timer = NULL;
    }
    if (s_lang_tick_timer) {
        lv_timer_delete(s_lang_tick_timer);
        s_lang_tick_timer = NULL;
    }
    s_timeout_lbl = NULL;
    s_return_cb   = NULL;
}

/* [C] Torna alla pagina chiamante e smantella i timer. */
static void lang_do_return(void)
{
    void (*cb)(void) = s_return_cb;
    lvgl_page_language_2_deactivate();  /* azzera tutto prima di chiamare il callback */
    if (cb) {
        cb();
    } else {
        lvgl_page_main_show();  /* fallback di sicurezza */
    }
}

static void lang_timeout_cb(lv_timer_t *t)
{
    (void)t;
    ESP_LOGI(TAG, "[C] Timeout selezione lingua (%d s), ritorno automatico", LANG_TIMEOUT_S);
    lang_do_return();
}

static void lang_tick_cb(lv_timer_t *t)
{
    (void)t;
    if (s_lang_ticks_left > 0) {
        s_lang_ticks_left--;
    }
    if (s_timeout_lbl && lv_obj_is_valid(s_timeout_lbl)) {
        char buf[24] = {0};
        snprintf(buf, sizeof(buf), "(%lu s)", (unsigned long)s_lang_ticks_left);
        lv_label_set_text(s_timeout_lbl, buf);
    }
}

static void panel_reenable_scanner_async(void *arg)
{
    (void)arg;
    panel_reenable_scanner("all'apertura scelta lingua");
}

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
    lang_do_return();  /* [C] Torna alla pagina chiamante (ads o programmi) */
}

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

static void style_lang_btn(lv_obj_t *btn, bool selected)
{
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_left(btn, 36, LV_PART_MAIN);
    lv_obj_set_style_pad_right(btn, 26, LV_PART_MAIN);

    if (selected) {
        lv_obj_set_style_bg_color(btn, COL_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(btn, COL_BG, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 3, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, COL_ACCENT, LV_PART_MAIN);
        lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    }
}

void lvgl_page_language_2_show(void (*return_cb)(void))
{
    s_return_cb = return_cb ? return_cb : lvgl_page_main_show;
    lv_obj_t *scr = lv_scr_act();

    lvgl_page_main_deactivate();

    /* [C] Ferma il timer chrome e resetta indev PRIMA di distruggere gli oggetti,
       per evitare crash in lv_event_mark_deleted durante le transizioni. */
    lvgl_page_chrome_remove();
    lv_indev_t *indev_l = lv_indev_get_next(NULL);
    while (indev_l) {
        lv_indev_reset(indev_l, NULL);
        indev_l = lv_indev_get_next(indev_l);
    }

    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lvgl_page_chrome_add(scr);

    const char *current_lang = device_config_get_ui_user_language();
    if (current_lang && strlen(current_lang) == 2) {
        strncpy(s_selected_user_lang, current_lang, sizeof(s_selected_user_lang) - 1);
        s_selected_user_lang[sizeof(s_selected_user_lang) - 1] = '\0';
    }

    const panel_language_option_t *active_opt = &s_panel_lang_opts[0];
    for (size_t i = 0; i < (sizeof(s_panel_lang_opts) / sizeof(s_panel_lang_opts[0])); i++) {
        if (strcmp(s_panel_lang_opts[i].code, s_selected_user_lang) == 0) {
            active_opt = &s_panel_lang_opts[i];
            break;
        }
    }

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Seleziona lingua");
    lv_obj_set_style_text_color(title, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(title, FONT_TITLE, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 78, 210);

    const int32_t btn_w = 620;
    const int32_t btn_h = 112;
    const int32_t gap = 16;
    const int32_t start_y = 360;

    for (size_t i = 0; i < (sizeof(s_panel_lang_opts) / sizeof(s_panel_lang_opts[0])); i++) {
        const panel_language_option_t *opt = &s_panel_lang_opts[i];
        bool selected = (strcmp(s_selected_user_lang, opt->code) == 0);

        lv_obj_t *btn = lv_button_create(scr);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, start_y + (int32_t)i * (btn_h + gap));
        style_lang_btn(btn, selected);

        char line[64] = {0};
        snprintf(line, sizeof(line), "%u %s", (unsigned)(i + 1), opt->label);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, line);
        lv_obj_set_style_text_color(lbl, selected ? COL_TEXT_DARK : COL_WHITE, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, FONT_LANG, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        if (opt->code) {
            lv_obj_t *img = lv_image_create(btn);
            lv_image_set_src(img, get_flag_src_for_language(opt->code));
            lv_image_set_scale(img, 256);
            lv_obj_align(img, LV_ALIGN_RIGHT_MID, 0, 0);
        }

        lv_obj_set_user_data(btn, (void *)opt);
        lv_obj_add_event_cb(btn, on_lang_btn, LV_EVENT_CLICKED, NULL);
    }

    ESP_LOGI(TAG, "[C] Pagina selezione lingua v2 visualizzata");
    lv_async_call(panel_reenable_scanner_async, NULL);

    /* [C] Timer timeout 60 s: torna automaticamente alla pagina chiamante */
    s_lang_ticks_left    = LANG_TIMEOUT_S;
    s_lang_timeout_timer = lv_timer_create(lang_timeout_cb, (uint32_t)LANG_TIMEOUT_S * 1000U, NULL);
    lv_timer_set_repeat_count(s_lang_timeout_timer, 1);
    s_lang_tick_timer    = lv_timer_create(lang_tick_cb, LANG_TICK_MS, NULL);

    /* [C] Label con conto alla rovescia in basso a destra */
    s_timeout_lbl = lv_label_create(scr);
    char ticks_buf[24] = {0};
    snprintf(ticks_buf, sizeof(ticks_buf), "(%d s)", LANG_TIMEOUT_S);
    lv_label_set_text(s_timeout_lbl, ticks_buf);
    lv_obj_set_style_text_color(s_timeout_lbl, lv_color_make(0x80, 0x80, 0x80), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_timeout_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(s_timeout_lbl, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
}
