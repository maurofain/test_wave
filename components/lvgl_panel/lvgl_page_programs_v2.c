#include "lvgl_panel.h"
#include "lvgl_panel_pages.h"
#include "esp_log.h"
#include <stdio.h>

/* LVGL includes */
#include "lvgl.h"

static const char *TAG = "lvgl_page_programs_v2";

/* UI Elements */
static lv_obj_t *s_program_grid = NULL;
static lv_obj_t *s_header_label = NULL;
static lv_obj_t *s_footer_label = NULL;

/* Program data structure */
typedef struct {
    const char *name;
    const char *price;
    const char *image_path;
    uint32_t program_id;
} program_info_t;

/* Program definitions */
static const program_info_t s_programs[] = {
    {"LAVAGGIO RAPIDO", "€ 4.00", NULL, 1},
    {"COTONE 90°C", "€ 5.50", NULL, 2},
    {"SINTETICI 40°C", "€ 3.50", NULL, 3},
    {"DELICATI 30°C", "€ 4.00", NULL, 4},
    {"BIANCHI 60°C", "€ 5.00", NULL, 5},
    {"COLORATI 40°C", "€ 4.50", NULL, 6},
    {"SCARICO", "€ 2.00", NULL, 7},
    {"RISCIACQUO", "€ 1.50", NULL, 8}
};
#define PROGRAM_COUNT (sizeof(s_programs) / sizeof(s_programs[0]))

/* Program button callback */
static void on_program_click(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    uint32_t program_id = (uint32_t)lv_obj_get_user_data(btn);
    
    ESP_LOGI(TAG, "[C] Program selected: %s (ID: %lu)", 
             s_programs[program_id - 1].name, program_id);
    
    /* TODO: Start the selected program */
    /* For now, just log the selection */
}

/* Create program button with image, name, and price */
static lv_obj_t* create_program_button(lv_obj_t *parent, const program_info_t *program)
{
    /* Create button container */
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, 320, 180);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xf0f0f0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xd0d0d0), LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 15, LV_PART_MAIN);
    lv_obj_add_event_cb(btn, on_program_click, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(btn, (void*)program->program_id);
    
    /* Create image placeholder (colored rectangle) */
    lv_obj_t *img_placeholder = lv_obj_create(btn);
    lv_obj_set_size(img_placeholder, 120, 120);
    lv_obj_align(img_placeholder, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(img_placeholder, lv_color_hex(0xe0e0e0), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(img_placeholder, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(img_placeholder, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(img_placeholder, lv_color_hex(0xc0c0c0), LV_PART_MAIN);
    lv_obj_set_style_radius(img_placeholder, 8, LV_PART_MAIN);
    
    /* Program name label */
    lv_obj_t *name_label = lv_label_create(btn);
    lv_label_set_text(name_label, program->name);
    lv_obj_set_style_text_font(name_label, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_label, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_align(name_label, LV_ALIGN_TOP_RIGHT, -10, 20);
    
    /* Price label */
    lv_obj_t *price_label = lv_label_create(btn);
    lv_label_set_text(price_label, program->price);
    lv_obj_set_style_text_font(price_label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(price_label, lv_color_hex(0x2ecc71), LV_PART_MAIN);
    lv_obj_align(price_label, LV_ALIGN_BOTTOM_RIGHT, -10, -20);
    
    return btn;
}

/* Create header */
static void create_header(lv_obj_t *parent)
{
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, 720, 80);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(header, 0, LV_PART_MAIN);
    
    s_header_label = lv_label_create(header);
    lv_label_set_text(s_header_label, "SELEZIONA PROGRAMMA");
    lv_obj_set_style_text_color(s_header_label, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_header_label, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_center(s_header_label);
}

/* Create footer */
static void create_footer(lv_obj_t *parent)
{
    lv_obj_t *footer = lv_obj_create(parent);
    lv_obj_set_size(footer, 720, 60);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(footer, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(footer, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(footer, 0, LV_PART_MAIN);
    
    s_footer_label = lv_label_create(footer);
    lv_label_set_text(s_footer_label, "Sistema pronto - Touch per selezionare");
    lv_obj_set_style_text_color(s_footer_label, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_footer_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(s_footer_label);
}

/* Create program grid */
static void create_program_grid(lv_obj_t *parent)
{
    /* Create grid container */
    s_program_grid = lv_obj_create(parent);
    lv_obj_set_size(s_program_grid, 680, 1040);
    lv_obj_align(s_program_grid, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_program_grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_program_grid, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_program_grid, 20, LV_PART_MAIN);
    
    /* Create flex layout for grid (2 columns) */
    lv_obj_set_flex_flow(s_program_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_program_grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    /* Add program buttons */
    for (uint32_t i = 0; i < PROGRAM_COUNT; i++) {
        create_program_button(s_program_grid, &s_programs[i]);
    }
}

/* Update footer text */
void lvgl_page_programs_v2_set_footer(const char *text)
{
    if (s_footer_label && text) {
        lv_label_set_text(s_footer_label, text);
    }
}

/* Show programs page v2 */
void lvgl_page_programs_v2_show(void)
{
    lv_obj_t *scr = lv_scr_act();
    
    ESP_LOGI(TAG, "[C] Showing programs page v2");
    
    /* Clear screen */
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xf5f5f5), LV_PART_MAIN);
    
    /* Create UI components */
    create_header(scr);
    create_program_grid(scr);
    create_footer(scr);
    
    ESP_LOGI(TAG, "[C] Programs page v2 displayed with %lu programs", PROGRAM_COUNT);
}

/* Hide/cleanup programs page v2 */
void lvgl_page_programs_v2_hide(void)
{
    ESP_LOGI(TAG, "[C] Hiding programs page v2");
    
    /* UI elements will be cleaned when screen is cleared */
    s_program_grid = NULL;
    s_header_label = NULL;
    s_footer_label = NULL;
}
