"""Box-fitted buildings from blocks + FLUX elevation plates (SPEC 15 projection fallback, without a bake).

    python -I art/buildings/src/block_building.py atlas <spec json>      pack the plates into textures/T_<atlas>.png
    blender -b --factory-startup -P art/buildings/src/block_building.py -- <spec json> [asset id ...]

Spec (src/blocks_<atlas>.json):
  {"atlas": "ATL_HH", "plates": ["HH_entrance", ...],            up to 8 plates, work/plates/<name>.png, 1024x512 each,
                                                                  slot i at column i%2, row i//2 of the 2048 atlas
   "assets": {"LM_X": {"size": [w, d, h], "tris": 10000, "fits_box": "...", "parts": [part, ...]}}}
  part: {"box": [x0, y0, z0, x1, y1, z1], "faces": {"front"|"back"|"left"|"right"|"top": face}}
        {"gable": [x0, y0, z0, x1, y1, z1], "ridge": "x"|"y", "plate": name}       (two roof slopes + two gable ends)
        {"pyramid": [x0, y0, z0, x1, y1, z1], "plate": name}
  face: "plate" (repeated every `pw` metres, default 18), or {"seq": [[plate, metres], ...]} laid left to right as seen
        from outside, or {"plate": p, "pw": m, "crop": [u0, u1, v0, v1]} (a sub-rectangle of the plate).
Front is -Y; left/right as seen from the front (-X / +X). Faces not listed are not built (they sit against something).
Planar mapping straight from the orthographic elevations = camera projection of the front/side views.
"""
import json, sys
from pathlib import Path

R = Path(__file__).resolve().parents[1]


def slot_rect(i):
    c, r = i % 2, i // 2
    return (c * 0.5, 1 - (r + 1) * 0.25, c * 0.5 + 0.5, 1 - r * 0.25)


