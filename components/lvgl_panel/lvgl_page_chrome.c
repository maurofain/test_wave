#include "lvgl_page_chrome.h"
#include "language_flags.h"
#include "lvgl_panel.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <stdio.h>
#include <time.h>

extern const lv_font_t GoogleSans35;

static lv_obj_t      *s_chrome_time_lbl   = NULL;
static lv_timer_t    *s_chrome_time_timer  = NULL;
static lv_obj_t      *s_chrome_status_icons[4] = {NULL, NULL, NULL, NULL};
static bool           s_chrome_status_ok[4] = {true, true, true, true};
static lv_event_cb_t  s_flag_cb            = NULL;  /* callback per click bandiera */
static void          *s_flag_ud            = NULL;

static const char *s_chrome_status_icon_ok_paths[4] = {
    "S:/spiffs/icons/CloudOk.png",
    "S:/spiffs/icons/CreditCardOk.png",
    "S:/spiffs/icons/MoneteOk3.png",
    "S:/spiffs/icons/QrOk.png",
};

static const char *s_chrome_status_icon_ko_paths[4] = {
    "S:/spiffs/icons/CloudKo.png",
    "S:/spiffs/icons/CreditCardKo.png",
    "S:/spiffs/icons/MoneteKo.png",
    "S:/spiffs/icons/QrKo.png",
};

static const uint8_t *s_chrome_status_icon_ok_data[4] = {NULL, NULL, NULL, NULL};
static size_t         s_chrome_status_icon_ok_size[4] = {0, 0, 0, 0};
static const uint8_t *s_chrome_status_icon_ko_data[4] = {NULL, NULL, NULL, NULL};
static size_t         s_chrome_status_icon_ko_size[4] = {0, 0, 0, 0};
static char           s_chrome_status_icon_ok_normalized[4][256] = {{0}};
static char           s_chrome_status_icon_ko_normalized[4][256] = {{0}};

static const char *chrome_normalize_spiffs_path(const char *path, char *out_buf, size_t out_buf_size)
{
    if (!path || !out_buf || out_buf_size == 0) {
        return NULL;
    }

    if (strncmp(path, "S:/", 3) == 0) {
        snprintf(out_buf, out_buf_size, "/%s", path + 3);
    } else {
        strncpy(out_buf, path, out_buf_size);
        out_buf[out_buf_size - 1] = '\0';
    }

    return out_buf;
}

static uint8_t *chrome_load_file_to_psram(const char *path, size_t *out_size)
{
    if (!path || !out_size) {
        return NULL;
    }

    char normalized_path[256];
    const char *path_to_open = chrome_normalize_spiffs_path(path, normalized_path, sizeof(normalized_path));
    if (!path_to_open) {
        return NULL;
    }

    FILE *f = fopen(path_to_open, "rb");
    if (!f) {
        ESP_LOGW("lvgl_page_chrome", "Impossibile aprire l'icona SPIFFS: %s", path_to_open);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long len = ftell(f);
    if (len <= 0) {
        fclose(f);
        return NULL;
    }

    rewind(f);
    size_t size = (size_t)len;
    uint8_t *data = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!data) {
        ESP_LOGE("lvgl_page_chrome", "PSRAM alloc fallita per %s (%u bytes)", path, (unsigned)size);
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, 1, size, f);
    fclose(f);
    if (read != size) {
        ESP_LOGE("lvgl_page_chrome", "Lettura incompleta %s: %u/%u", path, (unsigned)read, (unsigned)size);
        heap_caps_free(data);
        return NULL;
    }

    *out_size = size;
    return data;
}

static const char *chrome_get_status_icon_path(int index)
{
    if (index < 0 || index >= 4) {
        return NULL;
    }

    const char *raw_path = s_chrome_status_ok[index]
        ? s_chrome_status_icon_ok_paths[index]
        : s_chrome_status_icon_ko_paths[index];
    char *normalized = s_chrome_status_ok[index]
        ? s_chrome_status_icon_ok_normalized[index]
        : s_chrome_status_icon_ko_normalized[index];

    if (!raw_path) {
        return NULL;
    }

    if (!normalized[0]) {
        chrome_normalize_spiffs_path(raw_path, normalized, sizeof(s_chrome_status_icon_ok_normalized[index]));
    }

    return normalized[0] ? normalized : NULL;
}

static const void *chrome_get_status_icon_src(int index)
{
    if (index < 0 || index >= 4) {
        return NULL;
    }

    const uint8_t *data = s_chrome_status_ok[index]
        ? s_chrome_status_icon_ok_data[index]
        : s_chrome_status_icon_ko_data[index];

    if (data) {
        return data;
    }

    const char *path = chrome_get_status_icon_path(index);
    if (path) {
        ESP_LOGW("lvgl_page_chrome", "Icona chrome %d non pre-caricata in PSRAM, uso fallback file %s", index, path);
    } else {
        ESP_LOGW("lvgl_page_chrome", "Icona chrome %d non pre-caricata e path non disponibile", index);
    }
    return (const void *)path;
}

static void chrome_update_status_icons(void)
{
    for (int i = 0; i < 4; i++) {
        if (!s_chrome_status_icons[i] || !lv_obj_is_valid(s_chrome_status_icons[i])) {
            continue;
        }
        const void *src = chrome_get_status_icon_src(i);
        if (src) {
            lv_image_set_src(s_chrome_status_icons[i], src);
        }
    }
}

