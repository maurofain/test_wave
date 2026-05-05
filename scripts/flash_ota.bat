@echo off
setlocal enabledelayedexpansion

rem Flash build\bootloader.bin, partition table, app bin e ota_data_initial su factory/ota_0/ota_1
rem Flash build\storage.bin su partizione storage, se esiste
rem
rem Per Windows: installa Python 3 e poi esegui
rem   py -m pip install --upgrade pip
rem   py -m pip install esptool
rem Oppure usa il pacchetto ufficiale di esptool dal repository ESP-IDF.

if "%~1"=="" goto :usage
set "PORT=%~1"
goto :continue

:usage
echo Uso: %~nx0 COMx
echo.
echo Parametri:
echo   COMx                Porta seriale della board (es. COM3)
echo.
echo Esempio:
echo   %~nx0 COM3
echo.
echo Prerequisiti:
echo   Python 3 installato
echo   py -m pip install --upgrade pip
echo   py -m pip install esptool
echo.
echo Il comando flasha:
echo   build\bootloader.bin su 0x2000
echo   build\partition_table\partition-table.bin su 0x8000
echo   build\test_wave.bin su factory, ota_0 e ota_1
echo   build\ota_data_initial.bin su 0x93E000
echo   build\storage.bin su 0x940000 (se presente)
echo.
goto :end

:continue

set "BOOTLOADER_BIN=bootloader.bin"
set "PARTITION_BIN=partition-table.bin"
set "OTA_DATA_BIN=ota_data_initial.bin"
set "APP_BIN=test_wave.bin"
set "STORAGE_BIN=storage.bin"
set "BOOTLOADER_OFFSET=0x2000"
set "PARTITION_OFFSET=0x8000"
set "APP_FACTORY=0x10000"
set "APP_OTA0=0x310000"
set "APP_OTA1=0x610000"
set "OTA_DATA_OFFSET=0x93E000"
set "STORAGE_OFFSET=0x940000"
set "FLASH_MODE=dio"
set "FLASH_FREQ=80m"
set "FLASH_SIZE=16MB"
set "BAUD=460800"

if not exist "%BOOTLOADER_BIN%" (
  echo ERRORE: file non trovato: %BOOTLOADER_BIN%
  goto :end
)
if not exist "%PARTITION_BIN%" (
  echo ERRORE: file non trovato: %PARTITION_BIN%
  goto :end
)
if not exist "%APP_BIN%" (
  echo ERRORE: file non trovato: %APP_BIN%
  goto :end
)
if not exist "%OTA_DATA_BIN%" (
  echo ERRORE: file non trovato: %OTA_DATA_BIN%
  goto :end
)

set "HAS_STORAGE=0"
if exist "%STORAGE_BIN%" set "HAS_STORAGE=1"

where python >nul 2>&1
if errorlevel 1 (
  where py >nul 2>&1
  if errorlevel 1 (
    echo ERRORE: Python non trovato. Installa Python 3 e esptool.
    goto :end
  ) else (
    set "PYCMD=py -3"
  )
) else (
  set "PYCMD=python"
)

echo.
echo Flashing su %PORT%...
echo - Bootloader: %BOOTLOADER_OFFSET%
echo - Partition table: %PARTITION_OFFSET%
echo - App factory: %APP_FACTORY%
echo - App ota_0: %APP_OTA0%
echo - App ota_1: %APP_OTA1%
echo - OTA data: %OTA_DATA_OFFSET%
%PYCMD% -m esptool --chip esp32p4 --port %PORT% --baud %BAUD% write_flash --flash_mode %FLASH_MODE% --flash_freq %FLASH_FREQ% --flash_size %FLASH_SIZE% %BOOTLOADER_OFFSET% "%BOOTLOADER_BIN%" %PARTITION_OFFSET% "%PARTITION_BIN%" %APP_FACTORY% "%APP_BIN%" %APP_OTA0% "%APP_BIN%" %APP_OTA1% "%APP_BIN%" %OTA_DATA_OFFSET% "%OTA_DATA_BIN%"
if errorlevel 1 (
  echo ERRORE: il flash dell'app o dei dati OTA non e` andato a buon fine.
  goto :end
)

if "%HAS_STORAGE%"=="1" (
  echo.
  echo Flashing storage: %STORAGE_OFFSET%
  %PYCMD% -m esptool --chip esp32p4 --port %PORT% --baud %BAUD% write_flash --flash_mode %FLASH_MODE% --flash_freq %FLASH_FREQ% --flash_size %FLASH_SIZE% %STORAGE_OFFSET% "%STORAGE_BIN%"
  if errorlevel 1 (
    echo ERRORE: il flash dello storage non e` andato a buon fine.
    goto :end
  )
) else (
  echo.
  echo Nota: file %STORAGE_BIN% non trovato, omesso il flash storage.
)

echo.
echo Flash completato con successo.

:end
endlocal
