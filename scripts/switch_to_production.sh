#!/bin/bash

# Script per commutare l'ambiente sullo sviluppo PRODUZIONE
# Rinomina le cartelle per rendere 'app' il componente principale

set -euo pipefail

if [ ! -d "main" ]; then
    echo "❌ Errore: cartella 'main' non trovata. Stato progetto non valido."
    exit 1
fi

if [ -d "factory" ] && [ -d "app" ]; then
    echo "❌ Errore: trovate sia 'factory' che 'app'. Stato ambiguo, switch annullato."
    exit 1
fi

if [ -d "app" ]; then
    echo "Spostamento main -> factory (test hardware) e app -> main (produzione)"
    if [ -e "factory" ]; then
        echo "❌ Errore: esiste già 'factory', impossibile rinominare 'main' in 'factory'."
        exit 1
    fi
    mv main factory
    mv app main
    echo "Ambiente PRODUZIONE pronto."
    echo ""
    echo "Per flashare sulla partizione corretta usa:"
    echo "  idf.py app-flash --partition-name ota_0"
    echo ""
else
    if [ -d "factory" ]; then
        echo "✅ Ambiente già impostato su PRODUZIONE (main+factory presenti)."
    else
        echo "❌ Errore: cartella 'app' non trovata e 'factory' assente. Stato non riconosciuto."
        exit 1
    fi
fi
