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
exec npm start
