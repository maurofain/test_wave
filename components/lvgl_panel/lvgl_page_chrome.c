#include "lvgl_page_chrome.h"
#include "language_flags.h"
#include "lvgl_panel.h"

#include "cctalk.h"
#include "device_config.h"
#include "fsm.h"
#include "http_services.h"
#include "mdb.h"
#include "modbus_relay.h"
#include "usb_cdc_scanner.h"

#include "esp_log.h"

#include <stdio.h>
#include <time.h>
#include "embedded_icons.h"

extern const lv_font_t GoogleSans35;

static lv_obj_t      *s_chrome_time_lbl   = NULL;
static lv_timer_t    *s_chrome_time_timer  = NULL;
static lv_obj_t      *s_chrome_status_icons[LVGL_CHROME_STATUS_ICON_COUNT] = {NULL, NULL, NULL, NULL, NULL};
static device_component_status_t s_chrome_status[LVGL_CHROME_STATUS_ICON_COUNT] = {
    DEVICE_COMPONENT_STATUS_DISABLED,
    DEVICE_COMPONENT_STATUS_DISABLED,
    DEVICE_COMPONENT_STATUS_DISABLED,
    DEVICE_COMPONENT_STATUS_DISABLED,
    DEVICE_COMPONENT_STATUS_DISABLED,
};
static lv_event_cb_t  s_flag_cb            = NULL;  /* callback per click bandiera */
static void          *s_flag_ud            = NULL;
static const uint8_t  s_chrome_embedded_icon_map[LVGL_CHROME_STATUS_ICON_COUNT] = {4, 0, 1, 2, 3};

static device_component_status_t chrome_modbus_component_status(void)
{
    const device_config_t *cfg = device_config_get();
    modbus_relay_status_t modbus_status = {0};
    bool status_available = (modbus_relay_get_status(&modbus_status) == ESP_OK);

    if (!cfg || !cfg->sensors.rs485_enabled || !cfg->modbus.enabled) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (!status_available) {
        return DEVICE_COMPONENT_STATUS_OFFLINE;
    }

    if (modbus_status.running && modbus_status.poll_ok_count > 0U && modbus_status.last_error == ESP_OK) {
        return DEVICE_COMPONENT_STATUS_ONLINE;
    }

    if (modbus_status.initialized || modbus_status.running) {
        return DEVICE_COMPONENT_STATUS_ACTIVE;
    }

    return DEVICE_COMPONENT_STATUS_OFFLINE;
}

static bool chrome_is_payment_status_icon(int index)
{
    return index == LVGL_CHROME_STATUS_ICON_CARD ||
           index == LVGL_CHROME_STATUS_ICON_COIN ||
           index == LVGL_CHROME_STATUS_ICON_QR;
}

static bool chrome_is_program_payments_disabled(void)
{
    fsm_ctx_t snap = {0};
    if (!fsm_runtime_snapshot(&snap)) {
        return false;
    }

    return (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED);
}

static void chrome_sync_component_status(void)
{
    s_chrome_status[LVGL_CHROME_STATUS_ICON_MODBUS] = chrome_modbus_component_status();
    s_chrome_status[LVGL_CHROME_STATUS_ICON_CLOUD] = http_services_get_component_status();
    s_chrome_status[LVGL_CHROME_STATUS_ICON_CARD] = mdb_get_component_status();
    s_chrome_status[LVGL_CHROME_STATUS_ICON_COIN] = cctalk_driver_get_component_status();
    s_chrome_status[LVGL_CHROME_STATUS_ICON_QR] = usb_cdc_scanner_get_component_status();
}

static bool chrome_status_uses_ok_icon(int index)
{
    if (index < 0 || index >= LVGL_CHROME_STATUS_ICON_COUNT) {
        return false;
    }

    return s_chrome_status[index] != DEVICE_COMPONENT_STATUS_OFFLINE;
}

static const void *chrome_get_status_icon_src(int index)
{
    if (index < 0 || index >= LVGL_CHROME_STATUS_ICON_COUNT) {
        return NULL;
    }

    const void *emb = get_embedded_icon_src(s_chrome_embedded_icon_map[index], chrome_status_uses_ok_icon(index));
    if (!emb) {
        ESP_LOGW("lvgl_page_chrome", "[C] Icona chrome %d embedded non disponibile", index);
    }

    return emb;
}

