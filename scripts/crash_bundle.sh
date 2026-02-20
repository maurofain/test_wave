#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CRASH_ROOT="${ROOT_DIR}/crash"
BUILD_DIR="${ROOT_DIR}/build"

COREDUMP_SRC="${1:-}"

if [[ ! -f "${ROOT_DIR}/app_version.h" ]]; then
  echo "[ERR] app_version.h non trovato in ${ROOT_DIR}" >&2
  exit 1
fi

APP_VERSION="$(grep -E '^#define[[:space:]]+APP_VERSION[[:space:]]+"' "${ROOT_DIR}/app_version.h" | head -n1 | sed -E 's/.*"([^"]+)".*/\1/')"
APP_DATE="$(grep -E '^#define[[:space:]]+APP_DATE[[:space:]]+"' "${ROOT_DIR}/app_version.h" | head -n1 | sed -E 's/.*"([^"]+)".*/\1/')"
COMPILE_APP_VAL="$(grep -E '^#define[[:space:]]+COMPILE_APP' "${ROOT_DIR}/app_version.h" | head -n1 | awk '{print $3}')"
MODE="APP"
if [[ "${COMPILE_APP_VAL}" == "0" ]]; then
  MODE="FACTORY"
fi

STAMP="$(date +%Y%m%d_%H%M%S)"
DEST_DIR="${CRASH_ROOT}/v${APP_VERSION}_${MODE}_${STAMP}"
mkdir -p "${DEST_DIR}/build" "${DEST_DIR}/meta" "${DEST_DIR}/config"

copy_if_exists() {
  local src="$1"
  local dst="$2"
  if [[ -f "${src}" ]]; then
    mkdir -p "$(dirname "${dst}")"
    cp -f "${src}" "${dst}"
    echo "[OK] $(realpath --relative-to="${ROOT_DIR}" "${src}")"
  else
    echo "[WARN] file mancante: ${src}" >&2
  fi
}

echo "[INFO] Creo bundle crash in: ${DEST_DIR}"

# File principali per simbolizzazione coredump
copy_if_exists "${BUILD_DIR}/test_wave.elf" "${DEST_DIR}/build/test_wave.elf"
copy_if_exists "${BUILD_DIR}/test_wave.bin" "${DEST_DIR}/build/test_wave.bin"
copy_if_exists "${BUILD_DIR}/test_wave.map" "${DEST_DIR}/build/test_wave.map"
copy_if_exists "${BUILD_DIR}/bootloader/bootloader.elf" "${DEST_DIR}/build/bootloader/bootloader.elf"
copy_if_exists "${BUILD_DIR}/bootloader/bootloader.bin" "${DEST_DIR}/build/bootloader/bootloader.bin"
copy_if_exists "${BUILD_DIR}/partition_table/partition-table.bin" "${DEST_DIR}/build/partition_table/partition-table.bin"
copy_if_exists "${BUILD_DIR}/ota_data_initial.bin" "${DEST_DIR}/build/ota_data_initial.bin"
copy_if_exists "${BUILD_DIR}/storage.bin" "${DEST_DIR}/build/storage.bin"
copy_if_exists "${BUILD_DIR}/project_description.json" "${DEST_DIR}/build/project_description.json"
copy_if_exists "${BUILD_DIR}/flasher_args.json" "${DEST_DIR}/build/flasher_args.json"

# Argomenti di flash (usati da esptool/idf.py). I path sono relativi a build/.
copy_if_exists "${BUILD_DIR}/flash_args" "${DEST_DIR}/build/flash_args"
copy_if_exists "${BUILD_DIR}/flash_project_args" "${DEST_DIR}/build/flash_project_args"
copy_if_exists "${BUILD_DIR}/flash_app_args" "${DEST_DIR}/build/flash_app_args"
copy_if_exists "${BUILD_DIR}/flash_bootloader_args" "${DEST_DIR}/build/flash_bootloader_args"
copy_if_exists "${BUILD_DIR}/app-flash_args" "${DEST_DIR}/build/app-flash_args"
copy_if_exists "${BUILD_DIR}/bootloader-flash_args" "${DEST_DIR}/build/bootloader-flash_args"
copy_if_exists "${BUILD_DIR}/partition-table-flash_args" "${DEST_DIR}/build/partition-table-flash_args"
copy_if_exists "${BUILD_DIR}/otadata-flash_args" "${DEST_DIR}/build/otadata-flash_args"
copy_if_exists "${BUILD_DIR}/storage-flash_args" "${DEST_DIR}/build/storage-flash_args"

