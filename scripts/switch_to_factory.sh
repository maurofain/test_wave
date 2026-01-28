#!/bin/bash

# Script per commutare l'ambiente sullo sviluppo FACTORY
# Rinomina le cartelle per rendere 'factory' il componente principale

if [ -d "factory" ]; then
    echo "Spostamento main -> app (produzione) e factory -> main (test hardware)"
    mv main app
    mv factory main
    echo "Ambiente FACTORY pronto."
    echo ""
    echo "Per flashare sulla partizione corretta usa:"
    echo "  idf.py app-flash --partition-name factory"
    echo ""
else
    echo "L'ambiente è già impostato su FACTORY o la cartella 'factory' non esiste."
fi
