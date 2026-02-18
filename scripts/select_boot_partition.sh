#!/bin/bash
# Script per selezionare la partizione di boot
# Utilizzo:
#   ./scripts/select_boot_partition.sh -f [-p /dev/ttyPORT] [-b BAUD]
#   ./scripts/select_boot_partition.sh -0 [-p /dev/ttyPORT] [-b BAUD]
#   ./scripts/select_boot_partition.sh -1 [-p /dev/ttyPORT] [-b BAUD]

set -e

PORT="/dev/ttyACM0"
BAUD="2000000"
TARGET=""

usage() {
  echo "Utilizzo: $0 (-f | -0 | -1) [-p port] [-b baud]" >&2
  echo "  -f   Imposta boot su FACTORY (cancellando otadata)" >&2
  echo "  -0   Imposta boot su OTA_0" >&2
  echo "  -1   Imposta boot su OTA_1" >&2
  echo "  -p   Porta seriale (default: /dev/ttyACM0)" >&2
  echo "  -b   Baud rate (default: 2000000)" >&2
}

while getopts "f01p:b:" opt; do
  case "$opt" in
    f)
      [ -n "$TARGET" ] && { usage; exit 1; }
      TARGET="factory"
      ;;
    0)
      [ -n "$TARGET" ] && { usage; exit 1; }
      TARGET="ota0"
      ;;
    1)
      [ -n "$TARGET" ] && { usage; exit 1; }
      TARGET="ota1"
      ;;
    p)
      PORT="$OPTARG"
      ;;
    b)
      BAUD="$OPTARG"
      ;;
    *)
      usage
      exit 1
      ;;
  esac
done

if [ -z "$TARGET" ]; then
  usage
  exit 1
fi

if [ -z "$IDF_PATH" ] && [ -f "$HOME/esp/esp-idf/export.sh" ]; then
  . "$HOME/esp/esp-idf/export.sh" >/dev/null 2>&1
fi

OTATOOL="$IDF_PATH/components/app_update/otatool.py"
if [ -z "$IDF_PATH" ] || [ ! -f "$OTATOOL" ]; then
  echo "❌ Impossibile trovare otatool.py (IDF_PATH non configurato)."
  exit 1
fi

case "$TARGET" in
  factory)
    CMD=(python3 "$OTATOOL" --port "$PORT" --baud "$BAUD" --partition-table-file partitions.csv erase_otadata)
    MSG="FACTORY"
    ;;
  ota0)
    CMD=(python3 "$OTATOOL" --port "$PORT" --baud "$BAUD" --partition-table-file partitions.csv switch_ota_partition --slot 0)
    MSG="OTA_0"
    ;;
  ota1)
    CMD=(python3 "$OTATOOL" --port "$PORT" --baud "$BAUD" --partition-table-file partitions.csv switch_ota_partition --slot 1)
    MSG="OTA_1"
    ;;
esac

echo "🔁 Imposto il prossimo boot su $MSG..."
echo "📝 Comando: ${CMD[*]}"
"${CMD[@]}"

echo "✅ Boot impostato su $MSG."
