#include "lvgl_panel_pages.h"

#include "device_config.h"
#include "fsm.h"
#include "web_ui_programs.h"
#include "language_flags.h"
#include "lvgl_panel.h"
#include "main.h"

#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>
#include <time.h>


static const char *TAG = "lvgl_page_main";

#define PANEL_REFRESH_MS 200

#define COL_BG lv_color_make(0x1a, 0x1a, 0x2e)
#define COL_PROG lv_color_make(0x6c, 0x34, 0x83)
#define COL_PROG_ACT lv_color_make(0x1e, 0x8b, 0x45)
#define COL_PROG_LOW lv_color_make(0xc0, 0x39, 0x2b)
#define COL_PROG_PAUSE lv_color_make(0xe6, 0x7e, 0x22)
#define COL_WHITE lv_color_make(0xEE, 0xEE, 0xEE)
#define COL_GREY lv_color_make(0x88, 0x88, 0x99)
#define COL_STATE_IDL lv_color_make(0x20, 0x20, 0x48)

#define PROG_COUNT 10
#define PROG_ROWS 5

#define PANEL_W 720
#define PANEL_H 1280
#define PANEL_PAD_X 14
#define PANEL_PAD_Y 14
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

#define FONT_TIME (&lv_font_montserrat_20)
#define FONT_BIGNUM (&lv_font_montserrat_48)
#define FONT_LABEL (&lv_font_montserrat_32)
#define FONT_PROG_BTN (&lv_font_montserrat_32)
#define FONT_DATETIME (&lv_font_montserrat_20)

typedef struct
{
    uint8_t prog_id;
} prog_btn_ud_t;

static lv_obj_t *s_time_lbl = NULL;
static lv_obj_t *s_credit_box = NULL;
static lv_obj_t *s_status_box = NULL;
static lv_obj_t *s_center_box = NULL;
static lv_obj_t *s_credit_lbl = NULL;
static lv_obj_t *s_elapsed_lbl = NULL;
static lv_obj_t *s_pause_lbl = NULL;
static lv_obj_t *s_residual_credit_lbl = NULL;
static lv_obj_t *s_gauge = NULL;
static lv_obj_t *s_counter_fill = NULL;
static lv_obj_t *s_stop_btn = NULL;
static lv_obj_t *s_language_flag_btn = NULL;
static lv_obj_t *s_flag_img = NULL;  /* Flag image inside the button */
static lv_obj_t *s_prog_btns[PROG_COUNT];
static lv_obj_t *s_prog_lbls[PROG_COUNT];
static lv_timer_t *s_panel_timer = NULL;
static lv_timer_t *s_clock_timer = NULL;
static uint8_t s_active_prog = 0;
static char s_current_language[8] = "it";  /* [C] Current language code (default: Italian) */

static prog_btn_ud_t s_prog_ud[PROG_COUNT];

static char s_last_time_text[40] = "";
static char s_last_credit_text[16] = "";
static char s_last_elapsed_text[32] = "";
static char s_last_pause_text[32] = "";
static char s_last_residual_credit_text[16] = "";
static int32_t s_last_gauge_pct = -1;
static time_t s_last_minute_epoch = (time_t)-1;
static bool s_stop_pressed = false;
static bool s_btn_last_clickable[PROG_COUNT] = {0};
static bool s_btn_last_active[PROG_COUNT] = {0};
static bool s_btn_state_valid[PROG_COUNT] = {0};
static fsm_state_t s_last_fsm_state = FSM_STATE_IDLE;  /* Track last FSM state for credit reset */
static uint32_t s_credit_inactivity_start_ms = 0;      /* When CREDIT state was entered */

static char s_tr_credit[32] = "Credito";
static char s_tr_elapsed_fmt[32] = "Secondi   %s";
static char s_tr_pause_fmt[32] = "Pausa: %s";

/**
 * @brief Carica le traduzioni per il pannello.
 *
 * Questa funzione carica le traduzioni necessarie per il pannello utilizzando la configurazione del dispositivo.
 *
 * @param [in/out] s_tr_credit Puntatore alla stringa dove verrà memorizzata la traduzione del label "Credito".
 * @return void
 */
