idf_component_register(
    SRCS "lvgl_page_ads.c" "sevensegments_300.c"
         "arial96.c"
         "DroidSansBold32.c"
         "DroidSansMono18.c"
         "language_flags.c"
         "lvgl_panel.c"
         "lvgl_page_main.c"
         "lvgl_page_language.c"
         "lvgl_page_out_of_service.c"
         "lvgl_page_programs_v2.c"
    INCLUDE_DIRS "include"
    REQUIRES waveshare__esp32_p4_nano web_ui device_config usb_cdc_scanner
    PRIV_REQUIRES main esp_driver_jpeg
)
