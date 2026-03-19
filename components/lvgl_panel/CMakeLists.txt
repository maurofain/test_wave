idf_component_register(
    SRCS "lvgl_page_ads.c"
         "GoogleSans35.c"
         "GoogleSans40.c"
         "GoogleSans50.c"
         "GoogleSans70.c"
         "GoogleSans140.c"
         "language_flags.c"
         "lvgl_i18n.c"
         "lvgl_page_chrome.c"
         "lvgl_panel.c"
         "lvgl_page_programmi.c"
         "lvgl_page_language.c"
         "lvgl_page_language_2.c"
         "lvgl_page_out_of_service.c"
    INCLUDE_DIRS "include"
    REQUIRES waveshare__esp32_p4_nano web_ui device_config usb_cdc_scanner sd_card digital_io
    PRIV_REQUIRES main
)

# Accesso privato agli header interni di TJPGD (compilato dentro LVGL)
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_SOURCE_DIR}/managed_components/lvgl__lvgl/src/libs/tjpgd"
)
