#include "audio_player.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "device_config.h"
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "bsp/esp32_p4_nano.h"

#ifndef DNA_AUDIO
#define DNA_AUDIO 0
#endif

#if DNA_AUDIO == 0

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

#define WAV_HEADER_BYTES_MIN 44U
#define AUDIO_COPY_BUFFER_SIZE 2048U
#define MP3_SAMPLE_BUFFER_SAMPLES (MINIMP3_MAX_SAMPLES_PER_FRAME * 4)

static const char *TAG = "AUDIO_PLAYER";
static esp_codec_dev_handle_t s_speaker = NULL;
static bool s_audio_ready = false;
static bool s_audio_fault = false;
static bool s_stop_requested = false;
static bool s_is_playing = false;
static uint8_t s_volume = 75;

static esp_err_t read_u32_le(FILE *f, uint32_t *out)
{
    uint8_t b[4] = {0};
    if (fread(b, 1, sizeof(b), f) != sizeof(b)) {
        return ESP_FAIL;
    }
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return ESP_OK;
}

static esp_err_t read_u16_le(FILE *f, uint16_t *out)
{
    uint8_t b[2] = {0};
    if (fread(b, 1, sizeof(b), f) != sizeof(b)) {
        return ESP_FAIL;
    }
    *out = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    return ESP_OK;
}

