idf_component_register(SRCS "http_services.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_http_server mbedtls json)
