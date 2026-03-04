/**
 * @file lvgl_panel.c
 * @brief Pannello LVGL emulatore — display 7" 800×1280 verticale.
 *
 * Layout runtime:
 *   - Landing select_language con 5 pulsanti verticali
 *   - Main screen: | pulsanti 1-5 | counter | barra | pulsanti 6-10 |
 */

#include "lvgl_panel.h"
#include "init.h"

#include "device_config.h"

#include "lvgl.h"
#include "bsp/esp32_p4_nano.h"

/* font esterni generati */
extern const lv_font_t arial96;
extern const lv_font_t sevensegments_300;

/* fsm.h è in main/ — incluso come header del componente main */
#include "fsm.h"
#include "web_ui_programs.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "lvgl_panel";

#ifndef DNA_LVGL_COUNTDOWN_BG
#define DNA_LVGL_COUNTDOWN_BG 0
#endif

/* =========================================================================
 * Schermata "Fuori servizio" — ERROR_LOCK attivo
 * ========================================================================= */
void lvgl_panel_show_out_of_service(uint32_t reboots)
{
    /* Se il display non è ancora inizializzato, lo avviamo ora.
     * bsp_display_lock con timeout breve: se riesce → LVGL già attivo;
     * se fallisce → non inizializzato, forziamo init. */
    if (!bsp_display_lock(pdMS_TO_TICKS(200))) {
        ESP_LOGW(TAG, "display non attivo — init forzata per schermata errore");
        if (init_run_display_only() != ESP_OK) {
            ESP_LOGE(TAG, "init display forzata fallita, schermata non disponibile");
            return;
        }
    } else {
        bsp_display_unlock();
    }

    if (bsp_display_lock(0)) {
        lv_obj_t *scr = lv_scr_act();

        /* Sfondo rosso scuro */
        lv_obj_set_style_bg_color(scr, lv_color_make(0x6b, 0x00, 0x00), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_clean(scr);

        /* Icona simbolo ⚠ */
        lv_obj_t *ico = lv_label_create(scr);
        lv_label_set_text(ico, LV_SYMBOL_WARNING);
        lv_obj_set_style_text_font(ico, &lv_font_montserrat_48, LV_PART_MAIN);
        lv_obj_set_style_text_color(ico, lv_color_make(0xFF, 0xCC, 0x00), LV_PART_MAIN);
        lv_obj_align(ico, LV_ALIGN_CENTER, 0, -120);

        /* Testo principale */
        lv_obj_t *lbl = lv_label_create(scr);
        lv_label_set_text(lbl, "Fuori servizio");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_48, LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_MAIN);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -40);

        /* Sottotitolo con conteggio reboot */
        char sub[64];
        snprintf(sub, sizeof(sub), "Reboot consecutivi: %lu\nContattare l'assistenza", (unsigned long)reboots);
        lv_obj_t *sub_lbl = lv_label_create(scr);
        lv_label_set_text(sub_lbl, sub);
        lv_obj_set_style_text_font(sub_lbl, &lv_font_montserrat_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(sub_lbl, lv_color_make(0xFF, 0xAA, 0xAA), LV_PART_MAIN);
        lv_obj_set_style_text_align(sub_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_long_mode(sub_lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(sub_lbl, 700);
        lv_obj_align(sub_lbl, LV_ALIGN_CENTER, 0, 60);

        bsp_display_unlock();
    }
}

/* =========================================================================
 * Colori (tema scuro, palette analoga all'emulator web)
 * ========================================================================= */
#define COL_BG          lv_color_make(0x1a, 0x1a, 0x2e)
#define COL_CARD        lv_color_make(0x25, 0x25, 0x4a)
#define COL_HDR         lv_color_make(0x0d, 0x0d, 0x20)
#define COL_PROG        lv_color_make(0x6c, 0x34, 0x83)
#define COL_PROG_ACT    lv_color_make(0x1e, 0x8b, 0x45)
#define COL_PROG_LOW    lv_color_make(0xc0, 0x39, 0x2b)
#define COL_PROG_PAUSE  lv_color_make(0xe6, 0x7e, 0x22)
#define COL_QR          lv_color_make(0x16, 0xa0, 0x85)
#define COL_CARD_B      lv_color_make(0x29, 0x80, 0xb9)
#define COL_CASH        lv_color_make(0x2c, 0x3e, 0x50)
#define COL_WHITE       lv_color_make(0xEE, 0xEE, 0xEE)
#define COL_GREY        lv_color_make(0x88, 0x88, 0x99)
#define COL_STATE_RUN   lv_color_make(0x1e, 0x6b, 0x35)
#define COL_STATE_PAU   lv_color_make(0x7a, 0x42, 0x00)
#define COL_STATE_IDL   lv_color_make(0x20, 0x20, 0x48)

/* =========================================================================
 * Costanti layout
 * ========================================================================= */
#define PROG_COUNT       10
#define PROG_ROWS        5

#define PANEL_W          800
#define PANEL_H          1280
#define PANEL_PAD_X      14
#define PANEL_PAD_Y      14
#define PANEL_COL_GAP    10
#define PANEL_LEFT_W     150
#define PANEL_RIGHT_W    150
#define PANEL_BAR_W      64
#define PANEL_CONTENT_H  (PANEL_H - (PANEL_PAD_Y * 2))
#define PANEL_COUNTER_W  (PANEL_W - (PANEL_PAD_X * 2) - PANEL_LEFT_W - PANEL_RIGHT_W - PANEL_BAR_W - (PANEL_COL_GAP * 3))
#define PANEL_LEFT_X     PANEL_PAD_X
#define PANEL_COUNTER_X  (PANEL_LEFT_X + PANEL_LEFT_W + PANEL_COL_GAP)
#define PANEL_BAR_X      (PANEL_COUNTER_X + PANEL_COUNTER_W + PANEL_COL_GAP)
#define PANEL_RIGHT_X    (PANEL_BAR_X + PANEL_BAR_W + PANEL_COL_GAP)

/* Font disponibili. Se il build fallisce per font mancante,
 * abilitarlo in sdkconfig con CONFIG_LV_FONT_MONTSERRAT_XX=y */
#define FONT_TITLE       (&lv_font_montserrat_24)
#define FONT_TIME        (&lv_font_montserrat_18)
#define FONT_BIGNUM      (&sevensegments_300)   /* numero grande */
#define FONT_LABEL       (&lv_font_montserrat_32) /* testo sotto grande */
#define FONT_PROG_BTN    (&lv_font_montserrat_32)
#define FONT_MSG_TEXT    (&lv_font_montserrat_14)

/* =========================================================================
 * Widget handles
 * ========================================================================= */
static lv_obj_t *s_time_lbl    = NULL;
static lv_obj_t *s_status_box  = NULL;
static lv_obj_t *s_credit_lbl  = NULL;
static lv_obj_t *s_elapsed_lbl = NULL;
static lv_obj_t *s_pause_lbl   = NULL;
static lv_obj_t *s_gauge       = NULL;    /* barra verticale a destra */
static lv_obj_t *s_counter_fill = NULL;   /* sfondo countdown pieno (mode DNA_LVGL_COUNTDOWN_BG=1) */
static lv_obj_t *s_prog_btns[PROG_COUNT];
static lv_obj_t *s_prog_lbls[PROG_COUNT];
static lv_timer_t *s_panel_timer = NULL;

/* Stato runtime locale */
static uint8_t s_active_prog = 0;

/* =========================================================================
 * User data allocati staticamente per i pulsanti
 * ========================================================================= */
typedef struct { uint8_t prog_id; } prog_btn_ud_t;
static prog_btn_ud_t s_prog_ud[PROG_COUNT];

typedef struct {
    const char *code;
    const char *label;
} panel_language_option_t;

static const panel_language_option_t s_panel_lang_opts[] = {
    {.code = "it", .label = "🇮🇹 Italiano"},
    {.code = "en", .label = "🇬🇧 English"},
    {.code = "de", .label = "🇩🇪 Deutsch"},
    {.code = "fr", .label = "🇫🇷 Français"},
    {.code = "es", .label = "🇪🇸 Español"},
};

static char s_selected_user_lang[8] = "it";

static void btn_style(lv_obj_t *btn, lv_color_t bg);
static void panel_timer_cb(lv_timer_t *t);
static void panel_show_main_screen(void);
static void panel_show_language_select_screen(void);

/* Cerca metadati programma per id */
static const web_ui_program_entry_t *find_program_entry(uint8_t pid)
{
    const web_ui_program_table_t *tbl = web_ui_program_table_get();
    if (!tbl) {
        return NULL;
    }
    for (uint8_t i = 0; i < tbl->count; i++) {
        if (tbl->programs[i].program_id == pid) {
            return &tbl->programs[i];
        }
    }
    return NULL;
}

/* Aggiorna stato pulsanti programma in base a credito/FSM */
static void refresh_prog_buttons(const fsm_ctx_t *snap)
{
    bool has_snap = (snap != NULL);
    bool running_or_paused = has_snap &&
                             (snap->state == FSM_STATE_RUNNING || snap->state == FSM_STATE_PAUSED);

    if (has_snap && snap->credit_cents <= 0) {
        s_active_prog = 0;
    }

    for (int i = 0; i < PROG_COUNT; i++) {
        lv_obj_t *btn = s_prog_btns[i];
        if (!btn) {
            continue;
        }

        uint8_t pid = (uint8_t)(i + 1);
        const web_ui_program_entry_t *entry = find_program_entry(pid);
        bool program_enabled = (entry && entry->enabled);
        bool credit_ok = has_snap && entry &&
                         (snap->credit_cents > 0) &&
                         (snap->credit_cents >= (int32_t)entry->price_units);
        bool is_active = running_or_paused && (s_active_prog == pid) && has_snap && (snap->credit_cents > 0);

        if (is_active) {
            btn_style(btn, COL_PROG_ACT);
        } else {
            btn_style(btn, COL_PROG);
        }

        bool can_click = program_enabled && credit_ok;
        if (can_click) {
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
        } else {
            lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_opa(btn, LV_OPA_50, LV_PART_MAIN);
        }
    }
}

/* =========================================================================
 * Helpers
 * ========================================================================= */

/** Applica stile base a un pulsante (colore sfondo, radius, no border) */
static void btn_style(lv_obj_t *btn, lv_color_t bg)
{
    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 4, LV_PART_MAIN);
}

/** Formatta ms in "MM:SS" */
static void fmt_mm_ss(char *buf, size_t len, uint32_t ms)
{
    uint32_t s = ms / 1000;
    snprintf(buf, len, "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

/* Aggiorna l'orologio header — aggiorna LVGL solo se il testo è cambiato */
static void update_time(void)
{
    if (!s_time_lbl) return;
    time_t now = time(NULL);
    static char s_last_time[16] = "";
    char buf[16];
    if (now == (time_t)-1) {
        if (strcmp(s_last_time, "--:--:--") != 0) {
            lv_label_set_text(s_time_lbl, "--:--:--");
            strncpy(s_last_time, "--:--:--", sizeof(s_last_time) - 1);
        }
        return;
    }
    struct tm ti;
    localtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    if (strcmp(s_last_time, buf) != 0) {
        lv_label_set_text(s_time_lbl, buf);
        strncpy(s_last_time, buf, sizeof(s_last_time) - 1);
    }
}

/* Aggiorna box status in base a snapshot FSM (crediti, timer, stato) */
static void update_state(const fsm_ctx_t *snap)
{
    if (!snap) return;

    bool running = (snap->state == FSM_STATE_RUNNING);
    bool paused  = (snap->state == FSM_STATE_PAUSED);

    char tr_credit[32] = {0};
    char tr_elapsed_fmt[32] = {0};
    char tr_pause_fmt[32] = {0};
    if (device_config_get_ui_text_scoped("lvgl", "credit_label", "Credito", tr_credit, sizeof(tr_credit)) != ESP_OK) {
        strncpy(tr_credit, "Credito", sizeof(tr_credit) - 1);
    }
    if (device_config_get_ui_text_scoped("lvgl", "elapsed_fmt", "Secondi   %s", tr_elapsed_fmt, sizeof(tr_elapsed_fmt)) != ESP_OK) {
        strncpy(tr_elapsed_fmt, "Secondi   %s", sizeof(tr_elapsed_fmt) - 1);
    }
    if (device_config_get_ui_text_scoped("lvgl", "pause_fmt", "Pausa: %s", tr_pause_fmt, sizeof(tr_pause_fmt)) != ESP_OK) {
        strncpy(tr_pause_fmt, "Pausa: %s", sizeof(tr_pause_fmt) - 1);
    }

    /* sfondo status box — aggiorna solo se lo stato FSM è cambiato */
    static fsm_state_t s_last_fsm_state = (fsm_state_t)-1;
    if (s_status_box && snap->state != s_last_fsm_state) {
#if DNA_LVGL_COUNTDOWN_BG == 0
        lv_color_t col = running ? COL_STATE_RUN :
                         paused  ? COL_STATE_PAU :
                                   COL_STATE_IDL;
        lv_obj_set_style_bg_color(s_status_box, col, LV_PART_MAIN);
#else
        lv_obj_set_style_bg_color(s_status_box, COL_STATE_IDL, LV_PART_MAIN);
#endif
        s_last_fsm_state = snap->state;
    }

    /* Numero grande: countdown (secondi rimanenti) se running/paused,
     * altrimenti credito (coin) — aggiorna LVGL solo se testo cambiato */
    if (s_credit_lbl) {
        static char s_last_credit[16] = "";
        char buf[16];
        if ((running || paused) && snap->running_target_ms > 0) {
            uint32_t rem_ms = (snap->running_target_ms > snap->running_elapsed_ms)
                              ? snap->running_target_ms - snap->running_elapsed_ms
                              : 0;
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)(rem_ms / 1000));
        } else {
            snprintf(buf, sizeof(buf), "%ld", (long)snap->credit_cents);
        }
        if (strcmp(s_last_credit, buf) != 0) {
            lv_label_set_text(s_credit_lbl, buf);
            strncpy(s_last_credit, buf, sizeof(s_last_credit) - 1);
        }
    }

    /* Etichetta sotto: contestuale — aggiorna solo se testo cambiato */
    if (s_elapsed_lbl) {
        static char s_last_elapsed[32] = "";
        char buf[32];
        if (running || paused) {
            char mm[10];
            fmt_mm_ss(mm, sizeof(mm), snap->running_elapsed_ms);
            snprintf(buf, sizeof(buf), tr_elapsed_fmt, mm);
        } else {
            snprintf(buf, sizeof(buf), "%s", tr_credit);
        }
        if (strcmp(s_last_elapsed, buf) != 0) {
            lv_label_set_text(s_elapsed_lbl, buf);
            strncpy(s_last_elapsed, buf, sizeof(s_last_elapsed) - 1);
        }
    }

    /* timer pausa — aggiorna solo se testo cambiato */
    if (s_pause_lbl) {
        static char s_last_pause[32] = "";
        char buf[32];
        if (paused && snap->pause_elapsed_ms > 0) {
            char mm[10];
            fmt_mm_ss(mm, sizeof(mm), snap->pause_elapsed_ms);
            snprintf(buf, sizeof(buf), tr_pause_fmt, mm);
        } else {
            buf[0] = '\0';
        }
        if (strcmp(s_last_pause, buf) != 0) {
            lv_label_set_text(s_pause_lbl, buf);
            strncpy(s_last_pause, buf, sizeof(s_last_pause) - 1);
        }
    }

    int32_t pct = 0;
    if (snap->running_target_ms > 0 && (running || paused)) {
        uint32_t rem = snap->running_target_ms;
        if (rem > snap->running_elapsed_ms) {
            rem -= snap->running_elapsed_ms;
        } else {
            rem = 0;
        }
        pct = (int32_t)((rem * 100) / snap->running_target_ms);
    }

#if DNA_LVGL_COUNTDOWN_BG == 1
    /* countdown come sfondo pieno dell'area counter: il riempimento scende */
    if (s_counter_fill) {
        static int32_t s_last_pct = -1;
        if (pct != s_last_pct) {
            int32_t h = (PANEL_CONTENT_H * pct) / 100;
            lv_obj_set_size(s_counter_fill, PANEL_COUNTER_W, h);
            lv_obj_align(s_counter_fill, LV_ALIGN_BOTTOM_LEFT, 0, 0);
            s_last_pct = pct;
        }
    }
#else
    /* progress bar — aggiorna solo se la percentuale è cambiata */
    if (s_gauge) {
        static int32_t s_last_pct = -1;
        if (pct != s_last_pct) {
            lv_color_t gauge_col = (pct > 30) ? COL_PROG_ACT : COL_PROG_LOW;
            lv_obj_set_style_bg_color(s_gauge, gauge_col, LV_PART_INDICATOR);
            lv_obj_set_style_bg_grad_color(s_gauge, gauge_col, LV_PART_INDICATOR);
            lv_obj_set_style_bg_grad_dir(s_gauge, LV_GRAD_DIR_NONE, LV_PART_INDICATOR);
            lv_bar_set_value(s_gauge, pct, LV_ANIM_OFF);
            s_last_pct = pct;
        }
    }
#endif
}


/** Pubblica evento selezione/cambio programma, stessa struttura dell'emulator web */
static bool publish_program(uint8_t prog_id, bool pause_toggle,
                             const web_ui_program_entry_t *entry)
{
    if (!fsm_event_queue_init(0)) return false;

    if (pause_toggle) {
        fsm_input_event_t ev = {
            .from         = AGN_ID_LVGL,
            .to           = {AGN_ID_FSM},
            .action       = ACTION_ID_PROGRAM_PAUSE_TOGGLE,
            .type         = FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE,
            .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
            .value_i32    = (int32_t)prog_id,
            .value_u32    = 0,
            .aux_u32      = 0,
            .data_ptr     = NULL,
            .text         = {0},
        };
        if (entry) strncpy(ev.text, entry->name, sizeof(ev.text) - 1);
        return fsm_event_publish(&ev, pdMS_TO_TICKS(20));
    }

    /* start o switch: verifica prima lo stato attuale */
    fsm_ctx_t snap = {0};
    bool has_snap  = fsm_runtime_snapshot(&snap);
    bool is_switch = has_snap &&
                     (snap.state == FSM_STATE_RUNNING || snap.state == FSM_STATE_PAUSED);

    fsm_input_event_t ev = {
        .from         = AGN_ID_LVGL,
        .to           = {AGN_ID_FSM},
        .action       = ACTION_ID_PROGRAM_SELECTED,
        .type         = is_switch ? FSM_INPUT_EVENT_PROGRAM_SWITCH
                                  : FSM_INPUT_EVENT_PROGRAM_SELECTED,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32    = entry ? (int32_t)entry->price_units   : (int32_t)prog_id,
        .value_u32    = entry ? (uint32_t)entry->pause_max_suspend_sec * 1000U : 0,
        .aux_u32      = entry ? (uint32_t)entry->duration_sec * 1000U : 0,
        .data_ptr     = NULL,
        .text         = {0},
    };
    if (entry) strncpy(ev.text, entry->name, sizeof(ev.text) - 1);

    bool ok = fsm_event_publish(&ev, pdMS_TO_TICKS(20));
    if (!ok) {
        ESP_LOGW(TAG, "[C] Coda FSM piena per prog=%u", prog_id);
    }
    return ok;
}

/* =========================================================================
 * Event handler pulsanti
 * ========================================================================= */

static void on_prog_btn(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    prog_btn_ud_t *ud = (prog_btn_ud_t *)lv_obj_get_user_data(btn);
    if (!ud) return;

    uint8_t pid = ud->prog_id;

    const web_ui_program_entry_t *entry = find_program_entry(pid);
    if (!entry || !entry->enabled) {
        ESP_LOGW(TAG, "[C] Programma non disponibile: id=%u", pid);
        return;
    }

    fsm_ctx_t snap = {0};
    if (!fsm_runtime_snapshot(&snap)) {
        ESP_LOGW(TAG, "[C] Snapshot FSM non disponibile, click programma ignorato");
        return;
    }

    if (snap.credit_cents <= 0 || snap.credit_cents < (int32_t)entry->price_units) {
        s_active_prog = 0;
        refresh_prog_buttons(&snap);
        ESP_LOGI(TAG, "[C] Credito insufficiente, programma non selezionabile: id=%u credit=%ld price=%u",
                 pid,
                 (long)snap.credit_cents,
                 (unsigned)entry->price_units);
        return;
    }

    /* se è il programma già attivo → pause/toggle, altrimenti start */
    bool pause_toggle = (s_active_prog == pid);
    if (!pause_toggle) {
        for (int i = 0; i < PROG_COUNT; i++) {
            if (s_prog_btns[i]) {
                btn_style(s_prog_btns[i], COL_PROG);
            }
        }
        s_active_prog = pid;
        /* feedback visivo immediato */
        btn_style(btn, COL_PROG_ACT);
        lv_obj_set_style_bg_color(btn, COL_PROG_ACT, LV_PART_MAIN);
    }

    publish_program(pid, pause_toggle, entry);
}


/* =========================================================================
 * Costruzione UI — Header
 * ========================================================================= */

static void build_header(lv_obj_t *scr)
{
    s_time_lbl = lv_label_create(scr);
    lv_label_set_text(s_time_lbl, "--:--:--");
    lv_obj_set_style_text_color(s_time_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_time_lbl, FONT_TIME, LV_PART_MAIN);
    lv_obj_set_pos(s_time_lbl, PANEL_COUNTER_X + PANEL_COUNTER_W - 98, PANEL_PAD_Y + 10);
}

/* =========================================================================
 * Costruzione UI — Status box
 * ========================================================================= */

static void build_status(lv_obj_t *scr)
{
    s_status_box = lv_obj_create(scr);
    lv_obj_set_pos(s_status_box, PANEL_COUNTER_X, PANEL_PAD_Y);
    lv_obj_set_size(s_status_box, PANEL_COUNTER_W, PANEL_CONTENT_H);
    lv_obj_set_style_bg_color(s_status_box, COL_STATE_IDL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_status_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_status_box, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_status_box, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_status_box, LV_OBJ_FLAG_SCROLLABLE);

#if DNA_LVGL_COUNTDOWN_BG == 1
    s_counter_fill = lv_obj_create(s_status_box);
    lv_obj_set_size(s_counter_fill, PANEL_COUNTER_W, 0);
    lv_obj_align(s_counter_fill, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_counter_fill, COL_PROG_ACT, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(s_counter_fill, COL_PROG_LOW, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(s_counter_fill, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_counter_fill, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_counter_fill, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_counter_fill, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_counter_fill, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_counter_fill, LV_OBJ_FLAG_SCROLLABLE);
#else
    s_counter_fill = NULL;
#endif

    s_gauge = lv_bar_create(scr);
    lv_obj_set_pos(s_gauge, PANEL_BAR_X, PANEL_PAD_Y);
    lv_obj_set_size(s_gauge, PANEL_BAR_W, PANEL_CONTENT_H);
    lv_bar_set_range(s_gauge, 0, 100);
    lv_bar_set_value(s_gauge, 100, LV_ANIM_OFF);
    lv_bar_set_orientation(s_gauge, LV_BAR_ORIENTATION_VERTICAL);
    lv_obj_set_style_bg_color(s_gauge, lv_color_make(0x40, 0x40, 0x70), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_gauge, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_gauge, 3, LV_PART_MAIN);
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

    s_credit_lbl = lv_label_create(s_status_box);
    lv_label_set_text(s_credit_lbl, "0");
    lv_obj_set_style_text_color(s_credit_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_credit_lbl, FONT_BIGNUM, LV_PART_MAIN);
    lv_obj_align(s_credit_lbl, LV_ALIGN_CENTER, 0, -90);

    s_elapsed_lbl = lv_label_create(s_status_box);
    lv_label_set_text(s_elapsed_lbl, "Credito");
    lv_obj_set_style_text_color(s_elapsed_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_elapsed_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_elapsed_lbl, LV_ALIGN_CENTER, 0, 70);

    s_pause_lbl = lv_label_create(s_status_box);
    lv_label_set_text(s_pause_lbl, "");
    lv_obj_set_style_text_color(s_pause_lbl, COL_PROG_PAUSE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_pause_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_pause_lbl, LV_ALIGN_BOTTOM_MID, 0, -24);
}


/* =========================================================================
 * Costruzione UI — Pulsanti programma
 * ========================================================================= */

static void create_prog_button(lv_obj_t *scr, uint8_t pid, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (pid == 0 || pid > PROG_COUNT) {
        return;
    }

    lv_obj_t *btn = lv_button_create(scr);
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
}

static void build_prog_buttons(lv_obj_t *scr)
{
    const int32_t btn_gap = 10;
    const int32_t btn_h = (PANEL_CONTENT_H - (btn_gap * (PROG_ROWS - 1))) / PROG_ROWS;

    for (int row = 0; row < PROG_ROWS; row++) {
        int32_t y = PANEL_PAD_Y + row * (btn_h + btn_gap);
        uint8_t pid_left = (uint8_t)(row + 1);
        uint8_t pid_right = (uint8_t)(row + 6);
        create_prog_button(scr, pid_left, PANEL_LEFT_X, y, PANEL_LEFT_W, btn_h);
        create_prog_button(scr, pid_right, PANEL_RIGHT_X, y, PANEL_RIGHT_W, btn_h);
    }
}

static void clear_panel_handles(void)
{
    s_time_lbl = NULL;
    s_status_box = NULL;
    s_credit_lbl = NULL;
    s_elapsed_lbl = NULL;
    s_pause_lbl = NULL;
    s_gauge = NULL;
    s_counter_fill = NULL;
    for (int i = 0; i < PROG_COUNT; i++) {
        s_prog_btns[i] = NULL;
        s_prog_lbls[i] = NULL;
    }
}

static void panel_apply_selected_language_async(void *arg)
{
    (void)arg;

    device_config_t *cfg = device_config_get();
    if (cfg) {
        strncpy(cfg->ui.user_language, s_selected_user_lang, sizeof(cfg->ui.user_language) - 1);
        cfg->ui.user_language[sizeof(cfg->ui.user_language) - 1] = '\0';
        cfg->updated = true;
        if (device_config_save(cfg) != ESP_OK) {
            ESP_LOGW(TAG, "[C] Salvataggio lingua pannello fallito (%s)", s_selected_user_lang);
        }
    }

    panel_show_main_screen();
}

static void on_lang_btn(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const panel_language_option_t *opt = (const panel_language_option_t *)lv_obj_get_user_data(btn);
    if (!opt || !opt->code) {
        return;
    }

    strncpy(s_selected_user_lang, opt->code, sizeof(s_selected_user_lang) - 1);
    s_selected_user_lang[sizeof(s_selected_user_lang) - 1] = '\0';
    lv_async_call(panel_apply_selected_language_async, NULL);
}

static void panel_show_main_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    clear_panel_handles();
    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_active_prog = 0;

    build_status(scr);
    build_header(scr);
    build_prog_buttons(scr);

    update_time();

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap)) {
        update_state(&snap);
        refresh_prog_buttons(&snap);
    }

    if (s_panel_timer) {
        lv_timer_del(s_panel_timer);
        s_panel_timer = NULL;
    }
    s_panel_timer = lv_timer_create(panel_timer_cb, 700, NULL);

    ESP_LOGI(TAG, "[C] Pannello LVGL principale visualizzato");
}

