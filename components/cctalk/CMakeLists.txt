idf_component_register(SRCS "cctalk.c" "cctalk_driver.c"
                       INCLUDE_DIRS "include"
                       REQUIRES driver device_config
                       PRIV_REQUIRES esp_timer main)
