#!/usr/bin/env python3
"""Generate components/lvgl_panel/language_flags.c from PNG files.

Output format: LV_COLOR_FORMAT_RGB565A8 (LVGL v9)
  - Layout    : planar - tutti i pixel RGB565 (little-endian) poi tutti gli alpha
  - Byte order: little-endian (low byte prima, high byte secondo)
  - Stride    : FLAG_W * 2  (solo piano colore)
  - Total bytes: FLAG_W * FLAG_H * 3
"""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Dict, List, Tuple

from PIL import Image


FLAG_FILES: Dict[str, str] = {
    "it": "flag_IT.png",
    "en": "flag_GB.png",
    "de": "flag_DE.png",
    "fr": "flag_FR.png",
    "es": "flag_ES.png",
}


def log(msg: str) -> None:
    print(msg, flush=True)


def emit_rgb565a8_array(name: str, img_path: Path, size: Tuple[int, int]) -> str:
    """Encode image as LV_COLOR_FORMAT_RGB565A8 (planar, little-endian RGB565).

    Layout:
        bytes [0 .. W*H*2-1]  : RGB565 pixels, little-endian (low byte prima)
        bytes [W*H*2 .. end]  : alpha bytes, uno per pixel
    """
    w, h = size
    log(f"[IMG] Loading {img_path}")
    img = Image.open(img_path).convert("RGBA")
    if img.size != (w, h):
        log(f"[IMG] Resizing {img_path.name}: {img.size} -> {(w, h)}")
        img = img.resize((w, h), Image.Resampling.LANCZOS)

    color_plane: List[int] = []
    alpha_plane: List[int] = []

    for r, g, b, a in img.getdata():
        rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        color_plane.append(rgb565 & 0xFF)          # low byte  (little-endian, primo)
        color_plane.append((rgb565 >> 8) & 0xFF)   # high byte (little-endian, secondo)
        alpha_plane.append(a)

    data = color_plane + alpha_plane
    log(f"[IMG] {img_path.name}: pixels={w*h}, color={len(color_plane)}B, alpha={len(alpha_plane)}B, tot={len(data)}B")

    lines: List[str] = []
    lines.append(
        f"static const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST uint8_t {name}[FLAG_W * FLAG_H * 3] = {{"
    )
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{v:02X}" for v in data[i : i + 16])
        lines.append(f"    {chunk},")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def build_file_text(data_dir: Path, size: Tuple[int, int]) -> str:
    w, h = size

    arrays: Dict[str, str] = {}
    for lang, filename in FLAG_FILES.items():
        img_path = data_dir / filename
        if not img_path.exists():
            raise FileNotFoundError(f"Missing image file: {img_path}")
        arrays[lang] = emit_rgb565a8_array(f"s_flag_{lang}_map", img_path, size)

    lines: List[str] = []
    lines.append('#include "language_flags.h"')
    lines.append("")
    lines.append("#include <string.h>")
    lines.append("")
    lines.append(f"#define FLAG_W      {w}")
    lines.append(f"#define FLAG_H      {h}")
    lines.append("#define FLAG_STRIDE (FLAG_W * 2)  /* solo piano colore, RGB565 */")
    lines.append("")

    for lang in ("it", "en", "de", "fr", "es"):
        lines.append(arrays[lang].rstrip("\n"))

    lines.extend(
        [
            "#define DECLARE_FLAG_DSC(dsc_name, map_name)              \\",
            "    const lv_image_dsc_t dsc_name = {                    \\",
            "        .header.magic  = LV_IMAGE_HEADER_MAGIC,          \\",
            "        .header.cf     = LV_COLOR_FORMAT_RGB565A8,       \\",
            "        .header.flags  = 0,                              \\",
            "        .header.w      = FLAG_W,                         \\",
            "        .header.h      = FLAG_H,                         \\",
            "        .header.stride = FLAG_STRIDE,                    \\",
            "        .data_size     = sizeof(map_name),               \\",
            "        .data          = (const uint8_t *)map_name,      \\",
            "        .reserved      = NULL,                           \\",
            "    }",
            "",
            f"DECLARE_FLAG_DSC(g_flag_it_{w}x{h}, s_flag_it_map);",
            f"DECLARE_FLAG_DSC(g_flag_en_{w}x{h}, s_flag_en_map);",
            f"DECLARE_FLAG_DSC(g_flag_de_{w}x{h}, s_flag_de_map);",
            f"DECLARE_FLAG_DSC(g_flag_fr_{w}x{h}, s_flag_fr_map);",
            f"DECLARE_FLAG_DSC(g_flag_es_{w}x{h}, s_flag_es_map);",
            "",
            "const char *get_flag_path_for_language(const char *lang_code)",
            "{",
            "    if (!lang_code) {",
            '        return "S:/spiffs/flag_IT.png";',
            "    }",
            "",
            '    if (strcmp(lang_code, "it") == 0) {',
            '        return "S:/spiffs/flag_IT.png";',
            '    } else if (strcmp(lang_code, "en") == 0) {',
            '        return "S:/spiffs/flag_GB.png";',
            '    } else if (strcmp(lang_code, "de") == 0) {',
            '        return "S:/spiffs/flag_DE.png";',
            '    } else if (strcmp(lang_code, "fr") == 0) {',
            '        return "S:/spiffs/flag_FR.png";',
            '    } else if (strcmp(lang_code, "es") == 0) {',
            '        return "S:/spiffs/flag_ES.png";',
            "    }",
            "",
            '    return "S:/spiffs/flag_IT.png";',
            "}",
            "",
            "const lv_image_dsc_t *get_flag_bitmap_for_language(const char *lang_code)",
            "{",
            "    if (!lang_code) {",
            f"        return &g_flag_it_{w}x{h};",
            "    }",
            "",
            '    if (strcmp(lang_code, "it") == 0) {',
            f"        return &g_flag_it_{w}x{h};",
            '    } else if (strcmp(lang_code, "en") == 0) {',
            f"        return &g_flag_en_{w}x{h};",
            '    } else if (strcmp(lang_code, "de") == 0) {',
            f"        return &g_flag_de_{w}x{h};",
            '    } else if (strcmp(lang_code, "fr") == 0) {',
            f"        return &g_flag_fr_{w}x{h};",
            '    } else if (strcmp(lang_code, "es") == 0) {',
            f"        return &g_flag_es_{w}x{h};",
            "    }",
            "",
            f"    return &g_flag_it_{w}x{h};",
            "}",
            "",
            "const void *get_flag_src_for_language(const char *lang_code)",
            "{",
            "#if (defined(LV_USE_LODEPNG) && (LV_USE_LODEPNG != 0)) || \\",
            "    (defined(LV_USE_PNG) && (LV_USE_PNG != 0))",
            "    return (const void *)get_flag_path_for_language(lang_code);",
            "#else",
            "    return (const void *)get_flag_bitmap_for_language(lang_code);",
            "#endif",
            "}",
            "",
        ]
    )

    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Generate components/lvgl_panel/language_flags.c from PNG files.\n"
            "Output format: LV_COLOR_FORMAT_RGB565A8 (LVGL v9)\n"
            "  - Planar: tutti i pixel RGB565 (little-endian) poi tutti gli alpha\n"
            "  - Stride: FLAG_W * 2 (solo piano colore)"
        )
    )
    parser.add_argument("--project-root", default=".", help="Project root path")
    parser.add_argument("--data-dir", default="data", help="Directory containing flag PNG files")
    parser.add_argument(
        "--output",
        default="components/lvgl_panel/language_flags.c",
        help="Output C file path",
    )
    parser.add_argument("--width",  type=int, default=100)
    parser.add_argument("--height", type=int, default=50)
    args = parser.parse_args()

    root     = Path(args.project_root).resolve()
    data_dir = (root / args.data_dir).resolve()
    out_file = (root / args.output).resolve()

    log(f"[CFG] root={root}")
    log(f"[CFG] data_dir={data_dir}")
    log(f"[CFG] output={out_file}")
    log(f"[CFG] size={args.width}x{args.height}")
    log(f"[CFG] format=LV_COLOR_FORMAT_RGB565A8 (planar, little-endian)")

    text = build_file_text(data_dir, (args.width, args.height))

    out_file.parent.mkdir(parents=True, exist_ok=True)
    out_file.write_text(text, encoding="utf-8")

    log(f"[OK] Wrote {out_file}")
    log(f"[OK] Output lines: {text.count(chr(10)) + 1}")


if __name__ == "__main__":
    main()
