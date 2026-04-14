idf_component_register(SRCS "mdb.c" "mdb_cashless.c" "test/mdb_test.c"
                    INCLUDE_DIRS "include" "test"
                    REQUIRES driver device_config)
