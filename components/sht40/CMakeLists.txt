idf_component_register(SRCS "sht40.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver
                    PRIV_REQUIRES waveshare__esp32_p4_nano)

# Il flag DNA_SHT40 è definito centralmente in CMakeLists.txt radice.
