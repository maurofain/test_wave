#!/bin/bash

IP=""
FIRMWARE="build/test_wave.bin"
ENDPOINT="/ota/upload"
DRY_RUN=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    -i|--ip)
      IP="$2"
      shift 2
      ;;
    -f|--file|--ota-file)
      FIRMWARE="$2"
      shift 2
      ;;
    -e|--endpoint)
      ENDPOINT="$2"
      shift 2
      ;;
    --dry)
      DRY_RUN=true
      shift
      ;;
    -h|--help)
      echo "Utilizzo: $0 --ip <ip_dispositivo> [--file <firmware.bin>] [--endpoint /ota/upload] [--dry]"
      exit 0
      ;;
    *)
      echo "❌ Opzione non riconosciuta: $1"
      echo "Usa --help per i dettagli."
      exit 1
      ;;
  esac
done

if [[ -z "$IP" ]]; then
  echo "❌ IP dispositivo mancante. Usa --ip <ip_dispositivo>."
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOT_VER="$ROOT_DIR/app_version.h"
MAIN_VER="$ROOT_DIR/main/app_version.h"

get_compile_app() {
  local file="$1"
  awk '/^#define[[:space:]]+COMPILE_APP[[:space:]]+[01]/{print $3; exit}' "$file"
}

if [[ ! -f "$ROOT_VER" || ! -f "$MAIN_VER" ]]; then
  echo "❌ File versione non trovati (app_version.h / main/app_version.h)."
  exit 1
fi

ROOT_MODE="$(get_compile_app "$ROOT_VER")"
MAIN_MODE="$(get_compile_app "$MAIN_VER")"

if [[ -z "$ROOT_MODE" || -z "$MAIN_MODE" || "$ROOT_MODE" != "$MAIN_MODE" ]]; then
  echo "❌ COMPILE_APP incoerente tra root e main (root=$ROOT_MODE, main=$MAIN_MODE)."
  echo "   Esegui: ./scripts/switch_to_factory.sh"
  exit 1
fi

if [[ "$ROOT_MODE" != "0" ]]; then
  echo "❌ Modalità corrente APP (COMPILE_APP=$ROOT_MODE). Per FACTORY OTA imposta COMPILE_APP=0."
  echo "   Esegui: ./scripts/switch_to_factory.sh"
  exit 1
fi

if [[ "$FIRMWARE" != /* ]]; then
  FIRMWARE="$ROOT_DIR/$FIRMWARE"
fi

if [[ ! -f "$FIRMWARE" ]]; then
  echo "❌ Firmware non trovato: $FIRMWARE"
  echo "   Esegui prima: idfc -b"
  exit 1
fi

if [[ "$ENDPOINT" != /* ]]; then
  ENDPOINT="/$ENDPOINT"
fi

URL="http://$IP$ENDPOINT"
CMD=(curl --fail --show-error --silent -X POST -H "Content-Type: application/octet-stream" --data-binary "@$FIRMWARE" "$URL")

echo "🌐 Avvio flash FACTORY via OTA"
echo "   IP: $IP"
echo "   Endpoint: $ENDPOINT"
echo "   Firmware: $FIRMWARE"
echo "📝 Comando: ${CMD[*]}"
echo ""

if [[ "$DRY_RUN" == true ]]; then
  echo "🚫 Dry run: upload non eseguito"
  exit 0
fi

"${CMD[@]}"
RC=$?

if [[ $RC -eq 0 ]]; then
  echo "✅ Upload FACTORY OTA completato"
else
  echo "❌ Upload FACTORY OTA fallito (codice: $RC)"
  exit $RC
fi
