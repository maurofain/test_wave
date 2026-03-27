#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_player_init(void);
esp_err_t audio_player_set_volume(uint8_t volume);
esp_err_t audio_player_stop(void);
bool audio_player_is_playing(void);
esp_err_t audio_player_play_file(const char *path);

#ifdef __cplusplus
}
#endif
