#!/usr/bin/env bash
# Convert image icons in data/icons into embedded C resources for LVGL
# Usage: ./scripts/embed_icons.sh
# Requires: xxd (for -i) and optional image conversion if needed for LVGL

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ICONS_DIR="$ROOT/data/icons"
OUT_SRC="$ROOT/components/lvgl_panel/src/embedded_icons.c"
OUT_HEADER="$ROOT/components/lvgl_panel/include/embedded_icons_images.h"

mkdir -p "$(dirname "$OUT_SRC")"
mkdir -p "$(dirname "$OUT_HEADER")"

# Map names -> indices expected by code
declare -A MAP
MAP[CloudOk.png]=0
MAP[CloudKo.png]=0
MAP[CreditCardOk.png]=1
MAP[CreditCardKo.png]=1
MAP[MoneteOk3.png]=2
MAP[MoneteKo.png]=2
MAP[QrOk.png]=3
MAP[QrKo.png]=3

# Start output files
cat > "$OUT_HEADER" <<'EOF'
// Generated header: embedded icon arrays
#ifndef EMBEDDED_ICONS_IMAGES_H
#define EMBEDDED_ICONS_IMAGES_H

#include <stdint.h>

EOF

# We'll build a C file that defines arrays and a lookup function
cat > "$OUT_SRC" <<'EOF'
#include "embedded_icons.h"
#include "embedded_icons_images.h"

// Return embedded image pointer by index, or NULL
const void *get_embedded_icon_src(int index)
{
    switch(index) {
EOF

for f in "$ICONS_DIR"/*.{png,jpg,jpeg}; do
    [ -e "$f" ] || continue
    name=$(basename "$f")
    idx=${MAP[$name]:-}
    if [ -z "$idx" ]; then
        echo "Skipping $name (no mapping)"
        continue
    fi
    varname=$(echo "$name" | sed 's/[^a-zA-Z0-9]/_/g')
    # use xxd -i to dump array
    echo "Generating $name -> var $varname (idx $idx)"
    xxd -i "$f" > /tmp/icon_${varname}.i.c
    # extract array name and length using awk to avoid regex bracket issues
    arr_name=$(awk '/unsigned char/ {print $3; exit}' /tmp/icon_${varname}.i.c)
    len_name=$(awk '/_len/ {print $3; exit}' /tmp/icon_${varname}.i.c)
    # append to header
    sed -n '1,200p' /tmp/icon_${varname}.i.c >> "$OUT_HEADER"
    echo >> "$OUT_HEADER"
    # append to src switch
    cat >> "$OUT_SRC" <<EOF
        case $idx: return (const void *)${arr_name};
EOF
    rm /tmp/icon_${varname}.i.c
done

cat >> "$OUT_SRC" <<'EOF'
        default: return NULL;
    }
}
EOF

cat >> "$OUT_HEADER" <<'EOF'
#endif // EMBEDDED_ICONS_IMAGES_H
EOF

echo "Embedded icons generated to: $OUT_SRC and $OUT_HEADER"

# Note: Running this script will generate C arrays that must be compiled into the firmware.
# After running, re-run idf.py build.

exit 0
