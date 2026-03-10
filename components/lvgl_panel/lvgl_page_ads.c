#include "lvgl_panel_pages.h"

#include "device_config.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "driver/jpeg_decode.h"
#include "esp_lvgl_port.h"

#include <dirent.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "lvgl_page_ads";

/* Advertisement page state */
static lv_obj_t *s_ad_scr = NULL;
static lv_obj_t *s_ad_img = NULL;
static lv_obj_t *s_img_container = NULL;
static lv_obj_t *s_error_container = NULL;
static lv_obj_t *s_error_label = NULL;
static lv_obj_t *s_error_bg = NULL;
static lv_timer_t *s_ad_carousel_timer = NULL;

/* Image management */
static char **s_ad_images = NULL;
static uint32_t s_ad_image_count = 0;
static uint32_t s_ad_current_image = 0;
static uint32_t s_ad_rotation_ms = 30000;  /* Default 30 seconds */

/* Static buffer for image loading in PSRAM */
static uint8_t *s_image_buffer = NULL;
static uint8_t *s_rgb565_buffer = NULL;
static lv_image_dsc_t s_slideshow_img_dsc = {0};
static jpeg_decoder_handle_t s_jpeg_decoder = NULL;
static uint8_t *s_placeholder_buffer = NULL;  /* Static placeholder to avoid stack allocation */
#define IMAGE_BUFFER_SIZE (256 * 1024)  /* 256KB buffer */
#define PLACEHOLDER_SIZE (320 * 240 * 2)  /* 320x240 RGB565 placeholder */

#define COL_BG lv_color_make(0x00, 0x00, 0x00)

/**
 * @brief Load list of advertisement images from SPIFFS.
 *
 * Searches for files named img01.jpg, img02.jpg, etc.
 * Allocates memory for the file names - must be freed.
 */
