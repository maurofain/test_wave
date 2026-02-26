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
static lv_obj_t *s_prog_btns[PROG_COUNT];
static lv_obj_t *s_prog_lbls[PROG_COUNT];

/* Stato runtime locale */
static uint8_t s_active_prog = 0;

/* =========================================================================
 * User data allocati staticamente per i pulsanti
 * ========================================================================= */
typedef struct { uint8_t prog_id; } prog_btn_ud_t;
static prog_btn_ud_t s_prog_ud[PROG_COUNT];

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

/* Aggiorna l'orologio header */
static void update_time(void)
{
    if (!s_time_lbl) return;
    time_t now = time(NULL);
    if (now == (time_t)-1) {
        lv_label_set_text(s_time_lbl, "--:--:--");
        return;
    }
    struct tm ti;
    localtime_r(&now, &ti);
    char buf[16];
    strftime(buf, sizeof(buf), "%H:%M:%S", &ti);
    lv_label_set_text(s_time_lbl, buf);
}

/* Aggiorna box status in base a snapshot FSM (crediti, timer, stato) */
static void update_state(const fsm_ctx_t *snap)
{
    if (!snap) return;

    bool running = (snap->state == FSM_STATE_RUNNING);
    bool paused  = (snap->state == FSM_STATE_PAUSED);

    /* sfondo status box */
    if (s_status_box) {
        lv_color_t col = running ? COL_STATE_RUN :
                         paused  ? COL_STATE_PAU :
                                   COL_STATE_IDL;
        lv_obj_set_style_bg_color(s_status_box, col, LV_PART_MAIN);
    }

    /* Numero grande: countdown (secondi rimanenti) se running/paused,
     * altrimenti credito (coin) */
    if (s_credit_lbl) {
        char buf[16];
        if ((running || paused) && snap->running_target_ms > 0) {
            uint32_t rem_ms = (snap->running_target_ms > snap->running_elapsed_ms)
                              ? snap->running_target_ms - snap->running_elapsed_ms
                              : 0;
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)(rem_ms / 1000));
        } else {
            snprintf(buf, sizeof(buf), "%ld", (long)snap->credit_cents);
        }
        lv_label_set_text(s_credit_lbl, buf);
    }

    /* Etichetta sotto: contestuale */
    if (s_elapsed_lbl) {
        if (running || paused) {
            char mm[10];
            fmt_mm_ss(mm, sizeof(mm), snap->running_elapsed_ms);
            char buf[32];
            snprintf(buf, sizeof(buf), "Secondi   %s", mm);
            lv_label_set_text(s_elapsed_lbl, buf);
        } else {
            lv_label_set_text(s_elapsed_lbl, "Credito");
        }
    }

    /* timer pausa */
    if (s_pause_lbl) {
        if (paused && snap->pause_elapsed_ms > 0) {
            char mm[10];
            fmt_mm_ss(mm, sizeof(mm), snap->pause_elapsed_ms);
            char buf[32];
            snprintf(buf, sizeof(buf), "Pausa: %s", mm);
            lv_label_set_text(s_pause_lbl, buf);
        } else {
            lv_label_set_text(s_pause_lbl, "");
        }
    }

    /* progress bar -- percentuale residua programma */
    if (s_gauge) {
        if (snap->running_target_ms > 0 && (running || paused)) {
            uint32_t rem = snap->running_target_ms;
            if (rem > snap->running_elapsed_ms) {
                rem -= snap->running_elapsed_ms;
            } else {
                rem = 0;
            }
            uint32_t pct = (uint32_t)((rem * 100) / snap->running_target_ms);
            lv_bar_set_value(s_gauge, pct, LV_ANIM_OFF);
        } else {
            lv_bar_set_value(s_gauge, 0, LV_ANIM_OFF);
        }
    }
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

    /* cerca metadati programma */
    const web_ui_program_table_t *tbl = web_ui_program_table_get();
    const web_ui_program_entry_t *entry = NULL;
    if (tbl) {
        for (uint8_t i = 0; i < tbl->count; i++) {
            if (tbl->programs[i].program_id == pid) {
                entry = &tbl->programs[i];
                break;
            }
        }
    }

    /* se è il programma già attivo → pause/toggle, altrimenti start */
    bool pause_toggle = (s_active_prog == pid);
    if (!pause_toggle) {
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
    s_status_box = lv_obj_create(scr);
    lv_obj_set_pos(s_status_box, 0, SECT_STATUS_Y);
    lv_obj_set_size(s_status_box, 800, SECT_STATUS_H);
    lv_obj_set_style_bg_color(s_status_box, COL_STATE_IDL, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_status_box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_status_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_status_box, 0, LV_PART_MAIN);
    lv_obj_remove_flag(s_status_box, LV_OBJ_FLAG_SCROLLABLE);


    /* Barra di avanzamento verticale a DESTRA (0..100%) — occupa quasi tutta
     * l'altezza del box, larga 60 px con bordo dal bordo dx */
    s_gauge = lv_bar_create(s_status_box);
    lv_obj_set_size(s_gauge, 60, SECT_STATUS_H - 24);
    lv_obj_align(s_gauge, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_bar_set_range(s_gauge, 0, 100);
    lv_bar_set_value(s_gauge, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_gauge, COL_CARD, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_gauge, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_gauge, COL_PROG_ACT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_gauge, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_gauge, 12, LV_PART_INDICATOR);

    /* Numero grande centrato nell'area sinistra (lasciando 80 px per il gauge) */
    s_credit_lbl = lv_label_create(s_status_box);
    lv_label_set_text(s_credit_lbl, "0");
    lv_obj_set_style_text_color(s_credit_lbl, COL_WHITE, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_credit_lbl, FONT_BIGNUM, LV_PART_MAIN);
    /* centro verticale del box, spostato a sx per lasciare spazio al gauge */
    lv_obj_align(s_credit_lbl, LV_ALIGN_CENTER, -40, -60);

    /* Etichetta contestuale sotto il numero */
    s_elapsed_lbl = lv_label_create(s_status_box);
    lv_label_set_text(s_elapsed_lbl, "Credito");
    lv_obj_set_style_text_color(s_elapsed_lbl, COL_GREY, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_elapsed_lbl, FONT_LABEL, LV_PART_MAIN);
    lv_obj_align(s_elapsed_lbl, LV_ALIGN_CENTER, -40, 80);

    /* Timer pausa — in basso a sx */
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
    if (fsm_runtime_snapshot(&snap)) update_state(&snap);

    /* Timer periodico 700 ms */
    lv_timer_create(panel_timer_cb, 700, NULL);

    bsp_display_unlock();

    ESP_LOGI(TAG, "[C] Pannello emulatore LVGL visualizzato (800x1280)");
}
