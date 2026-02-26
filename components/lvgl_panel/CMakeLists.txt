idf_component_register(
    SRCS "sevensegments_300.c" "arial96.c" "lvgl_panel.c"
    INCLUDE_DIRS "include"
    REQUIRES waveshare__esp32_p4_nano web_ui device_config
    PRIV_REQUIRES main
)
