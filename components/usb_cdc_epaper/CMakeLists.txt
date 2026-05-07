idf_component_register(
    SRCS "usb_cdc_epaper.c"
    INCLUDE_DIRS "include"
    REQUIRES usb_host_cdc_acm
)

