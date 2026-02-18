#!/bin/bash

# Script per commutare l'ambiente sullo sviluppo FACTORY
# Rinomina le cartelle per rendere 'factory' il componente principale

set -euo pipefail

if [ ! -d "main" ]; then
    echo "❌ Errore: cartella 'main' non trovata. Stato progetto non valido."
    exit 1
fi

if [ -d "factory" ] && [ -d "app" ]; then
    echo "❌ Errore: trovate sia 'factory' che 'app'. Stato ambiguo, switch annullato."
    exit 1
fi

if [ -d "factory" ]; then
    echo "Spostamento main -> app (produzione) e factory -> main (test hardware)"
    if [ -e "app" ]; then
        echo "❌ Errore: esiste già 'app', impossibile rinominare 'main' in 'app'."
        exit 1
    fi
    mv main app
    mv factory main
    echo "Ambiente FACTORY pronto."
    echo ""
    echo "Per flashare sulla partizione corretta usa:"
    echo "  idf.py app-flash --partition-name factory"
    echo ""
else
    if [ -d "app" ]; then
        echo "✅ Ambiente già impostato su FACTORY (main+app presenti)."
    else
        echo "❌ Errore: cartella 'factory' non trovata e 'app' assente. Stato non riconosciuto."
        exit 1
    fi
fi
