#include "lvgl_panel_pages.h"

#include "device_config.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include <dirent.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "lvgl_page_ads";

/* Advertisement page state */
static lv_obj_t *s_ad_scr = NULL;
static lv_obj_t *s_ad_img = NULL;
static lv_obj_t *s_img_container = NULL;
static lv_timer_t *s_ad_carousel_timer = NULL;

/* Image management */
static char **s_ad_images = NULL;
static uint32_t s_ad_image_count = 0;
static uint32_t s_ad_current_image = 0;
static uint32_t s_ad_rotation_ms = 30000;  /* Default 30 seconds */

#define COL_BG lv_color_make(0x00, 0x00, 0x00)

/**
 * @brief Load list of advertisement images from SPIFFS.
 *
 * Searches for files named img01.jpg, img02.jpg, etc.
 * Allocates memory for the file names - must be freed.
 */
static void load_ad_images(void)
{
    if (s_ad_images != NULL) {
        for (uint32_t i = 0; i < s_ad_image_count; i++) {
            free(s_ad_images[i]);
        }
        free(s_ad_images);
        s_ad_images = NULL;
    }
    s_ad_image_count = 0;
    s_ad_current_image = 0;

    /* Try to find images in /spiffs/img{01..99}.jpg */
    const char *ad_dir = "/spiffs";
    DIR *dir = opendir(ad_dir);
    if (!dir) {
        ESP_LOGW(TAG, "[L] Impossibile aprire la directory %s", ad_dir);
        return;
    }

    /* Count images first */
    struct dirent *entry;
    uint32_t count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            /* Check if filename matches pattern img{01..99}.jpg */
            if (strncmp(entry->d_name, "img", 3) == 0 && 
                strlen(entry->d_name) >= strlen("img01.jpg")) {
                if (strcmp(entry->d_name + strlen(entry->d_name) - 4, ".jpg") == 0) {
                    count++;
                }
            }
        }
    }

    if (count == 0) {
        ESP_LOGW(TAG, "[L] Nessuna immagine pubblicitaria trovata in %s", ad_dir);
        closedir(dir);
        return;
    }

    /* Allocate memory for image paths */
    s_ad_images = malloc(count * sizeof(char *));
    if (!s_ad_images) {
        ESP_LOGE(TAG, "[L] Allocazione memoria fallita per array immagini");
        closedir(dir);
        return;
    }

    /* Rewind and collect image names */
    rewinddir(dir);
    uint32_t idx = 0;
    while ((entry = readdir(dir)) != NULL && idx < count) {
        if (entry->d_type == DT_REG) {
            if (strncmp(entry->d_name, "img", 3) == 0 && 
                strlen(entry->d_name) >= strlen("img01.jpg")) {
                if (strcmp(entry->d_name + strlen(entry->d_name) - 4, ".jpg") == 0) {
                    s_ad_images[idx] = malloc(strlen("/spiffs/") + strlen(entry->d_name) + 1);
                    if (s_ad_images[idx]) {
                        sprintf(s_ad_images[idx], "/spiffs/%s", entry->d_name);
                        idx++;
                    }
                }
            }
        }
    }
    closedir(dir);
    s_ad_image_count = idx;

    ESP_LOGI(TAG, "[L] Caricate %u immagini pubblicitarie", s_ad_image_count);
}

/**
 * @brief Display the current advertisement image.
 */
static void display_current_ad_image(void)
{
    if (s_ad_image_count == 0 || s_ad_current_image >= s_ad_image_count) {
        return;
    }

    if (s_ad_img) {
        lv_image_set_src(s_ad_img, s_ad_images[s_ad_current_image]);
        ESP_LOGI(TAG, "[D] Immagine visualizzata: %s (numero %u di %u)",
                 s_ad_images[s_ad_current_image],
                 s_ad_current_image + 1,
                 s_ad_image_count);
    }
}

/**
 * @brief Carousel timer callback - rotate to next image.
 */
static void ad_carousel_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_ad_image_count == 0) {
        return;
    }

    s_ad_current_image = (s_ad_current_image + 1) % s_ad_image_count;
    display_current_ad_image();
}

/**
 * @brief Handle touch on advertisement screen - return to programs.
 */
static void on_ad_touch(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "[C] Schermata pubblicitaria toccata, ritorno ai programmi");
    
    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
        s_ad_carousel_timer = NULL;
    }

    /* Return to main program selection page */
    lvgl_page_main_show();
}

/**
 * @brief Show advertisement slideshow.
 *
 * Displays a carousel of JPG images (img01.jpg, img02.jpg, etc.)
 * centered on a black screen. Automatically rotates every 30 seconds.
 * Touch anywhere to return to program selection.
 */
void lvgl_page_ads_show(void)
{
    lv_obj_t *scr = lv_scr_act();

    /* Load configuration for rotation interval */
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->timeouts.ad_rotation_ms > 0) {
        s_ad_rotation_ms = cfg->timeouts.ad_rotation_ms;
    }

    /* Load advertisement images if not already loaded */
    if (s_ad_images == NULL) {
        load_ad_images();
    }

    if (s_ad_image_count == 0) {
        ESP_LOGW(TAG, "[C] Nessuna immagine pubblicitaria disponibile");
        /* Just return to main page */
        lvgl_page_main_show();
        return;
    }

    /* Clean screen and setup background */
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* Create image container */
    s_img_container = lv_obj_create(scr);
    lv_obj_set_size(s_img_container, LV_PCT(100), LV_PCT(100));
    lv_obj_center(s_img_container);
    lv_obj_set_style_bg_opa(s_img_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_img_container, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_img_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Create image object */
    s_ad_img = lv_image_create(s_img_container);
    lv_obj_center(s_ad_img);

    /* Add touch event to entire screen */
    lv_obj_add_event_cb(scr, on_ad_touch, LV_EVENT_CLICKED, NULL);

    /* Reset carousel state */
    s_ad_current_image = 0;
    display_current_ad_image();

    /* Create carousel timer */
    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
    }
    s_ad_carousel_timer = lv_timer_create(ad_carousel_timer_cb, s_ad_rotation_ms, NULL);

    s_ad_scr = scr;
    ESP_LOGI(TAG, "[C] Schermata pubblicitaria visualizzata con %u immagini, rotazione ogni %u ms",
             s_ad_image_count, s_ad_rotation_ms);
}

/**
 * @brief Deactivate and cleanup advertisement page.
 */
void lvgl_page_ads_deactivate(void)
{
    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
        s_ad_carousel_timer = NULL;
    }

    s_ad_img = NULL;
    s_img_container = NULL;
    s_ad_scr = NULL;
    s_ad_current_image = 0;

    /* Note: image list is kept loaded for next time */
}

/**
 * @brief Free all loaded advertisement images.
 * 
 * Call this when shutting down or changing advertisement set.
 */
void lvgl_page_ads_unload_images(void)
{
    if (s_ad_images != NULL) {
        for (uint32_t i = 0; i < s_ad_image_count; i++) {
            free(s_ad_images[i]);
        }
        free(s_ad_images);
        s_ad_images = NULL;
    }
    s_ad_image_count = 0;
    s_ad_current_image = 0;

    ESP_LOGI(TAG, "[U] Immagini pubblicitarie scaricate dalla memoria");
}
