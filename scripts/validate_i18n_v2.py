#!/usr/bin/env python3
import argparse
import json
import re
from pathlib import Path

PLACEHOLDER_RE = re.compile(r"\{\{([0-9]{3})\}\}")
LABEL_RE = re.compile(r"^[a-z0-9]+(\.[a-z0-9_]+){2,}$")
JS_INCLUDE_RE = re.compile(r"\{\{JS:([^}]+)\}\}")


def extract_placeholders(text: str):
    return set(PLACEHOLDER_RE.findall(text or ""))


def collect_template_placeholders(www_dir: Path):
    per_page = {}
    if not www_dir.exists():
        return per_page

    for html in sorted(www_dir.glob("*.html")):
        content = html.read_text(encoding="utf-8", errors="ignore")
        page = html.stem
        placeholders = set(extract_placeholders(content))
        includes = JS_INCLUDE_RE.findall(content)
        for rel in includes:
            js_path = (www_dir / rel).resolve()
            try:
                if js_path.exists() and js_path.is_file():
                    js_content = js_path.read_text(encoding="utf-8", errors="ignore")
                    placeholders.update(extract_placeholders(js_content))
            except Exception:
                pass
        per_page[page] = placeholders
    return per_page


def validate_catalog(catalog: dict, per_page: dict, require_all_languages: bool = False):
    errors = []
    warnings = []

    base_language = str(catalog.get("base_language") or "it")
    web = catalog.get("web")
    catalog_languages = [
        str(x).strip().lower()
        for x in (catalog.get("languages") or [])
        if str(x).strip()
    ]
    if not catalog_languages:
        catalog_languages = ["it", "en", "fr", "de", "es"]
    if not isinstance(web, dict):
        errors.append("Campo 'web' mancante o non oggetto")
        return errors, warnings

    for page, page_map in web.items():
        if not isinstance(page_map, dict):
            errors.append(f"Pagina '{page}': valore non oggetto")
            continue

        seen_labels = set()
        for key, entry in page_map.items():
            if not re.fullmatch(r"[0-9]{3}", str(key)):
                errors.append(f"Pagina '{page}' key '{key}': formato key non valido (atteso NNN)")
            if not isinstance(entry, dict):
                errors.append(f"Pagina '{page}' key '{key}': entry non oggetto")
                continue

            label = str(entry.get("label") or "")
            if not label:
                errors.append(f"Pagina '{page}' key '{key}': label mancante")
            elif not LABEL_RE.fullmatch(label):
                errors.append(f"Pagina '{page}' key '{key}': label non valida '{label}'")
            elif label in seen_labels:
                errors.append(f"Pagina '{page}': label duplicata '{label}'")
            else:
                seen_labels.add(label)

            text = entry.get("text")
            if not isinstance(text, dict):
                errors.append(f"Pagina '{page}' key '{key}': campo text mancante/non oggetto")
                continue

            base_val = text.get(base_language)
            if not isinstance(base_val, str) or not base_val.strip():
                errors.append(f"Pagina '{page}' key '{key}': traduzione base '{base_language}' mancante")

            for lang, value in text.items():
                if not isinstance(value, str):
                    errors.append(f"Pagina '{page}' key '{key}': traduzione '{lang}' non stringa")

            if require_all_languages:
                missing_langs = [
                    lang
                    for lang in catalog_languages
                    if not isinstance(text.get(lang), str) or text.get(lang) == ""
                ]
                if missing_langs:
                    errors.append(
                        f"Pagina '{page}' key '{key}': traduzioni mancanti/vuote per lingue {missing_langs}"
                    )

        placeholders = per_page.get(page, set())
        json_keys = set(page_map.keys())

        missing_in_json = placeholders - json_keys
        for key in sorted(missing_in_json):
            errors.append(f"Pagina '{page}': placeholder '{{{{{key}}}}}' presente nel template ma assente nel JSON")

        unused_in_template = json_keys - placeholders
        for key in sorted(unused_in_template):
            warnings.append(f"Pagina '{page}': key '{key}' presente nel JSON ma non usata nel template")

    return errors, warnings


def main():
    parser = argparse.ArgumentParser(description="Valida catalogo i18n v2")
    parser.add_argument("--catalog", default="data/i18n_v2.json", help="Percorso catalogo i18n_v2.json")
    parser.add_argument("--www", default="data/www", help="Directory template HTML")
    parser.add_argument(
        "--require-all-languages",
        action="store_true",
        help="Richiede che ogni key abbia tutte le lingue definite nel catalogo",
    )
    args = parser.parse_args()

    catalog_path = Path(args.catalog)
    www_dir = Path(args.www)

    if not catalog_path.exists():
        print(f"ERROR: catalogo non trovato: {catalog_path}")
        return 1

    try:
        catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    except Exception as exc:
        print(f"ERROR: JSON non valido: {exc}")
        return 1

    per_page = collect_template_placeholders(www_dir)
    errors, warnings = validate_catalog(catalog, per_page, require_all_languages=args.require_all_languages)

    for w in warnings:
        print(f"WARNING: {w}")
    for e in errors:
        print(f"ERROR: {e}")

    print(f"\nSummary: {len(errors)} error(s), {len(warnings)} warning(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
