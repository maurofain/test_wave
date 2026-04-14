#!/usr/bin/env bash
# Genera le icone embed per il chrome LVGL a partire da docs/icone/normalized.
# Usage: ./scripts/embed_icons.sh
# Requires: xxd

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ICONS_DIR="$ROOT/docs/icone/normalized"
OUT_SRC="$ROOT/components/lvgl_panel/src/embedded_icons.c"
OUT_HEADER="$ROOT/components/lvgl_panel/include/embedded_icons_images.h"
TMP_DIR="$(mktemp -d)"

trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$(dirname "$OUT_SRC")"
mkdir -p "$(dirname "$OUT_HEADER")"

ICON_SPECS=(
    "0|cloud|CloudKo.png|CloudOk.png"
    "1|credit|CreditCardKo.png|CreditCardOk.png"
    "2|coin|MoneteKo.png|MoneteOk3.png"
    "3|qr|QrKo.png|QrOk.png"
)

declare -A KO_SYMBOLS
declare -A OK_SYMBOLS

dump_icon_to_header() {
    local file_path="$1"
    local relative_path="${file_path#$ROOT/}"
    local temp_file="$TMP_DIR/$(basename "$file_path").xxd"
    local symbol_name

    (cd "$ROOT" && xxd -i "$relative_path") > "$temp_file"
    cat "$temp_file" >> "$OUT_HEADER"
    printf '\n' >> "$OUT_HEADER"

    symbol_name="$(awk '/^unsigned char / {gsub(/\[\]/, "", $3); print $3; exit}' "$temp_file")"
    if [[ -z "$symbol_name" ]]; then
        echo "Errore: simbolo non rilevato per $relative_path" >&2
        exit 1
    fi

    printf '%s\n' "$symbol_name"
}

cat > "$OUT_HEADER" <<'EOF'
// Generated header: embedded icon arrays
#ifndef EMBEDDED_ICONS_IMAGES_H
#define EMBEDDED_ICONS_IMAGES_H

#include <stdint.h>

EOF

cat > "$OUT_SRC" <<'EOF'
#include "embedded_icons.h"
#include "embedded_icons_images.h"

#include "lvgl.h"

#include <stddef.h>

#define DECLARE_EMBEDDED_ICON_DSC(name, map_symbol, width_px, height_px) \
    static const lv_image_dsc_t name = {                               \
        .header.magic = LV_IMAGE_HEADER_MAGIC,                         \
        .header.cf = LV_COLOR_FORMAT_RAW_ALPHA,                        \
        .header.flags = 0,                                             \
        .header.w = (width_px),                                        \
        .header.h = (height_px),                                       \
        .header.stride = 0,                                            \
        .data_size = sizeof(map_symbol),                               \
        .data = (const uint8_t *)(map_symbol),                         \
        .reserved = NULL,                                              \
    }

EOF

for spec in "${ICON_SPECS[@]}"; do
    IFS='|' read -r idx alias ko_file ok_file <<< "$spec"
    ko_path="$ICONS_DIR/$ko_file"
    ok_path="$ICONS_DIR/$ok_file"

    if [[ ! -f "$ko_path" ]]; then
        echo "Errore: file KO mancante: $ko_path" >&2
        exit 1
    fi
    if [[ ! -f "$ok_path" ]]; then
        echo "Errore: file OK mancante: $ok_path" >&2
        exit 1
    fi

    echo "Generating $ko_file / $ok_file (idx $idx)"
    KO_SYMBOLS[$alias]="$(dump_icon_to_header "$ko_path")"
    OK_SYMBOLS[$alias]="$(dump_icon_to_header "$ok_path")"

    cat >> "$OUT_SRC" <<EOF
DECLARE_EMBEDDED_ICON_DSC(s_icon_${alias}_ko, ${KO_SYMBOLS[$alias]}, 48, 48);
DECLARE_EMBEDDED_ICON_DSC(s_icon_${alias}_ok, ${OK_SYMBOLS[$alias]}, 48, 48);

EOF
done

cat >> "$OUT_SRC" <<'EOF'
const void *get_embedded_icon_src(int index, bool ok)
{
    switch (index) {
EOF

for spec in "${ICON_SPECS[@]}"; do
    IFS='|' read -r idx alias ko_file ok_file <<< "$spec"
    cat >> "$OUT_SRC" <<EOF
        case $idx:
            return ok ? (const void *)&s_icon_${alias}_ok : (const void *)&s_icon_${alias}_ko;
EOF
done

cat >> "$OUT_SRC" <<'EOF'
        default:
            return NULL;
    }
}
EOF

cat >> "$OUT_HEADER" <<'EOF'
#endif // EMBEDDED_ICONS_IMAGES_H
EOF

echo "Embedded icons generated to: $OUT_SRC and $OUT_HEADER"