static void panel_show_language_select_screen(void)
{
    lv_obj_t *scr = lv_scr_act();
    clear_panel_handles();
    lv_obj_clean(scr);

    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    if (s_panel_timer) {
        lv_timer_del(s_panel_timer);
        s_panel_timer = NULL;
    }

    const char *current_lang = device_config_get_ui_user_language();
    if (current_lang && strlen(current_lang) == 2) {
        strncpy(s_selected_user_lang, current_lang, sizeof(s_selected_user_lang) - 1);
        s_selected_user_lang[sizeof(s_selected_user_lang) - 1] = '\0';
    }

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Seleziona lingua");
    lv_obj_set_style_text_color(title, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(title, FONT_TITLE, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    const int32_t btn_w = 560;
    const int32_t btn_h = 150;
    const int32_t gap = 14;
    const int32_t start_y = 120;
    for (size_t i = 0; i < (sizeof(s_panel_lang_opts) / sizeof(s_panel_lang_opts[0])); i++) {
        const panel_language_option_t *opt = &s_panel_lang_opts[i];
        bool selected = (strcmp(s_selected_user_lang, opt->code) == 0);

        lv_obj_t *btn = lv_button_create(scr);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, start_y + (int32_t)i * (btn_h + gap));
        btn_style(btn, selected ? COL_PROG_ACT : COL_PROG);
        lv_obj_set_style_border_width(btn, selected ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, COL_WHITE, LV_PART_MAIN);
        lv_obj_set_style_border_opa(btn, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, opt->label);
        lv_obj_set_style_text_color(lbl, COL_WHITE, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, FONT_LABEL, LV_PART_MAIN);
        lv_obj_center(lbl);

        lv_obj_set_user_data(btn, (void *)opt);
        lv_obj_add_event_cb(btn, on_lang_btn, LV_EVENT_CLICKED, NULL);
    }

    ESP_LOGI(TAG, "[C] Landing select_language visualizzata");
}



/** Callback timer LVGL: aggiorna tutti gli elementi dinamici */
static void panel_timer_cb(lv_timer_t *t)
{
    (void)t;

    update_time();

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap)) {
        update_state(&snap);
        refresh_prog_buttons(&snap);
    }
}

/* =========================================================================
 * API pubblica
 * ========================================================================= */

void lvgl_panel_show(void)
{
    if (!bsp_display_lock(0)) {
        ESP_LOGW(TAG, "[C] LVGL lock fallito in lvgl_panel_show");
        return;
    }

    panel_show_language_select_screen();

    bsp_display_unlock();

    ESP_LOGI(TAG, "[C] Pannello LVGL avviato (landing select_language)");
}

void lvgl_panel_refresh_texts(void)
{
    if (!bsp_display_lock(100)) return;

    for (int i = 0; i < PROG_COUNT; ++i) {
        if (!s_prog_lbls[i]) {
            continue;
        }
        char tmp[8] = {0};
        snprintf(tmp, sizeof(tmp), "%d", i + 1);
        lv_label_set_text(s_prog_lbls[i], tmp);
    }

    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap)) {
        update_state(&snap);
        refresh_prog_buttons(&snap);
    }

    bsp_display_unlock();
}
