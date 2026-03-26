#!/usr/bin/env bash
set -euo pipefail

# scripts/generate_doxygen.sh
# Rigenera la documentazione Doxygen (con grafi via Graphviz) e copia i file .md
# Uso: ./scripts/generate_doxygen.sh

WORKDIR="$(cd "$(dirname "$0")/.." && pwd)"
DOXYFILE="$WORKDIR/Doxyfile"
OUTDIR="$WORKDIR/doxygen"
MD_OUTDIR="$OUTDIR/markdown"
REL_OUTDIR="${OUTDIR#${WORKDIR}/}"

command -v doxygen >/dev/null 2>&1 || { echo "doxygen non trovato nel PATH. Installa doxygen e riprova." >&2; exit 1; }
command -v dot >/dev/null 2>&1 || {
  echo "WARNING: graphviz 'dot' non trovato. I grafi non verranno generati." >&2
  echo "Per avere i grafi installa 'graphviz' (es. apt install graphviz)." >&2
}

echo "Working dir: $WORKDIR"
echo "Using Doxyfile: $DOXYFILE"

# Assicura che l'output precedente sia rimosso per avere una rigenerazione pulita
if [ -d "$OUTDIR" ]; then
  echo "Pulisco output precedente: $OUTDIR"
  rm -rf "$OUTDIR"
fi

mkdir -p "$OUTDIR"

# Esegui Doxygen
echo "Eseguo: doxygen $DOXYFILE"
doxygen "$DOXYFILE"

# Copia solo i documenti approvati docs/**/a_*.md nella cartella markdown di output
mkdir -p "$MD_OUTDIR"
echo "Copio solo docs/**/a_*.md in $MD_OUTDIR"

# Protezione anti-ricorsione: l'output e' sotto WORKDIR, quindi deve essere escluso esplicitamente.
if [[ "$REL_OUTDIR" == "$OUTDIR" || -z "$REL_OUTDIR" ]]; then
  echo "ERRORE: impossibile determinare REL_OUTDIR per escludere la directory di output" >&2
  exit 1
fi

# include esclusivamente documenti approvati a_*.md dalla cartella docs
rsync -av --prune-empty-dirs \
  --exclude='doxygen/***' \
  --exclude='docs/doxygen/***' \
  --exclude='mh1001/***' \
  --exclude='**/node_modules/**' \
  --include='docs/' \
  --include='docs/**/' \
  --include='docs/a_*.md' \
  --exclude='docs/Internal/***' \
  --exclude='docs/Backup/***' \
  --include='docs/**/a_*.md' \
  --exclude='docs/***' \
  --exclude='*' \
  "$WORKDIR/" "$MD_OUTDIR/"

# Mostra riepilogo dimensioni output e numero di file generati
echo
echo "--- Riepilogo generazione Doxygen ---"
if [ -d "$OUTDIR" ]; then
  du -sh "$OUTDIR" || true
  echo "File HTML generati (top 10 per dimensione):"
  find "$OUTDIR" -type f -printf '%s %p\n' | sort -rn | head -n 10
else
  echo "Output non trovato: $OUTDIR"
fi

echo
echo "Documentazione rigenerata. HTML in: $OUTDIR/html (se abilitato)"
echo "File docs/a_*.md copiati in: $MD_OUTDIR"

echo "Nota: se vuoi convertire l'output Doxygen in Markdown (file .md di output), usa strumenti come doxygen-to-markdown o pandoc su \"$OUTDIR/xml\" (se abilitato GENERATE_XML)."

exit 0
