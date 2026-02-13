idf_component_register(
    SRCS "web_ui.c" "web_ui_register.c" "web_ui_reboot.c" "web_ui_api.c" "web_ui_pages.c" "web_ui_logs.c" "web_ui_common.c" "web_ui_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server device_config app_update esp_https_ota esp_http_client esp_netif mdb serial_test io_expander eeprom_24lc16 pwm sd_card sht40 aux_gpio usb_cdc_scanner driver http_services
    PRIV_REQUIRES json led main
)
