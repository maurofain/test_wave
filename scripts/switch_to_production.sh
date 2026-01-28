#!/bin/bash

# Script per commutare l'ambiente sullo sviluppo PRODUZIONE
# Rinomina le cartelle per rendere 'app' il componente principale

if [ -d "app" ]; then
    echo "Spostamento main -> factory (test hardware) e app -> main (produzione)"
    mv main factory
    mv app main
    echo "Ambiente PRODUZIONE pronto."
    echo ""
    echo "Per flashare sulla partizione corretta usa:"
    echo "  idf.py app-flash --partition-name ota_0"
    echo ""
else
    echo "L'ambiente è già impostato su PRODUZIONE o la cartella 'app' non esiste."
fi
