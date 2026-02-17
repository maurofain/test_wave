idf_component_register(SRCS "cctalk.c" "cctalk_driver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver
                       PRIV_REQUIRES esp_timer)
