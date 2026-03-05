cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Usa versione locale del BSP invece di quella managed
set(EXTRA_COMPONENT_DIRS components/waveshare__esp32_p4_nano)

# ---------------------------------------------------------------------------
# Flag DNA_* (DoNotActivate) — visibili a TUTTI i componenti e a main/
# Impostare a 1 per attivare il mockup del modulo corrispondente.
# Modificare qui invece che in main/app_version.h o nei CMakeLists dei componenti.
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
set(F_IO_EXPANDER     0)
set(F_GPIO            0)
set(F_PWM             1)
set(F_LED_STRIP       1)
set(F_RS232           1)
set(F_RS485           1)
set(F_MDB             1)
set(F_SHT40           0)
set(F_CCTALK          0)
set(F_REMOTE_LOGGING  0)
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
# Web pages source: 0=embedded (C), 1=SPIFFS (/spiffs/www), 2=SD (/sdcard/www)
set(F_WEB_UI_PAGE_SOURCE 1)
# Export pagine seed all'avvio quando source è esterno (0=off, 1=on)
set(F_WEB_UI_EXPORT_ON_BOOT 0)

# Embedded pages include switch: when source is embedded (0) include C HTML/JS,
# otherwise compile in external-only mode (SPIFFS/SD)
if(F_WEB_UI_PAGE_SOURCE EQUAL 0)
    set(F_WEB_UI_USE_EMBEDDED_PAGES 1)
else()
    set(F_WEB_UI_USE_EMBEDDED_PAGES 0)
endif()

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
    WEB_UI_PAGE_SOURCE=${F_WEB_UI_PAGE_SOURCE}
    WEB_UI_EXPORT_ON_BOOT=${F_WEB_UI_EXPORT_ON_BOOT}
    WEB_UI_USE_EMBEDDED_PAGES=${F_WEB_UI_USE_EMBEDDED_PAGES}
)

project(test_wave)
