"""Fits and exports every wardrobe base (wardrobe.BASES) for one sex, plus the body, into art/characters_v2/export.

Each base is fitted on a freshly opened body (MHCLO in MPFB's native frame), then exported by export_v2.run as
SKV2_<Sex>_<base>_<slot>.fbx with its source texture SKV2_<Sex>_<base>_D.png (the input to retex.py).

Run: blender -b --python tools/charv2/build_wardrobe.py -- <Male|Female> [base,base]
"""
import bpy, sys, json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_body as bb
import export_v2
import wardrobe

args = sys.argv[sys.argv.index('--') + 1:]
sex = args[0]
only = set(args[1].split(',')) if len(args) > 1 else set()
BODY = bb.OUT / 'src' / f'Body_{sex}.blend'
OUT = bb.OUT / 'export'
OUT.mkdir(parents=True, exist_ok=True)


def find_mhclo(stem):
    for root in (bb.PACKS, bb.SYS / 'clothes', bb.SYS / 'hair'):
        hits = list(Path(root).rglob(f'{stem}.mhclo'))
        if hits:
            return str(hits[0])
    return None


report = {}
if not only:
    bpy.ops.wm.open_mainfile(filepath=str(BODY))
    report['Body'] = export_v2.run(OUT, f'SKV2_{sex}', {}, with_body=True)
for base, (bsex, stem, pieces, covers, tmpl) in wardrobe.BASES.items():
    if bsex != sex or (only and base not in only):
        continue
    f = find_mhclo(stem)
    if not f:
        print('HL_WARD missing', base, stem)
        continue
    bpy.ops.wm.open_mainfile(filepath=str(BODY))
    body = next(o for o in bpy.data.objects if o.name.startswith('Body_') and '.' not in o.name)
    bb.HS.add_mhclo_asset(f, body, asset_type='Clothes', material_type='MAKESKIN')
    spec = '+'.join(dict.fromkeys(pieces.values())) + '@' + base
    r = export_v2.run(OUT, f'SKV2_{sex}', {stem: spec}, with_body=False)
    report[base] = r
    print('HL_WARD ok', base, {k: v for k, v in r.items() if k != '_textures'}, flush=True)
for stem, label in wardrobe.HAIR.get(sex, []):
    if only and stem not in only:
        continue
    f = find_mhclo(stem)
    if not f:
        print('HL_WARD missing hair', stem)
        continue
    bpy.ops.wm.open_mainfile(filepath=str(BODY))
    body = next(o for o in bpy.data.objects if o.name.startswith('Body_') and '.' not in o.name)
    bb.HS.add_mhclo_asset(f, body, asset_type='Hair', material_type='MAKESKIN')
    report[f'hair:{stem}'] = export_v2.run(OUT, f'SKV2_{sex}', {stem: f'hair@{stem}'}, with_body=False)
    print('HL_WARD ok hair', stem, flush=True)
prev = OUT / f'wardrobe_{sex}.json'
old = json.loads(prev.read_text()) if prev.exists() else {}
old.update(report)
prev.write_text(json.dumps(old, indent=1))
print('HL_WARD done', len(report))
