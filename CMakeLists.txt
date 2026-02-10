cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Usa versione locale del BSP invece di quella managed
set(EXTRA_COMPONENT_DIRS components/waveshare__esp32_p4_nano)

project(test_wave)
