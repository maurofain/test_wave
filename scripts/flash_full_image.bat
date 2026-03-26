@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "MODE="
set "PORT=COM3"
set "FILE="
set "BAUD=460800"
set "CHIP=esp32p4"
set "FLASH_SIZE=16777216"

if "%~1"=="" goto :usage

:parse_args
if "%~1"=="" goto :validate_args

if /I "%~1"=="-g" (
    if defined MODE goto :mode_conflict
    set "MODE=READ"
    shift
    goto :parse_args
)

if /I "%~1"=="-w" (
    if defined MODE goto :mode_conflict
    set "MODE=WRITE"
    shift
    goto :parse_args
)

if /I "%~1"=="-p" (
    if "%~2"=="" goto :missing_value
    set "PORT=%~2"
    shift
    shift
    goto :parse_args
)

if /I "%~1"=="-f" (
    if "%~2"=="" goto :missing_value
    set "FILE=%~2"
    shift
    shift
    goto :parse_args
)

if /I "%~1"=="-s" (
    if "%~2"=="" goto :missing_value
    set "BAUD=%~2"
    shift
    shift
    goto :parse_args
)

if /I "%~1"=="-h" goto :usage
if /I "%~1"=="--help" goto :usage

echo [ERRORE] Argomento non riconosciuto: %~1
goto :usage_err

:mode_conflict
echo [ERRORE] Specifica una sola modalita: -g oppure -w
goto :usage_err

:missing_value
echo [ERRORE] Manca il valore per il parametro %~1
goto :usage_err

:validate_args
if not defined MODE (
    echo [ERRORE] Devi specificare -g oppure -w
    goto :usage_err
)

if not defined FILE (
    echo [ERRORE] Devi specificare il file con -f
    goto :usage_err
)

where esptool.exe >nul 2>nul
if errorlevel 1 (
    echo [ERRORE] esptool.exe non trovato nel PATH.
    echo          Installa esptool standalone o aggiungilo al PATH.
    exit /b 1
)

if /I "%MODE%"=="READ" goto :do_read
if /I "%MODE%"=="WRITE" goto :do_write

echo [ERRORE] Modalita interna non valida: %MODE%
exit /b 2

:do_read
echo [INFO] Dump completo flash su "%FILE%"
echo [INFO] Porta=%PORT% Baud=%BAUD% Chip=%CHIP%
esptool.exe --chip %CHIP% -p "%PORT%" -b %BAUD% --before default_reset --after hard_reset read_flash 0x0 0x1000000 "%FILE%"
exit /b %errorlevel%

:do_write
if not exist "%FILE%" (
    echo [ERRORE] File non trovato: "%FILE%"
    exit /b 2
)

for %%I in ("%FILE%") do set "FILE_SIZE=%%~zI"
if not "!FILE_SIZE!"=="%FLASH_SIZE%" (
    echo [ERRORE] Dimensione file non valida: !FILE_SIZE! bytes (atteso: %FLASH_SIZE%)
    exit /b 2
)

echo [INFO] Scrittura completa flash da "%FILE%"
echo [INFO] Porta=%PORT% Baud=%BAUD% Chip=%CHIP%
esptool.exe --chip %CHIP% -p "%PORT%" -b %BAUD% --before default_reset --after hard_reset write_flash --force 0x0 "%FILE%"
exit /b %errorlevel%

:usage
echo Uso:
echo   %~nx0 -g -f FILE [-p PORTA] [-s BAUD]
echo   %~nx0 -w -f FILE [-p PORTA] [-s BAUD]
echo.
echo Opzioni:
echo   -g        Legge tutta la flash dal device e salva su file
echo   -w        Scrive tutta la flash sul device da file
echo   -p PORTA  Porta seriale (default: COM3)
echo   -f FILE   File immagine flash
echo   -s BAUD   Velocita seriale (default: 460800)
echo.
echo Esempi:
echo   %~nx0 -g -p COM3 -s 921600 -f backup_flash.bin
echo   %~nx0 -w -p COM4 -f backup_flash.bin
exit /b 0

:usage_err
echo.
echo Usa %~nx0 -h per la guida.
exit /b 2
