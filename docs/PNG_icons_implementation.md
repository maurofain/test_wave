# PNG Icon Implementation for LVGL Chrome Bar

## Scopo

Questa documentazione descrive l'implementazione delle icone PNG nel pannello LVGL, utilizzate per sostituire i puntini di stato sopra la bandiera della lingua.

Il codice è stato definito in `components/lvgl_panel/lvgl_page_chrome.c`.

## Comportamento

- Vengono creati due array di percorsi per otto icone PNG nello SPIFFS:
  - `S:/spiffs/icons/CloudOk.png` / `S:/spiffs/icons/CloudKo.png`
  - `S:/spiffs/icons/CreditCardOk.png` / `S:/spiffs/icons/CreditCardKo.png`
  - `S:/spiffs/icons/MoneteOk3.png` / `S:/spiffs/icons/MoneteKo.png`
  - `S:/spiffs/icons/QrOk.png` / `S:/spiffs/icons/QrKo.png`
- Le icone OK/KO vengono preloadate in PSRAM all'avvio tramite `lvgl_page_chrome_preload_status_icons()`.
- Queste icone vengono visualizzate nella parte superiore destra della pagina LVGL.
- Viene mantenuto un timer per aggiornare solo l'orologio, evitando di ricaricare le immagini dal filesystem ogni secondo.

## File

- `components/lvgl_panel/lvgl_page_chrome.c`

## Codice

```c
#include "lvgl_page_chrome.h"
#include "language_flags.h"
#include "lvgl_panel.h"

#include <time.h>

extern const lv_font_t GoogleSans35;

static lv_obj_t      *s_chrome_time_lbl      = NULL;
static lv_timer_t    *s_chrome_time_timer    = NULL;
static lv_obj_t      *s_chrome_status_icons[4] = {NULL, NULL, NULL, NULL};
static lv_event_cb_t  s_flag_cb              = NULL;
static void          *s_flag_ud              = NULL;

static const char *s_chrome_status_icon_paths[4] = {
    "S:/spiffs/icons/CloudOk.png",
    "S:/spiffs/icons/CreditCardOk.png",
    "S:/spiffs/icons/MoneteOk.png",
    "S:/spiffs/icons/QrOk.png",
};

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
        if (tm_now.tm_year + 1900 >= 2020) {
            strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &tm_now);
        }
    }
    lv_label_set_text(s_chrome_time_lbl, time_buf);
}

static void chrome_update_status_icons(void)
{
    for (int i = 0; i < 4; i++) {
        if (!s_chrome_status_icons[i] || !lv_obj_is_valid(s_chrome_status_icons[i])) {
            continue;
        }
        lv_image_set_src(s_chrome_status_icons[i], s_chrome_status_icon_paths[i]);
    }
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

    lv_obj_t *flag = lv_image_create(scr);
    lv_image_set_src(flag, get_flag_src_for_language(lang));
    lv_image_set_scale(flag, 256);
    lv_obj_align(flag, LV_ALIGN_TOP_RIGHT, -right_margin, top_margin + icon_size + gap);

    const int32_t icons_top = top_margin;
    for (int i = 0; i < 4; i++) {
        s_chrome_status_icons[i] = lv_image_create(scr);
        lv_obj_set_size(s_chrome_status_icons[i], icon_size, icon_size);
        lv_image_set_src(s_chrome_status_icons[i], s_chrome_status_icon_paths[i]);
        lv_image_set_scale(s_chrome_status_icons[i], 256);
        lv_obj_set_style_pad_all(s_chrome_status_icons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_chrome_status_icons[i], 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_chrome_status_icons[i], LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_align(s_chrome_status_icons[i], LV_ALIGN_TOP_RIGHT,
                     -(right_margin + (3 - i) * (icon_size + gap)),
                     icons_top);
    }

    if (s_flag_cb) {
        lv_obj_add_flag(flag, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(flag, LV_OPA_TRANSP, LV_STATE_PRESSED | LV_PART_MAIN);
        lv_obj_add_event_cb(flag, s_flag_cb, LV_EVENT_CLICKED, s_flag_ud);
    }
}
```

## Note

- Le icone vengono caricate da SPIFFS ogni volta che la schermata viene costruita.
- Il timer `chrome_time_timer_cb` ora aggiorna solo l’orologio, evitando accessi ripetuti alle immagini.
- Questo file può essere usato come riferimento per ripristinare l’implementazione dopo il rollback.

## Prompt utente dall'ultimo commit

- `aggiungi nel .md anche la lista dei miei prompt dall'ultimo commit`

## Commit di riferimento

- `cff8c9c` — v0.6.14 Spostato numero programmi in Programmi e aggiunte icone
