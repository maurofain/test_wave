cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Usa versione locale del BSP invece di quella managed
set(EXTRA_COMPONENT_DIRS components/waveshare__esp32_p4_nano)

# ---------------------------------------------------------------------------
# Flag DNA_* (DoNotActivate) — visibili a TUTTI i componenti e a main/
# Impostare a 1 per attivare il mockup del modulo corrispondente.
# Modificare qui invece che in app_version.h o nei CMakeLists dei componenti.
# ---------------------------------------------------------------------------

# --- Valori base (modificare qui) ------------------------------------------
set(F_LVGL            0)
set(F_ETHERNET        0)
set(F_WIFI            0)
set(F_SD_CARD         0)
set(F_TOUCHSCREEN     0)
set(F_USB_SCANNER     0)
# Se F_EXPANSION_BOARD=1 (board assente), tutti i sotto-componenti vengono
# forzati a 1 (mock) dal blocco if() qui sotto.
set(F_EXPANSION_BOARD 0)
set(F_IO_EXPANDER     1)
set(F_GPIO            1)
set(F_PWM             1)
set(F_LED_STRIP       1)
set(F_RS232           1)
set(F_RS485           1)
set(F_MDB             1)
set(F_SHT40           1)
set(F_CCTALK          0)
set(F_REMOTE_LOGGING  1)
set(F_ERROR_DUMP      1)
# not implemented
set(F_I2C             0)
set(F_SPI             0)
set(F_UART            0)
set(F_ADC             0)
set(F_DAC             0)
set(F_MIDI            0)
set(F_HID             0)
set(F_SYS_MONITOR     0)
set(F_SERVER_POST     0)
set(F_LVGL_COUNTDOWN_BG 0)

# --- Override: se la scheda di espansione è assente, mock tutti i moduli ---
if(F_EXPANSION_BOARD EQUAL 1)
    set(F_IO_EXPANDER 1)
    set(F_PWM         1)
    set(F_LED_STRIP   1)
    set(F_RS232       1)
    set(F_RS485       1)
    set(F_MDB         1)
    set(F_SHT40       1)
    set(F_CCTALK      1)
endif()

add_compile_definitions(
    DNA_LVGL=${F_LVGL}
    DNA_ETHERNET=${F_ETHERNET}
    DNA_WIFI=${F_WIFI}
    DNA_SD_CARD=${F_SD_CARD}
    DNA_TOUCHSCREEN=${F_TOUCHSCREEN}
    DNA_USB_SCANNER=${F_USB_SCANNER}
    DNA_EXPANSION_BOARD=${F_EXPANSION_BOARD}
    DNA_IO_EXPANDER=${F_IO_EXPANDER}
    DNA_GPIO=${F_GPIO}
    DNA_PWM=${F_PWM}
    DNA_LED_STRIP=${F_LED_STRIP}
    DNA_RS232=${F_RS232}
    DNA_RS485=${F_RS485}
    DNA_MDB=${F_MDB}
    DNA_SHT40=${F_SHT40}
    DNA_CCTALK=${F_CCTALK}
    DNA_REMOTE_LOGGING=${F_REMOTE_LOGGING}
    DNA_ERROR_DUMP=${F_ERROR_DUMP}
    DNA_I2C=${F_I2C}
    DNA_SPI=${F_SPI}
    DNA_UART=${F_UART}
    DNA_ADC=${F_ADC}
    DNA_DAC=${F_DAC}
    DNA_MIDI=${F_MIDI}
    DNA_HID=${F_HID}
    DNA_SYS_MONITOR=${F_SYS_MONITOR}
    DNA_SERVER_POST=${F_SERVER_POST}
    DNA_LVGL_COUNTDOWN_BG=${F_LVGL_COUNTDOWN_BG}
)

project(test_wave)
