idf_component_register(
    SRCS "led.c" "test/led_test.c"
    INCLUDE_DIRS "include" "test"
    REQUIRES led_strip device_config
)

# Il flag DNA_LED_STRIP è definito centralmente in CMakeLists.txt radice.
# Non è necessario impostarlo qui.

