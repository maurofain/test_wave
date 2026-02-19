#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="$(cd "${SRC_DIR}/.." && pwd)/mh1001"

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

EXCLUDE_FILE="$(mktemp)"
cleanup() {
  rm -f "${EXCLUDE_FILE}"
}
trap cleanup EXIT

cat > "${EXCLUDE_FILE}" <<'EOF'
.git/
mh1001/
docs/
scripts/
index.html
log/
logs/
*.log
**/*.log
EOF

if command -v git >/dev/null 2>&1 && git -C "${SRC_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git -C "${SRC_DIR}" ls-files --others --ignored --exclude-standard --directory >> "${EXCLUDE_FILE}" || true
fi

RSYNC_FLAGS=(-a --delete --prune-empty-dirs)
if [[ ${DRY_RUN} -eq 1 ]]; then
  RSYNC_FLAGS+=(--dry-run --itemize-changes)
fi

rsync "${RSYNC_FLAGS[@]}" \
  --include='docs/' \
  --include='docs/doxygen/' \
  --include='docs/doxygen/***' \
  --exclude-from="${EXCLUDE_FILE}" \
  "${SRC_DIR}/" "${DEST_DIR}/"

if [[ ${DRY_RUN} -eq 1 ]]; then
  echo "Dry-run completato: ${SRC_DIR} -> ${DEST_DIR}"
else
  echo "Sync completata: ${SRC_DIR} -> ${DEST_DIR}"
fi
