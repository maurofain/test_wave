idf_component_register(SRCS "eeprom_24lc16.c" "test/eeprom_test.c"
                    INCLUDE_DIRS "include" "test"
                    REQUIRES driver esp_http_server json device_config
                    PRIV_REQUIRES waveshare__esp32_p4_nano)
