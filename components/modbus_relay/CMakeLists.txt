idf_component_register(
    SRCS "modbus_relay.c"
    INCLUDE_DIRS "include"
    REQUIRES espressif__esp-modbus device_config esp_timer
)
