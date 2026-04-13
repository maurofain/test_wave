#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include "hw_common.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t audio_player_init(void);
esp_err_t audio_player_set_volume(uint8_t volume);
esp_err_t audio_player_stop(void);
bool audio_player_is_playing(void);
esp_err_t audio_player_play_file(const char *path);
hw_component_status_t audio_player_get_status(void);

#ifdef __cplusplus
}
#endif
