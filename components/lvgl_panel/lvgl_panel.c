/**
 * @file lvgl_panel.c
 * @brief Pannello LVGL emulatore — display 7" 800×1280 verticale.
 *
 * Layout (colonna unica, 800 px wide, 1280 px tall):
 *
 *   y=  0  h=  70  Header: orologio
 *   y= 70  h= 890  Status box GRANDE: countdown/credito + gauge verticale dx
 *   y=960  h= 320  Pulsanti programma (2 col × 4 righe) — in fondo
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
#define PROG_COUNT       8
#define PROG_COLS        2
#define PROG_ROWS        4

#define SECT_HDR_Y       0
#define SECT_HDR_H       70
#define SECT_STATUS_Y    70
#define SECT_STATUS_H    890
#define SECT_PROGS_Y     960
#define SECT_PROGS_H     320

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

/* Stato runtime locale */
static uint8_t s_active_prog = 0;

/* =========================================================================
 * User data allocati staticamente per i pulsanti
 * ========================================================================= */
typedef struct { uint8_t prog_id; } prog_btn_ud_t;
static prog_btn_ud_t s_prog_ud[PROG_COUNT];

static void btn_style(lv_obj_t *btn, lv_color_t bg);

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
            snprintf(buf, sizeof(buf), "Secondi   %s", mm);
        } else {
            snprintf(buf, sizeof(buf), "Credito");
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
            snprintf(buf, sizeof(buf), "Pausa: %s", mm);
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
            int32_t h = (SECT_STATUS_H * pct) / 100;
            lv_obj_set_size(s_counter_fill, 800, h);
            lv_obj_align(s_counter_fill, LV_ALIGN_BOTTOM_LEFT, 0, 0);
            s_last_pct = pct;
        }
    }