static void load_ad_images(void)
{
    /* Allocate image buffer in PSRAM if not already allocated */
    if (s_image_buffer == NULL) {
        s_image_buffer = heap_caps_malloc(IMAGE_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
        if (!s_image_buffer) {
            ESP_LOGE(TAG, "[L] Impossibile allocare buffer immagine in PSRAM");
            return;
        }
        ESP_LOGI(TAG, "[L] Buffer immagine %dB allocato in PSRAM", IMAGE_BUFFER_SIZE);
    }

    /* Allocate placeholder buffer once */
    if (s_placeholder_buffer == NULL) {
        s_placeholder_buffer = heap_caps_malloc(PLACEHOLDER_SIZE, MALLOC_CAP_SPIRAM);
        if (!s_placeholder_buffer) {
            ESP_LOGE(TAG, "[L] Impossibile allocare placeholder buffer in PSRAM");
            return;
        }
        
        // Pre-fill placeholder with gradient pattern
        uint16_t *placeholder = (uint16_t*)s_placeholder_buffer;
        for (size_t i = 0; i < PLACEHOLDER_SIZE/2; i++) {
            uint8_t val = (i * 255) / (PLACEHOLDER_SIZE/2);
            placeholder[i] = ((val >> 3) << 11) | ((val >> 2) << 5) | (val >> 3);
        }
        ESP_LOGI(TAG, "[L] Placeholder buffer %dB allocato e pre-compilato", PLACEHOLDER_SIZE);
    }

    /* Initialize JPEG decoder once */
    if (s_jpeg_decoder == NULL) {
        jpeg_decode_engine_cfg_t dec_cfg = {0};
        esp_err_t err = jpeg_new_decoder_engine(&dec_cfg, &s_jpeg_decoder);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "[L] Hardware JPEG decoder failed: %s, using software fallback", esp_err_to_name(err));
            s_jpeg_decoder = NULL; // Keep NULL to use software fallback
        } else {
            ESP_LOGI(TAG, "[L] Hardware JPEG decoder initialized");
        }
    }

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
        ESP_LOGE(TAG, "[L] Impossibile aprire la directory %s", ad_dir);
        return;
    }
    ESP_LOGI(TAG, "[L] Directory %s aperta correttamente", ad_dir);

    /* Count images first */
    struct dirent *entry;
    uint32_t count = 0;
    while ((entry = readdir(dir)) != NULL) {
        /* Check if filename matches pattern img{01..99}.jpg */
        if (strncmp(entry->d_name, "img", 3) == 0 && 
            strlen(entry->d_name) >= strlen("img01.jpg")) {
            if (strcmp(entry->d_name + strlen(entry->d_name) - 4, ".jpg") == 0) {
                ESP_LOGI(TAG, "[L] Trovata immagine: %s (type: %d)", entry->d_name, entry->d_type);
                count++;
            }
        }
    }

    if (count == 0) {
        ESP_LOGW(TAG, "[L] Nessuna immagine pubblicitaria trovata in %s", ad_dir);
        ESP_LOGI(TAG, "[L] Contenuto directory:");
        rewinddir(dir);
        struct dirent *debug_entry;
        while ((debug_entry = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "[L]  - %s (type: %d)", debug_entry->d_name, debug_entry->d_type);
        }
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
        /* Check if filename matches pattern img{01..99}.jpg */
        if (strncmp(entry->d_name, "img", 3) == 0 && 
            strlen(entry->d_name) >= strlen("img01.jpg")) {
            if (strcmp(entry->d_name + strlen(entry->d_name) - 4, ".jpg") == 0) {
                s_ad_images[idx] = malloc(strlen("S:/spiffs/") + strlen(entry->d_name) + 1);
                if (s_ad_images[idx]) {
                    sprintf(s_ad_images[idx], "S:/spiffs/%s", entry->d_name);
                    ESP_LOGI(TAG, "[L] Caricata: %s", s_ad_images[idx]);
                    idx++;
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
/**
 * @brief Decode JPEG to RGB565 using hardware decoder.
 */
static esp_err_t decode_jpeg_rgb565(const uint8_t *jpg, size_t jpg_size, 
                                   uint8_t **out_buf, size_t *out_buf_size, 
                                   int *out_w, int *out_h)
{
    if (!jpg || !out_buf || !out_buf_size || !out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }

    // Get JPEG info first
    jpeg_decode_picture_info_t info;
    esp_err_t err = jpeg_decoder_get_info(jpg, jpg_size, &info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[D] Failed to get JPEG info: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "[D] JPEG info: %dx%d", info.width, info.height);

    const size_t decoded_size = (size_t)info.width * (size_t)info.height * 2;
    size_t alloc_size = ((decoded_size + 63) / 64) * 64; // 64-byte alignment
    
    ESP_LOGI(TAG, "[D] JPEG decode: allocating %zu bytes (decoded_size=%zu)", alloc_size, decoded_size);
    
    uint8_t *decoded = (uint8_t *)heap_caps_aligned_alloc(64, alloc_size,
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (!decoded) {
        ESP_LOGE(TAG, "[D] Failed to allocate %zu bytes for JPEG decode", alloc_size);
        return ESP_ERR_NO_MEM;
    }

    // Use global decoder if available, otherwise use software fallback
    if (s_jpeg_decoder != NULL) {
        // Hardware decoder path
        uint32_t out_size = 0;
        jpeg_decode_cfg_t cfg = {
            .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
            .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
            .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
        };
        err = jpeg_decoder_process(s_jpeg_decoder, &cfg, jpg, (uint32_t)jpg_size, decoded, (uint32_t)decoded_size, &out_size);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "[D] Hardware JPEG decode failed: %s", esp_err_to_name(err));
            free(decoded);
            return err;
        }
    } else {
        // Software fallback - create a simple RGB565 placeholder with the correct dimensions
        ESP_LOGW(TAG, "[D] Using software fallback - creating placeholder %dx%d", info.width, info.height);
        
        // Recalculate buffer size based on actual image dimensions
        const size_t actual_decoded_size = (size_t)info.width * (size_t)info.height * 2;
        const size_t actual_alloc_size = ((actual_decoded_size + 63) / 64) * 64;
        
        // Reallocate buffer if needed
        if (actual_alloc_size > alloc_size) {
            free(decoded);
            alloc_size = actual_alloc_size;
            decoded = (uint8_t *)heap_caps_aligned_alloc(64, alloc_size,
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
            if (!decoded) {
                ESP_LOGE(TAG, "[D] Failed to reallocate %zu bytes for fallback", alloc_size);
                return ESP_ERR_NO_MEM;
            }
        }
        
        // Fill with a simple gradient pattern using actual image dimensions
        uint16_t *rgb565_buf = (uint16_t*)decoded;
        for (size_t i = 0; i < actual_decoded_size/2; i++) {
            // Create a blue gradient pattern
            uint8_t blue = (i * 255) / (actual_decoded_size/2);
            rgb565_buf[i] = ((blue >> 3) << 11) | ((blue >> 2) << 5) | (blue >> 3);
        }
    }

    *out_buf = decoded;
    *out_buf_size = decoded_size;
    *out_w = info.width;
    *out_h = info.height;
    
    ESP_LOGI(TAG, "[D] JPEG decode completed successfully");
    return ESP_OK;
}

/**
 * @brief Show error message in the error zone.
 * 
 * @param error_message The error message to display
 */
static void show_error_message(const char *error_message)
{
    if (s_error_label && s_error_container) {
        lv_label_set_text(s_error_label, error_message);
        lv_obj_clear_flag(s_error_container, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "[E] Errore mostrato: %s", error_message);
    }
}

/**
 * @brief Hide error message.
 */
static void hide_error_message(void)
{
    if (s_error_container) {
        lv_obj_add_flag(s_error_container, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGD(TAG, "[E] Errore nascosto");
    }
}

static void display_current_ad_image(void)
{
    if (s_ad_image_count == 0 || s_ad_current_image >= s_ad_image_count) {
        return;
    }

    if (s_ad_images && s_ad_images[s_ad_current_image]) {
        const char *jpg_path = s_ad_images[s_ad_current_image];
        ESP_LOGI(TAG, "[D] Slideshow: %s", jpg_path);
        
        /* Try to load and display the actual JPEG image */
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "%s", jpg_path + 2); /* Skip "S:" */
        
        FILE *f = fopen(file_path, "rb");
        if (!f) {
            ESP_LOGE(TAG, "[D] Impossibile aprire il file: %s", file_path);
            
            /* Fallback to red rectangle */
            lvgl_port_lock(0);
            if (s_ad_img) {
                lv_image_set_src(s_ad_img, NULL);
                lv_obj_set_style_bg_color(s_ad_img, lv_color_hex(0xFF0000), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(s_ad_img, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_size(s_ad_img, 100, 100);
                lv_obj_center(s_ad_img);
            }
            hide_error_message();
            lvgl_port_unlock();
            return;
        }
        
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        uint8_t *jpg = malloc(file_size);
        if (!jpg) {
            ESP_LOGE(TAG, "[D] Impossibile allocare memoria per JPEG");
            fclose(f);
            
            /* Fallback to red rectangle */
            lvgl_port_lock(0);
            if (s_ad_img) {
                lv_image_set_src(s_ad_img, NULL);
                lv_obj_set_style_bg_color(s_ad_img, lv_color_hex(0xFF0000), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(s_ad_img, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_size(s_ad_img, 100, 100);
                lv_obj_center(s_ad_img);
            }
            hide_error_message();
            lvgl_port_unlock();
            return;
        }
        
        size_t jpg_size = fread(jpg, 1, file_size, f);
        fclose(f);
        
        if (jpg_size != file_size) {
            ESP_LOGE(TAG, "[D] Errore lettura file: %zu/%ld bytes", jpg_size, file_size);
            free(jpg);
            
            /* Fallback to red rectangle */
            lvgl_port_lock(0);
            if (s_ad_img) {
                lv_image_set_src(s_ad_img, NULL);
                lv_obj_set_style_bg_color(s_ad_img, lv_color_hex(0xFF0000), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(s_ad_img, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_size(s_ad_img, 100, 100);
                lv_obj_center(s_ad_img);
            }
            hide_error_message();
            lvgl_port_unlock();
            return;
        }
        
        /* Try to decode JPEG */
        uint8_t *rgb565 = NULL;
        size_t rgb565_size = 0;
        int img_w = 0, img_h = 0;
        esp_err_t err = decode_jpeg_rgb565(jpg, jpg_size, &rgb565, &rgb565_size, &img_w, &img_h);
        free(jpg);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "[D] JPEG decode failed: %s", esp_err_to_name(err));
            
            /* Fallback to red rectangle */
            lvgl_port_lock(0);
            if (s_ad_img) {
                lv_image_set_src(s_ad_img, NULL);
                lv_obj_set_style_bg_color(s_ad_img, lv_color_hex(0xFF0000), LV_PART_MAIN);
                lv_obj_set_style_bg_opa(s_ad_img, LV_OPA_COVER, LV_PART_MAIN);
                lv_obj_set_size(s_ad_img, 100, 100);
                lv_obj_center(s_ad_img);
            }
            hide_error_message();
            lvgl_port_unlock();
            return;
        }
        
        /* Success! Create proper LVGL descriptor */
        if (s_rgb565_buffer) {
            free(s_rgb565_buffer);
        }
        
        s_rgb565_buffer = rgb565;
        s_slideshow_img_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        s_slideshow_img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        s_slideshow_img_dsc.header.flags = 0;
        s_slideshow_img_dsc.header.w = (uint32_t)img_w;
        s_slideshow_img_dsc.header.h = (uint32_t)img_h;
        s_slideshow_img_dsc.header.stride = img_w * 2;
        s_slideshow_img_dsc.data_size = rgb565_size;
        s_slideshow_img_dsc.data = s_rgb565_buffer;
        
        /* Display the actual image */
        lvgl_port_lock(0);
        if (s_ad_img && s_slideshow_img_dsc.data) {
            lv_image_set_src(s_ad_img, NULL); /* Clear first to force update */
            lv_image_set_src(s_ad_img, &s_slideshow_img_dsc);
            lv_obj_center(s_ad_img);
        } else {
            ESP_LOGW(TAG, "[D] Cannot display image: s_ad_img=%p, data=%p", 
                     (void*)s_ad_img, s_slideshow_img_dsc.data);
        }
        hide_error_message();
        lvgl_port_unlock();
        
        ESP_LOGI(TAG, "[D] Immagine JPEG visualizzata: %s (%dx%d RGB565)", jpg_path, img_w, img_h);
    }
}

/**
 * @brief Carousel timer callback - rotate to next image.
 */
static void ad_carousel_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    
    /* Quick check if page is still active before doing expensive operations */
    if (s_ad_img == NULL || s_slideshow_img_dsc.data == NULL) {
        ESP_LOGD(TAG, "[D] Carousel timer: page inactive or no image data, skipping rotation");
        return;
    }
    
    ESP_LOGI(TAG, "[D] Carousel timer callback triggered");
    
    if (s_ad_image_count == 0) {
        return;
    }

    /* Move to next image */
    s_ad_current_image = (s_ad_current_image + 1) % s_ad_image_count;
    
    ESP_LOGI(TAG, "[D] Rotating to image %u/%u", s_ad_current_image + 1, s_ad_image_count);
    display_current_ad_image();
}

/**
 * @brief Handle touch on advertisement screen - return to programs.
 */
static void on_ad_touch(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "[C] Schermata pubblicitaria toccata, ritorno ai programmi");
    
    /* Stop carousel timer immediately - this is critical for responsive touch */
    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
        s_ad_carousel_timer = NULL;
        ESP_LOGI(TAG, "[C] Carousel timer stopped");
    }

    /* Mark page as inactive to prevent further image updates */
    s_ad_img = NULL;
    
    /* Clear image descriptor to prevent any further operations */
    memset(&s_slideshow_img_dsc, 0, sizeof(s_slideshow_img_dsc));
    
    /* Return to main program selection page */
    lvgl_page_main_show();
    
    ESP_LOGI(TAG, "[C] Touch handled, page transition completed");
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
        ESP_LOGI(TAG, "[C] Ritorno alla pagina principale");
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

    /* Create main container with border layout like the image */
    s_img_container = lv_obj_create(scr);
    lv_obj_set_size(s_img_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(s_img_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_img_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_img_container, 20, LV_PART_MAIN);  /* 20px padding like in image */
    lv_obj_remove_flag(s_img_container, LV_OBJ_FLAG_SCROLLABLE);

    /* Create top section (logo area) */
    lv_obj_t *top_section = lv_obj_create(s_img_container);
    lv_obj_set_size(top_section, LV_PCT(100), 80);  /* Fixed height for logo area */
    lv_obj_align(top_section, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top_section, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(top_section, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(top_section, 0, LV_PART_MAIN);
    lv_obj_remove_flag(top_section, LV_OBJ_FLAG_SCROLLABLE);

    /* Top section - empty for clean look */
    /* No logo text for advertisement page */

    /* Create central advertisement area (main content) */
    lv_obj_t *ad_section = lv_obj_create(s_img_container);
    /* Calculate height: total(100%) - top(80px) - bottom(60px) - padding(40px) */
    lv_obj_set_size(ad_section, LV_PCT(100), LV_PCT(100));
    lv_obj_align(ad_section, LV_ALIGN_TOP_MID, 0, 80);  /* Position below logo */
    lv_obj_set_style_bg_opa(ad_section, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ad_section, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ad_section, 0, LV_PART_MAIN);
    lv_obj_remove_flag(ad_section, LV_OBJ_FLAG_SCROLLABLE);

    /* Create advertisement image object in central area */
    s_ad_img = lv_image_create(ad_section);
    lv_obj_set_size(s_ad_img, LV_PCT(100), LV_PCT(100));  /* Fill the central area */
    lv_obj_align(s_ad_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_ad_img, lv_color_hex(0x333333), LV_PART_MAIN);  /* Dark gray background */
    lv_obj_set_style_bg_opa(s_ad_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_ad_img, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_ad_img, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_ad_img, LV_OPA_50, LV_PART_MAIN);

    /* Create bottom section (status/info area) */
    lv_obj_t *bottom_section = lv_obj_create(s_img_container);
    lv_obj_set_size(bottom_section, LV_PCT(100), 80);  /* Increased height for button */
    lv_obj_align(bottom_section, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(bottom_section, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bottom_section, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bottom_section, 0, LV_PART_MAIN);
    lv_obj_remove_flag(bottom_section, LV_OBJ_FLAG_SCROLLABLE);

    /* Create 'Selezione Lavaggio' button */
    lv_obj_t *select_btn = lv_button_create(bottom_section);
    lv_obj_set_size(select_btn, 300, 60);
    lv_obj_align(select_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(select_btn, lv_color_hex(0x007BFF), LV_PART_MAIN); /* Blue button */
    lv_obj_set_style_radius(select_btn, 10, LV_PART_MAIN);
    lv_obj_add_event_cb(select_btn, on_ad_touch, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(select_btn);
    lv_label_set_text(btn_label, "Selezione Lavaggio");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(btn_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(btn_label, LV_ALIGN_CENTER, 0, 0);

    /* Adjust central area height to accommodate larger bottom section */
    lv_obj_set_size(ad_section, LV_PCT(100), LV_PCT(80)); /* Make room for bottom section */

    /* Create error message zone (initially hidden) */
    s_error_container = lv_obj_create(s_img_container);
    lv_obj_set_size(s_error_container, LV_PCT(100), 60);  /* Fixed height for error area */
    lv_obj_align(s_error_container, LV_ALIGN_BOTTOM_MID, 0, -80);  /* Position just above the button area */
    lv_obj_set_style_bg_opa(s_error_container, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_error_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_error_container, 10, LV_PART_MAIN);
    lv_obj_remove_flag(s_error_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_error_container, LV_OBJ_FLAG_HIDDEN);  /* Initially hidden */

    /* Create error background (semi-transparent red) */
    s_error_bg = lv_obj_create(s_error_container);
    lv_obj_set_size(s_error_bg, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_error_bg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_error_bg, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_error_bg, LV_OPA_30, LV_PART_MAIN);  /* Semi-transparent red */
    lv_obj_set_style_border_width(s_error_bg, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_error_bg, lv_color_hex(0xFF6666), LV_PART_MAIN);
    lv_obj_set_style_radius(s_error_bg, 8, LV_PART_MAIN);

    /* Create error label */
    s_error_label = lv_label_create(s_error_bg);
    lv_label_set_long_mode(s_error_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_error_label, "");  /* Empty by default */
    lv_obj_set_style_text_font(s_error_label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_error_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(s_error_label, LV_ALIGN_CENTER, 0, 0);

    /* Reset carousel state */
    s_ad_current_image = 0;
    display_current_ad_image();

    /* Create carousel timer */
    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
        s_ad_carousel_timer = NULL;
    }
    s_ad_carousel_timer = lv_timer_create(ad_carousel_timer_cb, s_ad_rotation_ms, NULL);

    ESP_LOGI(TAG, "[C] Carousel timer created with %u ms interval", s_ad_rotation_ms);
    ESP_LOGI(TAG, "[C] Schermata pubblicitaria visualizzata con %u immagini, rotazione ogni %u ms", 
             s_ad_image_count, s_ad_rotation_ms);
    s_ad_scr = scr;
}

void lvgl_page_ads_deactivate(void)
{
    if (s_ad_carousel_timer) {
        lv_timer_delete(s_ad_carousel_timer);
        s_ad_carousel_timer = NULL;
    }

    if (s_ad_img) {
        lv_obj_delete(s_ad_img);
        s_ad_img = NULL;
    }
    if (s_img_container) {
        lv_obj_delete(s_img_container);
        s_img_container = NULL;
    }
    
    /* Cleanup error zone objects */
    s_error_label = NULL;
    s_error_bg = NULL;
    s_error_container = NULL;
    
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
    
    /* Free image buffer */
    if (s_image_buffer != NULL) {
        heap_caps_free(s_image_buffer);
        s_image_buffer = NULL;
        ESP_LOGI(TAG, "[L] Buffer immagine liberato");
    }
    
    /* Free RGB565 buffer */
    if (s_rgb565_buffer != NULL) {
        free(s_rgb565_buffer);
        s_rgb565_buffer = NULL;
        ESP_LOGI(TAG, "[L] Buffer RGB565 liberato");
    }
    
    /* Cleanup JPEG decoder */
    if (s_jpeg_decoder != NULL) {
        jpeg_del_decoder_engine(s_jpeg_decoder);
        s_jpeg_decoder = NULL;
        ESP_LOGI(TAG, "[L] JPEG decoder deallocato");
    }
    
    /* Free placeholder buffer */
    if (s_placeholder_buffer != NULL) {
        heap_caps_free(s_placeholder_buffer);
        s_placeholder_buffer = NULL;
        ESP_LOGI(TAG, "[L] Placeholder buffer liberato");
    }
    s_ad_current_image = 0;

    ESP_LOGI(TAG, "[U] Immagini pubblicitarie scaricate dalla memoria");
}
