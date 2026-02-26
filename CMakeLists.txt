cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Usa versione locale del BSP invece di quella managed
set(EXTRA_COMPONENT_DIRS components/waveshare__esp32_p4_nano)

# ---------------------------------------------------------------------------
# Flag DNA_* (DoNotActivate) — visibili a TUTTI i componenti e a main/
# Impostare a 1 per attivare il mockup del modulo corrispondente.
# Modificare qui invece che in app_version.h o nei CMakeLists dei componenti.
# ---------------------------------------------------------------------------
add_compile_definitions(
    DNA_LVGL=0
    DNA_ETHERNET=0
    DNA_WIFI=0
    DNA_SD_CARD=0
    DNA_TOUCHSCREEN=0
    DNA_REMOTE_LOGGING=0
    DNA_IO_EXPANDER=1
    DNA_GPIO=1
    DNA_PWM=1
    DNA_LED_STRIP=1
    DNA_USB_SCANNER=1
    DNA_RS232=1
    DNA_RS485=1
    DNA_MDB=1
    DNA_SHT40=1
    DNA_CCTALK=0
    DNA_I2C=0
    DNA_SPI=0
    DNA_UART=0
    DNA_ADC=0
    DNA_DAC=0
    DNA_MIDI=0
    DNA_HID=0
    DNA_SYS_MONITOR=0
    DNA_SERVER_POST=0
)

project(test_wave)