#else
    /* progress bar — aggiorna solo se la percentuale è cambiata */
    if (s_gauge) {
        static int32_t s_last_pct = -1;
        if (pct != s_last_pct) {
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
    lv_obj_t *hdr = lv_obj_create(scr);
    lv_obj_set_pos(hdr, 0, SECT_HDR_Y);
    lv_obj_set_size(hdr, 800, SECT_HDR_H);
    lv_obj_set_style_bg_color(hdr, COL_HDR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(hdr, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(hdr, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(hdr, 0, LV_PART_MAIN);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);


    s_time_lbl = lv_label_create(hdr);
    lv_label_set_text(s_time_lbl, "--:--:--");
    lv_obj_set_style_text_color(s_time_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_time_lbl, FONT_TIME, LV_PART_MAIN);
    lv_obj_align(s_time_lbl, LV_ALIGN_RIGHT_MID, -18, 0);
}

/* =========================================================================
 * Costruzione UI — Status box
 * ========================================================================= */

static void build_status(lv_obj_t *scr)
{
    /* ---- contenitore principale ---------------------------------------- */
    s_status_box = lv_obj_create(scr);
    lv_obj_set_pos(s_status_box, 0, SECT_STATUS_Y);
    lv_obj_set_size(s_status_box, 800, SECT_STATUS_H);
    lv_obj_set_style_bg_color(s_status_box, COL_STATE_IDL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_status_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_status_box, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_status_box, LV_OBJ_FLAG_SCROLLABLE);

    /* colonne: info + gauge (mode barra) */
#if DNA_LVGL_COUNTDOWN_BG == 1
#define GAUGE_W   0
#else
#define GAUGE_W   180
#endif
#define INFO_W    (800 - GAUGE_W)
#define GAUGE_PAD 12

#if DNA_LVGL_COUNTDOWN_BG == 1
    /* Sfondo countdown pieno (il livello scende dall'alto verso il basso) */
    s_counter_fill = lv_obj_create(s_status_box);
    lv_obj_set_size(s_counter_fill, 800, 0);
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

#if DNA_LVGL_COUNTDOWN_BG == 0
    /* ---- barra di progressione — posizionata con coordinate assolute ---- */
    /* Evitare sub-container intermedi: in LVGL v9 possono creare problemi
     * di clipping/offset. La barra va direttamente su s_status_box. */
    s_gauge = lv_bar_create(s_status_box);
    lv_obj_set_pos(s_gauge, INFO_W + GAUGE_PAD, GAUGE_PAD);
    lv_obj_set_size(s_gauge, GAUGE_W - GAUGE_PAD * 2, SECT_STATUS_H - GAUGE_PAD * 2);
    lv_bar_set_range(s_gauge, 0, 100);
    lv_bar_set_value(s_gauge, 30, LV_ANIM_OFF);   /* valore visibile iniziale */
    lv_bar_set_orientation(s_gauge, LV_BAR_ORIENTATION_VERTICAL);
    lv_obj_set_style_bg_color(s_gauge, lv_color_make(0x40, 0x40, 0x70), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_gauge, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_gauge, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_gauge, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gauge, COL_PROG_ACT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(s_gauge, COL_PROG_LOW, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(s_gauge, LV_GRAD_DIR_VER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(s_gauge, 0, LV_PART_INDICATOR);
    lv_obj_set_style_border_width(s_gauge, 0, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_gauge, 8, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(s_gauge, 0, LV_PART_INDICATOR);
    lv_obj_set_style_anim_duration(s_gauge, 0, LV_PART_MAIN);
#else
    s_gauge = NULL;
#endif

    /* ---- testi centrati nell'area sinistra (0..INFO_W) ---- */
    /* lv_obj_align su s_status_box centra in 800px; offset -GAUGE_W/2 sposta
     * il centro di visualizzazione nella zona sinistra 700px */
    s_credit_lbl = lv_label_create(s_status_box);
    lv_label_set_text(s_credit_lbl, "0");
    lv_obj_set_style_text_color(s_credit_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_credit_lbl, FONT_BIGNUM, LV_PART_MAIN);
    lv_obj_align(s_credit_lbl, LV_ALIGN_CENTER, -(GAUGE_W / 2), -60);

    /* etichetta contestuale */
    s_elapsed_lbl = lv_label_create(s_status_box);
    lv_label_set_text(s_elapsed_lbl, "Credito");
    lv_obj_set_style_text_color(s_elapsed_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_elapsed_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_elapsed_lbl, LV_ALIGN_CENTER, -(GAUGE_W / 2), 80);

    /* timer pausa */
    s_pause_lbl = lv_label_create(s_status_box);
    lv_label_set_text(s_pause_lbl, "");
    lv_obj_set_style_text_color(s_pause_lbl, COL_PROG_PAUSE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_pause_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_pause_lbl, LV_ALIGN_BOTTOM_LEFT, 20, -20);
}


/* =========================================================================
 * Costruzione UI — Pulsanti programma
 * ========================================================================= */

static void build_prog_buttons(lv_obj_t *scr)
{
    /* Container opaco come sfondo della sezione */
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_pos(cont, 0, SECT_PROGS_Y);
    lv_obj_set_size(cont, 800, SECT_PROGS_H);
    lv_obj_set_style_bg_color(cont, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(cont, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cont, 0, LV_PART_MAIN);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    const int32_t PAD = 12;
    const int32_t GAP = 10;
    int32_t btn_w = (800 - PAD * 2 - GAP) / 2;           /* ~383 */
    int32_t btn_h = (SECT_PROGS_H - PAD * 2 - GAP * 3) / 4; /* ~79 */

    for (int i = 0; i < PROG_COUNT; i++) {
        int col = i % PROG_COLS;
        int row = i / PROG_COLS;
        int32_t x = PAD + col * (btn_w + GAP);
        int32_t y = PAD + row * (btn_h + GAP);

        lv_obj_t *btn = lv_button_create(cont);
        lv_obj_set_pos(btn, x, y);
        lv_obj_set_size(btn, btn_w, btn_h);
        btn_style(btn, COL_PROG);

        lv_obj_t *lbl = lv_label_create(btn);
        char txt[32];
        snprintf(txt, sizeof(txt), "Programma %d", i + 1);
        lv_label_set_text(lbl, txt);
        lv_obj_set_style_text_color(lbl, COL_WHITE, LV_PART_MAIN);
        lv_obj_set_style_text_font(lbl, FONT_PROG_BTN, LV_PART_MAIN);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, btn_w - 8);
        lv_obj_center(lbl);

        s_prog_ud[i].prog_id = (uint8_t)(i + 1);
        lv_obj_set_user_data(btn, &s_prog_ud[i]);
        lv_obj_add_event_cb(btn, on_prog_btn, LV_EVENT_CLICKED, NULL);

        s_prog_btns[i] = btn;
        s_prog_lbls[i] = lbl;
    }
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

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COL_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    s_active_prog = 0;

    build_header(scr);
    build_status(scr);
    build_prog_buttons(scr);

    /* Prima esecuzione immediata per popolare i dati */
    update_time();
    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap)) {
        update_state(&snap);
        refresh_prog_buttons(&snap);
    }

    /* Timer periodico 700 ms */
    lv_timer_create(panel_timer_cb, 700, NULL);

    bsp_display_unlock();

    ESP_LOGI(TAG, "[C] Pannello emulatore LVGL visualizzato (800x1280)");
}

void lvgl_panel_refresh_texts(void)
{
    if (!bsp_display_lock(100)) return;

    /* Aggiorna etichette dinamiche con i testi localizzati */
    char buf[128];

    if (s_elapsed_lbl) {
        if (device_config_get_ui_text_scoped("lvgl", "elapsed_label", "Credito", buf, sizeof(buf)) == ESP_OK) {
            lv_label_set_text(s_elapsed_lbl, buf);
        } else {
            lv_label_set_text(s_elapsed_lbl, "Credito");
        }
    }

    if (s_pause_lbl) {
        if (device_config_get_ui_text_scoped("lvgl", "pause_label", "", buf, sizeof(buf)) == ESP_OK) {
            lv_label_set_text(s_pause_lbl, buf);
        } else {
            lv_label_set_text(s_pause_lbl, "");
        }
    }

    /* Aggiorna etichette pulsanti programmi (usa formato con %d) */
    for (int i = 0; i < PROG_COUNT; ++i) {
        if (!s_prog_lbls[i]) continue;
        if (device_config_get_ui_text_scoped("lvgl", "program", "Programma %d", buf, sizeof(buf)) == ESP_OK) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), buf, i + 1);
            lv_label_set_text(s_prog_lbls[i], tmp);
        } else {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "Programma %d", i + 1);
            lv_label_set_text(s_prog_lbls[i], tmp);
        }
    }

    bsp_display_unlock();
}
