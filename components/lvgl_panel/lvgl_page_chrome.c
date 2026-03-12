#include "lvgl_page_chrome.h"
#include "device_config.h"
#include "language_flags.h"

#include <time.h>

static lv_obj_t *s_chrome_time_lbl = NULL;
static lv_timer_t *s_chrome_time_timer = NULL;

static void chrome_update_time_label(void)
{
    if (!s_chrome_time_lbl || !lv_obj_is_valid(s_chrome_time_lbl)) {
        return;
    }

    char time_buf[8] = "--:--";
    time_t now = time(NULL);
    if (now != (time_t)-1) {
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        strftime(time_buf, sizeof(time_buf), "%H:%M", &tm_now);
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
    lv_obj_set_style_text_font(s_chrome_time_lbl, &lv_font_montserrat_24, LV_PART_MAIN);
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

    const char *lang = device_config_get_ui_user_language();
    if (!lang || lang[0] == '\0') {
        lang = "it";
    }

    lv_obj_t *flag = lv_image_create(scr);
    lv_image_set_src(flag, get_flag_src_for_language(lang));
    lv_image_set_scale(flag, 256);
    lv_obj_align(flag, LV_ALIGN_TOP_RIGHT, -18, top_margin + 30);
}
