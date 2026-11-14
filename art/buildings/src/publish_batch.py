"""Write fitted/blocked assets (work/fit_results.json) into manifest.json: add or replace by id, keep everything else.

    python -I art/buildings/src/publish_batch.py <id> [<id> ...] [--atlas ATL_X=textures/T_ATL_X.png ...]
Extra per-asset fields come from work/publish_extra.json ({id: {fits_box, also_fits, concept, rodin, notes, attachments, ...}}).
Also lists the new files in manifest "files" and recounts texture_sets (unique non-sign, non-tileable textures).
"""
import json, sys
from pathlib import Path

R = Path(__file__).resolve().parents[1]
man = json.loads((R / 'manifest.json').read_text(encoding='utf-8'))
res = json.loads((R / 'work' / 'fit_results.json').read_text())
extra_p = R / 'work' / 'publish_extra.json'
extra = json.loads(extra_p.read_text()) if extra_p.exists() else {}
args = sys.argv[1:]
atl = [a for a in args if '=' in a]; ids = [a for a in args if '=' not in a and a != '--atlas']
for a in atl:
    k, v = a.split('='); man['atlases'][k] = {'texture': v, 'emissive': None}
by = {a['id']: i for i, a in enumerate(man['assets'])}
files = man.setdefault('files', [])
flist = files if isinstance(files, list) else None
for aid in ids:
    e = {k: v for k, v in res[aid].items() if k != 'audit'}
    e.update(extra.get(aid, {}))
    e.setdefault('fits_box', None); e.setdefault('also_fits', []); e.setdefault('recess', []); e.setdefault('quest_object', None)
    if aid in by: man['assets'][by[aid]] = e
    else: man['assets'].append(e); by[aid] = len(man['assets']) - 1
    if flist is not None:
        for f in (e['file'], e['texture'] if not e['texture'].startswith('atlas:') else man['atlases'][e['texture'][6:]]['texture']):
            if f not in flist: flist.append(f)
    print('published', aid, e['tris'])
def textures(assets):
    out = set()
    for a in assets:
        if a['kind'] in ('sign_card', 'wall', 'roof'): continue
        t = a['texture']; out.add(man['atlases'][t[6:]]['texture'] if t.startswith('atlas:') else t)
    return out
# keep the previous agent's count and add only the texture sets these assets introduce
new = textures([a for a in man['assets'] if a['id'] in ids]) - textures([a for a in man['assets'] if a['id'] not in ids])
man.setdefault('texture_sets_added_2026_11_13', [])
for t in sorted(new):
    if t not in man['texture_sets_added_2026_11_13']: man['texture_sets_added_2026_11_13'].append(t); man['texture_sets'] += 1
(R / 'manifest.json').write_text(json.dumps(man, indent=2) + '\n', encoding='utf-8')
print('texture_sets', man['texture_sets'])