static esp_err_t wav_locate_data_chunk(FILE *f,
                                        esp_codec_dev_sample_info_t *sample_info,
                                        uint32_t *data_size)
{
    char riff[4] = {0};
    if (fread(riff, 1, sizeof(riff), f) != sizeof(riff)) {
        return ESP_FAIL;
    }
    if (memcmp(riff, "RIFF", 4) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t riff_size = 0;
    if (read_u32_le(f, &riff_size) != ESP_OK) {
        return ESP_FAIL;
    }
    (void)riff_size;

    char wave[4] = {0};
    if (fread(wave, 1, sizeof(wave), f) != sizeof(wave)) {
        return ESP_FAIL;
    }
    if (memcmp(wave, "WAVE", 4) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    bool found_fmt = false;
    bool found_data = false;

    while (!found_data) {
        char chunk_id[4] = {0};
        uint32_t chunk_size = 0;

        if (fread(chunk_id, 1, sizeof(chunk_id), f) != sizeof(chunk_id)) {
            return ESP_FAIL;
        }
        if (read_u32_le(f, &chunk_size) != ESP_OK) {
            return ESP_FAIL;
        }

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            uint16_t audio_format = 0;
            uint16_t channels = 0;
            uint32_t sample_rate = 0;
            uint32_t byte_rate = 0;
            uint16_t block_align = 0;
            uint16_t bits_per_sample = 0;

            if (read_u16_le(f, &audio_format) != ESP_OK ||
                read_u16_le(f, &channels) != ESP_OK ||
                read_u32_le(f, &sample_rate) != ESP_OK ||
                read_u32_le(f, &byte_rate) != ESP_OK ||
                read_u16_le(f, &block_align) != ESP_OK ||
                read_u16_le(f, &bits_per_sample) != ESP_OK) {
                return ESP_FAIL;
            }
            (void)byte_rate;
            (void)block_align;

            if (chunk_size > 16U) {
                if (fseek(f, (long)(chunk_size - 16U), SEEK_CUR) != 0) {
                    return ESP_FAIL;
                }
            }

            if (audio_format != 1U) {
                ESP_LOGW(TAG, "[C] WAV non PCM (format=%u)", (unsigned)audio_format);
                return ESP_ERR_NOT_SUPPORTED;
            }

            if (channels == 0U || bits_per_sample == 0U || sample_rate == 0U) {
                return ESP_ERR_INVALID_ARG;
            }

            sample_info->bits_per_sample = (uint8_t)bits_per_sample;
            sample_info->channel = (uint8_t)channels;
            sample_info->channel_mask = 0;
            sample_info->sample_rate = sample_rate;
            sample_info->mclk_multiple = 0;
            found_fmt = true;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            if (!found_fmt) {
                ESP_LOGW(TAG, "[C] WAV senza chunk fmt prima del data");
                return ESP_ERR_INVALID_ARG;
            }
            *data_size = chunk_size;
            found_data = true;
        } else {
            if (fseek(f, (long)chunk_size, SEEK_CUR) != 0) {
                return ESP_FAIL;
            }
        }
    }

    return ESP_OK;
}

static esp_err_t play_mp3_file(const char *path)
{
    mp3dec_ex_t decoder = {0};
    int decoder_err = mp3dec_ex_open(&decoder, path, MP3D_SEEK_TO_SAMPLE);
    if (decoder_err) {
        ESP_LOGW(TAG, "[C] Apertura decoder MP3 fallita (%d): %s", decoder_err, path);
        return ESP_FAIL;
    }

    if (decoder.info.hz <= 0 || decoder.info.channels <= 0 || decoder.info.channels > 2) {
        mp3dec_ex_close(&decoder);
        ESP_LOGW(TAG, "[C] MP3 con formato non valido: hz=%d ch=%d", decoder.info.hz, decoder.info.channels);
        return ESP_ERR_INVALID_ARG;
    }

    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = 16,
        .channel = (uint8_t)decoder.info.channels,
        .channel_mask = 0,
        .sample_rate = (uint32_t)decoder.info.hz,
        .mclk_multiple = 0,
    };

    int open_res = esp_codec_dev_open(s_speaker, &sample_info);
    if (open_res != ESP_CODEC_DEV_OK) {
        mp3dec_ex_close(&decoder);
        s_audio_fault = true;
        ESP_LOGE(TAG, "[C] Apertura codec per MP3 fallita: %d", open_res);
        return ESP_FAIL;
    }

    int vol_res = esp_codec_dev_set_out_vol(s_speaker, s_volume);
    if (vol_res != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "[C] Applicazione volume MP3 fallita: %d", vol_res);
    }

    mp3d_sample_t pcm_samples[MP3_SAMPLE_BUFFER_SAMPLES] = {0};
    esp_err_t result = ESP_OK;

    while (true) {
        if (s_stop_requested) {
            result = ESP_ERR_INVALID_STATE;
            break;
        }

        size_t decoded_samples = mp3dec_ex_read(&decoder,
                                                pcm_samples,
                                                MP3_SAMPLE_BUFFER_SAMPLES);
        if (decoded_samples == 0) {
            break;
        }

        int decoded_bytes = (int)(decoded_samples * sizeof(mp3d_sample_t));
        int write_res = esp_codec_dev_write(s_speaker, pcm_samples, decoded_bytes);
        if (write_res != ESP_CODEC_DEV_OK) {
            s_audio_fault = true;
            ESP_LOGE(TAG, "[C] Scrittura PCM MP3 fallita: %d", write_res);
            result = ESP_FAIL;
            break;
        }
    }

    (void)esp_codec_dev_close(s_speaker);
    mp3dec_ex_close(&decoder);
    return result;
}

esp_err_t audio_player_init(void)
{
    if (s_audio_ready && s_speaker != NULL) {
        return ESP_OK;
    }

    s_speaker = bsp_audio_codec_speaker_init();
    if (!s_speaker) {
        s_audio_fault = true;
        ESP_LOGE(TAG, "[C] Inizializzazione speaker fallita");
        return ESP_FAIL;
    }

    int vol_res = esp_codec_dev_set_out_vol(s_speaker, s_volume);
    if (vol_res != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "[C] Impostazione volume fallita: %d", vol_res);
    }

    s_audio_ready = true;
    s_audio_fault = false;
    ESP_LOGI(TAG, "[C] Audio player inizializzato");
    return ESP_OK;
}

esp_err_t audio_player_set_volume(uint8_t volume)
{
    if (volume > 100U) {
        volume = 100U;
    }
    s_volume = volume;

    esp_err_t init_err = audio_player_init();
    if (init_err != ESP_OK) {
        return init_err;
    }

    /* Evita chiamate al codec a canale idle: il volume viene applicato in play_file dopo esp_codec_dev_open. */
    if (!s_is_playing) {
        ESP_LOGI(TAG, "[C] Volume audio memorizzato: %u", (unsigned)s_volume);
        return ESP_OK;
    }

    int vol_res = esp_codec_dev_set_out_vol(s_speaker, s_volume);
    if (vol_res != ESP_CODEC_DEV_OK) {
        s_audio_fault = true;
        ESP_LOGW(TAG, "[C] Impostazione volume fallita: %d", vol_res);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "[C] Volume audio impostato: %u", (unsigned)s_volume);
    return ESP_OK;
}

