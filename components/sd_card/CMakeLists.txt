idf_component_register(SRCS "sd_card.c"
                    INCLUDE_DIRS "include"
                    REQUIRES fatfs sdmmc esp_driver_sdmmc vfs)

# Il flag DNA_SD_CARD è definito centralmente in CMakeLists.txt radice.
# Non è necessario impostarlo qui.

