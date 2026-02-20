#!/bin/bash

# Script per impostare build APP/PRODUZIONE nel codice unificato

set -euo pipefail

for f in app_version.h main/app_version.h; do
    if [ ! -f "$f" ]; then
        echo "❌ Errore: file '$f' non trovato."
        exit 1
    fi
    sed -i 's/^#define COMPILE_APP .*/#define COMPILE_APP 1/' "$f"
done

echo "✅ Modalità APP impostata (COMPILE_APP=1)."
