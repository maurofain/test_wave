#!/usr/bin/env python3
"""
Genera i file i18n usati dalla app a partire da `language_models.h`.

Flusso previsto:
1) JSON -> H: scripts/generate_language_models.py
2) H -> JSON: scripts/generate_i18n_json_from_header.py  (questo script)

Output:
- data/i18n_<lang>.json
- data/i18n_<lang>.map.json
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional


ENTRY_RE = re.compile(
    r"\{\s*"
    r"\.symbol\s*=\s*\"(?P<symbol>(?:\\.|[^\"\\])*)\"\s*,\s*"
    r"\.scope_id\s*=\s*(?P<scope_id>\d+)\s*,\s*"
    r"\.key_id\s*=\s*(?P<key_id>\d+)\s*,\s*"
    r"\.scope_text\s*=\s*\"(?P<scope_text>(?:\\.|[^\"\\])*)\"\s*,\s*"
    r"\.key_text\s*=\s*\"(?P<key_text>(?:\\.|[^\"\\])*)\"\s*,\s*"
    r"\.values\s*=\s*\{(?P<values>.*?)\}\s*,\s*"
    r"\}\s*,",
    re.DOTALL,
)

VALUES_RE = re.compile(r'\"((?:\\.|[^\"\\])*)\"')
LANG_CODES_RE = re.compile(
    r"I18N_LANG_CODES\s*\[\s*I18N_LANG_COUNT\s*\]\s*=\s*\{(?P<codes>.*?)\};",
    re.DOTALL,
)

MODELS_COMMENT_RE = re.compile(
    r"/\*\s*I18N_LANGUAGE_MODELS_DATA_START(?P<body>.*?)I18N_LANGUAGE_MODELS_DATA_END\s*\*/",
    re.DOTALL,
)


@dataclass
class Entry:
    symbol: str
    scope_id: int
    key_id: int
    scope_text: str
    key_text: str
    values: Dict[str, str]


def _c_unescape(s: str) -> str:
    out = s.replace(r"\n", "\n").replace(r"\r", "\r").replace(r"\t", "\t")
    out = out.replace(r'\"', '"').replace(r"\\", "\\")
    return out


def _split_utf8_sections(text: str, max_bytes: int = 31) -> List[str]:
    if text == "":
        return [""]

    chunks: List[str] = []
    current_chars: List[str] = []
    current_size = 0

    for ch in text:
        b = len(ch.encode("utf-8"))
        if b > max_bytes:
            if current_chars:
                chunks.append("".join(current_chars))
                current_chars = []
                current_size = 0
            chunks.append(ch)
            continue

        if current_size + b > max_bytes:
            chunks.append("".join(current_chars))
            current_chars = [ch]
            current_size = b
        else:
            current_chars.append(ch)
            current_size += b

    if current_chars:
        chunks.append("".join(current_chars))

    return chunks


def _parse_lang_codes(header_text: str) -> List[str]:
    match = LANG_CODES_RE.search(header_text)
    if not match:
        raise ValueError("I18N_LANG_CODES non trovato in header")

    codes_src = match.group("codes")
    codes = [_c_unescape(x) for x in VALUES_RE.findall(codes_src)]
    if not codes:
        raise ValueError("Lista I18N_LANG_CODES vuota")
    return codes


def _parse_entries(header_text: str, lang_codes: List[str]) -> List[Entry]:
    entries_from_comment = _parse_entries_from_comment_block(header_text, lang_codes)
    if entries_from_comment is not None:
        return entries_from_comment

    return _parse_entries_from_legacy_array(header_text, lang_codes)


def _parse_entries_from_comment_block(header_text: str, lang_codes: List[str]) -> Optional[List[Entry]]:
    match = MODELS_COMMENT_RE.search(header_text)
    if not match:
        return None

    body = match.group("body")
    normalized_lines: List[str] = []
    for line in body.splitlines():
        stripped = line.lstrip()
        if stripped.startswith("*"):
            stripped = stripped[1:]
            if stripped.startswith(" "):
                stripped = stripped[1:]
        normalized_lines.append(stripped)

    payload_text = "\n".join(normalized_lines).strip()
    payload = json.loads(payload_text)

    payload_langs = payload.get("languages", [])
    if payload_langs and payload_langs != lang_codes:
        raise ValueError(
            f"Lingue nel blocco commentato non allineate a I18N_LANG_CODES: "
            f"{payload_langs} vs {lang_codes}"
        )

    raw_entries = payload.get("entries", [])
    entries: List[Entry] = []
    for row in raw_entries:
        values_raw = row.get("values", [])
        if len(values_raw) != len(lang_codes):
            raise ValueError(
                f"Entry commentata {row.get('symbol', '')} ha {len(values_raw)} values ma attese {len(lang_codes)}"
            )

        values = {lang_codes[idx]: str(values_raw[idx]) for idx in range(len(lang_codes))}

        entries.append(
            Entry(
                symbol=str(row.get("symbol", "")),
                scope_id=int(row.get("scope_id", 0)),
                key_id=int(row.get("key_id", 0)),
                scope_text=str(row.get("scope_text", "")),
                key_text=str(row.get("key_text", "")),
                values=values,
            )
        )

    if not entries:
        raise ValueError("Nessuna entry trovata nel blocco commentato I18N_LANGUAGE_MODELS_DATA")

    return entries


def _parse_entries_from_legacy_array(header_text: str, lang_codes: List[str]) -> List[Entry]:
    entries: List[Entry] = []

    for m in ENTRY_RE.finditer(header_text):
        symbol = _c_unescape(m.group("symbol"))
        scope_id = int(m.group("scope_id"))
        key_id = int(m.group("key_id"))
        scope_text = _c_unescape(m.group("scope_text"))
        key_text = _c_unescape(m.group("key_text"))

        values_raw = VALUES_RE.findall(m.group("values"))
        values_decoded = [_c_unescape(v) for v in values_raw]

        if len(values_decoded) != len(lang_codes):
            raise ValueError(
                f"Entry {symbol} ha {len(values_decoded)} values ma attese {len(lang_codes)}"
            )

        values = {lang_codes[idx]: values_decoded[idx] for idx in range(len(lang_codes))}

        entries.append(
            Entry(
                symbol=symbol,
                scope_id=scope_id,
                key_id=key_id,
                scope_text=scope_text,
                key_text=key_text,
                values=values,
            )
        )

    if not entries:
        raise ValueError("Nessuna entry trovata in I18N_LANGUAGE_MODELS")

    return entries


def _build_map(entries: List[Entry]) -> Dict[str, Dict[str, str]]:
    scopes: Dict[int, str] = {}
    keys: Dict[int, str] = {}

    for e in entries:
        prev_scope = scopes.get(e.scope_id)
        if prev_scope is not None and prev_scope != e.scope_text:
            raise ValueError(
                f"Conflitto scope_id={e.scope_id}: '{prev_scope}' vs '{e.scope_text}'"
            )
        scopes[e.scope_id] = e.scope_text

        prev_key = keys.get(e.key_id)
        if prev_key is not None and prev_key != e.key_text:
            raise ValueError(
                f"Conflitto key_id={e.key_id}: '{prev_key}' vs '{e.key_text}'"
            )
        keys[e.key_id] = e.key_text

    scope_obj = {str(k): scopes[k] for k in sorted(scopes)}
    key_obj = {str(k): keys[k] for k in sorted(keys)}
    return {"scopes": scope_obj, "keys": key_obj}


def _build_lang_records(entries: List[Entry], lang: str) -> List[Dict[str, object]]:
    out: List[Dict[str, object]] = []

    for e in sorted(entries, key=lambda x: (x.scope_id, x.key_id)):
        text = e.values.get(lang, "")
        parts = _split_utf8_sections(text, max_bytes=31)
        for section, part in enumerate(parts):
            out.append(
                {
                    "scope": e.scope_id,
                    "key": e.key_id,
                    "section": section,
                    "text": part,
                }
            )

    return out


def _write_or_check_file(path: Path, content: str, check: bool) -> str:
    if not path.exists():
        if check:
            return "missing"
        path.write_text(content, encoding="utf-8")
        return "created"

    current = path.read_text(encoding="utf-8")
    if current == content:
        return "unchanged"

    if check:
        return "would_update"

    path.write_text(content, encoding="utf-8")
    return "updated"


def _relative_to_root(path: Path, root: Path) -> str:
    try:
        return str(path.resolve().relative_to(root.resolve()))
    except Exception:
        return str(path)


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description="Genera i18n JSON/MAP da language_models.h")
    parser.add_argument(
        "--header",
        default="components/device_config/include/language_models.h",
        help="Header sorgente modello lingue",
    )
    parser.add_argument(
        "--data-dir",
        default="data",
        help="Directory di output per i18n_<lang>.json e i18n_<lang>.map.json",
    )
    parser.add_argument(
        "--languages",
        default="",
        help="Lista lingue separata da virgola (default: tutte quelle in header)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Non scrive file; verifica se i JSON/MAP sarebbero aggiornati",
    )
    parser.add_argument(
        "--report-file",
        default="",
        help="Percorso file report JSON deterministico (opzionale)",
    )
    parser.add_argument(
        "--project-root",
        default=".",
        help="Root progetto (default: directory corrente)",
    )

    args = parser.parse_args(list(argv))

    root = Path(args.project_root)
    header_path = (root / args.header)
    data_dir = (root / args.data_dir)

    if not header_path.exists():
        print(f"[ERR] Header non trovato: {header_path}", file=sys.stderr)
        return 2

    header_text = header_path.read_text(encoding="utf-8")
    lang_codes = _parse_lang_codes(header_text)
    entries = _parse_entries(header_text, lang_codes)

    selected_langs = lang_codes
    if args.languages.strip():
        requested = [x.strip() for x in args.languages.split(",") if x.strip()]
        unknown = [x for x in requested if x not in lang_codes]
        if unknown:
            print(f"[ERR] Lingue non presenti in header: {', '.join(unknown)}", file=sys.stderr)
            return 3
        selected_langs = requested

    data_dir.mkdir(parents=True, exist_ok=True)

    common_map = _build_map(entries)

    file_results = []

    for lang in selected_langs:
        records = _build_lang_records(entries, lang)

        out_json = data_dir / f"i18n_{lang}.json"
        out_map = data_dir / f"i18n_{lang}.map.json"

        json_content = json.dumps(records, ensure_ascii=False, indent=2) + "\n"
        map_content = json.dumps(common_map, ensure_ascii=False, indent=2) + "\n"

        json_status = _write_or_check_file(out_json, json_content, check=bool(args.check))
        map_status = _write_or_check_file(out_map, map_content, check=bool(args.check))

        if args.check:
            print(f"[OK] Check {out_json} ({len(records)} record) -> {json_status}")
            print(f"[OK] Check {out_map} -> {map_status}")
        else:
            print(f"[OK] Scritto {out_json} ({len(records)} record)")
            print(f"[OK] Scritto {out_map}")

        file_results.append(
            {
                "language": lang,
                "records": len(records),
                "json": {"path": _relative_to_root(out_json, root), "status": json_status},
                "map": {"path": _relative_to_root(out_map, root), "status": map_status},
            }
        )

    print(f"[OK] Completato: {len(entries)} entry modello, lingue={','.join(selected_langs)}")

    report = {
        "mode": "check" if args.check else "write",
        "header": _relative_to_root(header_path, root),
        "languages": selected_langs,
        "entry_count": len(entries),
        "results": file_results,
    }
    report_json = json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2)
    print(report_json)

    if args.report_file:
        report_path = (root / args.report_file)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(report_json + "\n", encoding="utf-8")
        print(f"[OK] Report scritto: {report_path}")

    if args.check:
        has_diff = any(
            item["json"]["status"] in ("missing", "would_update")
            or item["map"]["status"] in ("missing", "would_update")
            for item in file_results
        )
        if has_diff:
            return 10

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
