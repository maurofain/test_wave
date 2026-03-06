# Il flag DNA_SHT40 è definito centralmente in CMakeLists.txt radice.
# In modalità mock (F_SHT40=1) il componente non richiede dipendenze I2C.
if(DEFINED F_SHT40 AND "${F_SHT40}" STREQUAL "1")
    idf_component_register(SRCS "sht40.c"
                        INCLUDE_DIRS "include"
                        REQUIRES driver)
else()
    idf_component_register(SRCS "sht40.c"
                        INCLUDE_DIRS "include"
                        REQUIRES driver
                        PRIV_REQUIRES periph_i2c)
endif()
