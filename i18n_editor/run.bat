@echo off
REM Script per avviare l'i18n Editor Electron su Windows

cd /d "%~dp0"

REM Controlla se Node.js è disponibile
node --version >nul 2>&1
if errorlevel 1 (
    echo Errore: Node.js non trovato
    echo Installa Node.js 18+ da nodejs.org
    pause
    exit /b 1
)

REM Controlla se npm è disponibile
npm --version >nul 2>&1
if errorlevel 1 (
    echo Errore: npm non trovato
    echo Reinstalla Node.js includendo npm
    pause
    exit /b 1
)

if not exist node_modules\electron (
    echo Installazione dipendenze npm...
    npm install
    if errorlevel 1 (
        echo Errore durante npm install
        pause
        exit /b 1
    )
)

echo Avvio i18n Editor Electron...
npm start
pause
