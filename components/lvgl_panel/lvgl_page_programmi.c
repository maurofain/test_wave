#include "lvgl_panel_pages.h"

#include "lvgl_i18n.h"
#include "device_config.h"
#include "fsm.h"
#include "lvgl_panel.h"
#include "language_flags.h"
#include "program_repeat_icon.h"
#include "lvgl_page_chrome.h"
#include "main.h"
#include "tasks.h"
#include "usb_cdc_scanner.h"
#include "mdb.h"
#include "web_ui_programs.h"

#include "lvgl.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* Font GoogleSans per pagina programmi */
extern const lv_font_t GoogleSans140;
extern const lv_font_t GoogleSans35;
extern const lv_font_t GoogleSans40;

static const char *TAG = "lvgl_panel";

#define PANEL_REFRESH_MS 200

#define COL_BG lv_color_make(0x1a, 0x1a, 0x2e)
#define COL_PROG     lv_color_make(0x27, 0xd7, 0xb2)
#define COL_PROG_DIS lv_color_make(0x14, 0x4b, 0x43)
#define COL_PROG_ACT lv_color_make(0x27, 0xd7, 0xb2)
#define COL_PROG_LOW lv_color_make(0xc0, 0x39, 0x2b)
#define COL_PROG_PAUSE lv_color_make(0xe6, 0x7e, 0x22)
#define COL_PROG_PAUSED_BG lv_color_make(0x1f, 0x5f, 0xd6)
#define COL_TIMER_NORMAL lv_color_make(0x27, 0xd7, 0xb2)
#define COL_TIMER_WARN lv_color_make(0xb0, 0x0f, 0x6b)
#define COL_WHITE lv_color_make(0xEE, 0xEE, 0xEE)
#define COL_BLACK lv_color_make(0x00, 0x00, 0x00)
#define COL_RED lv_color_make(0xEE, 0x00, 0x00)
#define COL_GREY lv_color_make(0x88, 0x88, 0x99)
#define COL_STATE_IDL lv_color_make(0x20, 0x20, 0x48)

#define PROG_COUNT 10
#define PROG_ROWS 5

#define PANEL_W 720
#define PANEL_H 1280
#define PANEL_PAD_X 2
#define PANEL_PAD_Y 2
#define PANEL_COL_GAP 10
#define PANEL_LEFT_W 120
#define PANEL_RIGHT_W 120
#define PANEL_CONTENT_H (PANEL_H - (PANEL_PAD_Y * 2))
#define PANEL_BUTTONS_TOP_FREE_PCT 24
#define PANEL_STOP_BTN_H 80
#define PANEL_FULL_W (PANEL_W - (PANEL_PAD_X * 2))
#define PANEL_CREDIT_H ((PANEL_CONTENT_H * PANEL_BUTTONS_TOP_FREE_PCT) / 100)
#define PANEL_WORK_H (PANEL_CONTENT_H - PANEL_CREDIT_H - PANEL_STOP_BTN_H - 10)
#define PANEL_COUNTER_W (PANEL_FULL_W - PANEL_LEFT_W - PANEL_RIGHT_W - (PANEL_COL_GAP * 2))
#define PANEL_LEFT_X 0
#define PANEL_COUNTER_X (PANEL_LEFT_W + PANEL_COL_GAP)
#define PANEL_RIGHT_X (PANEL_COUNTER_X + PANEL_COUNTER_W + PANEL_COL_GAP)

// Costanti per layout pulsanti programma
#define PROG_BTN_SIDE_PAD 14    /* margine orizzontale */
#define PROG_BTN_ROW_PAD 6      /* margine verticale */
#define PROG_BTN_COL_GAP 10     /* spazio tra colonne */
#define PROG_BTN_ROW_GAP 6      /* spazio tra righe */
#define PROG_BUTTONS_AREA_H 640 /* altezza area pulsanti richiesta */
#define PROG_NUM_BADGE_SIZE 60
#define PROG_SELECTED_POPUP_MS 2500U
#define PROG_BTN_RADIUS 55      /* metà dell'altezza minima del pulsante con layout a 5 righe */

#define FONT_TIME (&GoogleSans40)
#define FONT_BIGNUM (&GoogleSans140)
#define FONT_LABEL (&GoogleSans35)
#define FONT_PROG_BTN (&GoogleSans35)
#define FONT_DATETIME (&GoogleSans40)

typedef struct
{
    uint8_t prog_id;
} prog_btn_ud_t;

static lv_obj_t *s_credit_box = NULL;
static lv_obj_t *s_status_box = NULL;
static lv_obj_t *s_center_box = NULL;
static lv_obj_t *s_credit_lbl = NULL;
static lv_obj_t *s_elapsed_lbl = NULL;
static lv_obj_t *s_pause_lbl = NULL;
static lv_obj_t *s_residual_credit_lbl = NULL;
static lv_obj_t *s_gauge = NULL;
static lv_obj_t *s_gauge_time_lbl = NULL;
static lv_obj_t *s_counter_fill = NULL;
static lv_obj_t *s_stop_btn = NULL;  /* Flag image inside the button */
static lv_obj_t *s_exit_btn = NULL;  /* pulsante ESCI opzionale che azzera VCD */
static lv_obj_t *s_stop_lbl = NULL;  /* label inside stop button */
static lv_obj_t *s_program_popup = NULL;
static lv_obj_t *s_program_popup_lbl = NULL;
static lv_obj_t *s_program_popup_repeat_img = NULL;
static lv_obj_t *s_program_end_overlay = NULL;
static lv_obj_t *s_program_end_card = NULL;
static lv_obj_t *s_program_end_title_lbl = NULL;
static lv_obj_t *s_program_end_credit_lbl = NULL;
static lv_obj_t *s_program_end_msg_lbl = NULL;
static lv_obj_t *s_prog_btns[PROG_COUNT];
static lv_obj_t *s_prog_lbls[PROG_COUNT];
static lv_timer_t *s_panel_timer = NULL;
static lv_timer_t *s_clock_timer = NULL;
static uint8_t s_active_prog = 0;
static uint8_t s_num_programs = 10;  /* [C] Numero programmi effettivamente costruiti al build corrente */
static char s_current_language[8] = "it";  /* [C] Current language code (default: Italian) */
static char s_prog_label_cache[PROG_COUNT][WEB_UI_PROGRAM_NAME_MAX] = {{0}};
static bool s_prog_label_cache_valid = false;
static char s_prog_label_cache_lang[8] = "";
static uint32_t s_program_popup_until_ms = 0U;
static bool s_program_popup_running_seen = false;
static uint32_t s_program_end_effect_until_ms = 0U;
static bool s_program_end_message_shown = false;  /* [C] Evita la riapertura del popup al rientro da ADS */
static bool s_program_end_nav_pending = false;
static bool s_program_end_nav_to_ads = false;
static bool s_program_end_nav_to_main = false;

static prog_btn_ud_t s_prog_ud[PROG_COUNT];

static char s_last_credit_text[32] = "";
static char s_last_elapsed_text[32] = "";
static char s_last_pause_text[192] = "";
static char s_last_residual_credit_text[32] = "";
static int32_t s_last_gauge_pct = -1;
static time_t s_last_minute_epoch = (time_t)-1;
static bool s_stop_pressed = false;
static bool s_stop_confirm = false;
static bool s_ecd_warning_dismissed = false;
static bool s_btn_last_clickable[PROG_COUNT] = {0};
static bool s_btn_last_active[PROG_COUNT] = {0};
static bool s_btn_last_paused[PROG_COUNT] = {0};
static bool s_btn_last_prefine[PROG_COUNT] = {0};
static bool s_btn_state_valid[PROG_COUNT] = {0};
static bool s_prog_suspended[PROG_COUNT] = {0};
static fsm_state_t s_last_fsm_state = FSM_STATE_IDLE;  /* Track last FSM state for credit reset */
static fsm_ctx_t s_prev_panel_snap = {0};  /* [C] Snapshot precedente usato dal timer pannello */
static bool s_prev_panel_snap_valid = false;
static uint32_t s_last_user_interaction_ms = 0;
static uint32_t s_last_ads_disabled_log_ms = 0;
static uint32_t s_last_program_timeout_log_ms = 0;
static lv_obj_t *s_touch_reset_bound_scr = NULL;

static int32_t panel_effective_credit_cents(const fsm_ctx_t *snap);

static char s_tr_credit[32] = "Credito";
static char s_tr_credits[32] = "Crediti";
static char s_tr_elapsed_fmt[32] = "Secondi   %s";
static char s_tr_pause_fmt[32] = "Pausa: %s";
static char s_tr_ecd_expire_fmt[64] = "Il credito scadrà tra %lu secondi";
static char s_tr_ecd_touch_hint[96] = "tocca il numero del credito per continuare";
static char s_tr_program_outputs_error[64] = "Errore uscite";
static char s_tr_program_end_title[64] = "Programma concluso";
static char s_tr_program_end_credits_fmt[64] = "%ld crediti residui";
static char s_tr_program_end_msg[96] = "Grazie per aver scelto il nostro autolavaggio";

static uint32_t get_program_end_effect_ms(void)
{
    uint8_t sec = 3U;
    device_config_t *cfg = device_config_get();
    if (cfg) {
        sec = cfg->ui.program_end_message_sec;
    }
    if (sec > 10U) {
        sec = 10U;
    }
    return (uint32_t)sec * 1000U;
}

static void hide_program_end_effect(void)
{
    if (!s_program_end_overlay) {
        return;
    }
    lv_obj_add_flag(s_program_end_overlay, LV_OBJ_FLAG_HIDDEN);
    s_program_end_effect_until_ms = 0U;
    s_program_end_message_shown = false;
}

static void show_program_end_effect(int32_t residual_credit)
{
    if (!s_program_end_overlay || !s_program_end_title_lbl || !s_program_end_credit_lbl || !s_program_end_msg_lbl) {
        return;
    }

    uint32_t effect_ms = get_program_end_effect_ms();
    if (effect_ms == 0U) {
        hide_program_end_effect();
        return;
    }

    char credit_line[96] = {0};
    snprintf(credit_line, sizeof(credit_line), s_tr_program_end_credits_fmt, (long)residual_credit);

    lv_label_set_text(s_program_end_title_lbl, s_tr_program_end_title);
    lv_label_set_text(s_program_end_credit_lbl, credit_line);
    lv_label_set_text(s_program_end_msg_lbl, s_tr_program_end_msg);

    esp_err_t audio_err = tasks_publish_play_audio("/spiffs/audio/grazie.wav", AGN_ID_LVGL);
    if (audio_err != ESP_OK) {
        ESP_LOGW(TAG, "[C] Audio MDR non pubblicato: %s", esp_err_to_name(audio_err));
    }

    lv_obj_clear_flag(s_program_end_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_program_end_overlay);
    lv_obj_invalidate(s_program_end_overlay);
    s_program_end_effect_until_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()) + effect_ms;
}

#define MAIN_PAGE_IDLE_TO_ADS_MS 60000U

/**
 * @brief Marca una interazione utente per il timeout della pagina programmi.
 */
