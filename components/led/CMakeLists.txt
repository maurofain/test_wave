idf_component_register(
    SRCS "led.c" "test/led_test.c"
    INCLUDE_DIRS "include" "test"
    REQUIRES led_strip device_config
)
