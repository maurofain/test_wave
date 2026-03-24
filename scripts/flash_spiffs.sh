#!/bin/bash
# Script per il flash della sola partizione SPIFFS (Storage)
# Utilizzo: ./scripts/flash_spiffs.sh [-p /dev/ttyPORT] [-b BAUD] [-m]

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

CMD="python3 -m esptool --chip esp32p4 -p $PORT -b $BAUD --before default_reset --after hard_reset write_flash --force 0x940000 build/storage.bin"

echo "🚀 Avvio flash partizione SPIFFS (storage) su $PORT a $BAUD baud..."
echo "📝 Comando: $CMD"
echo ""

eval $CMD

if [ $? -eq 0 ]; then
    echo "✅ Flash SPIFFS completato!"
    if [ "$MONITOR" = true ]; then
        echo "🖥️  Avvio monitor (115200 baud)..."
        idf.py monitor -p "$PORT" -b 115200
    fi
else
    echo "❌ Errore durante il flash."
    exit 1
fi