static void mark_user_interaction(void)
{
    s_last_user_interaction_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

static void reinit_payment_devices_if_reactivated(void);

/**
 * @brief Callback touch su area generica pagina programmi.
 *
 * Qualsiasi tocco sullo schermo (anche fuori da pulsanti azionabili)
 * deve resettare il timer di inattività della pagina programmi.
 */
static void on_main_page_touch(lv_event_t *e)
{
    (void)e;
    reinit_payment_devices_if_reactivated();
    mark_user_interaction();
}

/**
 * @brief Callback touch su area credito.
 */
static void on_credit_box_touch(lv_event_t *e)
{
    (void)e;
    reinit_payment_devices_if_reactivated();
    mark_user_interaction();
    s_ecd_warning_dismissed = true;
    hide_program_end_effect();
}

/**
 * @brief Carica le traduzioni per il pannello usando il nuovo sistema i18n LVGL.
 *
 * Questa funzione carica le traduzioni necessarie per il pannello utilizzando
 * il nuovo sistema ottimizzato i18n LVGL con cache in PSRAM.
 *
 * @return void
 */
static void panel_load_translations(void)
{
    // Usa il nuovo sistema i18n LVGL per le traduzioni principali
    esp_err_t ret;
    
    // Traduzione per "Credito"
    ret = lvgl_i18n_get_text("credit_label", "Credito", s_tr_credit, sizeof(s_tr_credit));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: credit_label -> '%s'", s_tr_credit);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: credit_label fallback -> '%s'", s_tr_credit);
    }

    ret = lvgl_i18n_get_text("credits_label", "Crediti", s_tr_credits, sizeof(s_tr_credits));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: credits_label -> '%s'", s_tr_credits);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: credits_label fallback -> '%s'", s_tr_credits);
    }
    
    // Traduzione per formato tempo
    ret = lvgl_i18n_get_text("elapsed_fmt", "Secondi   %s", s_tr_elapsed_fmt, sizeof(s_tr_elapsed_fmt));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: elapsed_fmt -> '%s'", s_tr_elapsed_fmt);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: elapsed_fmt fallback -> '%s'", s_tr_elapsed_fmt);
    }
    
    // Traduzione per formato pausa
    ret = lvgl_i18n_get_text("pause_fmt", "Pausa: %s", s_tr_pause_fmt, sizeof(s_tr_pause_fmt));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: pause_fmt -> '%s'", s_tr_pause_fmt);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: pause_fmt fallback -> '%s'", s_tr_pause_fmt);
    }

    // Riga 1 messaggio credito ECD in scadenza
    ret = lvgl_i18n_get_text("ecd_expire_line1_fmt",
                             "Il credito scadrà tra %lu secondi",
                             s_tr_ecd_expire_fmt,
                             sizeof(s_tr_ecd_expire_fmt));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: ecd_expire_line1_fmt -> '%s'", s_tr_ecd_expire_fmt);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: ecd_expire_line1_fmt fallback -> '%s'", s_tr_ecd_expire_fmt);
    }

    // Riga 2 messaggio credito ECD in scadenza
    ret = lvgl_i18n_get_text("ecd_expire_line2_hint",
                             "tocca il numero del credito per continuare",
                             s_tr_ecd_touch_hint,
                             sizeof(s_tr_ecd_touch_hint));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: ecd_expire_line2_hint -> '%s'", s_tr_ecd_touch_hint);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: ecd_expire_line2_hint fallback -> '%s'", s_tr_ecd_touch_hint);
    }

    ret = lvgl_i18n_get_text("program_outputs_error",
                             "Errore uscite",
                             s_tr_program_outputs_error,
                             sizeof(s_tr_program_outputs_error));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: program_outputs_error -> '%s'", s_tr_program_outputs_error);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: program_outputs_error fallback -> '%s'", s_tr_program_outputs_error);
    }

    ret = lvgl_i18n_get_text("program_end_title",
                             "Programma concluso",
                             s_tr_program_end_title,
                             sizeof(s_tr_program_end_title));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: program_end_title -> '%s'", s_tr_program_end_title);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: program_end_title fallback -> '%s'", s_tr_program_end_title);
    }

    ret = lvgl_i18n_get_text("program_end_credits_fmt",
                             "%ld crediti residui",
                             s_tr_program_end_credits_fmt,
                             sizeof(s_tr_program_end_credits_fmt));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: program_end_credits_fmt -> '%s'", s_tr_program_end_credits_fmt);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: program_end_credits_fmt fallback -> '%s'", s_tr_program_end_credits_fmt);
    }

    ret = lvgl_i18n_get_text("program_end_message",
                             "Grazie per aver scelto il nostro autolavaggio",
                             s_tr_program_end_msg,
                             sizeof(s_tr_program_end_msg));
    if (ret == ESP_OK) {
        ESP_LOGD(TAG, "[C] LVGL i18n: program_end_message -> '%s'", s_tr_program_end_msg);
    } else {
        ESP_LOGD(TAG, "[C] LVGL i18n: program_end_message fallback -> '%s'", s_tr_program_end_msg);
    }
}

/**
 * @brief Imposta lo stile di un pulsante LVGL.
 *
 * @param btn Puntatore all'oggetto pulsante da stилиizzare.
 * @param bg Colore di sfondo del pulsante.
 * @return void
 */
static void btn_style(lv_obj_t *btn, lv_color_t bg)
{
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, PROG_BTN_RADIUS, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 4, LV_PART_MAIN);
}

static const web_ui_program_entry_t *find_program_entry(uint8_t pid)
{
    const web_ui_program_table_t *tbl = web_ui_program_table_get();
    if (!tbl)
    {
        return NULL;
    }

    for (uint8_t i = 0; i < tbl->count; i++)
    {
        if (tbl->programs[i].program_id == pid)
        {
            return &tbl->programs[i];
        }
    }

    return NULL;
}

#define PROGRAMS_PAYMENT_REINIT_IDLE_MS 300000U

static void invalidate_program_label_cache(void)
{
    s_prog_label_cache_valid = false;
    s_prog_label_cache_lang[0] = '\0';
}

static void panel_enable_payment_devices_on_programs_open(void)
{
    device_config_t *cfg = device_config_get();
    if (!cfg) {
        ESP_LOGW(TAG, "[C] Config non disponibile: impossibile riabilitare scanner/gettoniera");
        return;
    }

    bool scanner_was_enabled = cfg->scanner.enabled;
    bool cctalk_was_enabled = cfg->sensors.cctalk_enabled;

    cfg->scanner.enabled = true;

    tasks_apply_n_run();

    esp_err_t on_err = usb_cdc_scanner_send_on_command();
    if (on_err != ESP_OK) {
        esp_err_t setup_err = usb_cdc_scanner_send_setup_command();
        esp_err_t retry_on_err = ESP_FAIL;
        if (setup_err == ESP_OK) {
            retry_on_err = usb_cdc_scanner_send_on_command();
        }
        if (!(setup_err == ESP_OK && retry_on_err == ESP_OK)) {
            ESP_LOGW(TAG,
                     "[C] Riabilitazione scanner in apertura programmi fallita (on=%s setup=%s retry_on=%s)",
                     esp_err_to_name(on_err),
                     esp_err_to_name(setup_err),
                     esp_err_to_name(retry_on_err));
        }
    }

    if (cfg->sensors.cctalk_enabled) {
        main_cctalk_send_initialization_sequence_async();
    }

    ESP_LOGI(TAG,
             "[C] Apertura programmi: scanner=%d->%d cctalk=%d->%d",
             scanner_was_enabled ? 1 : 0,
             cfg->scanner.enabled ? 1 : 0,
             cctalk_was_enabled ? 1 : 0,
             cfg->sensors.cctalk_enabled ? 1 : 0);
}

static void reinit_payment_devices_if_reactivated(void)
{
    uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
    if (s_last_user_interaction_ms == 0) {
        return;
    }

    if ((now_ms - s_last_user_interaction_ms) < PROGRAMS_PAYMENT_REINIT_IDLE_MS) {
        return;
    }

    ESP_LOGI(TAG,
             "[C] Utente riattiva dopo inattività > %lu ms: riavvio dispositivi pagamento",
             (unsigned long)PROGRAMS_PAYMENT_REINIT_IDLE_MS);
    panel_enable_payment_devices_on_programs_open();
}

static void rebuild_program_label_cache_if_needed(void)
{
    const char *runtime_lang = lvgl_panel_get_runtime_language();
    const char *lang = (runtime_lang && runtime_lang[0] != '\0') ? runtime_lang : "it";

    if (s_prog_label_cache_valid &&
        strncmp(s_prog_label_cache_lang, lang, sizeof(s_prog_label_cache_lang)) == 0)
    {
        return;
    }

    for (uint8_t pid = 1; pid <= PROG_COUNT; ++pid)
    {
        char fallback[8] = {0};
        const char *target_text = NULL;
        const web_ui_program_entry_t *entry = find_program_entry(pid);

        snprintf(fallback, sizeof(fallback), "%u", (unsigned)pid);
        if (entry && entry->name[0] != '\0')
        {
            target_text = entry->name;
        }
        else
        {
            target_text = fallback;
        }

        snprintf(s_prog_label_cache[(int)pid - 1],
                 sizeof(s_prog_label_cache[0]),
                 "%s",
                 target_text);
    }

    snprintf(s_prog_label_cache_lang, sizeof(s_prog_label_cache_lang), "%s", lang);
    s_prog_label_cache_valid = true;
}

static void set_program_label_text(lv_obj_t *label, uint8_t pid)
{
    if (!label || pid == 0 || pid > PROG_COUNT)
    {
        return;
    }

    rebuild_program_label_cache_if_needed();

    const char *target_text = s_prog_label_cache[(int)pid - 1];
    char fallback[8] = {0};
    if (!target_text || target_text[0] == '\0')
    {
        snprintf(fallback, sizeof(fallback), "%u", (unsigned)pid);
        target_text = fallback;
    }

    const char *current_text = lv_label_get_text(label);
    if (current_text && strcmp(current_text, target_text) == 0)
    {
        return;
    }

    lv_label_set_text(label, target_text);
}

static void get_program_label_text(uint8_t pid, char *out, size_t out_len)
{
    if (!out || out_len == 0) {
        return;
    }

    out[0] = '\0';
    if (pid == 0 || pid > PROG_COUNT) {
        return;
    }

    rebuild_program_label_cache_if_needed();
    const char *target_text = s_prog_label_cache[(int)pid - 1];
    if (target_text && target_text[0] != '\0') {
        snprintf(out, out_len, "%s", target_text);
        return;
    }

    snprintf(out, out_len, "%u", (unsigned)pid);
}

static void show_selected_program_popup(uint8_t pid)
{
    if (!s_program_popup || !s_program_popup_lbl || pid == 0 || pid > PROG_COUNT) {
        return;
    }

    char popup_text[WEB_UI_PROGRAM_NAME_MAX] = {0};
    get_program_label_text(pid, popup_text, sizeof(popup_text));
    if (popup_text[0] == '\0') {
        return;
    }

    lv_label_set_text(s_program_popup_lbl, popup_text);
    lv_obj_clear_flag(s_program_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_program_popup);
    lv_obj_invalidate(s_program_popup);
    s_program_popup_until_ms = 0U;
    s_program_popup_running_seen = false;
}

static bool program_is_active_for_snapshot(const web_ui_program_entry_t *entry, const fsm_ctx_t *snap)
{
    if (!entry || !snap) {
        return false;
    }

    if ((snap->state != FSM_STATE_RUNNING && snap->state != FSM_STATE_PAUSED) ||
        snap->running_program_name[0] == '\0') {
        return false;
    }

    if (entry->name[0] == '\0') {
        return false;
    }

    return (strcmp(entry->name, snap->running_program_name) == 0);
}

static bool program_is_clickable_for_snapshot(const web_ui_program_entry_t *entry, const fsm_ctx_t *snap)
{
    if (!entry || !entry->enabled || !snap) {
        return false;
    }

    /* [C] Il programma attivo deve restare sempre cliccabile per pausa/ripresa. */
    if (program_is_active_for_snapshot(entry, snap)) {
        return true;
    }

    if (snap->state == FSM_STATE_RUNNING || snap->state == FSM_STATE_PAUSED) {
        int32_t effective_credit = panel_effective_credit_cents(snap);
        if (effective_credit <= 0) {
            return false;
        }
        return (effective_credit >= (int32_t)entry->price_units);
    }

    if (snap->state == FSM_STATE_CREDIT) {
        int32_t effective_credit = panel_effective_credit_cents(snap);
        if (effective_credit <= 0) {
            return false;
        }
        return (effective_credit >= (int32_t)entry->price_units);
    }

    return false;
}

