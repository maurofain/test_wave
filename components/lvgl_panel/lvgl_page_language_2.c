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
static void panel_reenable_scanner(const char *context);

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
    lvgl_page_main_show();
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

void lvgl_page_language_2_show(void)
{
    lv_obj_t *scr = lv_scr_act();

    lvgl_page_main_deactivate();
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

    if (active_opt->code) {
        lv_obj_t *lang_img = lv_image_create(scr);
        lv_image_set_src(lang_img, get_flag_src_for_language(active_opt->code));
        lv_image_set_scale(lang_img, 1024);
        lv_obj_align(lang_img, LV_ALIGN_TOP_RIGHT, -34, 42);
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
            lv_image_set_scale(img, 1536);
            lv_obj_align(img, LV_ALIGN_RIGHT_MID, 0, 0);
        }

        lv_obj_set_user_data(btn, (void *)opt);
        lv_obj_add_event_cb(btn, on_lang_btn, LV_EVENT_CLICKED, NULL);
    }

    ESP_LOGI(TAG, "[C] Pagina selezione lingua v2 visualizzata");
    lv_async_call(panel_reenable_scanner_async, NULL);
}
