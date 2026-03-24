#!/bin/bash
# Script per il flash della sola partizione OTA_1 (offset da partitions.csv)
# Utilizzo: ./scripts/flash_ota1.sh [-p /dev/ttyPORT] [-b BAUD] [-m]

PORT="/dev/ttyACM0"
BAUD="2000000"
MONITOR=false

while getopts "p:b:m" opt; do
  case $opt in
    p) PORT="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
    m) MONITOR=true ;;
    \?) echo "Utilizzo: $0 [-p port] [-b baud] [-m]" >&2; exit 1 ;;
  esac
done

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOT_VER="$ROOT_DIR/app_version.h"
MAIN_VER="$ROOT_DIR/main/app_version.h"

get_compile_app() {
  local file="$1"
  awk '/^#define[[:space:]]+COMPILE_APP[[:space:]]+[01]/{print $3; exit}' "$file"
}

if [ ! -f "$ROOT_VER" ] || [ ! -f "$MAIN_VER" ]; then
  echo "❌ File versione non trovati (app_version.h / main/app_version.h)."
  exit 1
fi

ROOT_MODE="$(get_compile_app "$ROOT_VER")"
MAIN_MODE="$(get_compile_app "$MAIN_VER")"

if [ -z "$ROOT_MODE" ] || [ -z "$MAIN_MODE" ] || [ "$ROOT_MODE" != "$MAIN_MODE" ]; then
  echo "❌ COMPILE_APP incoerente tra root e main (root=$ROOT_MODE, main=$MAIN_MODE)."
  echo "   Esegui: ./scripts/switch_to_production.sh"
  exit 1
fi

if [ "$ROOT_MODE" != "1" ]; then
  echo "❌ Modalità corrente FACTORY (COMPILE_APP=$ROOT_MODE). Per flash OTA_1 imposta COMPILE_APP=1."
  echo "   Esegui: ./scripts/switch_to_production.sh"
  exit 1
fi

CMD="python3 -m esptool --chip esp32p4 -p $PORT -b $BAUD --before default_reset --after hard_reset write_flash --force 0x610000 build/test_wave.bin"

echo "🚀 Avvio flash partizione OTA_1"
echo "   Porta: $PORT"
echo "   Baud: $BAUD"
echo "   Offset: 0x610000"
echo "   Size: 0x300000 (3 MB)"
echo "   Immagine: build/test_wave.bin"
echo "📝 Comando: $CMD"
echo ""

eval $CMD

if [ $? -eq 0 ]; then
  echo "✅ Flash OTA_1 completato"
  if [ -z "$IDF_PATH" ] && [ -f "$HOME/esp/esp-idf/export.sh" ]; then
    . "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1
  fi

  OTATOOL="$IDF_PATH/components/app_update/otatool.py"
  if [ -z "$IDF_PATH" ] || [ ! -f "$OTATOOL" ]; then
    echo "❌ Impossibile trovare otatool.py (IDF_PATH non configurato)."
    exit 1
  fi

  BOOT_CMD="python3 $OTATOOL --port $PORT --baud $BAUD --partition-table-file partitions.csv switch_ota_partition --slot 1"
  echo "🔁 Imposto il prossimo boot su OTA_1..."
  echo "📝 Comando: $BOOT_CMD"
  eval $BOOT_CMD
  if [ $? -ne 0 ]; then
    echo "❌ Errore durante l'impostazione della partizione di boot OTA_1."
    exit 1
  fi
  echo "✅ Boot impostato su OTA_1."

    if [ "$MONITOR" = true ]; then
        echo "🖥️  Avvio monitor (115200 baud)..."
        idf.py monitor -p "$PORT" -b 115200
    fi
else
  echo "❌ Errore durante il flash OTA_1"
    exit 1
fi