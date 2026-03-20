#include "lvgl_page_chrome.h"
#include "language_flags.h"
#include "lvgl_panel.h"

#include <time.h>

extern const lv_font_t GoogleSans35;

static lv_obj_t      *s_chrome_time_lbl   = NULL;
static lv_timer_t    *s_chrome_time_timer  = NULL;
static lv_event_cb_t  s_flag_cb            = NULL;  /* callback per click bandiera */
static void          *s_flag_ud            = NULL;

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

    const int32_t circle_size = 14;
    const int32_t gap = 12;
    const int32_t right_margin = 18;
    const int32_t top_margin = 18;

    for (int i = 0; i < 4; i++) {
        lv_obj_t *dot = lv_obj_create(scr);
        lv_obj_set_size(dot, circle_size, circle_size);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_width(dot, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(dot, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
        lv_obj_set_style_pad_all(dot, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(dot, 0, LV_PART_MAIN);
        lv_obj_align(dot,
                     LV_ALIGN_TOP_RIGHT,
                     -(right_margin + (3 - i) * (circle_size + gap)),
                     top_margin);
    }

    const char *lang = lvgl_panel_get_runtime_language();
    if (!lang || lang[0] == '\0') {
        lang = "it";
    }

    lv_obj_t *flag = lv_image_create(scr);
    lv_image_set_src(flag, get_flag_src_for_language(lang));
    lv_image_set_scale(flag, 256);
    lv_obj_align(flag, LV_ALIGN_TOP_RIGHT, -18, top_margin + 30);

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

void lvgl_page_chrome_set_flag_callback(lv_event_cb_t cb, void *user_data)
{
    s_flag_cb = cb;
    s_flag_ud = user_data;
}
