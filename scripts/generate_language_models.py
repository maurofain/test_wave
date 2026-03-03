#!/usr/bin/env python3
"""
Genera `language_models.h` a partire dai file i18n compatti correnti (`data/i18n_<lang>.json`)
e dalla mappa (`data/i18n_<lang>.map.json`).

Flusso previsto:
1) JSON -> H: scripts/generate_language_models.py  (questo script)
2) H -> JSON: scripts/generate_i18n_json_from_header.py

Obiettivo:
- Rendere disponibile un modello sorgente C con ID, literal legacy e testi per lingua.
- Includere lingue base: it, en, de, fr, es.

Nota:
- Se una lingua non ha file JSON dedicato, i valori mancanti vengono popolati in best-effort:
  1) prova da inglese
  2) altrimenti prova da italiano
  3) applica un piccolo glossario di traduzione (quando possibile)
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, Iterable, List, Tuple


DEFAULT_LANG_ORDER = ["it", "en", "de", "fr", "es"]

# Traduzioni best-effort per stringhe molto comuni (match esatto, case-sensitive).
GLOSSARY = {
    "de": {
        "Home": "Startseite",
        "Config": "Konfiguration",
        "Emulator": "Emulator",
        "Logs": "Protokolle",
        "Statistics": "Statistiken",
        "Tasks": "Aufgaben",
        "Test": "Test",
        "Time not set": "Zeit nicht eingestellt",
        "Time not available": "Zeit nicht verfügbar",
        "Enabled": "Aktiviert",
        "Update": "Aktualisieren",
        "Password": "Passwort",
        "Server": "Server",
        "Display": "Anzeige",
        "Language": "Sprache",
    },
    "fr": {
        "Home": "Accueil",
        "Config": "Configuration",
        "Emulator": "Émulateur",
        "Logs": "Journaux",
        "Statistics": "Statistiques",
        "Tasks": "Tâches",
        "Test": "Test",
        "Time not set": "Heure non définie",
        "Time not available": "Heure non disponible",
        "Enabled": "Activé",
        "Update": "Mettre à jour",
        "Password": "Mot de passe",
        "Server": "Serveur",
        "Display": "Affichage",
        "Language": "Langue",
    },
    "es": {
        "Home": "Inicio",
        "Config": "Configuración",
        "Emulator": "Emulador",
        "Logs": "Registros",
        "Statistics": "Estadísticas",
        "Tasks": "Tareas",
        "Test": "Prueba",
        "Time not set": "Hora no configurada",
        "Time not available": "Hora no disponible",
        "Enabled": "Habilitado",
        "Update": "Actualizar",
        "Password": "Contraseña",
        "Server": "Servidor",
        "Display": "Pantalla",
        "Language": "Idioma",
    },
}


class _Translator:
    def __init__(self, enabled: bool):
        self.enabled = enabled
        self._ready = False
        self._error = None

    def _ensure(self):
        if self._ready or not self.enabled:
            return
        self._ready = True
        try:
            from deep_translator import GoogleTranslator  # type: ignore

            self._GoogleTranslator = GoogleTranslator
        except Exception as exc:
            self._error = str(exc)

    def translate(self, text: str, target_lang: str) -> str:
        if not text:
            return text
        if not self.enabled:
            return text

        self._ensure()
        if self._error:
            return text

        lang_code_map = {
            "de": "de",
            "fr": "fr",
            "es": "es",
        }
        target = lang_code_map.get(target_lang)
        if not target:
            return text

        try:
            return self._GoogleTranslator(source="en", target=target).translate(text)
        except Exception:
            return text


@dataclass(frozen=True)
class EntryKey:
    scope_id: int
    key_id: int


@dataclass
class EntryModel:
    scope_id: int
    key_id: int
    scope_text: str
    key_text: str
    values: Dict[str, str]


def _read_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def _c_escape(text: str) -> str:
    return (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def _sanitize_symbol(raw: str) -> str:
    raw = raw.strip().upper()
    raw = re.sub(r"[^A-Z0-9]+", "_", raw)
    raw = re.sub(r"_+", "_", raw).strip("_")
    return raw or "UNSPEC"


def _render_models_comment_block(models: List[EntryModel], lang_order: List[str]) -> List[str]:
    payload: Dict[str, Any] = {
        "languages": lang_order,
        "entries": [],
    }

    for m in models:
        payload["entries"].append(
            {
                "symbol": _sanitize_symbol(f"{m.scope_text}_{m.key_text}"),
                "scope_id": m.scope_id,
                "key_id": m.key_id,
                "scope_text": m.scope_text,
                "key_text": m.key_text,
                "values": [m.values.get(lang, "") for lang in lang_order],
            }
        )

    payload_json = json.dumps(payload, ensure_ascii=False, sort_keys=True, indent=2)

    lines: List[str] = []
    lines.append("/* I18N_LANGUAGE_MODELS_DATA_START")
    for line in payload_json.splitlines():
        lines.append(f" * {line}")
    lines.append(" * I18N_LANGUAGE_MODELS_DATA_END */")
    return lines


def _load_map(data_dir: Path, lang_hint: str) -> Tuple[Dict[int, str], Dict[int, str], Path]:
    candidates = [
        data_dir / f"i18n_{lang_hint}.map.json",
        data_dir / "i18n_en.map.json",
        data_dir / "i18n_it.map.json",
    ]
    map_path = None
    for c in candidates:
        if c.exists():
            map_path = c
            break
    if not map_path:
        raise FileNotFoundError("Nessun file i18n_*.map.json trovato in data/")

    root = _read_json(map_path)
    scopes_obj = root.get("scopes", {})
    keys_obj = root.get("keys", {})

    scopes = {int(k): str(v) for k, v in scopes_obj.items()}
    keys = {int(k): str(v) for k, v in keys_obj.items()}
    return scopes, keys, map_path


def _load_language_records(data_dir: Path, lang: str) -> Dict[EntryKey, str]:
    path = data_dir / f"i18n_{lang}.json"
    if not path.exists():
        return {}

    records = _read_json(path)
    grouped: Dict[EntryKey, List[Tuple[int, str]]] = {}

    for row in records:
        scope_id = int(row.get("scope", 0) or 0)
        key_id = int(row.get("key", 0) or 0)
        section = int(row.get("section", 0) or 0)
        text = str(row.get("text", "") or "")
        if scope_id <= 0 or key_id <= 0:
            continue
        ek = EntryKey(scope_id=scope_id, key_id=key_id)
        grouped.setdefault(ek, []).append((section, text))

    out: Dict[EntryKey, str] = {}
    for ek, chunks in grouped.items():
        chunks.sort(key=lambda item: item[0])
        out[ek] = "".join(part for _, part in chunks)

    return out


def _best_effort_translate(text: str, lang: str, translator: _Translator) -> str:
    if not text:
        return text
    table = GLOSSARY.get(lang)
    if table and text in table:
        return table[text]
    return translator.translate(text, lang)


def _build_models(
    scopes: Dict[int, str],
    keys: Dict[int, str],
    per_lang: Dict[str, Dict[EntryKey, str]],
    lang_order: List[str],
    translator: _Translator,
) -> List[EntryModel]:
    all_keys = set()
    for lang_dict in per_lang.values():
        all_keys.update(lang_dict.keys())

    sorted_keys = sorted(all_keys, key=lambda ek: (ek.scope_id, ek.key_id))
    models: List[EntryModel] = []

    for ek in sorted_keys:
        scope_text = scopes.get(ek.scope_id, f"scope_{ek.scope_id}")
        key_text = keys.get(ek.key_id, f"key_{ek.key_id}")

        values: Dict[str, str] = {}
        base_en = per_lang.get("en", {}).get(ek, "")
        base_it = per_lang.get("it", {}).get(ek, "")

        for lang in lang_order:
            direct = per_lang.get(lang, {}).get(ek)
            if direct is not None and direct != "":
                values[lang] = direct
                continue

            fallback = base_en or base_it or ""
            if lang in ("de", "fr", "es"):
                values[lang] = _best_effort_translate(fallback, lang, translator)
            else:
                values[lang] = fallback

        models.append(
            EntryModel(
                scope_id=ek.scope_id,
                key_id=ek.key_id,
                scope_text=scope_text,
                key_text=key_text,
                values=values,
            )
        )

    return models


def _render_header(models: List[EntryModel], lang_order: List[str], map_file: Path) -> str:
    # Scope define univoci per name -> id
    scope_name_to_id: Dict[str, int] = {}
    for m in models:
        scope_name_to_id.setdefault(m.scope_text, m.scope_id)

    key_name_to_id: Dict[str, int] = {}
    for m in models:
        key_name_to_id.setdefault(m.key_text, m.key_id)

    lines: List[str] = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <stddef.h>")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("/*")
    lines.append(" * FILE AUTO-GENERATO: non modificare manualmente.")
    lines.append(" * Sorgente: scripts/generate_language_models.py")
    lines.append(f" * Map di riferimento: data/{map_file.name}")
    lines.append(" */")
    lines.append("")

    lines.append(f"#define I18N_LANG_COUNT {len(lang_order)}")
    for idx, lang in enumerate(lang_order):
        lines.append(f"#define I18N_LANG_{lang.upper()} {idx}")
    lines.append("")

    lang_codes = ", ".join(f'\"{_c_escape(l)}\"' for l in lang_order)
    lines.append(f"static const char *const I18N_LANG_CODES[I18N_LANG_COUNT] = {{{lang_codes}}};")
    lines.append("")

    lines.append("/* Scope IDs */")
    for scope_name, scope_id in sorted(scope_name_to_id.items(), key=lambda x: x[1]):
        lines.append(f"#define I18N_SCOPE_{_sanitize_symbol(scope_name)} {scope_id}")
    lines.append("")

    lines.append("/* Key IDs */")
    for key_name, key_id in sorted(key_name_to_id.items(), key=lambda x: x[1]):
        lines.append(f"#define I18N_KEY_{_sanitize_symbol(key_name)} {key_id}")
    lines.append("")

    lines.append("typedef struct {")
    lines.append("    const char *symbol;")
    lines.append("    uint8_t scope_id;")
    lines.append("    uint16_t key_id;")
    lines.append("    const char *scope_text;")
    lines.append("    const char *key_text;")
    lines.append("    const char *values[I18N_LANG_COUNT];")
    lines.append("} i18n_language_model_t;")
    lines.append("")

    lines.append(f"#define I18N_MODEL_COUNT {len(models)}")
    lines.append("/*")
    lines.append(" * Dataset i18n completo usato dagli script di generazione JSON.")
    lines.append(" * Non viene compilato come array C per evitare uso memoria nel firmware.")
    lines.append(" */")
    lines.extend(_render_models_comment_block(models=models, lang_order=lang_order))
    lines.append("")

    return "\n".join(lines) + "\n"


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
    parser = argparse.ArgumentParser(description="Genera language_models.h da data/i18n_*.json")
    parser.add_argument("--data-dir", default="data", help="Directory contenente i18n_*.json e i18n_*.map.json")
    parser.add_argument(
        "--output",
        default="components/device_config/include/language_models.h",
        help="Percorso header di output",
    )
    parser.add_argument(
        "--languages",
        default=",".join(DEFAULT_LANG_ORDER),
        help="Lista codici lingua separati da virgola (ordine colonne)",
    )
    parser.add_argument(
        "--machine-translate",
        action="store_true",
        help="Prova a tradurre automaticamente i fallback (richiede deep_translator)",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Non scrive file; verifica se l'output sarebbe aggiornato",
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
    data_dir = (root / args.data_dir)
    out_path = (root / args.output)
    lang_order = [x.strip() for x in args.languages.split(",") if x.strip()]

    if not data_dir.exists():
        print(f"[ERR] data-dir non trovata: {data_dir}", file=sys.stderr)
        return 2

    scopes, keys, map_file = _load_map(data_dir, lang_hint="en")

    per_lang: Dict[str, Dict[EntryKey, str]] = {}
    for lang in lang_order:
        per_lang[lang] = _load_language_records(data_dir, lang)

    translator = _Translator(enabled=bool(args.machine_translate))
    models = _build_models(
        scopes=scopes,
        keys=keys,
        per_lang=per_lang,
        lang_order=lang_order,
        translator=translator,
    )
    header = _render_header(models=models, lang_order=lang_order, map_file=map_file)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    file_status = _write_or_check_file(out_path, header, check=bool(args.check))

    missing = []
    for lang in lang_order:
        count = len(per_lang.get(lang, {}))
        if count == 0:
            missing.append(lang)

    if args.check:
        print(f"[OK] Check: {out_path} -> {file_status}")
    else:
        print(f"[OK] Generato: {out_path}")
    print(f"[OK] Voci modello: {len(models)}")
    if missing:
        print(f"[WARN] Nessun JSON sorgente per lingue: {', '.join(missing)} (usato fallback best-effort)")

    report = {
        "mode": "check" if args.check else "write",
        "languages": lang_order,
        "model_count": len(models),
        "missing_source_languages": missing,
        "target": {
            "path": _relative_to_root(out_path, root),
            "status": file_status,
        },
    }

    report_json = json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2)
    print(report_json)

    if args.report_file:
        report_path = (root / args.report_file)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(report_json + "\n", encoding="utf-8")
        print(f"[OK] Report scritto: {report_path}")

    if args.check and file_status in ("missing", "would_update"):
        return 10

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
