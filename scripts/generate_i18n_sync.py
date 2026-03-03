#!/usr/bin/env python3
import json
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent.parent / 'data'

for bak in sorted(DATA_DIR.glob('i18n_*.json.bak')):
    lang = bak.name[len('i18n_'):-len('.json.bak')]
    out_path = DATA_DIR / f'i18n_{lang}_sync.json'
    print(f"Processing {bak} -> {out_path}")
    try:
        records = json.loads(bak.read_text(encoding='utf-8'))
    except Exception as e:
        print(f"ERROR reading {bak}: {e}")
        continue
    if not isinstance(records, list):
        print(f"Skipping {bak}: not a list")
        continue

    scopes = []
    keys = []
    for r in records:
        sc = r.get('scope', '')
        k = r.get('key', '')
        if sc not in scopes:
            scopes.append(sc)
        if k not in keys:
            keys.append(k)

    scope_map = {name: idx+1 for idx, name in enumerate(scopes)}
    key_map = {name: idx+1 for idx, name in enumerate(keys)}

    out_records = []
    for r in records:
        nr = dict(r)
        nr['scope_id'] = scope_map.get(r.get('scope',''), 0)
        nr['key_id'] = key_map.get(r.get('key',''), 0)
        out_records.append(nr)

    out_path.write_text(json.dumps(out_records, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Wrote {len(out_records)} records to {out_path}")

# Also write mapping files for convenience
for bak in sorted(DATA_DIR.glob('i18n_*.json.bak')):
    lang = bak.name[len('i18n_'):-len('.json.bak')]
    map_path = DATA_DIR / f'i18n_{lang}.map.json'
    records = json.loads(bak.read_text(encoding='utf-8'))
    scopes = []
    keys = []
    for r in records:
        sc = r.get('scope', '')
        k = r.get('key', '')
        if sc not in scopes:
            scopes.append(sc)
        if k not in keys:
            keys.append(k)
    map_obj = {
        'scopes': {str(idx+1): name for idx, name in enumerate(scopes)},
        'keys': {str(idx+1): name for idx, name in enumerate(keys)}
    }
    map_path.write_text(json.dumps(map_obj, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f"Wrote mapping to {map_path}")