/**
 * @brief Ritorna il credito effettivo da mostrare/valutare in UI.
 */
static int32_t panel_effective_credit_cents(const fsm_ctx_t *snap)
{
    if (!snap) {
        return 0;
    }

    if (snap->credit_cents > 0) {
        return snap->credit_cents;
    }

    int32_t buckets = snap->ecd_coins + snap->vcd_coins;
    if (buckets > 0) {
        return buckets;
    }

    return snap->credit_cents;
}

/**
 * @brief Aggiorna i pulsanti del programma in base allo stato corrente del contesto del finite state machine.
 *
 * @param [in] snap Puntatore al contesto del finite state machine.
 * @return Nessun valore di ritorno.
 */
static void refresh_prog_buttons(const fsm_ctx_t *snap)
{
    const bool prefine_active = snap &&
                                snap->pre_fine_ciclo_active &&
                                (snap->state == FSM_STATE_RUNNING || snap->state == FSM_STATE_PAUSED);

    for (int i = 0; i < PROG_COUNT; i++)
    {
        lv_obj_t *btn = s_prog_btns[i];
        if (!btn) continue;
        if (!lv_obj_is_valid(btn)) {
            s_prog_btns[i] = NULL;
            s_prog_lbls[i] = NULL;
            s_btn_state_valid[i] = false;
            continue;
        }

        const web_ui_program_entry_t *entry = find_program_entry((uint8_t)(i + 1));

        bool is_active = program_is_active_for_snapshot(entry, snap);
        bool can_click = program_is_clickable_for_snapshot(entry, snap);
        bool is_paused = is_active && s_prog_suspended[i];
        
        // Update button state only if changed
        if (!s_btn_state_valid[i] ||
            s_btn_last_active[i] != is_active ||
            s_btn_last_clickable[i] != can_click ||
            s_btn_last_paused[i] != is_paused ||
            s_btn_last_prefine[i] != prefine_active)
        {
            lv_color_t btn_color;
            if (prefine_active && !can_click) {
                btn_color = COL_PROG_DIS;
            } else if (prefine_active) {
                btn_color = COL_TIMER_WARN;
            } else if (is_paused) {
                btn_color = COL_PROG_PAUSED_BG;
            } else if (is_active) {
                btn_color = COL_PROG_ACT;
            } else if (can_click) {
                btn_color = COL_PROG;
            } else {
                btn_color = COL_PROG_DIS;
            }
            btn_style(btn, btn_color);
            lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, is_active ? 10: 0, LV_PART_MAIN);
            lv_obj_set_style_border_color(btn, COL_GREY, LV_PART_MAIN);
            lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
            
            // Update clickability
            if (can_click && !s_btn_last_clickable[i]) {
                lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            } else if (!can_click && s_btn_last_clickable[i]) {
                lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            }
            s_btn_last_active[i] = is_active;
            s_btn_last_clickable[i] = can_click;
            s_btn_last_paused[i] = is_paused;
            s_btn_last_prefine[i] = prefine_active;
        }

        s_btn_state_valid[i] = true;
    }

    /* [C] Gestisci lo stato del pulsante STOP: style only here; visibility controlled by update_state */
    if (s_stop_btn)
    {
        // Only update visual style here; actual visibility/clickability handled in update_state
        lv_obj_set_style_bg_color(s_stop_btn, prefine_active ? COL_TIMER_WARN : COL_PROG_LOW, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_stop_btn, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_stop_btn,
                                      prefine_active ? COL_WHITE : lv_color_make(0xFF, 0x80, 0x80),
                                      LV_PART_MAIN);
    }
}

/**
 * @brief Formatta un tempo in millisecondi in una stringa nel formato "mm:ss".
 *
 * @param [out] buf Puntatore al buffer dove verrà memorizzata la stringa formattata.
 * @param len Lunghezza massima del buffer.
 * @param ms Tempo in millisecondi da formattare.
 *
 * @return void
 */