esp_err_t audio_player_stop(void)
{
    s_stop_requested = true;
    return ESP_OK;
}

bool audio_player_is_playing(void)
{
    return s_is_playing;
}

device_component_status_t audio_player_get_component_status(void)
{
    const device_config_t *config = device_config_get();

    if (config == NULL || !config->audio.enabled) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (s_is_playing) {
        return DEVICE_COMPONENT_STATUS_ONLINE;
    }

    if (s_audio_fault) {
        return DEVICE_COMPONENT_STATUS_OFFLINE;
    }

    return DEVICE_COMPONENT_STATUS_ACTIVE;
}

esp_err_t audio_player_play_file(const char *path)
{
    if (!path || path[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (strncmp(path, "/spiffs/", 8) != 0) {
        ESP_LOGW(TAG, "[C] Path non SPIFFS: %s", path);
        return ESP_ERR_INVALID_ARG;
    }

    const char *ext = strrchr(path, '.');

    esp_err_t init_err = audio_player_init();
    if (init_err != ESP_OK) {
        return init_err;
    }

    s_stop_requested = false;
    s_is_playing = true;

    if (ext && strcasecmp(ext, ".mp3") == 0) {
        esp_err_t mp3_err = play_mp3_file(path);
        s_is_playing = false;
        if (mp3_err == ESP_OK) {
            ESP_LOGI(TAG, "[C] Riproduzione MP3 completata: %s", path);
        } else if (s_stop_requested) {
            ESP_LOGI(TAG, "[C] Riproduzione MP3 interrotta: %s", path);
            mp3_err = ESP_OK;
        }
        s_stop_requested = false;
        return mp3_err;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        s_is_playing = false;
        ESP_LOGW(TAG, "[C] File audio non trovato: %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        s_is_playing = false;
        return ESP_FAIL;
    }
    long file_size = ftell(f);
    if (file_size < (long)WAV_HEADER_BYTES_MIN) {
        fclose(f);
        s_is_playing = false;
        return ESP_ERR_INVALID_SIZE;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        s_is_playing = false;
        return ESP_FAIL;
    }

    esp_codec_dev_sample_info_t sample_info = {0};
    uint32_t data_size = 0;
    esp_err_t parse_err = wav_locate_data_chunk(f, &sample_info, &data_size);
    if (parse_err != ESP_OK) {
        fclose(f);
        s_is_playing = false;
        return parse_err;
    }

    int open_res = esp_codec_dev_open(s_speaker, &sample_info);
    if (open_res != ESP_CODEC_DEV_OK) {
        s_audio_fault = true;
        ESP_LOGE(TAG, "[C] Apertura codec fallita: %d", open_res);
        fclose(f);
        s_is_playing = false;
        return ESP_FAIL;
    }

    int vol_res = esp_codec_dev_set_out_vol(s_speaker, s_volume);
    if (vol_res != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "[C] Applicazione volume WAV fallita: %d", vol_res);
    }

    uint8_t buffer[AUDIO_COPY_BUFFER_SIZE] = {0};
    uint32_t remaining = data_size;
    esp_err_t result = ESP_OK;

    while (remaining > 0U) {
        if (s_stop_requested) {
            result = ESP_ERR_INVALID_STATE;
            break;
        }

        size_t to_read = (remaining > AUDIO_COPY_BUFFER_SIZE) ? AUDIO_COPY_BUFFER_SIZE : (size_t)remaining;
        size_t read_len = fread(buffer, 1, to_read, f);
        if (read_len == 0U) {
            result = ESP_FAIL;
            break;
        }

        int write_res = esp_codec_dev_write(s_speaker, buffer, (int)read_len);
        if (write_res != ESP_CODEC_DEV_OK) {
            s_audio_fault = true;
            ESP_LOGE(TAG, "[C] Scrittura audio fallita: %d", write_res);
            result = ESP_FAIL;
            break;
        }

        remaining -= (uint32_t)read_len;
    }

    (void)esp_codec_dev_close(s_speaker);
    fclose(f);
    s_is_playing = false;

    if (result == ESP_OK) {
        ESP_LOGI(TAG, "[C] Riproduzione completata: %s", path);
    } else if (s_stop_requested) {
        ESP_LOGI(TAG, "[C] Riproduzione interrotta: %s", path);
        result = ESP_OK;
    }

    s_stop_requested = false;

    return result;
}

/*
 * Mockup audio: nessun codec/I2S reale, nessun accesso hardware speaker.
 * Attiva quando DNA_AUDIO == 1.
 */
#else

static const char *TAG = "AUDIO_PLAYER";
static bool s_audio_ready = false;
static bool s_audio_fault = false;
static bool s_stop_requested = false;
static bool s_is_playing = false;
static uint8_t s_volume = 75;


/**
 * @brief Inizializza il player audio simulato.
 *
 * @return esp_err_t Sempre ESP_OK in modalita' mock.
 */
esp_err_t audio_player_init(void)
{
    s_audio_ready = true;
    s_audio_fault = false;
    ESP_LOGI(TAG, "[C] [MOCK] Audio player inizializzato (DNA_AUDIO=1)");
    return ESP_OK;
}


/**
 * @brief Imposta il volume del player audio simulato.
 *
 * @param [in] volume Volume richiesto (0-100).
 * @return esp_err_t Sempre ESP_OK in modalita' mock.
 */
esp_err_t audio_player_set_volume(uint8_t volume)
{
    if (volume > 100U) {
        volume = 100U;
    }

    s_volume = volume;
    s_audio_ready = true;
    s_audio_fault = false;
    ESP_LOGI(TAG, "[C] [MOCK] Volume audio impostato: %u", (unsigned)s_volume);
    return ESP_OK;
}


/**
 * @brief Richiede l'interruzione della riproduzione simulata.
 *
 * @return esp_err_t Sempre ESP_OK.
 */
esp_err_t audio_player_stop(void)
{
    s_stop_requested = true;
    s_is_playing = false;
    ESP_LOGI(TAG, "[C] [MOCK] audio_player_stop");
    return ESP_OK;
}


/**
 * @brief Indica se il player simulato risulta in riproduzione.
 *
 * @return true Se marcato come in riproduzione, false altrimenti.
 */
bool audio_player_is_playing(void)
{
    return s_is_playing;
}

device_component_status_t audio_player_get_component_status(void)
{
    const device_config_t *config = device_config_get();

    if (config == NULL || !config->audio.enabled) {
        return DEVICE_COMPONENT_STATUS_DISABLED;
    }

    if (s_is_playing) {
        return DEVICE_COMPONENT_STATUS_ONLINE;
    }

    if (s_audio_fault) {
        return DEVICE_COMPONENT_STATUS_OFFLINE;
    }

    return DEVICE_COMPONENT_STATUS_ACTIVE;
}


/**
 * @brief Simula la riproduzione di un file audio da SPIFFS.
 *
 * @param [in] path Path del file richiesto.
 * @return esp_err_t ESP_OK se il path e' valido, errore altrimenti.
 */
esp_err_t audio_player_play_file(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (strncmp(path, "/spiffs/", 8) != 0) {
        ESP_LOGW(TAG, "[C] [MOCK] Path non SPIFFS: %s", path);
        return ESP_ERR_INVALID_ARG;
    }

    s_audio_ready = true;
    s_stop_requested = false;
    s_is_playing = true;
    ESP_LOGI(TAG, "[C] [MOCK] Riproduzione audio simulata: %s (volume=%u)",
             path,
             (unsigned)s_volume);

    if (s_stop_requested) {
        s_is_playing = false;
        s_stop_requested = false;
        ESP_LOGI(TAG, "[C] [MOCK] Riproduzione interrotta: %s", path);
        return ESP_OK;
    }

    s_is_playing = false;
    return ESP_OK;
}

#endif /* DNA_AUDIO == 0 */
