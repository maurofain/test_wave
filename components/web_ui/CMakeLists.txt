idf_component_register(
    SRCS "webpages.c" "webpages_embedded.c" "web_ui_file_manager.c" "web_ui_test_pages.c" "web_ui_test_api.c" "web_ui_pages_runtime.c" "web_ui_programs_page.c" "web_ui_auth_emulator.c" "web_ui.c" "web_ui_logs.c" "web_ui_common.c" "web_ui_server.c" "web_ui_programs.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server device_config app_update esp_https_ota esp_http_client esp_netif mdb serial_test io_expander eeprom_24lc16 pwm sd_card sht40 aux_gpio usb_cdc_scanner driver http_services modbus_relay
    PRIV_REQUIRES json led main
)
