#!/bin/bash
# Script per il flash della sola partizione FACTORY (modalità COMPILE_APP=0)
# Utilizzo: ./scripts/flash_factory.sh [-p /dev/ttyPORT] [-b BAUD] [-m]

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

if [ ! -f "$MAIN_VER" ]; then
  echo "❌ File versione non trovato: main/app_version.h"
  exit 1
fi

MAIN_MODE="$(get_compile_app "$MAIN_VER")"
ROOT_MODE=""
if [ -f "$ROOT_VER" ]; then
  ROOT_MODE="$(get_compile_app "$ROOT_VER")"
fi

if [ -z "$MAIN_MODE" ]; then
  echo "❌ COMPILE_APP non trovato in main/app_version.h."
  exit 1
fi

if [ -n "$ROOT_MODE" ] && [ "$ROOT_MODE" != "$MAIN_MODE" ]; then
  echo "❌ COMPILE_APP incoerente tra root e main (root=$ROOT_MODE, main=$MAIN_MODE)."
  echo "   Esegui: ./scripts/switch_to_factory.sh"
  exit 1
fi

if [ "$MAIN_MODE" != "0" ]; then
  echo "❌ Modalità corrente APP (COMPILE_APP=$MAIN_MODE). Per flash FACTORY imposta COMPILE_APP=0."
  echo "   Esegui: ./scripts/switch_to_factory.sh"
  exit 1
fi

CMD="python3 -m esptool --chip esp32p4 -p $PORT -b $BAUD --before default_reset --after hard_reset write_flash --force 0x10000 build/test_wave.bin"

echo "🚀 Avvio flash partizione FACTORY"
echo "   Porta: $PORT"
echo "   Baud: $BAUD"
echo "   Offset: 0x10000"
echo "   Immagine: build/test_wave.bin"
echo "📝 Comando: $CMD"
echo ""

eval $CMD

if [ $? -eq 0 ]; then
  echo "✅ Flash FACTORY completato"
  BOOT_SCRIPT="$(dirname "$0")/select_boot_partition.sh"
  if [ -x "$BOOT_SCRIPT" ]; then
    echo "🔁 Imposto il prossimo boot su FACTORY..."
    "$BOOT_SCRIPT" -f -p "$PORT" -b "$BAUD"
  else
    echo "⚠️  Script boot selector non trovato o non eseguibile: $BOOT_SCRIPT"
    echo "   Esegui manualmente: ./scripts/select_boot_partition.sh -f -p $PORT -b $BAUD"
  fi

    if [ "$MONITOR" = true ]; then
        echo "🖥️  Avvio monitor (115200 baud)..."
        idf.py monitor -p "$PORT" -b 115200
    fi
else
  echo "❌ Errore durante il flash FACTORY"
    exit 1
fi
