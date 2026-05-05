#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="$(cd "${SRC_DIR}/.." && pwd)/mh1001/ESP32P4"

DRY_RUN=0
if [[ "${1:-}" == "--dry-run" || "${1:-}" == "-n" ]]; then
  DRY_RUN=1
elif [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  echo "Uso: $(basename "$0") [--dry-run|-n]"
  echo "  --dry-run, -n   Mostra cosa verrebbe sincronizzato senza copiare"
  exit 0
elif [[ $# -gt 0 ]]; then
  echo "Argomento non riconosciuto: $1" >&2
  echo "Usa --help per la sintassi." >&2
  exit 1
fi

mkdir -p "${DEST_DIR}"

RSYNC_FLAGS=(-a --delete --prune-empty-dirs --stats)
# include statistics so we can report total files and transferred/updated files
if [[ ${DRY_RUN} -eq 1 ]]; then
  RSYNC_FLAGS+=(--dry-run --itemize-changes)
fi

# run rsync and capture its output so that we can show the stats later
RSYNC_OUTPUT=$(
  rsync "${RSYNC_FLAGS[@]}" \
    --include='docs/' \
    --include='docs/**/' \
    --include='docs/README.md' \
    --include='docs/a_*.md' \
    --exclude='docs/Internal/***' \
    --exclude='docs/Backup/***' \
    --include='docs/**/a_*' \
    --exclude='docs/***' \
    --include='data/' \
    --include='data/***' \
    --include='main/' \
    --include='main/***' \
    --include='partition_table/' \
    --include='partition_table/***' \
    --exclude='.git' \
    --exclude='mh1001' \
    --exclude='report' \
    --exclude='versions' \
    --exclude='.github' \
    --exclude='.windsurf' \
    --exclude='crash' \
    --exclude='scripts' \
    --exclude='doxygen/markdown/doxygen' \
    --exclude='index.html' \
    --exclude='log' \
    --exclude='logs' \
    --exclude='*.log' \
    --exclude='**/*.log' \
    "${SRC_DIR}/" "${DEST_DIR}/" 2>&1
)

# print rsync output (includes stats)
echo "$RSYNC_OUTPUT"

if [[ ${DRY_RUN} -eq 1 ]]; then
  echo "Dry-run completato: ${SRC_DIR} -> ${DEST_DIR}"
else
  # extract summary lines from the stats (Number of files and transferred)
  echo
  echo "--- report finale ---"
  echo "$RSYNC_OUTPUT" | grep -E 'Number of files|Number of files transferred' || true
  echo
  echo "Sync completata: ${SRC_DIR} -> ${DEST_DIR}"
fi
