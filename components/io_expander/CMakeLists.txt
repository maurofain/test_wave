idf_component_register(SRCS "io_expander.c" "test/io_expander_test.c"
                    INCLUDE_DIRS "include" "test"
                    REQUIRES driver
                    PRIV_REQUIRES waveshare__esp32_p4_nano)
