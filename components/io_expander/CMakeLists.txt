idf_component_register(SRCS "io_expander.c" "test/io_expander_test.c"
                    INCLUDE_DIRS "include" "test"
                    REQUIRES driver device_config
                    PRIV_REQUIRES periph_i2c)

# Il flag DNA_IO_EXPANDER è definito centralmente in CMakeLists.txt radice.
# Non è necessario impostarlo qui.

