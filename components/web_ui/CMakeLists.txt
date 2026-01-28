idf_component_register(
    SRCS "web_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server device_config app_update esp_https_ota esp_http_client esp_netif mdb_bus
    PRIV_REQUIRES json led_control main
)
