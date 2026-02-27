#!/bin/bash

# Script per impostare build APP/PRODUZIONE nel codice unificato

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

updated=0
for f in "$REPO_ROOT/main/app_version.h" "$REPO_ROOT/app_version.h"; do
    if [ -f "$f" ]; then
        sed -i 's/^#define COMPILE_APP .*/#define COMPILE_APP 1/' "$f"
        updated=1
    fi
done

if [ "$updated" -eq 0 ]; then
    echo "❌ Errore: nessun file app_version.h trovato in '$REPO_ROOT'."
    exit 1
fi

echo "✅ Modalità APP impostata (COMPILE_APP=1)."
