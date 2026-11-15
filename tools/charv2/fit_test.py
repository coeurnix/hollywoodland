"""Fits MHCLO garments onto a saved v2 body and renders them (review only).

Run: blender -b art/characters_v2/src/Body_<Sex>.blend --python tools/charv2/fit_test.py -- <tag> <mhclo> [<mhclo> ...]
Writes art/characters_v2/review/fit_<tag>_{front,back,side}.png and prints mesh stats (verts, loose parts, materials).
"""
import bpy, sys, math, bmesh
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_body as bb

args = sys.argv[sys.argv.index('--') + 1:]
tag, files = args[0], args[1:]
body = next(o for o in bpy.data.objects if o.name.startswith('Body_'))
for f in files:
    c = bb.HS.add_mhclo_asset(f, body, asset_type='Clothes', material_type='MAKESKIN')
    bm = bmesh.new()
    bm.from_mesh(c.data)
    seen, parts = set(), []
    for v in bm.verts:
        if v.index in seen:
            continue
        stack, n = [v], 0
        seen.add(v.index)
        while stack:
            x = stack.pop()
            n += 1
            for e in x.link_edges:
                o = e.other_vert(x)
                if o.index not in seen:
                    seen.add(o.index)
                    stack.append(o)
        parts.append(n)
    print(f'HL_FIT {Path(f).stem}: verts {len(c.data.vertices)} faces {len(c.data.polygons)} parts {sorted(parts, reverse=True)[:12]} '
          f'mats {[m.name for m in c.data.materials]} uvs {[u.name for u in c.data.uv_layers]}')
    bm.free()
h = 1.85 if 'Male' in body.name else 1.72
sex = 'male' if 'Male' in body.name else 'female'
sc, cam = bb.ensure_scene()
for name, loc in [('front', (0, -6, h * 0.5)), ('back', (0, 6, h * 0.5)), ('side', (6, 0, h * 0.5))]:
    cam.location = loc
    cam.rotation_euler = (Vector((0, 0, h * 0.5)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    cam.data.type = 'ORTHO'
    cam.data.ortho_scale = h * 1.1
    sc.render.filepath = str(bb.OUT / 'review' / f'fit_{tag}_{name}.png')
    bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(bb.OUT / 'src' / f'Fit_{tag}.blend'))
