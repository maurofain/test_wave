#include "embedded_icons.h"
#include "embedded_icons_images.h"

#include "lvgl.h"

#include <stddef.h>

#define DECLARE_EMBEDDED_ICON_DSC(name, map_symbol, width_px, height_px) \
    static const lv_image_dsc_t name = {                               \
        .header.magic = LV_IMAGE_HEADER_MAGIC,                         \
        .header.cf = LV_COLOR_FORMAT_RAW_ALPHA,                        \
        .header.flags = 0,                                             \
        .header.w = (width_px),                                        \
        .header.h = (height_px),                                       \
        .header.stride = 0,                                            \
        .data_size = sizeof(map_symbol),                               \
        .data = (const uint8_t *)(map_symbol),                         \
        .reserved = NULL,                                              \
    }

DECLARE_EMBEDDED_ICON_DSC(s_icon_cloud_ko, docs_icone_normalized_CloudKo_png, 48, 48);
DECLARE_EMBEDDED_ICON_DSC(s_icon_cloud_ok, docs_icone_normalized_CloudOk_png, 48, 48);

DECLARE_EMBEDDED_ICON_DSC(s_icon_credit_ko, docs_icone_normalized_CreditCardKo_png, 48, 48);
DECLARE_EMBEDDED_ICON_DSC(s_icon_credit_ok, docs_icone_normalized_CreditCardOk_png, 48, 48);

DECLARE_EMBEDDED_ICON_DSC(s_icon_coin_ko, docs_icone_normalized_MoneteKo_png, 48, 48);
DECLARE_EMBEDDED_ICON_DSC(s_icon_coin_ok, docs_icone_normalized_MoneteOk3_png, 48, 48);

DECLARE_EMBEDDED_ICON_DSC(s_icon_qr_ko, docs_icone_normalized_QrKo_png, 48, 48);
DECLARE_EMBEDDED_ICON_DSC(s_icon_qr_ok, docs_icone_normalized_QrOk_png, 48, 48);

DECLARE_EMBEDDED_ICON_DSC(s_icon_skio_ko, docs_icone_normalized_SkioKo_png, 48, 48);
DECLARE_EMBEDDED_ICON_DSC(s_icon_skio_ok, docs_icone_normalized_SkioOk_png, 48, 48);

const void *get_embedded_icon_src(int index, bool ok)
{
    switch (index) {
        case 0:
            return ok ? (const void *)&s_icon_cloud_ok : (const void *)&s_icon_cloud_ko;
        case 1:
            return ok ? (const void *)&s_icon_credit_ok : (const void *)&s_icon_credit_ko;
        case 2:
            return ok ? (const void *)&s_icon_coin_ok : (const void *)&s_icon_coin_ko;
        case 3:
            return ok ? (const void *)&s_icon_qr_ok : (const void *)&s_icon_qr_ko;
        case 4:
            return ok ? (const void *)&s_icon_skio_ok : (const void *)&s_icon_skio_ko;
        default:
            return NULL;
    }
}
