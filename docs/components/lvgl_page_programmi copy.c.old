#include "lvgl_panel_pages.h"

#include "device_config.h"
#include "fsm.h"
#include "lvgl_panel.h"
#include "language_flags.h"
#include "lvgl_page_chrome.h"
#include "main.h"
#include "web_ui_programs.h"

#include "lvgl.h"
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
#define COL_TIMER_NORMAL lv_color_make(0x27, 0xd7, 0xb2)
#define COL_TIMER_WARN lv_color_make(0xb0, 0x0f, 0x6b)
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

// Costanti per layout pulsanti programma
#define PROG_BTN_SIDE_PAD 14    /* margine orizzontale */
#define PROG_BTN_ROW_PAD 8      /* margine verticale */
#define PROG_BTN_COL_GAP 10     /* spazio tra colonne */
#define PROG_BTN_ROW_GAP 8      /* spazio tra righe */
#define PROG_BUTTONS_AREA_H 600 /* altezza area pulsanti richiesta */
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
static lv_obj_t *s_stop_lbl = NULL;  /* label inside stop button */
static lv_obj_t *s_prog_btns[PROG_COUNT];
static lv_obj_t *s_prog_lbls[PROG_COUNT];
static lv_timer_t *s_panel_timer = NULL;
static lv_timer_t *s_clock_timer = NULL;
static uint8_t s_active_prog = 0;
static uint8_t s_num_programs = 10;  /* [C] Numero programmi effettivamente costruiti al build corrente */
static char s_current_language[8] = "it";  /* [C] Current language code (default: Italian) */

static prog_btn_ud_t s_prog_ud[PROG_COUNT];

static char s_last_credit_text[32] = "";
static char s_last_elapsed_text[32] = "";
static char s_last_pause_text[32] = "";
static char s_last_residual_credit_text[32] = "";
static int32_t s_last_gauge_pct = -1;
static time_t s_last_minute_epoch = (time_t)-1;
static bool s_stop_pressed = false;
static bool s_stop_confirm = false;
static bool s_btn_last_clickable[PROG_COUNT] = {0};
static bool s_btn_last_active[PROG_COUNT] = {0};
static bool s_btn_last_warning[PROG_COUNT] = {0};
static bool s_btn_state_valid[PROG_COUNT] = {0};
static bool s_prog_suspended[PROG_COUNT] = {0};
static fsm_state_t s_last_fsm_state = FSM_STATE_IDLE;  /* Track last FSM state for credit reset */
static uint32_t s_credit_inactivity_start_ms = 0;      /* When CREDIT state was entered */
static uint32_t s_last_user_interaction_ms = 0;

static char s_tr_credit[32] = "Credito";
static char s_tr_elapsed_fmt[32] = "Secondi   %s";
static char s_tr_pause_fmt[32] = "Pausa: %s";

#define MAIN_PAGE_IDLE_TO_ADS_MS 60000U

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

