#!/usr/bin/env python3
"""
Compare original i18n .bak files with reconstructed strings from the compact format.
Generates data/i18n_compare_<lang>.md reports with diffs for mismatches.
"""
import json
from pathlib import Path
import difflib

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / 'data'


def load_json(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except Exception as e:
        print(f"ERROR reading {path}: {e}")
        return None


def group_compact(records):
    groups = {}
    for r in records:
        # compact records use numeric fields 'scope' and 'key'
        s = int(r.get('scope', 0))
        k = int(r.get('key', 0))
        sec = int(r.get('section', 0))
        text = r.get('text', '') or ''
        groups.setdefault((s, k), []).append((sec, text))
    # sort by section
    for k in groups:
        groups[k].sort(key=lambda x: x[0])
    return groups


def reconstruct_from_group(group):
    return ''.join(part for (_sec, part) in group)


def compare_language(lang):
    bak_path = DATA / f'i18n_{lang}.json.bak'
    sync_path = DATA / f'i18n_{lang}_sync.json'
    compact_path = DATA / f'i18n_{lang}.json'
    out_md = DATA / f'i18n_compare_{lang}.md'

    if not bak_path.exists():
        print(f"Skipping {lang}: {bak_path} missing")
        return
    if not sync_path.exists():
        print(f"Skipping {lang}: {sync_path} missing")
        return
    if not compact_path.exists():
        print(f"Skipping {lang}: {compact_path} missing")
        return

    orig = load_json(bak_path)
    sync = load_json(sync_path)
    compact = load_json(compact_path)
    if orig is None or sync is None or compact is None:
        return

    # build compact groups
    groups = group_compact(compact)

    mismatches = []
    total = 0
    for idx, rec in enumerate(sync):
        total += 1
        # sync contains original fields plus scope_id/key_id
        orig_scope = rec.get('scope', '')
        orig_key = rec.get('key', '')
        orig_text = rec.get('text', '') or ''
        scope_id = int(rec.get('scope_id', 0) or 0)
        key_id = int(rec.get('key_id', 0) or 0)

        reconstructed = ''
        grp = groups.get((scope_id, key_id))
        if grp:
            reconstructed = reconstruct_from_group(grp)

        if orig_text != reconstructed:
            # generate short diff
            diff = '\n'.join(difflib.unified_diff(
                orig_text.splitlines(), reconstructed.splitlines(),
                fromfile='original', tofile='reconstructed', lineterm=''))
            mismatches.append({
                'index': idx,
                'scope': orig_scope,
                'key': orig_key,
                'scope_id': scope_id,
                'key_id': key_id,
                'original': orig_text,
                'reconstructed': reconstructed,
                'diff': diff,
            })

    # write markdown
    with out_md.open('w', encoding='utf-8') as f:
        f.write(f"# i18n compare {lang}\n\n")
        f.write(f"Total records: {total}  \n\n")
        f.write(f"Mismatches: {len(mismatches)}\n\n")
        for m in mismatches:
            f.write(f"## Record {m['index']} — {m['scope']}.{m['key']} (ids {m['scope_id']}/{m['key_id']})\n\n")
            f.write("**Original:**\n\n```")
            f.write(m['original'] or '')
            f.write("```\n\n")
            f.write("**Reconstructed:**\n\n```")
            f.write(m['reconstructed'] or '')
            f.write("```\n\n")
            if m['diff']:
                f.write("**Diff:**\n\n```")
                f.write(m['diff'])
                f.write("```\n\n")
        print(f"Wrote {out_md} with {len(mismatches)} mismatches")


def main():
    for bak in sorted(DATA.glob('i18n_*.json.bak')):
        lang = bak.name[len('i18n_'):-len('.json.bak')]
        compare_language(lang)


if __name__ == '__main__':
    main()
