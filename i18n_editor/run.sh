#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")"

if ! command -v node >/dev/null 2>&1; then
    echo "Errore: node non trovato"
    echo "Installa Node.js 18+ e npm"
    exit 1
fi

if ! command -v npm >/dev/null 2>&1; then
    echo "Errore: npm non trovato"
    echo "Installa npm insieme a Node.js"
    exit 1
fi

if [ ! -d "node_modules/electron" ]; then
    echo "Installazione dipendenze npm..."
    npm install
fi

echo "Avvio i18n Editor Electron..."

SANDBOX_BIN="node_modules/electron/dist/chrome-sandbox"

if [ -f "$SANDBOX_BIN" ]; then
    owner_uid="$(stat -c '%u' "$SANDBOX_BIN" 2>/dev/null || echo "")"
    mode_octal="$(stat -c '%a' "$SANDBOX_BIN" 2>/dev/null || echo "")"
    if [ "$owner_uid" = "0" ] && [ "$mode_octal" = "4755" ]; then
        exec npm start
    fi
fi

echo "Attenzione: sandbox SUID non configurato correttamente."
echo "Avvio in modalità sviluppo con --no-sandbox."
echo "Per modalità sicura configura:"
echo "  sudo chown root:root $SANDBOX_BIN"
echo "  sudo chmod 4755 $SANDBOX_BIN"
exec npm start -- --no-sandbox
