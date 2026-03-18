idf_component_register(
    SRCS "digital_io.c"
    INCLUDE_DIRS "include"
    REQUIRES device_config io_expander modbus_relay
)
