#include "rs232_epaper.h"
#include "rs232.h"
#include "device_config.h"
#include "esp_log.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "RS232_EPD";
static const size_t MAX_AUTO_TEXT_LEN = 127;

static esp_err_t rs232_epaper_send_packet(const uint8_t *data, size_t len)
{
    if (!data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int written = rs232_send(data, len);
    if (written < 0) {
        ESP_LOGE(TAG, "rs232_send failed (%d)", written);
        return ESP_FAIL;
    }
    if ((size_t)written != len) {
        ESP_LOGW(TAG, "rs232_send incomplete (%d/%u)", written, (unsigned)len);
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

bool rs232_epaper_is_enabled(void)
{
    const device_config_t *cfg = device_config_get();
    return cfg && cfg->display.enabled && cfg->display.type == DEVICE_DISPLAY_TYPE_EPAPER_USB && cfg->sensors.rs232_enabled;
}

esp_err_t rs232_epaper_send_command(uint8_t cmd)
{
    uint8_t packet[2] = {0x00, cmd};
    return rs232_epaper_send_packet(packet, sizeof(packet));
}

esp_err_t rs232_epaper_init(void)
{
    if (!rs232_epaper_is_enabled()) {
        ESP_LOGW(TAG, "E-Paper RS232 non abilitato o RS232 disabilitato");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Inizializzazione E-Paper RS232: invio INIT");
    esp_err_t err = rs232_epaper_send_command(0xAA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init packet fallito");
        return err;
    }

    return rs232_epaper_clear_display();
}

esp_err_t rs232_epaper_clear_display(void)
{
    return rs232_epaper_send_command(0x00);
}

esp_err_t rs232_epaper_show_logo(void)
{
    return rs232_epaper_send_command(0xFF);
}

esp_err_t rs232_epaper_set_theme(bool inverted)
{
    return rs232_epaper_send_command(inverted ? 0x05 : 0x04);
}

static esp_err_t rs232_epaper_send_text_packet(const char *text, size_t len)
{
    if (!text) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        uint8_t packet[2] = {0x01, 0x00};
        return rs232_epaper_send_packet(packet, sizeof(packet));
    }

    if (len > MAX_AUTO_TEXT_LEN) {
        len = MAX_AUTO_TEXT_LEN;
    }

    uint8_t packet[2 + MAX_AUTO_TEXT_LEN];
    packet[0] = 0x01;
    packet[1] = (uint8_t)len;
    memcpy(packet + 2, text, len);
    return rs232_epaper_send_packet(packet, 2 + len);
}

static esp_err_t rs232_epaper_send_text_formatted_packet(uint8_t font_id,
                                                         uint8_t pos_x,
                                                         uint8_t pos_y,
                                                         const char *text)
{
    if (!text) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strnlen(text, MAX_AUTO_TEXT_LEN + 1);
    if (len > MAX_AUTO_TEXT_LEN) {
        ESP_LOGW(TAG, "Testo formattato troppo lungo, troncato a %u caratteri", (unsigned)MAX_AUTO_TEXT_LEN);
        len = MAX_AUTO_TEXT_LEN;
    }

    size_t packet_len = 5 + len + 1;
    if (packet_len > sizeof(uint8_t) * (2 + MAX_AUTO_TEXT_LEN + 4)) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t packet[2 + MAX_AUTO_TEXT_LEN + 4];
    packet[0] = 0x01;
    packet[1] = 0xFF;
    packet[2] = font_id;
    packet[3] = pos_x;
    packet[4] = pos_y;
    memcpy(packet + 5, text, len);
    packet[5 + len] = 0x00;

    return rs232_epaper_send_packet(packet, 5 + len + 1);
}

esp_err_t rs232_epaper_display_text(const char *text)
{
    if (!rs232_epaper_is_enabled()) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!text || text[0] == '\0') {
        return rs232_epaper_send_text_packet("", 0);
    }

    size_t len = strnlen(text, MAX_AUTO_TEXT_LEN + 1);
    if (len > MAX_AUTO_TEXT_LEN) {
        ESP_LOGW(TAG, "Testo troppo lungo, troncato a %u caratteri", (unsigned)MAX_AUTO_TEXT_LEN);
        len = MAX_AUTO_TEXT_LEN;
    }
    return rs232_epaper_send_text_packet(text, len);
}

esp_err_t rs232_epaper_display_text_formatted(uint8_t font_id,
                                             uint8_t pos_x,
                                             uint8_t pos_y,
                                             const char *text)
{
    if (!rs232_epaper_is_enabled()) {
        return ESP_ERR_INVALID_STATE;
    }
    return rs232_epaper_send_text_formatted_packet(font_id, pos_x, pos_y, text);
}

esp_err_t rs232_epaper_display_welcome(void)
{
    const char *msg = "§Benvenuto\rSeleziona programma";
    return rs232_epaper_display_text(msg);
}

esp_err_t rs232_epaper_display_credit(int32_t credit_cents)
{
    if (!rs232_epaper_is_enabled()) {
        return ESP_ERR_INVALID_STATE;
    }

    int32_t euros = credit_cents / 100;
    int32_t cents = credit_cents % 100;
    if (cents < 0) {
        cents = -cents;
    }

    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "§Credito: %" PRId32 ",%02" PRId32 " EUR", euros, cents);
    if (len < 0) {
        return ESP_FAIL;
    }
    return rs232_epaper_send_text_packet(buffer, (size_t)len);
}

esp_err_t rs232_epaper_display_credit_big(int32_t credit_cents)
{
    if (!rs232_epaper_is_enabled()) {
        return ESP_ERR_INVALID_STATE;
    }

    int32_t euros = credit_cents / 100;
    int32_t cents = credit_cents % 100;
    if (cents < 0) {
        cents = -cents;
    }

    char credit_text[32];
    int credit_len = snprintf(credit_text, sizeof(credit_text), "%" PRId32 ",%02" PRId32, euros, cents);
    if (credit_len < 0) {
        return ESP_FAIL;
    }

    esp_err_t err = rs232_epaper_clear_display();
    if (err != ESP_OK) {
        return err;
    }

    err = rs232_epaper_display_text_formatted(14, 10, 20, credit_text);
    if (err != ESP_OK) {
        return err;
    }

    return rs232_epaper_display_text_formatted(13, 10, 130, "Credito");
}
