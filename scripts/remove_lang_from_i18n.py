#!/usr/bin/env python3
import json
from pathlib import Path

data_dir = Path(__file__).resolve().parent.parent / 'data'
for p in sorted(data_dir.glob('i18n_*.json')):
    bak = p.with_suffix(p.suffix + '.bak')
    if not bak.exists():
        p.replace(bak)
        bak.rename(bak)
    else:
        # if backup exists, operate on current file
        pass
    # read from backup if exists
    src = bak if bak.exists() else p
    try:
        j = json.loads(src.read_text(encoding='utf-8'))
    except Exception as e:
        print(f"ERROR reading {src}: {e}")
        continue
    if not isinstance(j, list):
        print(f"Skipping {p}: not a list")
        continue
    changed = False
    for obj in j:
        if isinstance(obj, dict) and 'lang' in obj:
            del obj['lang']
            changed = True
    if changed:
        p.write_text(json.dumps(j, ensure_ascii=False, indent=2) + "\n", encoding='utf-8')
        print(f"Updated {p} (removed 'lang')")
    else:
        print(f"No change for {p}")
