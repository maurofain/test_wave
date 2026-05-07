idf_component_register(
    SRCS "usb_cdc_scanner.c"
    INCLUDE_DIRS "include"
    REQUIRES usb usb_host_cdc_acm device_config usb_cdc_epaper
    PRIV_REQUIRES waveshare__esp32_p4_nano web_ui
)
