#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "device_config.h"

#ifdef __cplusplus
extern "C" {
#endif

bool rs232_epaper_is_enabled(void);

esp_err_t rs232_epaper_init(void);
esp_err_t rs232_epaper_clear_display(void);
esp_err_t rs232_epaper_show_logo(void);
esp_err_t rs232_epaper_set_theme(bool inverted);
esp_err_t rs232_epaper_display_welcome(void);
esp_err_t rs232_epaper_display_credit(int32_t credit_cents);
esp_err_t rs232_epaper_display_credit_big(int32_t credit_cents);
esp_err_t rs232_epaper_display_text(const char *text);
esp_err_t rs232_epaper_display_text_formatted(uint8_t font_id,
                                             uint8_t pos_x,
                                             uint8_t pos_y,
                                             const char *text);

#ifdef __cplusplus
}
#endif
