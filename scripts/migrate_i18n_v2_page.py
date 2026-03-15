#!/usr/bin/env python3
import argparse
import json
import re
from pathlib import Path

SCRIPT_BLOCK_RE = re.compile(r"(<script\b[^>]*>)(.*?)(</script>)", re.IGNORECASE | re.DOTALL)
TEXT_NODE_RE = re.compile(r">([^<]+)<")
ATTR_RE = re.compile(r"\b(placeholder|title|aria-label|value)=(['\"])(.*?)\2", re.IGNORECASE)
JS_STRING_RE = re.compile(r"([\"'])(.*?)(?<!\\)\1", re.DOTALL)
PLACEHOLDER_RE = re.compile(r"^\{\{[0-9]{3}\}\}$")
JS_KEYWORD_RE = re.compile(r"\b(function|const|let|var|return|if|else|for|while|switch|case|break|continue|try|catch|finally|new|class|import|export)\b")
JS_RUNTIME_TOKEN_RE = re.compile(r"\b(document|window|console|localStorage|sessionStorage|fetch|Promise|setTimeout|setInterval|querySelector|getElementById|addEventListener)\b|=>|===|!==|&&|\|\|")
HTML_TAG_RE = re.compile(r"</?[a-z][^>]*>", re.IGNORECASE)
CSS_BLOCK_RE = re.compile(r"[.#]?[A-Za-z0-9_-]+\s*\{[^}]*\}")


def should_localize_text(value: str) -> bool:
    if value is None:
        return False
    text = value.strip()
    if not text:
        return False
    if PLACEHOLDER_RE.match(text):
        return False
    if text.startswith("http://") or text.startswith("https://"):
        return False
    if text.startswith("/"):
        return False
    if "{" in text or "}" in text:
        return False
    return any(ch.isalpha() for ch in text)


def should_localize_js_literal(value: str) -> bool:
    text = (value or "").strip()
    if not should_localize_text(text):
        return False
    if len(text) > 160:
        return False
    if "\n" in text:
        return False
    if text.startswith("http://") or text.startswith("https://") or text.startswith("/"):
        return False
    if text in {"GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"}:
        return False
    if text in {"Content-Type", "application/json", "application/octet-stream", "no-store", "Authorization", "Bearer"}:
        return False
    if JS_KEYWORD_RE.search(text):
        return False
    if JS_RUNTIME_TOKEN_RE.search(text):
        return False
    if HTML_TAG_RE.search(text):
        return False
    if CSS_BLOCK_RE.search(text):
        return False
    if re.fullmatch(r"[A-Za-z0-9_./:-]+", text):
        return False
    return True


def next_key(page_map: dict) -> str:
    for i in range(1, 1000):
        code = f"{i:03d}"
        if code not in page_map:
            return code
    raise RuntimeError("Nessuna key libera (001..999)")


def ensure_page_catalog(catalog: dict, page: str):
    web = catalog.setdefault("web", {})
    return web.setdefault(page, {})


def add_entry(page_map: dict, page: str, label_base: str, text_it: str) -> str:
    # Riutilizzo se già presente (per testo IT identico)
    for code, entry in page_map.items():
        txt = (((entry or {}).get("text") or {}).get("it") or "").strip()
        if txt == text_it.strip():
            return code

    code = next_key(page_map)
    page_map[code] = {
        "label": f"{page}.{label_base}.{code}",
        "text": {"it": text_it.strip()},
    }
    return code


def process_non_script_segment(segment: str, page: str, page_map: dict):
    def repl_text(match):
        raw = match.group(1)
        stripped = raw.strip()
        if not should_localize_text(stripped):
            return match.group(0)
        code = add_entry(page_map, page, "text", stripped)
        leading_ws = raw[: len(raw) - len(raw.lstrip())]
        trailing_ws = raw[len(raw.rstrip()) :]
        return f">{leading_ws}{{{{{code}}}}}{trailing_ws}<"

    def repl_attr(match):
        name, quote, raw = match.group(1), match.group(2), match.group(3)
        stripped = raw.strip()
        if not should_localize_text(stripped):
            return match.group(0)
        code = add_entry(page_map, page, "attr", stripped)
        return f"{name}={quote}{{{{{code}}}}}{quote}"

    segment = TEXT_NODE_RE.sub(repl_text, segment)
    segment = ATTR_RE.sub(repl_attr, segment)
    return segment


def process_script_segment(script_content: str, page: str, page_map: dict):
    def repl_js_string(match):
        quote, raw = match.group(1), match.group(2)
        if not should_localize_js_literal(raw):
            return match.group(0)
        code = add_entry(page_map, page, "js", raw)
        return f"{quote}{{{{{code}}}}}{quote}"

    return JS_STRING_RE.sub(repl_js_string, script_content)


def migrate_html(content: str, page: str, page_map: dict):
    out = []
    last = 0
    for m in SCRIPT_BLOCK_RE.finditer(content):
        before = content[last : m.start()]
        out.append(process_non_script_segment(before, page, page_map))
        script_open, script_body, script_close = m.group(1), m.group(2), m.group(3)
        script_body = process_script_segment(script_body, page, page_map)
        out.append(f"{script_open}{script_body}{script_close}")
        last = m.end()
    out.append(process_non_script_segment(content[last:], page, page_map))
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(description="Migra una pagina HTML a placeholder i18n v2")
    ap.add_argument("--catalog", default="data/i18n_v2.json")
    ap.add_argument("--html", required=True, help="Percorso file html da migrare")
    args = ap.parse_args()

    catalog_path = Path(args.catalog)
    html_path = Path(args.html)

    if not catalog_path.exists():
        raise SystemExit(f"Catalogo non trovato: {catalog_path}")
    if not html_path.exists():
        raise SystemExit(f"HTML non trovato: {html_path}")

    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    page = html_path.stem
    page_map = ensure_page_catalog(catalog, page)

    original = html_path.read_text(encoding="utf-8", errors="ignore")
    migrated = migrate_html(original, page, page_map)

    html_path.write_text(migrated, encoding="utf-8")
    catalog_path.write_text(json.dumps(catalog, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"Migrata pagina '{page}'")
    print(f"Chiavi totali pagina: {len(page_map)}")


if __name__ == "__main__":
    main()
