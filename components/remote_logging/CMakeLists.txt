idf_component_register(
    SRCS "remote_logging.c"
    INCLUDE_DIRS "include"
    REQUIRES device_config esp_netif web_ui
)