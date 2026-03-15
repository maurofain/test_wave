#!/usr/bin/env python3
import argparse
import re
from pathlib import Path

SCRIPT_RE = re.compile(r"<script\b(?![^>]*\bsrc=)[^>]*>(.*?)</script>", re.IGNORECASE | re.DOTALL)


def extract_for_file(html_path: Path, js_dir: Path):
    page = html_path.stem
    html = html_path.read_text(encoding="utf-8", errors="ignore")

    idx = 0

    def repl(match):
        nonlocal idx
        body = match.group(1)
        idx += 1
        js_name = f"{page}_{idx:02d}.js"
        js_path = js_dir / js_name
        js_path.write_text(body.strip() + "\n", encoding="utf-8")
        return f"{{{{JS:js/{js_name}}}}}"

    new_html, count = SCRIPT_RE.subn(repl, html)
    if count > 0:
        html_path.write_text(new_html, encoding="utf-8")
    return count


def main():
    parser = argparse.ArgumentParser(description="Estrae script inline HTML in moduli JS per i18n v2")
    parser.add_argument("--www", default="data/www", help="Directory HTML")
    args = parser.parse_args()

    www_dir = Path(args.www)
    if not www_dir.exists():
        raise SystemExit(f"Directory non trovata: {www_dir}")

    js_dir = www_dir / "js"
    js_dir.mkdir(parents=True, exist_ok=True)

    total = 0
    for html_path in sorted(www_dir.glob("*.html")):
        n = extract_for_file(html_path, js_dir)
        total += n
        print(f"{html_path.name}: estratti {n} script")

    print(f"Totale script estratti: {total}")


if __name__ == "__main__":
    main()
