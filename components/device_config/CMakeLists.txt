idf_component_register(
    SRCS "device_config.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash eeprom_24lc16 app_update
    PRIV_REQUIRES json esp_rom
)