static void chrome_update_time_label(void)
{
    if (!s_chrome_time_lbl || !lv_obj_is_valid(s_chrome_time_lbl)) {
        return;
    }

    char time_buf[9] = "--:--:--";
    time_t now = time(NULL);
    if (now > 0) {
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        /* Mostra l'ora solo se l'anno è plausibile (NTP sincronizzato) */
        if (tm_now.tm_year + 1900 >= 2020) {
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_now);
        }
    }
    lv_label_set_text(s_chrome_time_lbl, time_buf);
}

static void chrome_time_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    chrome_update_time_label();
}

void lvgl_page_chrome_add(lv_obj_t *scr)
{
    if (!scr) {
        return;
    }

    if (s_chrome_time_timer) {
        lv_timer_del(s_chrome_time_timer);
        s_chrome_time_timer = NULL;
    }

    s_chrome_time_lbl = lv_label_create(scr);
    lv_obj_set_style_text_color(s_chrome_time_lbl, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_chrome_time_lbl, &GoogleSans35, LV_PART_MAIN);
    lv_obj_align(s_chrome_time_lbl, LV_ALIGN_TOP_LEFT, 18, 14);
    chrome_update_time_label();
    s_chrome_time_timer = lv_timer_create(chrome_time_timer_cb, 1000, NULL);

    const int32_t icon_size = 48;
    const int32_t gap = 12;
    const int32_t right_margin = 18;
    const int32_t top_margin = 18;

    const char *lang = lvgl_panel_get_runtime_language();
    if (!lang || lang[0] == '\0') {
        lang = "it";
    }

#if (defined(LV_USE_LODEPNG) && (LV_USE_LODEPNG != 0)) || \
    (defined(LV_USE_PNG) && (LV_USE_PNG != 0))
    const void *flag_src = (const void *)get_flag_bitmap_for_language(lang);
    if (!flag_src) {
        flag_src = get_flag_src_for_language(lang);
    }
#else
    const void *flag_src = get_flag_src_for_language(lang);
#endif

    lv_obj_t *flag = lv_image_create(scr);
    lv_image_set_src(flag, flag_src);
    lv_image_set_scale(flag, 256);
    lv_obj_align(flag, LV_ALIGN_TOP_RIGHT, -right_margin, top_margin + icon_size + gap);

    const int32_t icons_top = top_margin;
    for (int i = 0; i < 4; i++) {
        s_chrome_status_icons[i] = lv_image_create(scr);
        lv_obj_set_size(s_chrome_status_icons[i], icon_size, icon_size);
        lv_image_set_src(s_chrome_status_icons[i], chrome_get_status_icon_src(i));
        lv_image_set_scale(s_chrome_status_icons[i], 256);
        lv_obj_set_style_pad_all(s_chrome_status_icons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_chrome_status_icons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_chrome_status_icons[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_align(s_chrome_status_icons[i], LV_ALIGN_TOP_RIGHT,
                     -(right_margin + (3 - i) * (icon_size + gap)),
                     icons_top);
    }

    /* [C] Se la pagina ha registrato un callback, rende la bandiera cliccabile */
    if (s_flag_cb) {
        lv_obj_add_flag(flag, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(flag, LV_OPA_TRANSP, LV_STATE_PRESSED | LV_PART_MAIN);
        lv_obj_add_event_cb(flag, s_flag_cb, LV_EVENT_CLICKED, s_flag_ud);
    }
}

void lvgl_page_chrome_remove(void)
{
    /* [C] Elimina il timer prima che lv_obj_clean distrugga la label;
       NON elimina le widget (ci pensa lv_obj_clean del chiamante). */
    if (s_chrome_time_timer) {
        lv_timer_delete(s_chrome_time_timer);
        s_chrome_time_timer = NULL;
    }
    s_chrome_time_lbl = NULL;
    /* [C] Azzera il callback bandiera: ogni pagina lo re-imposta prima di chrome_add */
    s_flag_cb = NULL;
    s_flag_ud = NULL;
}

void lvgl_page_chrome_preload_status_icons(void)
{
    for (int i = 0; i < 4; i++) {
        if (!s_chrome_status_icon_ok_data[i]) {
            s_chrome_status_icon_ok_data[i] = chrome_load_file_to_psram(
                s_chrome_status_icon_ok_paths[i], &s_chrome_status_icon_ok_size[i]);
            if (!s_chrome_status_icon_ok_data[i]) {
                ESP_LOGW("lvgl_page_chrome", "Impossibile pre-caricare icona OK %s", s_chrome_status_icon_ok_paths[i]);
            }
        }
        if (!s_chrome_status_icon_ko_data[i]) {
            s_chrome_status_icon_ko_data[i] = chrome_load_file_to_psram(
                s_chrome_status_icon_ko_paths[i], &s_chrome_status_icon_ko_size[i]);
            if (!s_chrome_status_icon_ko_data[i]) {
                ESP_LOGW("lvgl_page_chrome", "Impossibile pre-caricare icona KO %s", s_chrome_status_icon_ko_paths[i]);
            }
        }
    }
}

void lvgl_page_chrome_set_flag_callback(lv_event_cb_t cb, void *user_data)
{
    s_flag_cb = cb;
    s_flag_ud = user_data;
}

void lvgl_page_chrome_set_status_icon_state(uint8_t idx, bool ok)
{
    if (idx >= 4) {
        return;
    }
    s_chrome_status_ok[idx] = ok;
    chrome_update_status_icons();
}

void lvgl_page_chrome_set_status_icons(bool cloud_ok, bool card_ok, bool coin_ok, bool qr_ok)
{
    s_chrome_status_ok[0] = cloud_ok;
    s_chrome_status_ok[1] = card_ok;
    s_chrome_status_ok[2] = coin_ok;
    s_chrome_status_ok[3] = qr_ok;
    chrome_update_status_icons();
}