static void panel_load_translations(void)
{
    if (device_config_get_ui_text_scoped("lvgl", "credit_label", "Credito", s_tr_credit, sizeof(s_tr_credit)) != ESP_OK)
    {
        strncpy(s_tr_credit, "Credito", sizeof(s_tr_credit) - 1);
        s_tr_credit[sizeof(s_tr_credit) - 1] = '\0';
    }
    if (device_config_get_ui_text_scoped("lvgl", "elapsed_fmt", "Secondi   %s", s_tr_elapsed_fmt, sizeof(s_tr_elapsed_fmt)) != ESP_OK)
    {
        strncpy(s_tr_elapsed_fmt, "Secondi   %s", sizeof(s_tr_elapsed_fmt) - 1);
        s_tr_elapsed_fmt[sizeof(s_tr_elapsed_fmt) - 1] = '\0';
    }
    if (device_config_get_ui_text_scoped("lvgl", "pause_fmt", "Pausa: %s", s_tr_pause_fmt, sizeof(s_tr_pause_fmt)) != ESP_OK)
    {
        strncpy(s_tr_pause_fmt, "Pausa: %s", sizeof(s_tr_pause_fmt) - 1);
        s_tr_pause_fmt[sizeof(s_tr_pause_fmt) - 1] = '\0';
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
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
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

/**
 * @brief Aggiorna i pulsanti del programma in base allo stato corrente del contesto del finite state machine.
 *
 * @param [in] snap Puntatore al contesto del finite state machine.
 * @return Nessun valore di ritorno.
 */
static void refresh_prog_buttons(const fsm_ctx_t *snap)
{
    bool has_snap = (snap != NULL);
    bool running_or_paused = has_snap &&
                             (snap->state == FSM_STATE_RUNNING || snap->state == FSM_STATE_PAUSED);

    if (has_snap && snap->credit_cents <= 0)
    {
        s_active_prog = 0;
    }

    for (int i = 0; i < PROG_COUNT; i++)
    {
        lv_obj_t *btn = s_prog_btns[i];
        if (!btn)
        {
            continue;
        }

        uint8_t pid = (uint8_t)(i + 1);
        const web_ui_program_entry_t *entry = find_program_entry(pid);
        bool program_enabled = (entry && entry->enabled);
        bool credit_ok = has_snap && entry &&
                         (snap->credit_cents > 0) &&
                         (snap->credit_cents >= (int32_t)entry->price_units);
        bool is_active = running_or_paused && (s_active_prog == pid) && has_snap && (snap->credit_cents > 0);
        bool can_click = program_enabled && credit_ok;

        if (!s_btn_state_valid[i] || s_btn_last_active[i] != is_active)
        {
            btn_style(btn, is_active ? COL_PROG_ACT : COL_PROG);
            s_btn_last_active[i] = is_active;
        }

        if (!s_btn_state_valid[i] || s_btn_last_clickable[i] != can_click)
        {
            if (can_click)
            {
                lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
            }
            else
            {
                lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
                lv_obj_set_style_opa(btn, LV_OPA_50, LV_PART_MAIN);
            }
            s_btn_last_clickable[i] = can_click;
        }

        s_btn_state_valid[i] = true;
    }

    /* [C] Gestisci lo stato del pulsante STOP: abilitato solo durante RUNNING/PAUSED */
    if (s_stop_btn)
    {
        bool stop_enabled = running_or_paused;

        if (stop_enabled)
        {
            lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_opa(s_stop_btn, LV_OPA_COVER, LV_PART_MAIN);
        }
        else
        {
            lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_opa(s_stop_btn, LV_OPA_50, LV_PART_MAIN);
        }
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
    if (!s_time_lbl)
    {
        return;
    }

    time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        const char *fallback = "--/--/---- --:--";
        if (strcmp(s_last_time_text, fallback) != 0)
        {
            lv_label_set_text(s_time_lbl, fallback);
            strncpy(s_last_time_text, fallback, sizeof(s_last_time_text) - 1);
            s_last_time_text[sizeof(s_last_time_text) - 1] = '\0';
        }
        return;
    }

    struct tm ti;
    localtime_r(&now, &ti);

    char buf[40] = {0};
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M", &ti);

    if (strcmp(s_last_time_text, buf) != 0)
    {
        lv_label_set_text(s_time_lbl, buf);
        strncpy(s_last_time_text, buf, sizeof(s_last_time_text) - 1);
        s_last_time_text[sizeof(s_last_time_text) - 1] = '\0';
    }

    if (publish_minute_message)
    {
        time_t minute_epoch = now / 60;
        if (s_last_minute_epoch == (time_t)-1)
        {
            s_last_minute_epoch = minute_epoch;
        }
        else if (minute_epoch != s_last_minute_epoch)
        {
            char msg[FSM_EVENT_TEXT_MAX_LEN] = {0};
            s_last_minute_epoch = minute_epoch;
            strftime(msg, sizeof(msg), "Ora %H:%M", &ti);
            fsm_append_message(msg);
        }
    }
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

    if (s_status_box)
    {
        lv_obj_set_style_bg_color(s_status_box, COL_STATE_IDL, LV_PART_MAIN);
    }

    if (s_credit_lbl)
    {
        char buf[16] = {0};

        if ((running || paused) && snap->running_target_ms > 0)
        {
            uint32_t rem_ms = (snap->running_target_ms > snap->running_elapsed_ms)
                                  ? (snap->running_target_ms - snap->running_elapsed_ms)
                                  : 0;
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)(rem_ms / 1000));
        }
        else
        {
            //snprintf(buf, sizeof(buf), "%ld", (long)snap->credit_cents);
            snprintf(buf, sizeof(buf), "%ld", (long)snap->credit_cents / 100);
        }

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

        if (running || paused)
        {
            char mm[10] = {0};
            fmt_mm_ss(mm, sizeof(mm), snap->running_elapsed_ms);
            snprintf(buf, sizeof(buf), s_tr_elapsed_fmt, mm);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%s", s_tr_credit);
        }

        if (strcmp(s_last_elapsed_text, buf) != 0)
        {
            lv_label_set_text(s_elapsed_lbl, buf);
            strncpy(s_last_elapsed_text, buf, sizeof(s_last_elapsed_text) - 1);
            s_last_elapsed_text[sizeof(s_last_elapsed_text) - 1] = '\0';
        }
    }

    if (s_pause_lbl)
    {
        char buf[32] = {0};

        if (paused && snap->pause_elapsed_ms > 0)
        {
            char mm[10] = {0};
            fmt_mm_ss(mm, sizeof(mm), snap->pause_elapsed_ms);
            snprintf(buf, sizeof(buf), s_tr_pause_fmt, mm);
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
            snprintf(buf, sizeof(buf), "%ld €", (long)snap->credit_cents / 100);
        }
        else
        {
            /* A riposo, mostra il credito disponibile per il prossimo programma */
            snprintf(buf, sizeof(buf), "%ld €", (long)snap->credit_cents / 100);
        }

        if (strcmp(s_last_residual_credit_text, buf) != 0)
        {
            lv_label_set_text(s_residual_credit_lbl, buf);
            strncpy(s_last_residual_credit_text, buf, sizeof(s_last_residual_credit_text) - 1);
            s_last_residual_credit_text[sizeof(s_last_residual_credit_text) - 1] = '\0';
        }
    }

    int32_t pct = 0;
    if (snap->running_target_ms > 0 && (running || paused))
    {
        uint32_t rem = (snap->running_target_ms > snap->running_elapsed_ms)
                           ? (snap->running_target_ms - snap->running_elapsed_ms)
                           : 0;
        pct = (int32_t)((rem * 100U) / snap->running_target_ms);
    }

    if (s_gauge && pct != s_last_gauge_pct)
    {
        lv_color_t gauge_col = (pct > 30) ? COL_PROG_ACT : COL_PROG_LOW;
        lv_obj_set_style_bg_color(s_gauge, gauge_col, LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_color(s_gauge, gauge_col, LV_PART_INDICATOR);
        lv_obj_set_style_bg_grad_dir(s_gauge, LV_GRAD_DIR_NONE, LV_PART_INDICATOR);
        lv_bar_set_value(s_gauge, pct, LV_ANIM_OFF);
        s_last_gauge_pct = pct;
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
    lv_obj_t *btn = lv_event_get_target(e);
    prog_btn_ud_t *ud = (prog_btn_ud_t *)lv_obj_get_user_data(btn);
    if (!ud)
    {
        return;
    }

    uint8_t pid = ud->prog_id;
    const web_ui_program_entry_t *entry = find_program_entry(pid);
    if (!entry || !entry->enabled)
    {
        ESP_LOGW(TAG, "[C] Programma non disponibile: id=%u", pid);
        return;
    }

    fsm_ctx_t snap = {0};
    if (!fsm_runtime_snapshot(&snap))
    {
        ESP_LOGW(TAG, "[C] Snapshot FSM non disponibile, click programma ignorato");
        return;
    }

    if (snap.credit_cents <= 0 || snap.credit_cents < (int32_t)entry->price_units)
    {
        s_active_prog = 0;
        refresh_prog_buttons(&snap);
        ESP_LOGI(TAG,
                 "[C] Credito insufficiente, programma non selezionabile: id=%u credit=%ld price=%u",
                 pid,
                 (long)snap.credit_cents,
                 (unsigned)entry->price_units);
        return;
    }

    bool pause_toggle = (s_active_prog == pid);
    if (!pause_toggle)
    {
        s_active_prog = pid;
    }

    refresh_prog_buttons(&snap);
    if (!pause_toggle)
    {
        btn_style(btn, COL_PROG_ACT);
    }

    publish_program(pid, pause_toggle, entry);
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
    s_stop_pressed = true;
    ESP_LOGI(TAG, "[C] Pulsante STOP premuto");
}

static void on_lang_btn(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "[C] Pulsante bandiera lingua premuto");
    lvgl_panel_show_language_select();
}

static void update_language_flag(const char *lang_code)
{
    if (!lang_code || !s_flag_img) {
        return;
    }

    strncpy(s_current_language, lang_code, sizeof(s_current_language) - 1);
    s_current_language[sizeof(s_current_language) - 1] = '\0';
    
    lv_image_set_src(s_flag_img, get_flag_for_language(lang_code));
    ESP_LOGI(TAG, "[C] Bandiera lingua aggiornata: %s", lang_code);
}

static void sync_language_from_config(void)
{
    device_config_t *cfg = device_config_get();
    if (cfg && cfg->ui.user_language[0] != '\0') {
        update_language_flag(cfg->ui.user_language);
        ESP_LOGI(TAG, "[C] Lingua sincronizzata da device_config: %s", cfg->ui.user_language);
    } else {
        /* Default to Italian if not set */
        update_language_flag("it");
        ESP_LOGI(TAG, "[C] Lingua impostata al default (italiano)");
    }
}

/**
 * @brief Costruisce l'intestazione della schermata.
 *
 * @param scr Puntatore all'oggetto di schermata su cui costruire l'intestazione.
 * @return void Nessun valore di ritorno.
 */
static void build_header(lv_obj_t *scr)
{
    lv_obj_t *parent = s_center_box ? s_center_box : (s_status_box ? s_status_box : scr);

    s_time_lbl = lv_label_create(parent);
    lv_label_set_text(s_time_lbl, "--/--/---- --:--:--");
    lv_obj_set_style_text_color(s_time_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_time_lbl, FONT_DATETIME, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_time_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(s_time_lbl, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_time_lbl, LV_SIZE_CONTENT);
    lv_obj_align(s_time_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

    /* Crea il pulsante bandiera lingua in alto a destra */
    s_language_flag_btn = lv_btn_create(scr);
    lv_obj_set_size(s_language_flag_btn, 200, 200);
    lv_obj_align(s_language_flag_btn, LV_ALIGN_TOP_RIGHT, -20, 10);
    lv_obj_set_style_bg_color(s_language_flag_btn, lv_color_hex(0x1a1a2e), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_language_flag_btn, 8, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_language_flag_btn, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_radius(s_language_flag_btn, 100, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_language_flag_btn, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(s_language_flag_btn, on_lang_btn, LV_EVENT_CLICKED, NULL);

    /* Crea l'immagine della bandiera all'interno del pulsante */
    s_flag_img = lv_image_create(s_language_flag_btn);
    lv_image_set_src(s_flag_img, get_flag_for_language(s_current_language));
    lv_obj_center(s_flag_img);

    ESP_LOGI(TAG, "[H] Bandiera lingua creata in alto a destra, linguaggio: %s", s_current_language);
}

/** @brief Costruisce lo stato dell'interfaccia utente.
 *
 *  @param scr Puntatore all'oggetto di schermo su cui costruire lo stato.
 *  @return Nessun valore di ritorno.
 */
static void build_status(lv_obj_t *scr)
{
    s_credit_box = lv_obj_create(scr);
    lv_obj_set_pos(s_credit_box, PANEL_PAD_X, PANEL_PAD_Y);
    lv_obj_set_size(s_credit_box, PANEL_FULL_W, PANEL_CREDIT_H);
    lv_obj_set_style_bg_color(s_credit_box, COL_STATE_IDL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_credit_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_credit_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_credit_box, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_credit_box, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_credit_box, LV_OBJ_FLAG_SCROLLABLE);

    s_status_box = lv_obj_create(scr);
    lv_obj_set_pos(s_status_box, PANEL_PAD_X, PANEL_PAD_Y + PANEL_CREDIT_H);
    lv_obj_set_size(s_status_box, PANEL_FULL_W, PANEL_WORK_H);
    lv_obj_set_style_bg_color(s_status_box, COL_STATE_IDL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_status_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_status_box, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_status_box, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_status_box, LV_OBJ_FLAG_SCROLLABLE);

    s_center_box = lv_obj_create(s_status_box);
    lv_obj_set_pos(s_center_box, PANEL_COUNTER_X, 0);
    lv_obj_set_size(s_center_box, PANEL_COUNTER_W, PANEL_WORK_H);
    lv_obj_set_style_bg_opa(s_center_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_center_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_center_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_center_box, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_center_box, LV_OBJ_FLAG_SCROLLABLE);

    s_counter_fill = NULL;

    /* Crea il gauge (countdown bar) più piccolo per fare spazio al credito residuo sotto */
    int32_t gauge_height = PANEL_WORK_H - 100;  /* Ridotto per fare spazio al label credito residuo */
    s_gauge = lv_bar_create(s_center_box);
    lv_obj_set_size(s_gauge, PANEL_COUNTER_W, gauge_height);
    lv_obj_align(s_gauge, LV_ALIGN_TOP_MID, 0, 10);
    lv_bar_set_range(s_gauge, 0, 100);
    lv_bar_set_value(s_gauge, 100, LV_ANIM_OFF);
    lv_bar_set_orientation(s_gauge, LV_BAR_ORIENTATION_VERTICAL);
    lv_obj_set_style_bg_color(s_gauge, lv_color_make(0x40, 0x40, 0x70), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_gauge, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_gauge, 10, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_gauge, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_gauge, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gauge, COL_PROG_ACT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(s_gauge, COL_PROG_ACT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(s_gauge, LV_GRAD_DIR_NONE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(s_gauge, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(s_gauge, 0, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_gauge, 8, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(s_gauge, 0, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(s_gauge, 0, LV_PART_MAIN);

    /* [C] Credito residuo mostrato sotto il countdown */
    s_residual_credit_lbl = lv_label_create(s_center_box);
    lv_label_set_text(s_residual_credit_lbl, "0");
    lv_obj_set_style_text_color(s_residual_credit_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_residual_credit_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_residual_credit_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

    s_credit_lbl = lv_label_create(s_credit_box);
    lv_label_set_text(s_credit_lbl, "0");
    lv_obj_set_style_text_color(s_credit_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_credit_lbl, FONT_BIGNUM, LV_PART_MAIN);
    lv_obj_align(s_credit_lbl, LV_ALIGN_TOP_MID, 0, -5);

    s_elapsed_lbl = lv_label_create(s_credit_box);
    lv_label_set_text(s_elapsed_lbl, "Credito");
    lv_obj_set_style_text_color(s_elapsed_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_elapsed_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_elapsed_lbl, LV_ALIGN_BOTTOM_MID, 0, -46);

    s_pause_lbl = lv_label_create(s_credit_box);
    lv_label_set_text(s_pause_lbl, "");
    lv_obj_set_style_text_color(s_pause_lbl, COL_PROG_PAUSE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_pause_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_pause_lbl, LV_ALIGN_BOTTOM_MID, 0, -8);

    /* [C] Pulsante STOP rosso in basso */
    s_stop_btn = lv_button_create(scr);
    lv_obj_set_pos(s_stop_btn, PANEL_PAD_X, PANEL_PAD_Y + PANEL_CREDIT_H + PANEL_WORK_H);
    lv_obj_set_size(s_stop_btn, PANEL_FULL_W, PANEL_STOP_BTN_H);
    lv_obj_set_style_bg_color(s_stop_btn, COL_PROG_LOW, LV_PART_MAIN);  /* Rosso */
    lv_obj_set_style_bg_opa(s_stop_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_stop_btn, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_stop_btn, lv_color_make(0xFF, 0x80, 0x80), LV_PART_MAIN);
    lv_obj_set_style_radius(s_stop_btn, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_stop_btn, 0, LV_PART_MAIN);

    lv_obj_t *stop_lbl = lv_label_create(s_stop_btn);
    lv_label_set_text(stop_lbl, "STOP");
    lv_obj_set_style_text_color(stop_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(stop_lbl, FONT_PROG_BTN, LV_PART_MAIN);
    lv_obj_center(stop_lbl);

    lv_obj_add_event_cb(s_stop_btn, on_stop_btn, LV_EVENT_CLICKED, NULL);
    lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(s_stop_btn, LV_OPA_50, LV_PART_MAIN);
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
    if (pid == 0 || pid > PROG_COUNT)
    {
        return;
    }

    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    btn_style(btn, COL_PROG);

    lv_obj_t *lbl = lv_label_create(btn);
    char txt[8] = {0};
    snprintf(txt, sizeof(txt), "%u", (unsigned)pid);
    lv_label_set_text(lbl, txt);
    lv_obj_set_style_text_color(lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, FONT_PROG_BTN, LV_PART_MAIN);
    lv_obj_center(lbl);

    int idx = (int)pid - 1;
    s_prog_ud[idx].prog_id = pid;
    lv_obj_set_user_data(btn, &s_prog_ud[idx]);
    lv_obj_add_event_cb(btn, on_prog_btn, LV_EVENT_CLICKED, NULL);

    s_prog_btns[idx] = btn;
    s_prog_lbls[idx] = lbl;

    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(btn, LV_OPA_50, LV_PART_MAIN);
}

/** @brief Costruisce i pulsanti del programma.
 *
 *  Questa funzione si occupa di creare e configurare i pulsanti utilizzati
 *  all'interno dell'interfaccia del programma.
 *
 *  @return Niente.
 */
static void build_prog_buttons(void)
{
    const int32_t btn_gap = 10;
    const int32_t btn_h = (PANEL_WORK_H - (btn_gap * (PROG_ROWS - 1))) / PROG_ROWS;
    const int32_t left_btn_w = (PANEL_LEFT_W * 75) / 100;
    const int32_t right_btn_w = (PANEL_RIGHT_W * 75) / 100;
    const int32_t left_btn_x = PANEL_LEFT_X + ((PANEL_LEFT_W - left_btn_w) / 2);
    const int32_t right_btn_x = PANEL_RIGHT_X + ((PANEL_RIGHT_W - right_btn_w) / 2);

    for (int row = 0; row < PROG_ROWS; row++)
    {
        int32_t y = row * (btn_h + btn_gap);
        uint8_t pid_left = (uint8_t)(row + 1);
        uint8_t pid_right = (uint8_t)(row + 6);
        create_prog_button(s_status_box, pid_left, left_btn_x, y, left_btn_w, btn_h);
        create_prog_button(s_status_box, pid_right, right_btn_x, y, right_btn_w, btn_h);
    }
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
    s_time_lbl = NULL;
    s_credit_box = NULL;
    s_status_box = NULL;
    s_center_box = NULL;
    s_credit_lbl = NULL;
    s_elapsed_lbl = NULL;
    s_pause_lbl = NULL;
    s_residual_credit_lbl = NULL;
    s_gauge = NULL;
    s_counter_fill = NULL;
    s_stop_btn = NULL;

    for (int i = 0; i < PROG_COUNT; i++)
    {
        s_prog_btns[i] = NULL;
        s_prog_lbls[i] = NULL;
        s_btn_last_clickable[i] = false;
        s_btn_last_active[i] = false;
        s_btn_state_valid[i] = false;
    }

    s_last_time_text[0] = '\0';
    s_last_credit_text[0] = '\0';
    s_last_elapsed_text[0] = '\0';
    s_last_pause_text[0] = '\0';
    s_last_residual_credit_text[0] = '\0';
    s_last_gauge_pct = -1;
    s_last_minute_epoch = (time_t)-1;
    s_stop_pressed = false;
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
        /* [C] Detect CREDIT state entry and track inactivity for credit reset */
        if (snap.state == FSM_STATE_CREDIT && s_last_fsm_state != FSM_STATE_CREDIT) {
            /* Just entered CREDIT state */
            s_credit_inactivity_start_ms = snap.inactivity_ms;
            ESP_LOGI(TAG, "[C] Entrato nello stato CREDIT, inizio timeout reset crediti");
        }

        /* [C] Check for credit reset timeout in CREDIT state */
        if (snap.state == FSM_STATE_CREDIT) {
            device_config_t *cfg = device_config_get();
            uint32_t reset_timeout_ms = 300000;  /* Default 5 minutes */
            if (cfg && cfg->timeouts.credit_reset_timeout_ms > 0) {
                reset_timeout_ms = cfg->timeouts.credit_reset_timeout_ms;
            }

            uint32_t time_in_credit = snap.inactivity_ms - s_credit_inactivity_start_ms;
            if (time_in_credit >= reset_timeout_ms && snap.credit_cents > 0) {
                ESP_LOGI(TAG, "[C] Timeout reset crediti raggiunto (%u ms >= %u ms), azzerando crediti e reimpostando lingua",
                         time_in_credit, reset_timeout_ms);
                
                /* Reset credits by publishing zero credit event (or through FSM) */
                // For now, we'll just log and reset; actual credit reset would need FSM event
                
                /* Reset language to Italian */
                update_language_flag("it");
                
                /* Show advertisement page if available */
                lvgl_page_ads_show();
                
                return;  /* Exit early, don't process normal updates */
            }
        }

        /* [C] Show advertisement page when IDLE and inactive for some time */
        if (snap.state == FSM_STATE_IDLE) {
            device_config_t *cfg = device_config_get();
            uint32_t ads_timeout_ms = 30000;  /* Default 30 seconds */
            if (cfg && cfg->timeouts.idle_before_ads_ms > 0) {
                ads_timeout_ms = cfg->timeouts.idle_before_ads_ms;
            }

            /* Show ads if inactive for specified time and no credit */
            if (snap.inactivity_ms >= ads_timeout_ms && snap.credit_cents == 0) {
                ESP_LOGI(TAG, "[C] Timeout ads raggiunto (%u ms >= %u ms), mostro slideshow (credit=%u)",
                         snap.inactivity_ms, ads_timeout_ms, snap.credit_cents);
                
                /* Show advertisement page */
                lvgl_page_ads_show();
                
                return;  /* Exit early, don't process normal updates */
            }
        }

        /* [C] Auto-restart logic: se il programma è terminato e c'è credito,
           riavvia automaticamente lo stesso programma a meno che STOP sia stato premuto */
        static fsm_ctx_t last_snap = {0};
        bool was_running = (last_snap.state == FSM_STATE_RUNNING || last_snap.state == FSM_STATE_PAUSED);
        bool is_now_idle = (snap.state == FSM_STATE_CREDIT);

        if (was_running && is_now_idle && s_active_prog > 0 && !s_stop_pressed && snap.credit_cents > 0)
        {
            /* Condizioni per auto-restart soddisfatte */
            const web_ui_program_entry_t *entry = find_program_entry(s_active_prog);
            if (entry && entry->enabled && snap.credit_cents >= (int32_t)entry->price_units)
            {
                ESP_LOGI(TAG, "[C] Auto-restart del programma %u (credito residuo: %ld cents)", 
                         s_active_prog, snap.credit_cents);
                publish_program(s_active_prog, false, entry);
            }
        }

        /* Reset s_stop_pressed quando torniamo a CREDIT (fine del programma) */
        if (is_now_idle)
        {
            s_stop_pressed = false;
        }

        s_last_fsm_state = snap.state;
        last_snap = snap;

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
    /* Use the new V2 programs page instead of the old one */
    lvgl_page_programs_v2_show();
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
    s_last_elapsed_text[0] = '\0';
    s_last_pause_text[0] = '\0';

    for (int i = 0; i < PROG_COUNT; ++i)
    {
        if (!s_prog_lbls[i])
        {
            continue;
        }

        char tmp[8] = {0};
        snprintf(tmp, sizeof(tmp), "%d", i + 1);
        lv_label_set_text(s_prog_lbls[i], tmp);
    }

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap))
    {
        update_state(&snap);
        refresh_prog_buttons(&snap);
    }
}