static void set_program_label_text(lv_obj_t *label, uint8_t pid)
{
    if (!label)
    {
        return;
    }

    char fallback[8] = {0};
    const char *target_text = NULL;
    const web_ui_program_entry_t *entry = find_program_entry(pid);

    if (entry && entry->name[0] != '\0')
    {
        if (strncmp(entry->name, "__i18n__", 8) == 0) {
            static char resolved[WEB_UI_PROGRAM_NAME_MAX];
            const char *keyname = entry->name + 8;
            if (device_config_get_ui_text_scoped("lvgl", keyname, entry->name, resolved, sizeof(resolved)) == ESP_OK) {
                target_text = resolved;
            } else {
                target_text = entry->name;
            }
        } else {
            target_text = entry->name;
        }
    }
    else
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

/**
 * @brief Aggiorna i pulsanti del programma in base allo stato corrente del contesto del finite state machine.
 *
 * @param [in] snap Puntatore al contesto del finite state machine.
 * @return Nessun valore di ritorno.
 */
static void refresh_prog_buttons(const fsm_ctx_t *snap)
{
    bool running = snap && (snap->state == FSM_STATE_RUNNING);
    bool paused = snap && (snap->state == FSM_STATE_PAUSED);
    bool warning_active = false;

    if (snap && (running || paused) && snap->running_target_ms > 0) {
        uint32_t rem_ms = (snap->running_target_ms > snap->running_elapsed_ms)
                              ? (snap->running_target_ms - snap->running_elapsed_ms)
                              : 0;
        warning_active = (rem_ms <= (snap->running_target_ms * 30 / 100));
    }
    
    for (int i = 0; i < PROG_COUNT; i++)
    {
        lv_obj_t *btn = s_prog_btns[i];
        if (!btn) continue;

        if (s_prog_lbls[i])
        {
            set_program_label_text(s_prog_lbls[i], (uint8_t)(i + 1));
        }
        
        bool is_active = false;
        bool can_click = true;

        if ((running || paused) && s_active_prog == (uint8_t)(i + 1)) {
            is_active = true;
        } else if (snap && (running || paused)) {
            const web_ui_program_entry_t *entry = find_program_entry((uint8_t)(i + 1));
            if (entry && entry->name[0] != '\0' &&
                strcmp(entry->name, snap->running_program_name) == 0) {
                is_active = true;
            }
        }
        
        // Update button state only if changed
        if (!s_btn_state_valid[i] ||
            s_btn_last_active[i] != is_active ||
            s_btn_last_clickable[i] != can_click ||
            s_btn_last_warning[i] != warning_active)
        {
            lv_color_t btn_color;
            if (!can_click) {
                btn_color = COL_PROG_DIS;
            } else if (warning_active) {
                btn_color = COL_TIMER_WARN;
            } else if (is_active) {
                btn_color = COL_PROG_ACT;
            } else if (can_click) {
                btn_color = COL_PROG;
            } else {
                btn_color = COL_PROG_DIS;
            }
            btn_style(btn, btn_color);
            lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(btn, is_active ? 10 : 0, LV_PART_MAIN);
            lv_obj_set_style_border_color(btn, (is_active && paused) ? COL_PROG_LOW : COL_WHITE, LV_PART_MAIN);
            lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
            
            // Update clickability
            if (can_click && !s_btn_last_clickable[i]) {
                lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            } else if (!can_click && s_btn_last_clickable[i]) {
                lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            }
            s_btn_last_clickable[i] = can_click;
        }

        s_btn_last_active[i] = is_active;
        s_btn_last_warning[i] = warning_active;
        s_btn_state_valid[i] = true;
    }

    /* [C] Gestisci lo stato del pulsante STOP: style only here; visibility controlled by update_state */
    if (s_stop_btn)
    {
        // Only update visual style here; actual visibility/clickability handled in update_state
        lv_obj_set_style_bg_color(s_stop_btn, COL_PROG_LOW, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_stop_btn, LV_OPA_COVER, LV_PART_MAIN);
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

    if (s_status_box)
    {
        lv_obj_set_style_bg_color(s_status_box, COL_STATE_IDL, LV_PART_MAIN);
    }

    if (s_credit_lbl)
    {
        char buf[24] = {0};

        snprintf(buf, sizeof(buf), "%ld",
                 (long)(snap->credit_cents / 100));

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
        snprintf(buf, sizeof(buf), "%s", s_tr_credit);

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
        char buf[32] = {0};

        snprintf(buf, sizeof(buf), "%ld €",
                 (long)(snap->credit_cents / 100));

        if (strcmp(s_last_residual_credit_text, buf) != 0)
        {
            lv_label_set_text(s_residual_credit_lbl, buf);
            strncpy(s_last_residual_credit_text, buf, sizeof(s_last_residual_credit_text) - 1);
            s_last_residual_credit_text[sizeof(s_last_residual_credit_text) - 1] = '\0';
        }
    }

    uint32_t rem_ms = 0;
    bool warning_active = false;
    if (snap->running_target_ms > 0 && (running || paused))
    {
        rem_ms = (snap->running_target_ms > snap->running_elapsed_ms)
                     ? (snap->running_target_ms - snap->running_elapsed_ms)
                     : 0;
        warning_active = (rem_ms <= (snap->running_target_ms * 30 / 100));
    }

    if (s_credit_box) {
        lv_obj_set_style_border_color(s_credit_box,
                                      warning_active ? COL_TIMER_WARN : COL_TIMER_NORMAL,
                                      LV_PART_MAIN);
    }

    if (s_gauge)
    {
        if (running || paused) {
            lv_obj_clear_flag(s_gauge, LV_OBJ_FLAG_HIDDEN);
            int32_t pct = 100 - (int32_t)((rem_ms * 90U) / snap->running_target_ms);
            if (pct < 10) pct = 10;
            if (pct > 100) pct = 100;

            lv_color_t gauge_col = warning_active ? COL_TIMER_WARN : COL_TIMER_NORMAL;
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

    if (s_gauge_time_lbl) {
        if (running || paused) {
            char buf[16] = {0};
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)(rem_ms / 1000));
            lv_label_set_text(s_gauge_time_lbl, buf);
            lv_obj_clear_flag(s_gauge_time_lbl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_gauge_time_lbl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* Manage STOP button visibility and label based on FSM state */
    if (s_stop_btn && s_stop_lbl) {
        if (running || paused) {
            lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_add_flag(s_stop_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
            s_stop_confirm = false;
            char stop_text[64];
            device_config_get_ui_text_scoped("lvgl", "program_stop", "STOP", stop_text, sizeof(stop_text));
            lv_label_set_text(s_stop_lbl, stop_text);
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
                            device_config_get_ui_text_scoped("lvgl", "program_suspend", "Sospendi", suspend_text, sizeof(suspend_text));
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
    } else if (running || paused) {
        if (s_active_prog > 0 && s_active_prog <= PROG_COUNT) {
            int idx = (int)s_active_prog - 1;
            if (paused) {
                s_prog_suspended[idx] = true;
            } else {
                s_prog_suspended[idx] = false;
                if (s_prog_lbls[idx]) {
                    set_program_label_text(s_prog_lbls[idx], s_active_prog);
                }
            }
        }
    } else {
        /* No running program: restore labels */
        s_active_prog = 0;
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
    s_last_user_interaction_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());

    lv_obj_t *btn = lv_event_get_target(e);
    prog_btn_ud_t *ud = (prog_btn_ud_t *)lv_obj_get_user_data(btn);
    if (!ud) {
        return;
    }

    uint8_t pid = ud->prog_id;
    const web_ui_program_entry_t *entry = find_program_entry(pid);

    fsm_ctx_t snap = {0};
    bool has_snap = fsm_runtime_snapshot(&snap);
    bool is_active = false;
    if (has_snap && (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED)) {
        if (s_active_prog == pid) {
            is_active = true;
        } else if (entry && entry->name[0] != '\0' && strcmp(entry->name, snap.running_program_name) == 0) {
            is_active = true;
        }
    }

    int idx = (int)pid - 1;

    if (is_active) {
        /* Toggle pause/resume for active program */
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
        if (entry) {
            strncpy(ev.text, entry->name, sizeof(ev.text) - 1);
        }
        if (fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
            /* Toggle local suspended state and update label */
            s_prog_suspended[idx] = !s_prog_suspended[idx];
            if (s_prog_suspended[idx]) {
                if (s_prog_lbls[idx]) lv_label_set_text(s_prog_lbls[idx], "Sospendi");
            } else {
                if (s_prog_lbls[idx]) set_program_label_text(s_prog_lbls[idx], pid);
            }
        } else {
            ESP_LOGW(TAG, "[C] publish pause_toggle failed for pid=%u", (unsigned)pid);
        }
    } else {
        bool is_switch = has_snap && (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED);
        fsm_input_event_t ev = {
            .from = AGN_ID_LVGL,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PROGRAM_SELECTED,
            .type = is_switch ? FSM_INPUT_EVENT_PROGRAM_SWITCH : FSM_INPUT_EVENT_PROGRAM_SELECTED,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32 = entry ? (int32_t)entry->price_units : (int32_t)pid,
            .value_u32 = entry ? (uint32_t)entry->pause_max_suspend_sec * 1000U : 0,
            .aux_u32 = entry ? (uint32_t)entry->duration_sec * 1000U : 0,
            .data_ptr = NULL,
            .text = {0},
        };
        if (entry) strncpy(ev.text, entry->name, sizeof(ev.text) - 1);
        if (!fsm_event_publish(&ev, pdMS_TO_TICKS(20))) {
            ESP_LOGW(TAG, "[C] publish program %s failed for pid=%u", is_switch ? "switch" : "selected", (unsigned)pid);
        } else {
            s_active_prog = pid;
        }
    }
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
    s_last_user_interaction_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
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

    if (!s_stop_confirm) {
        /* First press: ask for confirmation and suspend program */
        s_stop_confirm = true;
        char stop_confirm_text[64];
        device_config_get_ui_text_scoped("lvgl", "program_confirm_cancel", "Conferma annullamento", stop_confirm_text, sizeof(stop_confirm_text));
        lv_label_set_text(s_stop_lbl, stop_confirm_text);

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
        (void)fsm_event_publish(&ev, pdMS_TO_TICKS(20));
    } else {
        /* Second press: confirm cancellation => publish STOP */
        s_stop_confirm = false;
        if (s_stop_lbl) lv_label_set_text(s_stop_lbl, "STOP");

        fsm_input_event_t ev = {
            .from = AGN_ID_LVGL,
            .to = {AGN_ID_FSM},
            .action = ACTION_ID_PROGRAM_STOP,
            .type = FSM_INPUT_EVENT_PROGRAM_STOP,
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
            ESP_LOGW(TAG, "[C] publish program stop/cancel failed for pid=%u", (unsigned)pid);
        }
    }
}

static void update_language_flag(const char *lang_code)
{
    /* Bandiera lingua rimossa su richiesta dell'utente */
    if (lang_code) {
        strncpy(s_current_language, lang_code, sizeof(s_current_language) - 1);
        s_current_language[sizeof(s_current_language) - 1] = '\0';
        ESP_LOGI(TAG, "[C] Lingua aggiornata (senza bandiera): %s", lang_code);
    }
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
    /* s_time_lbl rimosso su richiesta dell'utente */
    /* Pulsante bandiera lingua rimosso su richiesta dell'utente */

    ESP_LOGI(TAG, "[H] Header creato senza bandiera lingua");
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
    const int32_t credit_h = 256;
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
    lv_label_set_text(s_gauge_time_lbl, "0");
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
    device_config_get_ui_text_scoped("lvgl", "program_zero", "0", zero_text, sizeof(zero_text));
    lv_label_set_text(s_credit_lbl, zero_text);
    lv_obj_set_style_text_color(s_credit_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_credit_lbl, FONT_BIGNUM, LV_PART_MAIN);
    lv_obj_align(s_credit_lbl, LV_ALIGN_RIGHT_MID, -20, 0);

    s_elapsed_lbl = lv_label_create(s_credit_box);
    char credit_text[64];
    device_config_get_ui_text_scoped("lvgl", "credit_label", "Credito", credit_text, sizeof(credit_text));
    lv_label_set_text(s_elapsed_lbl, credit_text);
    lv_obj_set_style_text_color(s_elapsed_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_elapsed_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_elapsed_lbl, LV_ALIGN_TOP_LEFT, 20, 16);

    s_pause_lbl = lv_label_create(s_credit_box);
    char empty_text[64];
    device_config_get_ui_text_scoped("lvgl", "program_empty", "", empty_text, sizeof(empty_text));
    lv_label_set_text(s_pause_lbl, empty_text);
    lv_obj_set_style_text_color(s_pause_lbl, COL_PROG_PAUSE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_pause_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_pause_lbl, LV_ALIGN_BOTTOM_LEFT, 20, -12);

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
    device_config_get_ui_text_scoped("lvgl", "program_stop", "STOP", stop_text2, sizeof(stop_text2));
    lv_label_set_text(s_stop_lbl, stop_text2);
    lv_obj_set_style_text_color(s_stop_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_stop_lbl, FONT_PROG_BTN, LV_PART_MAIN);
    lv_obj_center(s_stop_lbl);

    lv_obj_add_event_cb(s_stop_btn, on_stop_btn, LV_EVENT_CLICKED, NULL);
    /* [C] Rosso scuro iniziale: non disponibile finché non è in RUNNING/PAUSED */
    lv_obj_clear_flag(s_stop_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(s_stop_btn, lv_color_make(0x50, 0x20, 0x20), LV_PART_MAIN);
    lv_obj_set_style_opa(s_stop_btn, LV_OPA_COVER, LV_PART_MAIN);
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
    ESP_LOGI(TAG, "[C] === create_prog_button START: pid=%u ===", (unsigned)pid);
    
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
    
    // ESP_LOGI(TAG, "[C] create_prog_button: btn=%p created and positioned for pid=%u", (void*)btn, (unsigned)pid);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_color(lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, FONT_PROG_BTN, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, w - 8);
    set_program_label_text(lbl, pid);
    lv_obj_center(lbl);
    
    // ESP_LOGI(TAG, "[C] create_prog_button: label=%p created with text='%s' for pid=%u", 
    //          (void*)lbl, lv_label_get_text(lbl), (unsigned)pid);

    int idx = (int)pid - 1;
    s_prog_ud[idx].prog_id = pid;
    lv_obj_set_user_data(btn, &s_prog_ud[idx]);
    lv_obj_add_event_cb(btn, on_prog_btn, LV_EVENT_CLICKED, NULL);

    s_prog_btns[idx] = btn;
    s_prog_lbls[idx] = lbl;
    
    // ESP_LOGI(TAG, "[C] create_prog_button: STORED - s_prog_btns[%d]=%p, s_prog_lbls[%d]=%p", 
    //          idx, (void*)s_prog_btns[idx], idx, (void*)s_prog_lbls[idx]);

    /* [C] Inizialmente non selezionabile: colore scuro, opacità piena (refresh_prog_buttons aggiornerà) */
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    btn_style(btn, COL_PROG_DIS);
    lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    
//     ESP_LOGI(TAG, "[C] === create_prog_button END: pid=%u, btn=%p, lbl=%p ===", 
//              (unsigned)pid, (void*)btn, (void*)lbl);
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
    
    int32_t status_w = PANEL_FULL_W;
    int32_t status_h = (s_status_box) ? lv_obj_get_height(s_status_box) : PROG_BUTTONS_AREA_H;
    // ESP_LOGI(TAG, "[C] build_prog_buttons: status_box h=%d, w=%d", (int)status_h, (int)status_w);

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
        int32_t btn_h = usable_h / num_progs;
        
        // ESP_LOGI(TAG, "[C] build_prog_buttons: SINGLE COLUMN - num_progs=%d, btn_w=%d, btn_h=%d", 
        //          (int)num_progs, (int)btn_w, (int)btn_h);
        
        for (int i = 0; i < num_progs; i++) {
            int32_t y = PROG_BTN_ROW_PAD + i * (btn_h + PROG_BTN_ROW_GAP);
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
        int32_t btn_h  = usable_h / n_rows;
        int32_t x_left  = PROG_BTN_SIDE_PAD;
        int32_t x_right = PROG_BTN_SIDE_PAD + col_w + PROG_BTN_COL_GAP;
        
        // ESP_LOGI(TAG, "[C] build_prog_buttons: GRID LAYOUT - n_rows=%d, col_w=%d, btn_h=%d", 
        //          (int)n_rows, (int)col_w, (int)btn_h);
        // ESP_LOGI(TAG, "[C] build_prog_buttons: GRID LAYOUT - x_left=%d, x_right=%d", 
        //          (int)x_left, (int)x_right);

        int buttons_created = 0;
        for (int32_t row = 0; row < n_rows; row++) {
            int32_t y      = PROG_BTN_ROW_PAD + row * (btn_h + PROG_BTN_ROW_GAP);
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
    /* s_language_flag_btn e s_flag_img rimossi su richiesta dell'utente */

    for (int i = 0; i < PROG_COUNT; i++) {
        s_prog_btns[i] = NULL;
        s_prog_lbls[i] = NULL;
        s_btn_state_valid[i] = false;
        s_btn_last_active[i] = false;
        s_btn_last_clickable[i] = false;
        s_btn_last_warning[i] = false;
    }

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
        uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
        device_config_t *cfg = device_config_get();

        /* [C] Detect CREDIT state entry and track inactivity for credit reset */
        if (snap.state == FSM_STATE_CREDIT && s_last_fsm_state != FSM_STATE_CREDIT) {
            /* Just entered CREDIT state */
            s_credit_inactivity_start_ms = snap.inactivity_ms;
            s_last_user_interaction_ms = now_ms;
            ESP_LOGI(TAG, "[C] Entrato nello stato CREDIT, inizio timeout reset crediti");
        }

        /* [C] Check for credit reset timeout in CREDIT state */
        if (snap.state == FSM_STATE_CREDIT) {
            uint32_t reset_timeout_ms = 300000;  /* Default 5 minutes */
            if (cfg && cfg->timeouts.credit_reset_timeout_ms > 0) {
                reset_timeout_ms = cfg->timeouts.credit_reset_timeout_ms;
            }

            uint32_t time_in_credit = snap.inactivity_ms - s_credit_inactivity_start_ms;
            if (time_in_credit >= reset_timeout_ms && snap.credit_cents > 0) {
                ESP_LOGI(TAG, "[C] Timeout reset crediti raggiunto (%u ms >= %u ms), richiesta azzeramento crediti al FSM",
                         time_in_credit, reset_timeout_ms);

                fsm_input_event_t ev = {
                    .from = AGN_ID_LVGL,
                    .to = {AGN_ID_FSM},
                    .action = ACTION_ID_CREDIT_ENDED,
                    .type = FSM_INPUT_EVENT_CREDIT_ENDED,
                    .timestamp_ms = now_ms,
                    .value_i32 = 0,
                    .value_u32 = 0,
                    .aux_u32 = 0,
                    .data_ptr = NULL,
                    .text = {0},
                };
                (void)fsm_event_publish(&ev, pdMS_TO_TICKS(20));
                return;
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
            // Temporarily disabled until web_ui integration is fixed
            // const web_ui_program_entry_t *entry = find_program_entry(s_active_prog);
            // if (entry && entry->enabled && snap.credit_cents >= (int32_t)entry->price_units)
            if (snap.credit_cents > 0) // Simplified condition
            {
                ESP_LOGI(TAG, "[C] Auto-restart del programma %u (credito residuo: %ld cents)", 
                         s_active_prog, snap.credit_cents);
                // publish_program(s_active_prog, false, entry);
                ESP_LOGI(TAG, "[C] Auto-restart temporarily disabled");
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
    ESP_LOGI(TAG, "[C] ===== lvgl_page_main_show START =====");
    
    /* Invia sequenza init CCTalk prima di ogni caricamento della pagina programmi */
    main_cctalk_send_initialization_sequence_async();

    lv_obj_t *scr = lv_scr_act();
    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: scr=%p", (void*)scr);

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
    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: screen cleaned");

    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_active_prog = 0;
    s_last_user_interaction_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());

    panel_load_translations();
    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: translations loaded");

    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: building UI components...");
    build_status(scr);
    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: build_status completed");
    
    build_header(scr);
//  ESP_LOGI(TAG, "[C] lvgl_page_main_show: build_header completed");
    
    sync_language_from_config();  /* Sync language flag from device config */
    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: language synced");
    
    build_prog_buttons();
    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: build_prog_buttons completed");
    
    lvgl_page_chrome_add(scr);
    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: chrome added");

    update_time(true);

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap))
    {
        update_state(&snap);
        refresh_prog_buttons(&snap);
    }

    /* Log finale con riepilogo dei pulsanti creati */
    // ESP_LOGI(TAG, "[C] lvgl_page_main_show: BUTTONS SUMMARY - s_num_programs=%u", (unsigned)s_num_programs);
    // for (int i = 0; i < PROG_COUNT && i < (int)s_num_programs; i++) {
    //     ESP_LOGI(TAG, "[C] lvgl_page_main_show: s_prog_btns[%d]=%p, s_prog_lbls[%d]=%p", 
    //              i, (void*)s_prog_btns[i], i, (void*)s_prog_lbls[i]);
    // }
    
    // ESP_LOGI(TAG, "[C] ===== lvgl_page_main_show END =====");

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