# Config/versioning
copy_if_exists "${ROOT_DIR}/sdkconfig" "${DEST_DIR}/config/sdkconfig"
copy_if_exists "${ROOT_DIR}/sdkconfig.defaults" "${DEST_DIR}/config/sdkconfig.defaults"
copy_if_exists "${ROOT_DIR}/partitions.csv" "${DEST_DIR}/config/partitions.csv"
copy_if_exists "${ROOT_DIR}/partition_table/partitionTable.csv" "${DEST_DIR}/config/partitionTable.csv"
copy_if_exists "${ROOT_DIR}/app_version.h" "${DEST_DIR}/config/app_version.h"
copy_if_exists "${ROOT_DIR}/main/app_version.h" "${DEST_DIR}/config/main_app_version.h"
copy_if_exists "${ROOT_DIR}/docs/COMPILE_FLAGS.md" "${DEST_DIR}/config/COMPILE_FLAGS.md"
copy_if_exists "${ROOT_DIR}/docs/CRASH_ANALYSIS.md" "${DEST_DIR}/config/CRASH_ANALYSIS.md"

# Script per flash diretto dal bundle
cat > "${DEST_DIR}/flash_from_bundle.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
Uso:
  ./flash_from_bundle.sh [-p /dev/ttyPORT] [-b BAUD] [-m] [--all]

Opzioni:
  -p    Porta seriale (default: /dev/ttyACM0)
  -b    Baudrate (default: 2000000)
  -m    Avvia anche monitor (idf.py monitor -b 115200)
  --all Flash completo (bootloader + partition table + app + ota_data + storage)

Default:
  Flash solo APP (offset 0x10000) usando build/flash_app_args
USAGE
}

PORT="/dev/ttyACM0"
BAUD="2000000"
MONITOR=false
FLASH_MODE="app"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p)
      PORT="${2:-}"; shift 2 ;;
    -b)
      BAUD="${2:-}"; shift 2 ;;
    -m)
      MONITOR=true; shift ;;
    --all)
      FLASH_MODE="all"; shift ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "[ERR] opzione non valida: $1" >&2
      usage
      exit 1
      ;;
  esac
done

BUNDLE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${BUNDLE_DIR}"

ARGS_FILE="build/flash_app_args"
if [[ "${FLASH_MODE}" == "all" ]]; then
  ARGS_FILE="build/flash_args"
fi

if [[ ! -f "${ARGS_FILE}" ]]; then
  echo "[ERR] File args non trovato: ${ARGS_FILE}" >&2
  exit 1
fi

echo "🚀 Flash da bundle: ${BUNDLE_DIR}"
echo "   Porta: ${PORT}"
echo "   Baud:  ${BAUD}"
echo "   Mode:  ${FLASH_MODE}"

python3 -m esptool --chip esp32p4 -p "${PORT}" -b "${BAUD}" \
  --before default_reset --after hard_reset \
  write_flash --force "@${ARGS_FILE}"

echo "✅ Flash completato"

if [[ "${MONITOR}" == "true" ]]; then
  if [[ -z "${IDF_PATH:-}" ]] && [[ -f "${HOME}/esp/esp-idf/export.sh" ]]; then
    . "${HOME}/esp/esp-idf/export.sh" >/dev/null 2>&1 || true
  fi
  echo "🖥️  Avvio monitor (115200 baud)..."
  idf.py monitor -p "${PORT}" -b 115200
fi
EOF
chmod +x "${DEST_DIR}/flash_from_bundle.sh"

# Coredump opzionale passato come argomento
if [[ -n "${COREDUMP_SRC}" ]]; then
  if [[ -f "${COREDUMP_SRC}" ]]; then
    cp -f "${COREDUMP_SRC}" "${DEST_DIR}/$(basename "${COREDUMP_SRC}")"
    echo "[OK] coredump copiato: ${COREDUMP_SRC}"
  else
    echo "[WARN] coredump specificato ma non trovato: ${COREDUMP_SRC}" >&2
  fi
fi

# Metadata git
GIT_COMMIT="N/A"
GIT_BRANCH="N/A"
GIT_DESCRIBE="N/A"
if git -C "${ROOT_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  GIT_COMMIT="$(git -C "${ROOT_DIR}" rev-parse HEAD || true)"
  GIT_BRANCH="$(git -C "${ROOT_DIR}" rev-parse --abbrev-ref HEAD || true)"
  GIT_DESCRIBE="$(git -C "${ROOT_DIR}" describe --always --dirty --tags 2>/dev/null || true)"
fi

cat > "${DEST_DIR}/meta/manifest.txt" <<EOF
bundle_created_at=${STAMP}
mode=${MODE}
app_version=${APP_VERSION}
app_date=${APP_DATE}
git_branch=${GIT_BRANCH}
git_commit=${GIT_COMMIT}
git_describe=${GIT_DESCRIBE}

decode_command=source ~/esp/esp-idf/export.sh && idf.py -C ${ROOT_DIR} coredump-info -c <coredump.elf>

flash_app_command=./flash_from_bundle.sh -p /dev/ttyACM0
flash_all_command=./flash_from_bundle.sh -p /dev/ttyACM0 --all
EOF

echo "[DONE] Bundle crash pronto: ${DEST_DIR}"
echo "[NEXT] Commit consigliato: git add crash && git commit -m \"v${APP_VERSION} crash bundle\""
