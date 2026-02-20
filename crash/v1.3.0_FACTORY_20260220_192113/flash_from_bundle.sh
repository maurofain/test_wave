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
