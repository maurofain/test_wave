idf_component_register(
    SRCS "fsm.c" "main.c" "init.c" "tasks.c"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_eth esp_http_server esp_https_ota app_update nvs_flash esp_netif esp_netif_stack esp_event driver led_strip spiffs soc esp_hw_support lwip led device_config web_ui remote_logging mdb pwm rs232 rs485 io_expander eeprom_24lc16 sd_card sht40 waveshare__esp32_p4_nano error_log device_activity espcoredump lvgl_panel cctalk
)

spiffs_create_partition_image(storage ${CMAKE_CURRENT_LIST_DIR}/../data FLASH_IN_PROJECT)