static lv_color_t chrome_get_status_icon_color(int index)
{
    if (index < 0 || index >= LVGL_CHROME_STATUS_ICON_COUNT) {
        return lv_color_hex(0xFFFFFF);
    }

    if (!chrome_status_uses_ok_icon(index)) {
        return lv_color_hex(0xFFFFFF);
    }

    if (chrome_is_payment_status_icon(index) && chrome_is_program_payments_disabled()) {
        return lv_color_hex(0xFF0000);
    }

    return lv_color_hex(0xFFFFFF);
}

static void chrome_apply_status_icon_style(lv_obj_t *icon, int index)
{
    if (!icon || !lv_obj_is_valid(icon)) {
        return;
    }

    if (!chrome_status_uses_ok_icon(index)) {
        lv_obj_set_style_img_recolor_opa(icon, LV_OPA_TRANSP, LV_PART_MAIN);
        return;
    }

    lv_color_t color = chrome_get_status_icon_color(index);
    lv_obj_set_style_img_recolor(icon, color, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
}

static void chrome_update_status_icons(void)
{
    chrome_sync_component_status();

    for (int i = 0; i < LVGL_CHROME_STATUS_ICON_COUNT; i++) {
        if (!s_chrome_status_icons[i] || !lv_obj_is_valid(s_chrome_status_icons[i])) {
            continue;
        }
        const void *src = chrome_get_status_icon_src(i);
        if (src) {
            lv_image_set_src(s_chrome_status_icons[i], src);
        }
        chrome_apply_status_icon_style(s_chrome_status_icons[i], i);
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
    chrome_update_status_icons();
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
    lv_obj_align(s_chrome_time_lbl, LV_ALIGN_TOP_LEFT, 18, 10);
    chrome_update_time_label();
    s_chrome_time_timer = lv_timer_create(chrome_time_timer_cb, 1000, NULL);

    const int32_t icon_size = 48;
    const int32_t gap = 8;
    const int32_t right_margin = 18;
    const int32_t top_margin = 4;  //mf era 8

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
    chrome_sync_component_status();
    for (int i = 0; i < LVGL_CHROME_STATUS_ICON_COUNT; i++) {
        s_chrome_status_icons[i] = lv_image_create(scr);
        lv_obj_set_size(s_chrome_status_icons[i], icon_size, icon_size);
        lv_image_set_src(s_chrome_status_icons[i], chrome_get_status_icon_src(i));
        lv_image_set_scale(s_chrome_status_icons[i], 256);
        lv_obj_set_style_pad_all(s_chrome_status_icons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_chrome_status_icons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_chrome_status_icons[i], LV_OPA_TRANSP, LV_PART_MAIN);
        chrome_apply_status_icon_style(s_chrome_status_icons[i], i);
        lv_obj_align(s_chrome_status_icons[i], LV_ALIGN_TOP_RIGHT,
                     -(right_margin + ((LVGL_CHROME_STATUS_ICON_COUNT - 1) - i) * (icon_size + gap)),
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
    chrome_sync_component_status();
    for (int i = 0; i < LVGL_CHROME_STATUS_ICON_COUNT; i++) {
        (void)chrome_get_status_icon_src(i);
    }
}

void lvgl_page_chrome_set_flag_callback(lv_event_cb_t cb, void *user_data)
{
    s_flag_cb = cb;
    s_flag_ud = user_data;
}

static device_component_status_t chrome_status_from_bool(bool ok)
{
    return ok ? DEVICE_COMPONENT_STATUS_ONLINE : DEVICE_COMPONENT_STATUS_OFFLINE;
}

void lvgl_page_chrome_set_status_icon_state(uint8_t idx, bool ok)
{
    if (idx >= LVGL_CHROME_STATUS_ICON_COUNT) {
        return;
    }

    s_chrome_status[idx] = chrome_status_from_bool(ok);
    chrome_update_status_icons();
}

void lvgl_page_chrome_set_status_icons(bool modbus_ok, bool cloud_ok, bool card_ok, bool coin_ok, bool qr_ok)
{
    s_chrome_status[LVGL_CHROME_STATUS_ICON_MODBUS] = chrome_status_from_bool(modbus_ok);
    s_chrome_status[LVGL_CHROME_STATUS_ICON_CLOUD] = chrome_status_from_bool(cloud_ok);
    s_chrome_status[LVGL_CHROME_STATUS_ICON_CARD] = chrome_status_from_bool(card_ok);
    s_chrome_status[LVGL_CHROME_STATUS_ICON_COIN] = chrome_status_from_bool(coin_ok);
    s_chrome_status[LVGL_CHROME_STATUS_ICON_QR] = chrome_status_from_bool(qr_ok);
    chrome_update_status_icons();
}
