idf_component_register(
    SRCS "sevensegments_300.c" "arial96.c" "lvgl_panel.c"
    INCLUDE_DIRS "include"
    REQUIRES waveshare__esp32_p4_nano web_ui
    PRIV_REQUIRES main
)
