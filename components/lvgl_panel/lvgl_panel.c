#include "lvgl_panel.h"
#include "lvgl_panel_pages.h"
#include "init.h"

#include "lvgl.h"
#include "bsp/esp32_p4_nano.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <stdio.h>

extern const lv_font_t arial96;

static const char *TAG = "lvgl_panel";

#define COL_BG    lv_color_make(0x1a, 0x1a, 0x2e)
#define COL_BOOT_BG lv_color_make(0x00, 0x00, 0x00)
#define COL_WHITE lv_color_make(0xEE, 0xEE, 0xEE)
#define BOOT_LOGO_SPIFFS_PATH "/spiffs/logo.jpg"
#define BOOT_LOGO_LVGL_PATH   "S:/spiffs/logo.jpg"

static bool panel_boot_logo_file_exists(void)
{
    FILE *f = fopen(BOOT_LOGO_SPIFFS_PATH, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

static void panel_show_boot_logo_screen(void)
{
    lv_obj_t *scr = lv_scr_act();

    lvgl_page_main_deactivate();
    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, COL_BOOT_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    if (panel_boot_logo_file_exists()) {
        lv_obj_t *logo_img = lv_image_create(scr);
        lv_image_set_src(logo_img, BOOT_LOGO_LVGL_PATH);
        lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, 0);
        ESP_LOGI(TAG, "[C] Pagina logo boot visualizzata da SPIFFS (%s)", BOOT_LOGO_SPIFFS_PATH);
        return;
    }

    lv_obj_t *logo_lbl = lv_label_create(scr);
    lv_label_set_text(logo_lbl, "MicroHard");
    lv_obj_set_style_text_color(logo_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(logo_lbl, &arial96, LV_PART_MAIN);
    lv_obj_align(logo_lbl, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGW(TAG, "[C] Logo SPIFFS non trovato (%s), uso fallback testuale", BOOT_LOGO_SPIFFS_PATH);
}

void lvgl_panel_show(void)
{
    lvgl_panel_show_boot_logo();
}

void lvgl_panel_show_boot_logo(void)
{
    if (!bsp_display_lock(0)) {
        ESP_LOGW(TAG, "[C] LVGL lock fallito in lvgl_panel_show_boot_logo");
        return;
    }

    panel_show_boot_logo_screen();

    bsp_display_unlock();
}

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

    lvgl_page_language_show();

    bsp_display_unlock();

    ESP_LOGI(TAG, "[C] Pagina selezione lingua visualizzata");
}

void lvgl_panel_refresh_texts(void)
{
    if (!bsp_display_lock(100)) {
        return;
    }

    lvgl_page_main_refresh_texts();

    bsp_display_unlock();
}

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
