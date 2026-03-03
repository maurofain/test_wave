#!/usr/bin/env python3
import json
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent.parent / 'data'
MAX_TEXT_BYTES = 31  # max payload bytes; +1 for terminator to reach 32


def split_text_by_bytes(s, max_bytes):
    # split string into chunks where each chunk's UTF-8 encoding <= max_bytes
    chunks = []
    if s is None:
        return ['']
    cur = ''
    for ch in s:
        cand = cur + ch
        if len(cand.encode('utf-8')) <= max_bytes:
            cur = cand
        else:
            if cur == '':
                # single char too large (rare for single multibyte > max_bytes), force it
                chunks.append(ch)
                cur = ''
            else:
                chunks.append(cur)
                cur = ch
    if cur != '':
        chunks.append(cur)
    if len(chunks) == 0:
        return ['']
    return chunks


for p in sorted(DATA_DIR.glob('i18n_*.json')):
    if p.suffix == '.bak':
        continue
    src = p
    bak = p.with_suffix(p.suffix + '.bak')
    if not bak.exists():
        src.replace(bak)
        # restore original to work on
        bak.rename(bak)
        # but we need src to refer to original: bak now has original name, so set src to bak
        src = bak
        # and target will be original path
        out_path = p
    else:
        # backup exists, read from current file
        src = p
        out_path = p
    print(f"Processing {src} -> {out_path}")
    j = json.loads(src.read_text(encoding='utf-8'))
    if not isinstance(j, list):
        print(f"Skipping {p}: not a list")
        continue

    # map scopes to uint8 ids
    scope_names = []
    for obj in j:
        scope = obj.get('scope', '')
        if scope not in scope_names:
            scope_names.append(scope)
    scope_map = {name: idx+1 for idx, name in enumerate(scope_names)}

    # map keys (original key strings) to uint16 ids (unique per language)
    key_names = []
    for obj in j:
        k = obj.get('key', '')
        if k not in key_names:
            key_names.append(k)
    key_map = {name: idx+1 for idx, name in enumerate(key_names)}

    out_records = []
    for obj in j:
        scope_name = obj.get('scope', '')
        key_name = obj.get('key', '')
        text = obj.get('text', '')
        scope_id = scope_map.get(scope_name, 0)
        key_id = key_map.get(key_name, 0)
        chunks = split_text_by_bytes(text, MAX_TEXT_BYTES)
        for sec_idx, chunk in enumerate(chunks):
            rec = {
                'scope': scope_id,
                'key': key_id,
                'section': sec_idx,
                'text': chunk
            }
            out_records.append(rec)

    out_path.write_text(json.dumps(out_records, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Wrote {len(out_records)} compact records to {out_path}")