def pack(spec_path):
    from PIL import Image
    spec = json.loads(Path(spec_path).read_text())
    at = Image.new('RGB', (2048, 2048), (90, 80, 70))
    for i, name in enumerate(spec['plates']):
        im = Image.open(R / 'work' / 'plates' / f'{name}.png').convert('RGB').resize((1024, 512), Image.LANCZOS)
        at.paste(im, ((i % 2) * 1024, (i // 2) * 512))
    out = R / 'textures' / f"T_{spec['atlas']}.png"; at.save(out); print('atlas', out)


if __name__ == '__main__' and len(sys.argv) > 2 and sys.argv[1] == 'atlas':
    pack(sys.argv[2]); sys.exit(0)

import bpy, bmesh
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
from asset_io import export_asset

args = sys.argv[sys.argv.index('--') + 1:]
spec = json.loads((R / args[0]).read_text())
only = set(args[1:])
SLOT = {n: i for i, n in enumerate(spec['plates'])}


def uv_of(plate, crop=None):
    u0, v0, u1, v1 = slot_rect(SLOT[plate])
    e = 1.5 / 2048   # stay off the plate edge (no bleeding at 1K)
    u0, v0, u1, v1 = u0 + e, v0 + e, u1 - e, v1 - e
    if crop:
        a, b, c, d = crop
        return (u0 + (u1 - u0) * a, v0 + (v1 - v0) * c, u0 + (u1 - u0) * b, v0 + (v1 - v0) * d)
    return (u0, v0, u1, v1)


def build(aid, a):
    bm = bmesh.new(); uvl = bm.loops.layers.uv.new('UVMap')

    def quad(p0, r, u, L, H, rect):
        # p0 bottom-left seen from outside, r/u unit vectors (r x u = outward normal)
        vs = [bm.verts.new(p0), bm.verts.new(p0 + r * L), bm.verts.new(p0 + r * L + u * H), bm.verts.new(p0 + u * H)]
        f = bm.faces.new(vs); u0, v0, u1, v1 = rect
        for l, uv in zip(f.loops, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]): l[uvl].uv = uv

    def tri(p0, p1, p2, uvs):
        f = bm.faces.new([bm.verts.new(p0), bm.verts.new(p1), bm.verts.new(p2)])
        for l, uv in zip(f.loops, uvs): l[uvl].uv = uv

    def strip(p0, r, u, L, H, face):
        if isinstance(face, str): face = {'plate': face}
        if 'seq' in face:
            tot = sum(m for _, m in face['seq']); t = 0.0
            for plate, m in face['seq']:
                w = L * m / tot; quad(p0 + r * t, r, u, w, H, uv_of(plate)); t += w
            return
        pw = face.get('pw', 18.0); n = max(1, round(L / pw)); w = L / n
        for i in range(n): quad(p0 + r * (w * i), r, u, w, H, uv_of(face['plate'], face.get('crop')))

    X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
    for p in a['parts']:
        if 'box' in p:
            x0, y0, z0, x1, y1, z1 = p['box']; H = z1 - z0; F = p['faces']
            if 'front' in F: strip(Vector((x0, y0, z0)), X, Z, x1 - x0, H, F['front'])
            if 'back' in F: strip(Vector((x1, y1, z0)), -X, Z, x1 - x0, H, F['back'])
            if 'left' in F: strip(Vector((x0, y1, z0)), -Y, Z, y1 - y0, H, F['left'])
            if 'right' in F: strip(Vector((x1, y0, z0)), Y, Z, y1 - y0, H, F['right'])
            if 'top' in F:
                face = F['top'] if isinstance(F['top'], dict) else {'plate': F['top']}
                pw = face.get('pw', 18.0); nx = max(1, round((x1 - x0) / pw)); ny = max(1, round((y1 - y0) / (pw / 2)))
                wx, wy = (x1 - x0) / nx, (y1 - y0) / ny
                for i in range(nx):
                    for j in range(ny): quad(Vector((x0 + i * wx, y0 + j * wy, z1)), X, Y, wx, wy, uv_of(face['plate'], face.get('crop')))
        elif 'gable' in p:
            x0, y0, z0, x1, y1, z1 = p['gable']; rect = uv_of(p['plate'])
            if p.get('ridge', 'x') == 'x':
                ym = (y0 + y1) / 2; s1 = Vector((0, ym - y0, z1 - z0)); L1 = s1.length
                strip(Vector((x0, y0, z0)), X, s1.normalized(), x1 - x0, L1, p['plate'])
                strip(Vector((x1, y1, z0)), -X, Vector((0, ym - y1, z1 - z0)).normalized(), x1 - x0, L1, p['plate'])
                e = p.get('end', p['plate']); u0, v0, u1, v1 = uv_of(e, p.get('end_crop'))
                tri((x0, y1, z0), (x0, y0, z0), (x0, ym, z1), [(u0, v0), (u1, v0), ((u0 + u1) / 2, v1)])
                tri((x1, y0, z0), (x1, y1, z0), (x1, ym, z1), [(u0, v0), (u1, v0), ((u0 + u1) / 2, v1)])
            else:
                xm = (x0 + x1) / 2; s1 = Vector((xm - x0, 0, z1 - z0)); L1 = s1.length
                strip(Vector((x0, y1, z0)), -Y, s1.normalized(), y1 - y0, L1, p['plate'])
                strip(Vector((x1, y0, z0)), Y, Vector((xm - x1, 0, z1 - z0)).normalized(), y1 - y0, L1, p['plate'])
                e = p.get('end', p['plate']); u0, v0, u1, v1 = uv_of(e, p.get('end_crop'))
                tri((x0, y0, z0), (x1, y0, z0), (xm, y0, z1), [(u0, v0), (u1, v0), ((u0 + u1) / 2, v1)])
                tri((x1, y1, z0), (x0, y1, z0), (xm, y1, z1), [(u0, v0), (u1, v0), ((u0 + u1) / 2, v1)])
        elif 'pyramid' in p:
            x0, y0, z0, x1, y1, z1 = p['pyramid']; u0, v0, u1, v1 = uv_of(p['plate'])
            apex = ((x0 + x1) / 2, (y0 + y1) / 2, z1); c = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)]
            for i in range(4): tri(c[i], c[(i + 1) % 4], apex, [(u0, v0), (u1, v0), ((u0 + u1) / 2, v1)])
    me = bpy.data.meshes.new(aid); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(aid, me); bpy.context.scene.collection.objects.link(ob)
    mat = bpy.data.materials.new('M_' + spec['atlas']); mat.use_nodes = True; me.materials.append(mat)
    t = mat.node_tree.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(str(R / 'textures' / f"T_{spec['atlas']}.png"), check_existing=True)
    mat.node_tree.links.new(t.outputs['Color'], mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
    meta = {'id': aid, 'kind': 'landmark', 'material': 'M_' + spec['atlas'], 'texture': 'atlas:' + spec['atlas'], 'emissive': None,
            'nominal': a['size'], 'origin': 'footprint-centre-ground'}
    entry, audit = export_asset(ob, meta, a.get('tris', 10000))
    # open shells by design (faces against the ground or a neighbour are omitted, SPEC 3), so boundary edges are expected
    res_p = R / 'work' / 'fit_results.json'; res = json.loads(res_p.read_text()) if res_p.exists() else {}
    res[aid] = {**entry, 'audit': audit}; res_p.write_text(json.dumps(res, indent=1)); print('BLOCK', aid, entry['tris'], entry['bounds'])


bpy.ops.wm.read_factory_settings(use_empty=True)
for aid, a in spec['assets'].items():
    if not only or aid in only: build(aid, a)