static void fmt_mm_ss(char *buf, size_t len, uint32_t ms)
{
    uint32_t s = ms / 1000;
    snprintf(buf, len, "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

/**
 * @brief Aggiorna il tempo e pubblica un messaggio se richiesto.
 *
 * @param publish_minute_message Flag booleano che indica se pubblicare un messaggio ogni minuto.
 * @return void Non restituisce alcun valore.
 */
static void update_time(bool publish_minute_message)
{
    /* s_time_lbl rimosso su richiesta dell'utente - funzione vuota */
    (void)publish_minute_message;
}

/**
 * @brief Callback per il timer del clock.
 *
 * Questa funzione viene chiamata periodicamente dal timer del clock.
 *
 * @param [in] t Puntatore al timer LVGL.
 * @return Nessun valore di ritorno.
 */
static void clock_timer_cb(lv_timer_t *t)
{
    (void)t;
    update_time(true);
}

/**
 * @brief Aggiorna lo stato del contesto del finite state machine.
 *
 * Questa funzione aggiorna lo stato del contesto del finite state machine utilizzando lo stato snapshot fornito.
 *
 * @param [in] snap Puntatore al contesto del finite state machine da aggiornare.
 * @return Nessun valore di ritorno.
 */
static void update_state(const fsm_ctx_t *snap)
{
    if (!snap)
    {
        return;
    }

    bool running = (snap->state == FSM_STATE_RUNNING);
    bool paused = (snap->state == FSM_STATE_PAUSED);
    int32_t effective_credit_cents = panel_effective_credit_cents(snap);
    bool prefine_active = false;
    uint32_t rem_ms = 0;
    int32_t remaining_pct = 0;

    if ((running || paused) && snap->running_target_ms > 0)
    {
        rem_ms = (snap->running_target_ms > snap->running_elapsed_ms)
                     ? (snap->running_target_ms - snap->running_elapsed_ms)
                     : 0;
        remaining_pct = (int32_t)((rem_ms * 100U) / snap->running_target_ms);
        prefine_active = snap->pre_fine_ciclo_active;
    }

    if (s_status_box)
    {
        lv_obj_set_style_bg_color(s_status_box, COL_STATE_IDL, LV_PART_MAIN);
    }

    if (s_credit_box)
    {
        lv_obj_set_style_border_color(s_credit_box,
                                      prefine_active ? COL_TIMER_WARN : COL_TIMER_NORMAL,
                                      LV_PART_MAIN);
    }

    if (s_credit_lbl)
    {
        char buf[16] = {0};
        lv_color_t credit_color = snap->qr_credit_pending ? COL_RED : COL_WHITE;

        snprintf(buf, sizeof(buf), "%ld", (long)effective_credit_cents);

        lv_obj_set_style_text_color(s_credit_lbl, credit_color, LV_PART_MAIN);

        if (strcmp(s_last_credit_text, buf) != 0)
        {
            lv_label_set_text(s_credit_lbl, buf);
            strncpy(s_last_credit_text, buf, sizeof(s_last_credit_text) - 1);
            s_last_credit_text[sizeof(s_last_credit_text) - 1] = '\0';
        }
    }

    if (s_elapsed_lbl)
    {
        char buf[32] = {0};

        (void)running;
        (void)paused;
        (void)prefine_active;
        snprintf(buf, sizeof(buf), "%s", s_tr_credits);

        if (strcmp(s_last_elapsed_text, buf) != 0)
        {
            lv_label_set_text(s_elapsed_lbl, buf);
            strncpy(s_last_elapsed_text, buf, sizeof(s_last_elapsed_text) - 1);
            s_last_elapsed_text[sizeof(s_last_elapsed_text) - 1] = '\0';
        }
    }

    if (s_pause_lbl)
    {
        char buf[192] = {0};

        if (paused && snap->pause_max_ms > 0)
        {
            char mm[10] = {0};
            uint32_t pause_remaining_ms =
                (snap->pause_max_ms > snap->pause_elapsed_ms)
                    ? (snap->pause_max_ms - snap->pause_elapsed_ms)
                    : 0U;
            fmt_mm_ss(mm, sizeof(mm), pause_remaining_ms);
            snprintf(buf, sizeof(buf), s_tr_pause_fmt, mm);
        }
        else
        {
            buf[0] = '\0';
        }

        if (strcmp(s_last_pause_text, buf) != 0)
        {
            lv_label_set_text(s_pause_lbl, buf);
            strncpy(s_last_pause_text, buf, sizeof(s_last_pause_text) - 1);
            s_last_pause_text[sizeof(s_last_pause_text) - 1] = '\0';
        }
    }

    /* [C] Aggiorna il credito residuo mostrato sotto il countdown */
    if (s_residual_credit_lbl)
    {
        char buf[16] = {0};

        if (running || paused)
        {
            /* Durante il programma, mostra il credito residuo (totale disponibile) */
            snprintf(buf, sizeof(buf), "%ld", (long)effective_credit_cents);
        }
        else
        {
            /* A riposo, mostra il credito disponibile per il prossimo programma */
            snprintf(buf, sizeof(buf), "%ld", (long)effective_credit_cents);
        }

        if (strcmp(s_last_residual_credit_text, buf) != 0)
        {
            lv_label_set_text(s_residual_credit_lbl, buf);
            strncpy(s_last_residual_credit_text, buf, sizeof(s_last_residual_credit_text) - 1);
            s_last_residual_credit_text[sizeof(s_last_residual_credit_text) - 1] = '\0';
        }
    }

    if (s_gauge)
    {
        if ((running || paused) && snap->running_target_ms > 0) {
            lv_obj_clear_flag(s_gauge, LV_OBJ_FLAG_HIDDEN);
            int32_t pct = 100 - (int32_t)((rem_ms * 90U) / snap->running_target_ms);
            if (pct < 10) pct = 10;
            if (pct > 100) pct = 100;

            lv_color_t gauge_col = prefine_active ? COL_TIMER_WARN : COL_TIMER_NORMAL;
            lv_obj_set_style_bg_color(s_gauge, gauge_col, LV_PART_INDICATOR);
            lv_obj_set_style_bg_grad_color(s_gauge, gauge_col, LV_PART_INDICATOR);
            lv_obj_set_style_bg_grad_dir(s_gauge, LV_GRAD_DIR_NONE, LV_PART_INDICATOR);
            lv_obj_set_style_border_color(s_gauge, gauge_col, LV_PART_MAIN);
            lv_bar_set_value(s_gauge, pct, LV_ANIM_OFF);
            s_last_gauge_pct = pct;
        } else {
            lv_obj_add_flag(s_gauge, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_gauge_time_lbl)
    {
        if ((running || paused) && snap->running_target_ms > 0) {
            char buf[16] = {0};
            snprintf(buf, sizeof(buf), "%ld%%", (long)remaining_pct);
            lv_label_set_text(s_gauge_time_lbl, buf);
            lv_obj_set_style_text_color(s_gauge_time_lbl,
                                        prefine_active ? COL_WHITE : lv_color_make(0x00, 0x00, 0x00),
                                        LV_PART_MAIN);
            lv_obj_clear_flag(s_gauge_time_lbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_gauge_time_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_program_popup) {
        bool popup_visible = !lv_obj_has_flag(s_program_popup, LV_OBJ_FLAG_HIDDEN);
        bool autorepeat_active = (running || paused) && !snap->stop_after_cycle_requested;

        if (s_program_popup_lbl) {
            lv_obj_set_style_text_color(s_program_popup_lbl,
                                        prefine_active ? COL_TIMER_WARN : COL_BLACK,
                                        LV_PART_MAIN);
        }

        if (s_program_popup_repeat_img) {
            lv_color_t text_col = s_program_popup_lbl
                                  ? lv_obj_get_style_text_color(s_program_popup_lbl, LV_PART_MAIN)
                                  : COL_BLACK;
            lv_obj_set_style_image_recolor(s_program_popup_repeat_img, text_col, LV_PART_MAIN);
            lv_obj_set_style_image_recolor_opa(s_program_popup_repeat_img, LV_OPA_COVER, LV_PART_MAIN);

            if (popup_visible && autorepeat_active) {
                lv_obj_clear_flag(s_program_popup_repeat_img, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(s_program_popup_repeat_img, LV_OBJ_FLAG_HIDDEN);
            }
        }

        if ((running || paused) && popup_visible) {
            s_program_popup_running_seen = true;
        }
        if (s_program_popup_running_seen && !running && !paused) {
            lv_obj_add_flag(s_program_popup, LV_OBJ_FLAG_HIDDEN);
            s_program_popup_until_ms = 0U;
            s_program_popup_running_seen = false;
        }
    }

    if (s_program_end_overlay && s_program_end_effect_until_ms > 0U) {
        uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
        if (now_ms >= s_program_end_effect_until_ms) {
            hide_program_end_effect();
        }
    }

    /* [C] Gestione STOP + avviso scadenza credito monete */
    bool ecd_warning_active = false;
    uint32_t ecd_rem_s = 0;
    const uint32_t ecd_warning_threshold_s = 60U;
    bool coin_credit_session = (snap->state == FSM_STATE_CREDIT) &&
                               (snap->session_mode == FSM_SESSION_MODE_OPEN_PAYMENTS) &&
                               (snap->session_source == FSM_SESSION_SOURCE_COIN) &&
                               (snap->ecd_coins > 0);

    if (!coin_credit_session || running || paused) {
        s_ecd_warning_dismissed = false;
    }

    if (coin_credit_session && !s_ecd_warning_dismissed) {
        if (snap->splash_screen_time_ms > snap->inactivity_ms) {
            ecd_rem_s = (snap->splash_screen_time_ms - snap->inactivity_ms) / 1000U;
            if (ecd_rem_s <= ecd_warning_threshold_s) {
                ecd_warning_active = true;
            }
        }
    }

    bool outputs_error_active = tasks_program_outputs_verify_error_active();

    if (s_stop_btn && s_stop_lbl) {
        if ((running || paused) && outputs_error_active) {
            lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_stop_btn, COL_RED, LV_PART_MAIN);
            lv_obj_set_style_border_color(s_stop_btn, COL_WHITE, LV_PART_MAIN);
            lv_label_set_text(s_stop_lbl, s_tr_program_outputs_error);
        } else if (running || paused) {
            lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_stop_btn, prefine_active ? COL_TIMER_WARN : COL_PROG_LOW, LV_PART_MAIN);
            char stop_text[64];
            if (s_stop_confirm) {
                (void)lvgl_i18n_get_text("program_confirm_cancel",
                                         "Conferma annullamento",
                                         stop_text,
                                         sizeof(stop_text));
            } else {
                (void)lvgl_i18n_get_text("program_stop", "STOP", stop_text, sizeof(stop_text));
            }
            lv_label_set_text(s_stop_lbl, stop_text);
        } else if (ecd_warning_active) {
            lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_stop_btn, COL_TIMER_WARN, LV_PART_MAIN);

            char line1[96] = {0};
            char warn_text[192] = {0};
            snprintf(line1, sizeof(line1), s_tr_ecd_expire_fmt, (unsigned long)ecd_rem_s);
            snprintf(warn_text, sizeof(warn_text), "%s\n%s", line1, s_tr_ecd_touch_hint);
            lv_label_set_text(s_stop_lbl, warn_text);
        } else {
            lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
            s_stop_confirm = false;
            char stop_text[64];
            (void)lvgl_i18n_get_text("program_stop", "STOP", stop_text, sizeof(stop_text));
            lv_label_set_text(s_stop_lbl, stop_text);
        }
    }

    /* [C] Gestione pulsante ESCI (opzionale, azzera VCD) */
    if (s_exit_btn) {
        if (!lv_obj_is_valid(s_exit_btn)) {
            s_exit_btn = NULL;
        } else {
        const device_config_t *cfg = device_config_get();
        bool exit_enabled = cfg && cfg->timeouts.allow_exit_programs_clears_vcd;
        bool vcd_present = (snap->vcd_coins > 0 || snap->vcd_cents_residual > 0);
        bool show_exit = exit_enabled && !running && !paused && vcd_present && (snap->state == FSM_STATE_CREDIT);

        if (show_exit) {
            lv_obj_clear_flag(s_exit_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_exit_btn, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_add_flag(s_exit_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_exit_btn, LV_OBJ_FLAG_CLICKABLE);
        }
        }
    }

    /* Sync program button labels with running/paused state */
    if ((running || paused) && snap->running_program_name[0] != '\0') {
        const web_ui_program_table_t *tbl = web_ui_program_table_get();
        if (tbl) {
            for (uint8_t i = 0; i < tbl->count; ++i) {
                const web_ui_program_entry_t *p = &tbl->programs[i];
                int idx = (int)p->program_id - 1;
                if (p->name[0] != '\0' && strcmp(p->name, snap->running_program_name) == 0) {
                    s_active_prog = p->program_id;
                    if (paused) {
                        s_prog_suspended[idx] = true;
                        if (s_prog_lbls[idx]) {
                            char suspend_text[64];
                            (void)lvgl_i18n_get_text("program_suspend", "Sospendi", suspend_text, sizeof(suspend_text));
                            lv_label_set_text(s_prog_lbls[idx], suspend_text);
                        }
                    } else {
                        s_prog_suspended[idx] = false;
                        if (s_prog_lbls[idx]) set_program_label_text(s_prog_lbls[idx], p->program_id);
                    }
                } else {
                    if (s_prog_lbls[(int)p->program_id - 1]) set_program_label_text(s_prog_lbls[(int)p->program_id - 1], p->program_id);
                    s_prog_suspended[(int)p->program_id - 1] = false;
                }
            }
        }
    } else {
        /* No running program: restore labels */
        const web_ui_program_table_t *tbl = web_ui_program_table_get();
        if (tbl) {
            for (uint8_t i = 0; i < tbl->count; ++i) {
                const web_ui_program_entry_t *p = &tbl->programs[i];
                if (s_prog_lbls[(int)p->program_id - 1]) set_program_label_text(s_prog_lbls[(int)p->program_id - 1], p->program_id);
                s_prog_suspended[(int)p->program_id - 1] = false;
            }
        }
    }
}

/**
 * @brief Pubblica un programma in base all'ID fornito e gestisce il pausa/toggle.
 *
 * @param prog_id ID del programma da pubblicare.
 * @param pause_toggle Flag per attivare/disattivare il pausa/toggle.
 * @param entry Puntatore alla struttura del programma da pubblicare.
 * @return true Se la pubblicazione è stata avviata con successo.
 * @return false Se la pubblicazione non è stata avviata.
 */
// Temporarily commented to fix compilation
/*
static bool publish_program(uint8_t prog_id, bool pause_toggle, const web_ui_program_entry_t *entry)
{
    if (!fsm_event_queue_init(0))
    {
        return false;
    }

    if (pause_toggle)
    {
        fsm_input_event_t ev = {
            .from = AGN_ID_LVGL,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PROGRAM_PAUSE_TOGGLE,
            .type = FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = (int32_t)prog_id,
            .value_u32 = 0,
            .aux_u32 = 0,
            .data_ptr = NULL,
            .text = {0},
        };

        if (entry)
        {
            strncpy(ev.text, entry->name, sizeof(ev.text) - 1);
        }

        return fsm_event_publish(&ev, pdMS_TO_TICKS(20));
    }

    fsm_ctx_t snap = {0};
    bool has_snap = fsm_runtime_snapshot(&snap);
    bool is_switch = has_snap &&
                     (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED);

    fsm_input_event_t ev = {
        .from = AGN_ID_LVGL,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_PROGRAM_SELECTED,
        .type = is_switch ? FSM_INPUT_EVENT_PROGRAM_SWITCH : FSM_INPUT_EVENT_PROGRAM_SELECTED,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = entry ? (int32_t)entry->price_units : (int32_t)prog_id,
        .value_u32 = entry ? (uint32_t)entry->pause_max_suspend_sec * 1000U : 0,
        .aux_u32 = entry ? (uint32_t)entry->duration_sec * 1000U : 0,
        .data_ptr = NULL,
        .text = {0},
    };

    if (entry)
    {
        strncpy(ev.text, entry->name, sizeof(ev.text) - 1);
    }

    bool ok = fsm_event_publish(&ev, pdMS_TO_TICKS(20));
    if (!ok)
    {
        ESP_LOGW(TAG, "[C] Coda FSM piena per prog=%u", prog_id);
    }

    return ok;
}
*/

/**
 * @brief Gestisce l'evento del pulsante di programmazione.
 *
 * Questa funzione viene chiamata quando viene generato un evento sul pulsante di programmazione.
 *
 * @param e Puntatore all'evento generato.
 * @return Nessun valore di ritorno.
 */
static void on_prog_btn(lv_event_t *e)
{
    mark_user_interaction();

    if (s_stop_confirm) {
        s_stop_confirm = false;
        if (s_stop_lbl) {
            char stop_text[64];
            (void)lvgl_i18n_get_text("program_stop", "STOP", stop_text, sizeof(stop_text));
            lv_label_set_text(s_stop_lbl, stop_text);
        }
    }

    lv_obj_t *btn = lv_event_get_target(e);
    prog_btn_ud_t *ud = (prog_btn_ud_t *)lv_obj_get_user_data(btn);
    if (!ud) {
        return;
    }

    uint8_t pid = ud->prog_id;
    show_selected_program_popup(pid);

    const web_ui_program_entry_t *entry = find_program_entry(pid);
    if (!entry) {
        ESP_LOGW(TAG, "[C] Programma non trovato pid=%u", (unsigned)pid);
        return;
    }

    if (!entry->enabled) {
        ESP_LOGI(TAG, "[C] Programma disabilitato pid=%u", (unsigned)pid);
        return;
    }

    esp_err_t err = tasks_publish_program_button_action(pid, AGN_ID_LVGL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[C] Azione programma pid=%u non pubblicata: %s", (unsigned)pid, tasks_err_to_name(err));
    } else {
        /* [C] Audio avvio solo su pressione manuale del pulsante programma.
           L'autorepeat usa altri percorsi FSM e non passa da questo callback. */
        esp_err_t audio_err = tasks_publish_play_audio("/spiffs/audio/avvio.wav", AGN_ID_LVGL);
        if (audio_err != ESP_OK) {
            ESP_LOGW(TAG, "[C] Audio avvio programma non pubblicato: %s", esp_err_to_name(audio_err));
        }

        s_active_prog = pid;
        s_stop_pressed = false;
        s_stop_confirm = false;
        if (s_stop_lbl) {
            char stop_text[64];
            (void)lvgl_i18n_get_text("program_stop", "STOP", stop_text, sizeof(stop_text));
            lv_label_set_text(s_stop_lbl, stop_text);
        }
        if (s_pause_lbl) {
            lv_label_set_text(s_pause_lbl, "");
            s_last_pause_text[0] = '\0';
        }
    }
}

/**
 * @brief [C] Gestisce il click sul pulsante ESCI (azzera VCD e torna al menu).
 *
 * Questa funzione viene chiamata quando il pulsante ESCI viene premuto.
 * Azzera i crediti VCD (vcd_coins, vcd_used, vcd_cents_residual)
 * e ritorna a ADS se abilitato, altrimenti a IDLE.
 *
 * @param e Puntatore all'evento generato.
 * @return Nessun valore di ritorno.
 */
static void on_exit_btn(lv_event_t *e)
{
    (void)e;
    mark_user_interaction();
    ESP_LOGI(TAG, "[C] Pulsante ESCI premuto - azzero VCD e torno al menu");

    fsm_ctx_t snap = {0};
    bool has_snap = fsm_runtime_snapshot(&snap);
    
    if (!has_snap) {
        ESP_LOGW(TAG, "[C] on_exit_btn: non posso leggere FSM snapshot");
        return;
    }

    // Verifico che siamo in stato appropriato (non running, non paused)
    if (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED) {
        ESP_LOGI(TAG, "[C] ESCI ignorato: programma è in corso");
        return;
    }

    // Azzero i crediti VCD
    fsm_input_event_t ev = {
        .from = AGN_ID_LVGL,
        .to = {AGN_ID_FSM},
        .action = ACTION_ID_CREDIT_ENDED,  // Usa azione generica di fine credito
        .type = FSM_INPUT_EVENT_CREDIT_ENDED,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = 0,
        .value_u32 = 0,
        .aux_u32 = 1,  /* Richiede azzeramento VCD nel FSM */
        .data_ptr = NULL,
        .text = {0},
    };

    // Pubblica l'evento per azzerare crediti e tornare a menu
    if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
        ESP_LOGW(TAG, "[C] publish CREDIT_ENDED from EXIT button failed");
    }

    ESP_LOGI(TAG, "[C] ESCI: evento CREDIT_ENDED pubblicato verso FSM");
}

/**
 * @brief [C] Gestisce il click sul pulsante STOP.
 *
 * Questa funzione viene chiamata quando il pulsante STOP viene premuto.
 * Imposta il flag s_stop_pressed per evitare il riavvio automatico del programma
 * quando il programma corrente termina.
 *
 * @param e Puntatore all'evento generato.
 * @return Nessun valore di ritorno.
 */
static void on_stop_btn(lv_event_t *e)
{
    (void)e;
    mark_user_interaction();
    ESP_LOGI(TAG, "[C] Pulsante STOP premuto (confirm=%d)", (int)s_stop_confirm);

    uint8_t pid = s_active_prog;
    fsm_ctx_t snap = {0};
    bool has_snap = fsm_runtime_snapshot(&snap);
    if (!pid && has_snap && (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED) && snap.running_program_name[0] != '\0') {
        const web_ui_program_table_t *tbl = web_ui_program_table_get();
        if (tbl) {
            for (uint8_t i = 0; i < tbl->count; ++i) {
                if (tbl->programs[i].name[0] != '\0' && strcmp(tbl->programs[i].name, snap.running_program_name) == 0) {
                    pid = tbl->programs[i].program_id;
                    break;
                }
            }
        }
    }

    bool running = has_snap && (snap.state == FSM_STATE_RUNNING);
    bool paused = has_snap && (snap.state == FSM_STATE_PAUSED);

    if (!running && !paused) {
        ESP_LOGI(TAG, "[C] STOP ignorato: stato FSM non running/paused");
        return;
    }

    if (!s_stop_confirm) {
        /* First press: ask for confirmation and suspend program */
        s_stop_confirm = true;
        s_stop_pressed = true;
        char stop_confirm_text[64];
        (void)lvgl_i18n_get_text("program_confirm_cancel",
                                 "Conferma annullamento",
                                 stop_confirm_text,
                                 sizeof(stop_confirm_text));
        lv_label_set_text(s_stop_lbl, stop_confirm_text);

        if (running) {
            fsm_input_event_t ev = {
                .from = AGN_ID_LVGL,
                .to = {AGN_ID_FSM},
                .action = ACTION_ID_PROGRAM_PAUSE_TOGGLE,
                .type = FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE,
                .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
                .value_i32 = (int32_t)pid,
                .value_u32 = 0,
                .aux_u32 = 0,
                .data_ptr = NULL,
                .text = {0},
            };
            if (pid) {
                const web_ui_program_entry_t *entry = find_program_entry(pid);
                if (entry) strncpy(ev.text, entry->name, sizeof(ev.text) - 1);
            }
            if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
                ESP_LOGW(TAG, "[C] publish pause_toggle from stop failed for pid=%u", (unsigned)pid);
            }
        } else {
            ESP_LOGI(TAG, "[C] Conferma annullamento attiva con programma già in pausa");
        }
    } else {
        /* Second press: conferma stop IMMEDIATO e mostra popup ringraziamento */
        s_stop_confirm = false;
        if (s_stop_lbl) {
            char stop_text[64];
            (void)lvgl_i18n_get_text("program_stop", "STOP", stop_text, sizeof(stop_text));
            lv_label_set_text(s_stop_lbl, stop_text);
        }

        s_stop_pressed = true;

        if (has_snap) {
            /* [C] Su STOP manuale il messaggio MDR deve apparire sempre, anche se il flag anti-duplicazione era rimasto attivo. */
            show_program_end_effect(panel_effective_credit_cents(&snap));
            s_program_end_message_shown = true;
            ESP_LOGI(TAG, "[C] MDR mostrato su STOP manuale (credito=%ld)",
                     (long)panel_effective_credit_cents(&snap));
        }
        
        /* [M] Invia STOP IMMEDIATO (aux_u32=0 per stop immediato, non a fine ciclo) */
        /* [M] Il popup di ringraziamento verrà mostrato dal ciclo UI quando vede la transizione RUNNING->CREDIT */
        fsm_input_event_t ev = {
            .from = AGN_ID_LVGL,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PROGRAM_STOP,
            .type = FSM_INPUT_EVENT_PROGRAM_STOP,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = (int32_t)pid,
            .value_u32 = 0,
            .aux_u32 = 0U,  /* [M] 0 = stop immediato (non 1 = stop a fine ciclo) */
            .data_ptr = NULL,
            .text = {0},
        };
        if (pid) {
            const web_ui_program_entry_t *entry = find_program_entry(pid);
            if (entry) strncpy(ev.text, entry->name, sizeof(ev.text) - 1);
        }
        if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
            ESP_LOGW(TAG, "[C] publish stop immediately failed for pid=%u", (unsigned)pid);
        } else {
            ESP_LOGI(TAG,
                     "[C] Stop immediato confermato pid=%u",
                     (unsigned)pid);
        }
    }
}

static void main_to_lang_async(void *arg)
{
    (void)arg;
    /* [C] Bandiera premuta: va a selezione lingua con ritorno alla pagina programmi */
    lvgl_page_language_2_show(lvgl_page_main_show);
}

static void on_lang_btn(lv_event_t *e)
{
    (void)e;
    mark_user_interaction();
    ESP_LOGI(TAG, "[C] Pulsante bandiera lingua premuto");
    lv_async_call(main_to_lang_async, NULL);
}

static void update_language_flag(const char *lang_code)
{
    if (lang_code) {
        strncpy(s_current_language, lang_code, sizeof(s_current_language) - 1);
        s_current_language[sizeof(s_current_language) - 1] = '\0';
        ESP_LOGI(TAG, "[C] Lingua aggiornata: %s", lang_code);
    }
}

static void sync_language_from_config(void)
{
    const char *runtime_lang = lvgl_panel_get_runtime_language();
    update_language_flag(runtime_lang && runtime_lang[0] ? runtime_lang : "it");
    ESP_LOGI(TAG, "[C] Lingua sincronizzata da runtime LVGL: %s",
             runtime_lang && runtime_lang[0] ? runtime_lang : "it");
}

/**
 * @brief Costruisce l'intestazione della schermata.
 *
 * @param scr Puntatore all'oggetto di schermata su cui costruire l'intestazione.
 * @return void Nessun valore di ritorno.
 */
static void build_header(lv_obj_t *scr)
{
    (void)scr;

    /* Header locale: ora e bandiera sono gestite dal chrome comune. */

    ESP_LOGI(TAG, "[H] Header creato (bandiera lingua gestita da chrome)");
}

/** @brief Costruisce lo stato dell'interfaccia utente.
 *
 *  @param scr Puntatore all'oggetto di schermo su cui costruire lo stato.
 *  @return Nessun valore di ritorno.
 */
static void build_status(lv_obj_t *scr)
{
    const int32_t timer_y = PANEL_PAD_Y + 120;
    const int32_t timer_h = 56;
    const int32_t timer_to_credit_gap = 14;
    const int32_t credit_y = timer_y + timer_h + timer_to_credit_gap;
    const int32_t credit_h = 320;
    const int32_t status_y = credit_y + credit_h + 12;
    const int32_t stop_y = PANEL_H - PANEL_PAD_Y - PANEL_STOP_BTN_H;
    const int32_t status_h = stop_y - status_y - 8;

    ESP_LOGI(TAG, "[C] build_status: timer_y=%d, credit_y=%d, status_y=%d, stop_y=%d, status_h=%d", 
             (int)timer_y, (int)credit_y, (int)status_y, (int)stop_y, (int)status_h);

    s_gauge = lv_bar_create(scr);
    lv_obj_set_pos(s_gauge, PANEL_PAD_X, timer_y);
    lv_obj_set_size(s_gauge, PANEL_FULL_W, timer_h);
    lv_bar_set_range(s_gauge, 0, 100);
    lv_bar_set_value(s_gauge, 10, LV_ANIM_OFF);
    lv_bar_set_orientation(s_gauge, LV_BAR_ORIENTATION_HORIZONTAL);
    lv_obj_set_style_bg_color(s_gauge, lv_color_make(0x08, 0x08, 0x16), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_gauge, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_gauge, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_gauge, COL_TIMER_NORMAL, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_gauge, 30, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gauge, COL_TIMER_NORMAL, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(s_gauge, COL_TIMER_NORMAL, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(s_gauge, LV_GRAD_DIR_NONE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(s_gauge, 0, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_gauge, 30, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(s_gauge, 0, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(s_gauge, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_gauge, LV_OBJ_FLAG_HIDDEN);

    s_gauge_time_lbl = lv_label_create(s_gauge);
    lv_label_set_text(s_gauge_time_lbl, "0%");
    lv_obj_set_style_text_color(s_gauge_time_lbl, lv_color_make(0x00, 0x00, 0x00), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_gauge_time_lbl, FONT_TIME, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_gauge_time_lbl, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(s_gauge_time_lbl, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_add_flag(s_gauge_time_lbl, LV_OBJ_FLAG_HIDDEN);

    s_credit_box = lv_obj_create(scr);
    lv_obj_set_pos(s_credit_box, PANEL_PAD_X, credit_y);
    lv_obj_set_size(s_credit_box, PANEL_FULL_W, credit_h);
    lv_obj_set_style_bg_opa(s_credit_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_credit_box, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_credit_box, COL_TIMER_NORMAL, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_credit_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_credit_box, 40, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_credit_box, 24, LV_PART_MAIN);
    lv_obj_remove_flag(s_credit_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_credit_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_credit_box, on_credit_box_touch, LV_EVENT_CLICKED, NULL);

    s_program_end_overlay = lv_obj_create(scr);
    lv_obj_set_size(s_program_end_overlay, PANEL_W, PANEL_H);
    lv_obj_set_style_bg_color(s_program_end_overlay, lv_color_make(0x10, 0x12, 0x18), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(s_program_end_overlay, lv_color_make(0x1f, 0x24, 0x2f), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(s_program_end_overlay, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_program_end_overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_program_end_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_program_end_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_program_end_overlay, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_program_end_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_program_end_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_program_end_overlay, on_credit_box_touch, LV_EVENT_CLICKED, NULL);

    s_program_end_card = lv_obj_create(s_program_end_overlay);
    lv_obj_set_size(s_program_end_card, PANEL_FULL_W - 140, 760);
    lv_obj_center(s_program_end_card);
    lv_obj_set_style_bg_color(s_program_end_card, lv_color_make(0xEE, 0xEE, 0xEE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_program_end_card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_program_end_card, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_program_end_card, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_radius(s_program_end_card, 90, LV_PART_MAIN);
    lv_obj_set_style_pad_top(s_program_end_card, 90, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_program_end_card, 70, LV_PART_MAIN);
    lv_obj_set_style_pad_left(s_program_end_card, 60, LV_PART_MAIN);
    lv_obj_set_style_pad_right(s_program_end_card, 60, LV_PART_MAIN);
    lv_obj_remove_flag(s_program_end_card, LV_OBJ_FLAG_SCROLLABLE);

    s_program_end_title_lbl = lv_label_create(s_program_end_card);
    lv_label_set_text(s_program_end_title_lbl, "");
    lv_obj_set_style_text_color(s_program_end_title_lbl, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_program_end_title_lbl, FONT_TIME, LV_PART_MAIN);
    lv_label_set_long_mode(s_program_end_title_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_program_end_title_lbl, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(s_program_end_title_lbl, PANEL_FULL_W - 260);
    lv_obj_align(s_program_end_title_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    s_program_end_credit_lbl = lv_label_create(s_program_end_card);
    lv_label_set_text(s_program_end_credit_lbl, "");
    lv_obj_set_style_text_color(s_program_end_credit_lbl, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_program_end_credit_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_program_end_credit_lbl, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(s_program_end_credit_lbl, PANEL_FULL_W - 260);
    lv_obj_align(s_program_end_credit_lbl, LV_ALIGN_TOP_LEFT, 0, 280);

    s_program_end_msg_lbl = lv_label_create(s_program_end_card);
    lv_label_set_text(s_program_end_msg_lbl, "");
    lv_obj_set_style_text_color(s_program_end_msg_lbl, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_program_end_msg_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_label_set_long_mode(s_program_end_msg_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_program_end_msg_lbl, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(s_program_end_msg_lbl, PANEL_FULL_W - 260);
    lv_obj_align(s_program_end_msg_lbl, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    s_program_popup = lv_obj_create(s_credit_box);
    lv_obj_set_size(s_program_popup, PANEL_FULL_W - 220, 48);
    lv_obj_align(s_program_popup, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(s_program_popup, COL_TIMER_NORMAL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_program_popup, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_program_popup, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_program_popup, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_radius(s_program_popup, 24, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_program_popup, 6, LV_PART_MAIN);
    lv_obj_remove_flag(s_program_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_program_popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_program_popup, LV_OBJ_FLAG_HIDDEN);

    s_program_popup_lbl = lv_label_create(s_program_popup);
    lv_label_set_text(s_program_popup_lbl, "");
    lv_obj_set_style_text_color(s_program_popup_lbl, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_program_popup_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_label_set_long_mode(s_program_popup_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_program_popup_lbl, PANEL_FULL_W - 290);
    lv_obj_center(s_program_popup_lbl);

    s_program_popup_repeat_img = lv_image_create(s_program_popup);
    lv_image_set_src(s_program_popup_repeat_img, &g_program_repeat_icon_32x32);
    lv_obj_set_style_image_recolor(s_program_popup_repeat_img, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_image_recolor_opa(s_program_popup_repeat_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(s_program_popup_repeat_img, LV_ALIGN_BOTTOM_RIGHT, -6, -2);
    lv_obj_add_flag(s_program_popup_repeat_img, LV_OBJ_FLAG_HIDDEN);

    s_status_box = lv_obj_create(scr);
    lv_obj_set_pos(s_status_box, PANEL_PAD_X, status_y);
    lv_obj_set_size(s_status_box, PANEL_FULL_W, status_h);
    lv_obj_set_style_bg_opa(s_status_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_status_box, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_status_box, LV_OBJ_FLAG_SCROLLABLE);

    s_center_box = NULL;
    s_counter_fill = NULL;
    s_residual_credit_lbl = NULL;

    s_credit_lbl = lv_label_create(s_credit_box);
    char zero_text[64];
    (void)lvgl_i18n_get_text("program_zero", "0", zero_text, sizeof(zero_text));
    lv_label_set_text(s_credit_lbl, zero_text);
    lv_obj_set_style_text_color(s_credit_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_credit_lbl, FONT_BIGNUM, LV_PART_MAIN);
    lv_obj_align(s_credit_lbl, LV_ALIGN_RIGHT_MID, -20, 0);

    s_elapsed_lbl = lv_label_create(s_credit_box);
    char credit_text[64];
    (void)lvgl_i18n_get_text("credit_label", "Credito", credit_text, sizeof(credit_text));
    lv_label_set_text(s_elapsed_lbl, credit_text);
    lv_obj_set_style_text_color(s_elapsed_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_elapsed_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_elapsed_lbl, LV_ALIGN_TOP_LEFT, 20, 16);

    s_pause_lbl = lv_label_create(s_credit_box);
    char empty_text[64];
    (void)lvgl_i18n_get_text("program_empty", "", empty_text, sizeof(empty_text));
    lv_label_set_text(s_pause_lbl, empty_text);
    lv_obj_set_style_text_color(s_pause_lbl, COL_PROG_PAUSE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_pause_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_label_set_long_mode(s_pause_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_pause_lbl, PANEL_FULL_W - 220);
    lv_obj_set_style_text_align(s_pause_lbl, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(s_pause_lbl, LV_ALIGN_TOP_LEFT, 20, 74);

    /* [C] Pulsante STOP rosso in basso */
    s_stop_btn = lv_button_create(scr);
    lv_obj_set_pos(s_stop_btn, PANEL_PAD_X, stop_y);
    lv_obj_set_size(s_stop_btn, PANEL_FULL_W, PANEL_STOP_BTN_H);
    lv_obj_set_style_bg_color(s_stop_btn, COL_PROG_LOW, LV_PART_MAIN);  /* Rosso */
    lv_obj_set_style_bg_opa(s_stop_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_stop_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_stop_btn, lv_color_make(0xFF, 0x80, 0x80), LV_PART_MAIN);
    lv_obj_set_style_radius(s_stop_btn, 25, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_stop_btn, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);  /* Nascondi inizialmente */

    s_stop_lbl = lv_label_create(s_stop_btn);
    char stop_text2[64];
    (void)lvgl_i18n_get_text("program_stop", "STOP", stop_text2, sizeof(stop_text2));
    lv_label_set_text(s_stop_lbl, stop_text2);
    lv_obj_set_style_text_color(s_stop_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_stop_lbl, FONT_PROG_BTN, LV_PART_MAIN);
    lv_obj_center(s_stop_lbl);

    lv_obj_add_event_cb(s_stop_btn, on_stop_btn, LV_EVENT_CLICKED, NULL);
    /* [C] Rosso scuro iniziale: non disponibile finché non è in RUNNING/PAUSED */
    lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(s_stop_btn, lv_color_make(0x50, 0x20, 0x20), LV_PART_MAIN);
    lv_obj_set_style_opa(s_stop_btn, LV_OPA_COVER, LV_PART_MAIN);

    /* [C] Pulsante ESCI opzionale (azzera VCD e torna a menu) */
    s_exit_btn = lv_button_create(scr);
    lv_obj_set_pos(s_exit_btn, PANEL_PAD_X, stop_y);
    lv_obj_set_size(s_exit_btn, PANEL_FULL_W, PANEL_STOP_BTN_H);
    lv_obj_set_style_bg_color(s_exit_btn, lv_color_make(0x27, 0xd7, 0xb2), LV_PART_MAIN);  /* Verde turchese */
    lv_obj_set_style_bg_opa(s_exit_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_exit_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_exit_btn, lv_color_make(0x80, 0xFF, 0x80), LV_PART_MAIN);
    lv_obj_set_style_radius(s_exit_btn, 25, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_exit_btn, 0, LV_PART_MAIN);
    lv_obj_add_flag(s_exit_btn, LV_OBJ_FLAG_HIDDEN);  /* Nascondi inizialmente */

    lv_obj_t *exit_lbl = lv_label_create(s_exit_btn);
    char exit_text[64];
    (void)lvgl_i18n_get_text("program_exit", "ESCI", exit_text, sizeof(exit_text));
    lv_label_set_text(exit_lbl, exit_text);
    lv_obj_set_style_text_color(exit_lbl, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_text_font(exit_lbl, FONT_PROG_BTN, LV_PART_MAIN);
    lv_obj_center(exit_lbl);

    lv_obj_add_event_cb(s_exit_btn, on_exit_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_opa(s_exit_btn, LV_OPA_COVER, LV_PART_MAIN);
}

/**
 * Crea un pulsante per un programma.
 *
 * @brief Crea un pulsante per un programma all'interno di un oggetto genitore.
 *
 * @param parent Puntatore all'oggetto genitore in cui verrà creato il pulsante.
 * @param pid Identificatore del programma associato al pulsante.
 * @param x Coordinata x dell'angolo superiore sinistro del pulsante.
 * @param y Coordinata y dell'angolo superiore sinistro del pulsante.
 * @param w Larghezza del pulsante.
 * @param h Altezza del pulsante.
 *
 * @return Niente.
 */
static void create_prog_button(lv_obj_t *parent, uint8_t pid, int32_t x, int32_t y, int32_t w, int32_t h)
{
    // ESP_LOGI(TAG, "[C] === create_prog_button START: pid=%u ===", (unsigned)pid);
    
    if (pid == 0 || pid > PROG_COUNT)
    {
        ESP_LOGE(TAG, "[C] create_prog_button: ERRORE - pid=%u non valido (range: 1-%d)", 
                 (unsigned)pid, PROG_COUNT);
        return;
    }

    // ESP_LOGI(TAG, "[C] create_prog_button: pid=%u, parent=%p, pos=(%d,%d), size=(%d,%d)",
    //          (unsigned)pid, (void*)parent, (int)x, (int)y, (int)w, (int)h);

    if (!parent) {
        ESP_LOGE(TAG, "[C] create_prog_button: ERRORE - parent è NULL per pid=%u", (unsigned)pid);
        return;
    }

    lv_obj_t *btn = lv_button_create(parent);
    if (!btn) {
        ESP_LOGE(TAG, "[C] create_prog_button: ERRORE - lv_button_create fallito per pid=%u", (unsigned)pid);
        return;
    }
    
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    btn_style(btn, COL_PROG);

    bool is_full_width = (w >= (PANEL_FULL_W - (2 * PROG_BTN_SIDE_PAD)));
    bool is_right_column = (!is_full_width) && (x > (PANEL_FULL_W / 2));
    
    // ESP_LOGI(TAG, "[C] create_prog_button: btn=%p created and positioned for pid=%u", (void*)btn, (unsigned)pid);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_color(lbl, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, FONT_PROG_BTN, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, is_right_column ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl, w - (PROG_NUM_BADGE_SIZE + 36));
    set_program_label_text(lbl, pid);

    lv_obj_t *badge = lv_obj_create(btn);
    lv_obj_set_size(badge, PROG_NUM_BADGE_SIZE, PROG_NUM_BADGE_SIZE);
    lv_obj_set_style_bg_color(badge, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(badge, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(badge, PROG_NUM_BADGE_SIZE / 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(badge, 0, LV_PART_MAIN);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *badge_lbl = lv_label_create(badge);
    lv_label_set_text_fmt(badge_lbl, "%u", (unsigned)pid);
    lv_obj_set_style_text_color(badge_lbl, COL_BLACK, LV_PART_MAIN);
    lv_obj_set_style_text_font(badge_lbl, FONT_PROG_BTN, LV_PART_MAIN);
    lv_obj_center(badge_lbl);

    if (is_right_column) {
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_align(badge, LV_ALIGN_RIGHT_MID, -12, 0);
    } else {
        lv_obj_align(badge, LV_ALIGN_LEFT_MID, 12, 0);
        lv_obj_align_to(lbl, badge, LV_ALIGN_OUT_RIGHT_MID, 14, 0);
    }
    
    // ESP_LOGI(TAG, "[C] create_prog_button: label=%p created with text='%s' for pid=%u", 
            //  (void*)lbl, lv_label_get_text(lbl), (unsigned)pid);

    int idx = (int)pid - 1;
    s_prog_ud[idx].prog_id = pid;
    lv_obj_set_user_data(btn, &s_prog_ud[idx]);
    lv_obj_add_event_cb(btn, on_prog_btn, LV_EVENT_CLICKED, NULL);

    s_prog_btns[idx] = btn;
    s_prog_lbls[idx] = lbl;
    
    // ESP_LOGI(TAG, "[C] create_prog_button: STORED - s_prog_btns[%d]=%p, s_prog_lbls[%d]=%p", 
            //  idx, (void*)s_prog_btns[idx], idx, (void*)s_prog_lbls[idx]);

    /* [C] Inizialmente non selezionabile: colore scuro, opacità piena (refresh_prog_buttons aggiornerà) */
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    btn_style(btn, COL_PROG_DIS);
    lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
    // ESP_LOGI(TAG, "[C] === create_prog_button END: pid=%u, btn=%p, lbl=%p ===", 
    //          (unsigned)pid, (void*)btn, (void*)lbl);
}

/** @brief Costruisce i pulsanti del programma.
 *
 *  Il numero e la disposizione dei pulsanti dipende da device_config.num_programs.
 *  Valori ammessi: 1, 2, 4, 6, 8, 10.
 *  - n=1: unico pulsante largo tutta l'area
 *  - n>1: 2 colonne, n/2 righe
 *
 *  @return Niente.
 */
static void build_prog_buttons(void)
{
    ESP_LOGI(TAG, "[C] ===== build_prog_buttons START =====");
    
    // ESP_LOGI(TAG, "[C] build_prog_buttons: s_status_box=%p", (void*)s_status_box);
    
    if (!s_status_box) {
        ESP_LOGE(TAG, "[C] build_prog_buttons: ERRORE - s_status_box è NULL!");
        return;
    }
    
    int32_t status_h = lv_obj_get_height(s_status_box);
    if (status_h <= 0) {
        status_h = PROG_BUTTONS_AREA_H;
    }

    /* Legge numero programmi dalla configurazione */
    device_config_t *cfg = device_config_get();
    uint8_t num_progs = (cfg && cfg->num_programs) ? cfg->num_programs : 10;
    // ESP_LOGI(TAG, "[C] build_prog_buttons: cfg=%p, cfg->num_programs=%u, num_progs=%u", 
    //          (void*)cfg, cfg ? cfg->num_programs : 0, (unsigned)num_progs);

    /* Validazione: solo valori ammessi; default 10 */
    static const uint8_t s_valid[] = {1, 2, 3, 4, 5, 6, 8, 10};
    bool np_valid = false;
    for (int vi = 0; vi < (int)(sizeof(s_valid) / sizeof(s_valid[0])); vi++) {
        if (s_valid[vi] == num_progs) { np_valid = true; break; }
    }
    if (!np_valid) {
        ESP_LOGW(TAG, "[C] build_prog_buttons: num_progs %u non valido, uso default 10", (unsigned)num_progs);
        num_progs = 10;
    }

    s_num_programs = num_progs;
    // ESP_LOGI(TAG, "[C] build_prog_buttons: FINAL num_progs=%u", (unsigned)num_progs);

    if (num_progs <= 5) {
        /* Layout a colonna singola per 1-5 pulsanti */
        int32_t btn_w = PANEL_FULL_W - 2 * PROG_BTN_SIDE_PAD;
        int32_t usable_h = status_h - 2 * PROG_BTN_ROW_PAD - (num_progs - 1) * PROG_BTN_ROW_GAP;
        if (usable_h < (int32_t)num_progs) {
            usable_h = (int32_t)num_progs;
        }
        int32_t btn_h = usable_h / num_progs;
        int32_t total_h = (btn_h * num_progs) + ((num_progs - 1) * PROG_BTN_ROW_GAP);
        int32_t y_start = (status_h - total_h) / 2;
        if (y_start < PROG_BTN_ROW_PAD) {
            y_start = PROG_BTN_ROW_PAD;
        }
        
        // ESP_LOGI(TAG, "[C] build_prog_buttons: SINGLE COLUMN - num_progs=%d, btn_w=%d, btn_h=%d", 
        //          (int)num_progs, (int)btn_w, (int)btn_h);
        
        for (int i = 0; i < num_progs; i++) {
            int32_t y = y_start + i * (btn_h + PROG_BTN_ROW_GAP);
            uint8_t pid = (uint8_t)(i + 1);
            // ESP_LOGI(TAG, "[C] build_prog_buttons: SINGLE COL - creating button pid=%d at y=%d", 
            //          (int)pid, (int)y);
            create_prog_button(s_status_box, pid, PROG_BTN_SIDE_PAD, y, btn_w, btn_h);
        }
    } else {
        /* Layout a 2 colonne per 6, 8, 10 pulsanti */
        int32_t n_rows = (int32_t)(num_progs / 2);
        int32_t col_w  = (PANEL_FULL_W - 2 * PROG_BTN_SIDE_PAD - PROG_BTN_COL_GAP) / 2;
        int32_t usable_h = status_h - 2 * PROG_BTN_ROW_PAD - (n_rows - 1) * PROG_BTN_ROW_GAP;
        if (usable_h < n_rows) {
            usable_h = n_rows;
        }
        int32_t btn_h  = usable_h / n_rows;
        int32_t total_h = (btn_h * n_rows) + ((n_rows - 1) * PROG_BTN_ROW_GAP);
        int32_t y_start = (status_h - total_h) / 2;
        if (y_start < PROG_BTN_ROW_PAD) {
            y_start = PROG_BTN_ROW_PAD;
        }
        int32_t x_left  = PROG_BTN_SIDE_PAD;
        int32_t x_right = PROG_BTN_SIDE_PAD + col_w + PROG_BTN_COL_GAP;
        
        // ESP_LOGI(TAG, "[C] build_prog_buttons: GRID LAYOUT - n_rows=%d, col_w=%d, btn_h=%d", 
        //          (int)n_rows, (int)col_w, (int)btn_h);
        // ESP_LOGI(TAG, "[C] build_prog_buttons: GRID LAYOUT - x_left=%d, x_right=%d", 
        //          (int)x_left, (int)x_right);

        int buttons_created = 0;
        for (int32_t row = 0; row < n_rows; row++) {
            int32_t y      = y_start + row * (btn_h + PROG_BTN_ROW_GAP);
            uint8_t p_left  = (uint8_t)(row * 2 + 1);
            uint8_t p_right = (uint8_t)(row * 2 + 2);
            
            // ESP_LOGI(TAG, "[C] build_prog_buttons: ROW %d - y=%d, left_prog=%d, right_prog=%d", 
            //          (int)row, (int)y, (int)p_left, (int)p_right);
            
            create_prog_button(s_status_box, p_left,  x_left,  y, col_w, btn_h);
            buttons_created++;
            
            if (p_right <= num_progs) {
                create_prog_button(s_status_box, p_right, x_right, y, col_w, btn_h);
                buttons_created++;
            }
        }
        
        ESP_LOGI(TAG, "[C] build_prog_buttons: GRID COMPLETE - buttons_created=%d", buttons_created);
    }
    
    ESP_LOGI(TAG, "[C] ===== build_prog_buttons END =====");
}

/**
 * @brief Cancella tutti i gestori del pannello.
 *
 * Questa funzione rimuove tutti i gestori associati ai pannelli, liberando la memoria e
 * preparando il sistema per una nuova sessione.
 *
 * @param [in/out] Nessun parametro specifico.
 *
 * @return Nessun valore di ritorno.
 */
static void clear_panel_handles(void)
{
    /* s_time_lbl rimosso su richiesta dell'utente */
    s_credit_box = NULL;
    s_status_box = NULL;
    s_center_box = NULL;
    s_counter_fill = NULL;
    s_residual_credit_lbl = NULL;
    s_elapsed_lbl = NULL;
    s_pause_lbl = NULL;
    s_credit_lbl = NULL;
    s_gauge = NULL;
    s_gauge_time_lbl = NULL;
    s_stop_btn = NULL;
    s_program_popup = NULL;
    s_program_popup_lbl = NULL;
    s_program_popup_repeat_img = NULL;
    s_program_end_overlay = NULL;
    s_program_end_card = NULL;
    s_program_end_title_lbl = NULL;
    s_program_end_credit_lbl = NULL;
    s_program_end_msg_lbl = NULL;
    s_program_popup_until_ms = 0U;
    s_program_popup_running_seen = false;
    s_program_end_effect_until_ms = 0U;
    s_last_program_timeout_log_ms = 0U;
    /* s_language_flag_btn e s_flag_img rimossi su richiesta dell'utente */

    for (int i = 0; i < PROG_COUNT; i++) {
        s_prog_btns[i] = NULL;
        s_prog_lbls[i] = NULL;
        s_btn_state_valid[i] = false;
        s_btn_last_active[i] = false;
        s_btn_last_clickable[i] = false;
        s_btn_last_paused[i] = false;
        s_btn_last_prefine[i] = false;
        s_prog_suspended[i] = false;
    }

    s_last_gauge_pct = -1;
    s_last_minute_epoch = (time_t)-1;
    s_stop_pressed = false;
    invalidate_program_label_cache();
}

/**
 * @brief Callback chiamata quando scade il timer del pannello.
 *
 * @param t Puntatore al timer LVGL.
 * @return void Nessun valore di ritorno.
 */
static void panel_timer_cb(lv_timer_t *t)
{
    (void)t;

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap))
    {
        uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
        device_config_t *cfg = device_config_get();
        bool mdr_active = (s_program_end_effect_until_ms > 0U) &&
                          s_program_end_overlay &&
                          !lv_obj_has_flag(s_program_end_overlay, LV_OBJ_FLAG_HIDDEN);

        if (snap.state == FSM_STATE_CREDIT) {
            uint32_t idle_ms = snap.inactivity_ms;
            uint32_t idle_to_ads_ms = MAIN_PAGE_IDLE_TO_ADS_MS;
            if (cfg) {
                if (cfg->timeouts.idle_before_ads_ms > 0) {
                    idle_to_ads_ms = cfg->timeouts.idle_before_ads_ms;
                } else if (cfg->timeouts.exit_programs_ms > 0) {
                    /* compat legacy */
                    idle_to_ads_ms = cfg->timeouts.exit_programs_ms;
                }
            }

            if ((now_ms - s_last_program_timeout_log_ms) >= 1000U) {
                uint32_t rem_ms = (idle_ms < idle_to_ads_ms) ? (idle_to_ads_ms - idle_ms) : 0U;
                ESP_LOGI(TAG,
                         "[C] Timeout programmi rimanente: %lu ms (idle=%lu/%lu)",
                         (unsigned long)rem_ms,
                         (unsigned long)idle_ms,
                         (unsigned long)idle_to_ads_ms);
                s_last_program_timeout_log_ms = now_ms;
            }

            if (!mdr_active && idle_ms >= idle_to_ads_ms) {
                // Verifica se ADS è abilitato prima di tornare alla slideshow
                bool ads_enabled = cfg ? cfg->display.ads_enabled : snap.ads_enabled;
                if (ads_enabled) {
                    ESP_LOGI(TAG, "[C] Timeout inattività scelta programmi (%lu ms), ritorno a slideshow ADS", (unsigned long)idle_ms);
                    lvgl_page_ads_show();
                } else {
                    if ((now_ms - s_last_ads_disabled_log_ms) >= 5000U) {
                        ESP_LOGI(TAG, "[C] Timeout inattività scelta programmi (%lu ms) ma ADS disabilitato, rimango sulla pagina", (unsigned long)idle_ms);
                        s_last_ads_disabled_log_ms = now_ms;
                    }
                }
                return;
            }
        }

        /* [C] Auto-restart: se il programma termina e c'è credito sufficiente,
           ripubblica la selezione dello stesso programma (salvo STOP manuale). */
        bool was_running = s_prev_panel_snap_valid &&
                   (s_prev_panel_snap.state == FSM_STATE_RUNNING || s_prev_panel_snap.state == FSM_STATE_PAUSED);
        bool is_now_program_state = (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED);
        bool session_ended = was_running && !is_now_program_state;
        bool is_now_credit = (snap.state == FSM_STATE_CREDIT);

        if (session_ended) {
            if (!s_program_end_message_shown) {
                show_program_end_effect(panel_effective_credit_cents(&snap));
                s_program_end_message_shown = true;
                ESP_LOGI(TAG,
                         "[C] MDR mostrato a fine sessione programma (prev=%d now=%d credito=%ld)",
                         (int)s_prev_panel_snap.state,
                         (int)snap.state,
                         (long)panel_effective_credit_cents(&snap));
            }

            if (snap.state == FSM_STATE_ADS) {
                s_program_end_nav_pending = true;
                s_program_end_nav_to_ads = true;
                s_program_end_nav_to_main = false;
            } else if (snap.state == FSM_STATE_IDLE) {
                bool ads_enabled = cfg ? cfg->display.ads_enabled : snap.ads_enabled;
                s_program_end_nav_pending = true;
                s_program_end_nav_to_ads = ads_enabled;
                s_program_end_nav_to_main = !ads_enabled;
            }
            
            /* [M] Forza il refresh del credito resettando il cache quando il programma termina */
            s_last_credit_text[0] = '\0';
            s_last_residual_credit_text[0] = '\0';
            ESP_LOGI(TAG, "[C] Cache credito resettato per forza refresh dopo fine programma");
        }

        bool end_effect_active = (s_program_end_effect_until_ms > 0U) &&
                                 !lv_obj_has_flag(s_program_end_overlay, LV_OBJ_FLAG_HIDDEN);
        int32_t effective_credit_cents = panel_effective_credit_cents(&snap);

        if (s_program_end_nav_pending && !end_effect_active) {
            bool go_ads = s_program_end_nav_to_ads;
            bool go_main = s_program_end_nav_to_main;
            s_program_end_nav_pending = false;
            s_program_end_nav_to_ads = false;
            s_program_end_nav_to_main = false;

            if (go_ads) {
                ESP_LOGI(TAG, "[C] Richiesta ADS eseguita alla chiusura MDR");
                lvgl_page_ads_show();
                return;
            }

            if (go_main) {
                ESP_LOGI(TAG, "[C] Richiesta Main eseguita alla chiusura MDR");
                lvgl_page_main_show();
                return;
            }
        }

        if (was_running && is_now_credit && s_active_prog > 0 && !s_stop_pressed && !s_stop_confirm && effective_credit_cents > 0 && !end_effect_active)
        {
            const web_ui_program_entry_t *entry = find_program_entry(s_active_prog);
            if (entry && entry->enabled && effective_credit_cents >= (int32_t)entry->price_units) {
                esp_err_t restart_err = tasks_publish_program_button_action(s_active_prog, AGN_ID_LVGL);
                if (restart_err == ESP_OK) {
                    ESP_LOGI(TAG, "[C] Auto-restart programma %u pubblicato (credito=%ld)",
                             (unsigned)s_active_prog,
                             (long)effective_credit_cents);
                } else {
                    ESP_LOGW(TAG, "[C] Auto-restart programma %u fallito: %s",
                             (unsigned)s_active_prog,
                             tasks_err_to_name(restart_err));
                }
            } else {
                ESP_LOGI(TAG, "[C] Auto-restart programma %u non eseguibile (entry/credito)",
                         (unsigned)s_active_prog);
            }
        }

        if (s_last_fsm_state == FSM_STATE_PAUSED && snap.state == FSM_STATE_RUNNING) {
            s_stop_confirm = false;
            if (s_stop_lbl) {
                char stop_text[64];
                (void)lvgl_i18n_get_text("program_stop", "STOP", stop_text, sizeof(stop_text));
                lv_label_set_text(s_stop_lbl, stop_text);
            }
            ESP_LOGI(TAG, "[C] Ripristino stato STOP dopo ripresa programma");
        }

        if (s_last_fsm_state == FSM_STATE_CREDIT && snap.state != FSM_STATE_CREDIT) {
            s_program_end_message_shown = false;
        }

        s_last_fsm_state = snap.state;
        s_prev_panel_snap = snap;
        s_prev_panel_snap_valid = true;

        const mdb_status_t *mdb_status = mdb_get_status();
        bool mdb_online = mdb_status && mdb_status->coin.is_online;
        lvgl_page_chrome_set_status_icon_state(LVGL_CHROME_STATUS_ICON_CARD, mdb_online);

        update_state(&snap);
        refresh_prog_buttons(&snap);
    }
}

/**
 * @brief Disattiva la pagina principale.
 *
 * Questa funzione disattiva la pagina principale dell'interfaccia utente.
 *
 * @param [in/out] Nessun parametro specifico.
 * @return Nessun valore di ritorno.
 */
void lvgl_page_main_deactivate(void)
{
    if (s_panel_timer)
    {
        lv_timer_del(s_panel_timer);
        s_panel_timer = NULL;
    }

    if (s_clock_timer)
    {
        lv_timer_del(s_clock_timer);
        s_clock_timer = NULL;
    }

    clear_panel_handles();
    s_active_prog = 0;
    s_prev_panel_snap_valid = false;
    s_program_end_nav_pending = false;
    s_program_end_nav_to_ads = false;
    s_program_end_nav_to_main = false;
}

/** @brief Mostra la pagina principale dell'interfaccia grafica LVGL.
 *
 *  Questa funzione visualizza la pagina principale dell'interfaccia grafica LVGL.
 *  Prima di caricare la pagina, invia la sequenza di inizializzazione CCTalk al dispositivo
 *  gettoniera (se abilitato nella configurazione).
 *
 *  @return Nessun valore di ritorno.
 */
void lvgl_page_main_show(void)
{
    ESP_LOGI(TAG, "[C] ===== lvgl_page_main_show START =====");

    panel_enable_payment_devices_on_programs_open();

    esp_err_t dio_err = digital_io_init();
    if (dio_err != ESP_OK) {
        ESP_LOGW(TAG, "[C] Init digital_io per mappa pulsanti fallita: %s", esp_err_to_name(dio_err));
    }
    
    lv_obj_t *scr = lv_scr_act();
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: scr=%p", (void*)scr);

    lvgl_page_ads_deactivate();
    lvgl_page_main_deactivate();
    lvgl_page_language_2_deactivate();  /* [C] Sicurezza: ferma eventuali timer lingua */
    clear_panel_handles();

    /* [C] Ferma il timer chrome e resetta indev PRIMA di distruggere gli oggetti,
       per evitare che lv_event_mark_deleted acceda a puntatori invalidi. */
    lvgl_page_chrome_remove();
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        lv_indev_reset(indev, NULL);
        indev = lv_indev_get_next(indev);
    }

    lv_obj_clean(scr);
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: screen cleaned");

    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    if (s_touch_reset_bound_scr != scr) {
        lv_obj_add_event_cb(scr, on_main_page_touch, LV_EVENT_PRESSED, NULL);
        s_touch_reset_bound_scr = scr;
    }

    s_active_prog = 0;
    mark_user_interaction();
    invalidate_program_label_cache();

    panel_load_translations();
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: translations loaded");

    ESP_LOGI(TAG, "[C] lvgl_page_main_show: building UI components...");
    build_status(scr);
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: build_status completed");
    
    build_header(scr);
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: build_header completed");
    
    sync_language_from_config();
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: language synced");
    
    build_prog_buttons();
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: build_prog_buttons completed");

    lvgl_page_chrome_set_flag_callback(on_lang_btn, NULL);
    lvgl_page_chrome_add(scr);

    const mdb_status_t *mdb_status = mdb_get_status();
    bool mdb_online = mdb_status && mdb_status->cashless.is_online;
    lvgl_page_chrome_set_status_icon_state(LVGL_CHROME_STATUS_ICON_CARD, mdb_online);

    ESP_LOGI(TAG, "[C] lvgl_page_main_show: chrome added (MDB %s)", mdb_online ? "ONLINE" : "OFFLINE");

    update_time(true);

    /* [C] Resetta il cache delle etichette di credito al caricamento della pagina
       per garantire che venga letto il valore corretto indipendentemente dal punto di ingresso */
    s_last_credit_text[0] = '\0';
    s_last_elapsed_text[0] = '\0';
    s_last_pause_text[0] = '\0';
    s_last_residual_credit_text[0] = '\0';
    s_last_gauge_pct = -1;
    s_program_end_message_shown = false;
    s_program_end_nav_pending = false;
    s_program_end_nav_to_ads = false;
    s_program_end_nav_to_main = false;
    s_prev_panel_snap_valid = false;
    invalidate_program_label_cache();
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: cache etichette resettato");

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap))
    {
        update_state(&snap);
        refresh_prog_buttons(&snap);
        s_prev_panel_snap = snap;
        s_prev_panel_snap_valid = true;
    }

    /* Log finale con riepilogo dei pulsanti creati */
    ESP_LOGI(TAG, "[C] lvgl_page_main_show: BUTTONS SUMMARY - s_num_programs=%u", (unsigned)s_num_programs);
    for (int i = 0; i < PROG_COUNT && i < (int)s_num_programs; i++) {
        ESP_LOGI(TAG, "[C] lvgl_page_main_show: s_prog_btns[%d]=%p, s_prog_lbls[%d]=%p", 
                 i, (void*)s_prog_btns[i], i, (void*)s_prog_lbls[i]);
    }
    
    ESP_LOGI(TAG, "[C] ===== lvgl_page_main_show END =====");

    s_panel_timer = lv_timer_create(panel_timer_cb, PANEL_REFRESH_MS, NULL);
    s_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);

    ESP_LOGI(TAG, "[C] Pagina principale LVGL visualizzata");
}

/**
 * @brief Aggiorna i testi principali della pagina LVGL.
 *
 * Questa funzione si occupa di aggiornare i testi principali della pagina LVGL.
 * Non ha parametri di input o output.
 *
 * @return Nessun valore di ritorno.
 */
void lvgl_page_main_refresh_texts(void)
{
    panel_load_translations();
    invalidate_program_label_cache();
    s_last_elapsed_text[0] = '\0';
    s_last_pause_text[0] = '\0';

    for (int i = 0; i < PROG_COUNT; ++i)
    {
        if (!s_prog_lbls[i])
        {
            continue;
        }

        set_program_label_text(s_prog_lbls[i], (uint8_t)(i + 1));
    }

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap))
    {
        update_state(&snap);
        refresh_prog_buttons(&snap);
    }
}
