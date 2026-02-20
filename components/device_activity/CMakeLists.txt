idf_component_register(SRCS "device_activity.c"
                       INCLUDE_DIRS "."
                       REQUIRES spiffs
                       PRIV_REQUIRES json)
