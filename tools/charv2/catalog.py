"""Garment catalog: fits every MakeHuman garment in the local packs onto a v2 body, one at a time, and renders a labelled
front thumbnail of each (art/characters_v2/review/catalog_<sex>_NN.png sheets), to choose the garments for each look.

Run: blender -b art/characters_v2/src/Body_<Sex>.blend --python tools/charv2/catalog.py -- <Male|Female>
"""
import bpy, sys, math
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_body as bb

argv = sys.argv[sys.argv.index('--') + 1:]
sex = argv[0]
HAIR = len(argv) > 1 and argv[1] == 'hair'
SKIP = ('genital', 'underwear', 'bikini', 'swim', 'tube_top', 'crop', 'bra', 'mini_skirt', 'sci', 'cyborg', 'spider', 'rei_ayanami',
        'yoko', 'skinsuit', 'bodysuit', 'galactic', 'disco', 'flip_flop', 'kill_bill', 'sneaker', 'running', 'cycling', 'tennis',
        'deathnote', 'logo', 'camo', 'zombie', 'gogo', 'water_boots', 'xmas', 'bag_on_head', 'maga', 'skull', 'jeans_shorts')
files = sorted(set(str(p) for root in (bb.PACKS, bb.SYS / 'clothes') for p in Path(root).rglob('*.mhclo')))
files = [f for f in files if not any(k in Path(f).stem.lower() for k in SKIP) and '/hair' not in f.replace('\\', '/')
         and 'eyebrow' not in f and 'eyelash' not in f and '/eyes/' not in f.replace('\\', '/') and 'proxymeshes' not in f
         and 'teeth' not in f and 'tongue' not in f and 'bodyparts' not in f and 'glasses' not in f and 'equipment' not in f]
if HAIR:  # hair mode: every hair in the packs, head close-ups
    files = sorted(set(str(p) for root in (bb.PACKS, bb.SYS / 'hair') for p in Path(root).rglob('*.mhclo') if '/hair' in p.as_posix()))
print(f'HL_CAT {len(files)} garments')
body = next(o for o in bpy.data.objects if o.name.startswith('Body_') and '.' not in o.name)
sc, cam = bb.ensure_scene()
sc.render.resolution_x, sc.render.resolution_y = 360, 600
h = 1.85 if sex == 'Male' else 1.72
cam.location = (0, -6, h * 0.5)
cam.rotation_euler = (Vector((0, 0, h * 0.5)) - Vector(cam.location)).to_track_quat('-Z', 'Y').to_euler()
cam.data.type = 'ORTHO'
cam.data.ortho_scale = h * 1.08
if HAIR:
    sc.render.resolution_x, sc.render.resolution_y = 360, 360
    cam.location = (0.55, -1.25, h * 0.93)
    cam.rotation_euler = (Vector((0, 0, h * 0.92)) - Vector(cam.location)).to_track_quat('-Z', 'Y').to_euler()
    cam.data.type = 'PERSP'
    cam.data.lens = 85
out = bb.OUT / 'review' / ('catalog_hair' if HAIR else 'catalog') / sex
out.mkdir(parents=True, exist_ok=True)
done = []
for f in files:
    name = Path(f).stem
    if (out / f'{name}.png').exists():
        done.append(name)
        continue
    before = set(bpy.data.objects)
    try:
        bb.HS.add_mhclo_asset(f, body, asset_type='Hair' if HAIR else 'Clothes', material_type='MAKESKIN')
    except Exception as e:
        print('HL_CAT fail', name, e)
        continue
    sc.render.filepath = str(out / f'{name}.png')
    bpy.ops.render.render(write_still=True)
    for o in set(bpy.data.objects) - before:
        bpy.data.objects.remove(o, do_unlink=True)
    for m in list(body.modifiers):
        if m.name.startswith('Delete.'):
            body.modifiers.remove(m)
    done.append(name)
    print('HL_CAT ok', name, flush=True)
print('HL_CAT done', len(done))
