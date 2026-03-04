#include "language_flags.h"

#define FLAG_W 24
#define FLAG_H 16
#define FLAG_STRIDE (FLAG_W * 2)

#define C_WHITE  0xFFFF
#define C_RED    0xF800
#define C_GREEN  0x07E0
#define C_BLUE   0x001F
#define C_YELLOW 0xFFE0
#define C_BLACK  0x0000

#define REP2(c) c, c
#define REP4(c) REP2(c), REP2(c)
#define REP8(c) REP4(c), REP4(c)
#define REP24(c) REP8(c), REP8(c), REP8(c)

#define ROW_IT REP8(C_GREEN), REP8(C_WHITE), REP8(C_RED)
#define ROW_FR REP8(C_BLUE), REP8(C_WHITE), REP8(C_RED)

#define ROW_DE_TOP REP24(C_BLACK)
#define ROW_DE_MID REP24(C_RED)
#define ROW_DE_BOT REP24(C_YELLOW)

#define ROW_ES_TOP REP24(C_RED)
#define ROW_ES_MID REP24(C_YELLOW)
#define ROW_ES_BOT REP24(C_RED)

#define UK_B C_BLUE
#define UK_W C_WHITE
#define UK_R C_RED

#define ROW_UK_00 UK_R, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_R
#define ROW_UK_01 UK_W, UK_R, UK_R, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_R, UK_R, UK_W
#define ROW_UK_02 UK_B, UK_B, UK_W, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_W, UK_B, UK_B
#define ROW_UK_03 UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_W, UK_B, UK_B, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_B, UK_B, UK_W, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B
#define ROW_UK_04 UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_R, UK_W, UK_W, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_W, UK_W, UK_R, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B
#define ROW_UK_05 UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B
#define ROW_UK_06 UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W
#define ROW_UK_07 UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R
#define ROW_UK_08 UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R, UK_R
#define ROW_UK_09 UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W, UK_W
#define ROW_UK_10 UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B
#define ROW_UK_11 UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_R, UK_W, UK_W, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_W, UK_W, UK_R, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B
#define ROW_UK_12 UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_W, UK_B, UK_B, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_B, UK_B, UK_W, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B
#define ROW_UK_13 UK_B, UK_B, UK_W, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_W, UK_B, UK_B
#define ROW_UK_14 UK_W, UK_R, UK_R, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_R, UK_R, UK_W
#define ROW_UK_15 UK_R, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_W, UK_R, UK_R, UK_W, UK_W, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_B, UK_W, UK_R

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint16_t s_flag_it_map[FLAG_W * FLAG_H] = {
    ROW_IT, ROW_IT, ROW_IT, ROW_IT,
    ROW_IT, ROW_IT, ROW_IT, ROW_IT,
    ROW_IT, ROW_IT, ROW_IT, ROW_IT,
    ROW_IT, ROW_IT, ROW_IT, ROW_IT,
};

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint16_t s_flag_fr_map[FLAG_W * FLAG_H] = {
    ROW_FR, ROW_FR, ROW_FR, ROW_FR,
    ROW_FR, ROW_FR, ROW_FR, ROW_FR,
    ROW_FR, ROW_FR, ROW_FR, ROW_FR,
    ROW_FR, ROW_FR, ROW_FR, ROW_FR,
};

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint16_t s_flag_de_map[FLAG_W * FLAG_H] = {
    ROW_DE_TOP, ROW_DE_TOP, ROW_DE_TOP, ROW_DE_TOP, ROW_DE_TOP,
    ROW_DE_MID, ROW_DE_MID, ROW_DE_MID, ROW_DE_MID, ROW_DE_MID, ROW_DE_MID,
    ROW_DE_BOT, ROW_DE_BOT, ROW_DE_BOT, ROW_DE_BOT, ROW_DE_BOT,
};

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint16_t s_flag_es_map[FLAG_W * FLAG_H] = {
    ROW_ES_TOP, ROW_ES_TOP, ROW_ES_TOP, ROW_ES_TOP,
    ROW_ES_MID, ROW_ES_MID, ROW_ES_MID, ROW_ES_MID,
    ROW_ES_MID, ROW_ES_MID, ROW_ES_MID, ROW_ES_MID,
    ROW_ES_BOT, ROW_ES_BOT, ROW_ES_BOT, ROW_ES_BOT,
};

static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint16_t s_flag_en_map[FLAG_W * FLAG_H] = {
    ROW_UK_00,
    ROW_UK_01,
    ROW_UK_02,
    ROW_UK_03,
    ROW_UK_04,
    ROW_UK_05,
    ROW_UK_06,
    ROW_UK_07,
    ROW_UK_08,
    ROW_UK_09,
    ROW_UK_10,
    ROW_UK_11,
    ROW_UK_12,
    ROW_UK_13,
    ROW_UK_14,
    ROW_UK_15,
};

#define DECLARE_FLAG_DSC(dsc_name, map_name)        \
    const lv_image_dsc_t dsc_name = {               \
        .header.magic = LV_IMAGE_HEADER_MAGIC,      \
        .header.cf = LV_COLOR_FORMAT_RGB565,        \
        .header.flags = 0,                          \
        .header.w = FLAG_W,                         \
        .header.h = FLAG_H,                         \
        .header.stride = FLAG_STRIDE,               \
        .data_size = sizeof(map_name),              \
        .data = (const uint8_t *)map_name,          \
        .reserved = NULL,                           \
    }

DECLARE_FLAG_DSC(g_flag_it_24x16, s_flag_it_map);
DECLARE_FLAG_DSC(g_flag_en_24x16, s_flag_en_map);
DECLARE_FLAG_DSC(g_flag_de_24x16, s_flag_de_map);
DECLARE_FLAG_DSC(g_flag_fr_24x16, s_flag_fr_map);
DECLARE_FLAG_DSC(g_flag_es_24x16, s_flag_es_map);
