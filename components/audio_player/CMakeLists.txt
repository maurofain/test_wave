idf_component_register(
    SRCS "audio_player.c"
    INCLUDE_DIRS "include" "third_party"
    REQUIRES waveshare__esp32_p4_nano device_config
)
