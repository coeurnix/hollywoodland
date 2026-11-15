"""Hats on the v2 heads, and hat-safe hair.

- MakeHuman hats are fitted by MHCLO; Rodin hats (hats.py GLBs) are scaled to the head's measured width by kind and seated
  at a kind-specific depth below the crown, centred over the skull. Both export as static meshes in body space
  (SKV2_<Sex>_<hat>_hat.fbx with <hat>_D.png), which HLCharV2 attaches to the head bone.
- Every hair style also exports a hat variant (SKV2_<Sex>_<stem>_hairhat.fbx): strands above the brim line are pressed
  in under the crown so no hat cuts through them, and hair below the brim is untouched.

Run: blender -b --python tools/charv2/fit_hats.py -- <Male|Female> [hats|hair]
"""
import bpy, sys, json, math
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_body as bb
import export_v2
import wardrobe

args = sys.argv[sys.argv.index('--') + 1:]
sex = args[0]
what = args[1] if len(args) > 1 else 'hats hair'
BODY = bb.OUT / 'src' / f'Body_{sex}.blend'
OUT = bb.OUT / 'export'
HATS = bb.OUT / 'hats'
# MakeHuman hats by id -> MHCLO stem
MH_HATS = {'newsboy': 'jujube_newsboy_cap', 'cloche': 'aethelraed_unraed_cloche_hat', 'fedora': 'fedora01', 'fedora_tilted': 'fedora_cocked',
           'm1_helmet': 'mrgreaterthan_m1_helmet', 'stunt_helmet': 'culturalibre_motorcycle_helmet', 'warrior_helmet': 'culturalibre_warrior_helmet',
           'great_helm': 'javherre_casco_caballero_templario_templar_knight_helmet'}
# Rodin hats: (width between the 5th and 95th percentiles / head width, rim depth under the crown in head heights)
RODIN = {'top_hat': (1.45, 0.30), 'feather_headband': (1.08, 0.24), 'chitin_tiara': (1.05, 0.26), 'flying_cap': (1.15, 0.58),
         'cowboy_hat': (2.0, 0.30), 'football_helmet': (1.25, 0.72), 'brodie_helmet': (1.7, 0.26), 'chitin_helmet': (1.25, 0.45),
         'miner_helmet': (1.4, 0.21), 'nurse_cap': (1.0, 0.22)}
BRIM = {'brodie_helmet'}  # placed by the brim's edge (the chin strap hangs below it)


def find_mhclo(stem):
    for root in (bb.PACKS, bb.SYS / 'clothes', bb.SYS / 'hair'):
        hits = list(Path(root).rglob(f'{stem}.mhclo'))
        if hits:
            return str(hits[0])
    return None


def head_metrics(body):
    """crown z, head centre (x, y), head width, head height from the body's head vertices (the body is in metres here)."""
    me = body.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    g = body.vertex_groups['body'].index
    pts = [body.matrix_world @ v.co for v in me.vertices if any(x.group == g and x.weight > 0.5 for x in v.groups)]
    top = max(p.z for p in pts)
    head = [p for p in pts if p.z > top - 0.24]
    band = [p for p in head if top - 0.13 < p.z < top - 0.08]  # around the temples
    cx = sum(p.x for p in band) / len(band)
    cy = sum(p.y for p in band) / len(band)
    w = max(p.x for p in band) - min(p.x for p in band)
    return top, Vector((cx, cy)), w, 0.24


def open_body():
    bpy.ops.wm.open_mainfile(filepath=str(BODY))
    return next(o for o in bpy.data.objects if o.name.startswith('Body_') and '.' not in o.name)


def import_rodin(key, body):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(HATS / f'{key}.glb'))
    new = [o for o in set(bpy.data.objects) - before if o.type == 'MESH']
    for o in set(bpy.data.objects) - before:
        if o.type != 'MESH':
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action='DESELECT')
    for o in new:
        o.select_set(True)
    bpy.context.view_layer.objects.active = new[0]
    if len(new) > 1:
        bpy.ops.object.join()
    h = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    h.name = f'{body.name}.{key}'
    # images packed in the GLB: write them out so export_v2.textures() can copy them
    for m in h.data.materials:
        if m and m.use_nodes:
            for n in m.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image and not n.image.filepath:
                    p = HATS / f'{key}_{n.image.name}.png'
                    n.image.filepath_raw = str(p)
                    n.image.file_format = 'PNG'
                    n.image.save()
    return h


def skull_tree(body):
    """BVH of the body's own surface (the 'body' group: no helper geometry such as the hair and tights helpers)."""
    me = body.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    g = body.vertex_groups['body'].index
    ok = [any(x.group == g and x.weight > 0.5 for x in v.groups) for v in me.vertices]
    mw = body.matrix_world
    verts = [mw @ v.co for v in me.vertices]
    polys = [list(p.vertices) for p in me.polygons if all(ok[i] for i in p.vertices)]
    return BVHTree.FromPolygons(verts, polys)


def seat(h, body, top, ctr, hw, hh, width_k, depth_k, brim=False):
    """Scales the hat's body (5th-95th percentile width, so plumes and straps do not count) to width_k head widths,
    centres it over the skull and puts its rim (8th percentile height) depth_k head heights under the crown."""
    pts = [h.matrix_world @ v.co for v in h.data.vertices]
    xs = sorted(p.x for p in pts); ys = sorted(p.y for p in pts); zs = sorted(p.z for p in pts)
    q = lambda a, f: a[min(len(a) - 1, int(f * len(a)))]
    s = hw * width_k / max(q(xs, 0.95) - q(xs, 0.05), 1e-6)
    rim = q(zs, 0.08)
    if brim:  # the brim's edge (the widest point) rather than the lowest vertices (a hanging chin strap)
        mx, my = (q(xs, 0.05) + q(xs, 0.95)) / 2, (q(ys, 0.05) + q(ys, 0.95)) / 2
        rim = max(pts, key=lambda p: (p.x - mx) ** 2 + (p.y - my) ** 2).z
    c = Vector(((q(xs, 0.05) + q(xs, 0.95)) / 2, (q(ys, 0.05) + q(ys, 0.95)) / 2, rim))
    inv = h.matrix_world.inverted()
    for v, p in zip(h.data.vertices, pts):
        v.co = inv @ ((p - c) * s + Vector((ctr.x, ctr.y + 0.005, top - hh * depth_k)))
    print(f'HL_HAT seat scale {s:.3f}')


def shot(key, top, ctr):
    """Front-three-quarter review render of the head with the hat (review/hats/<sex>_<key>.png)."""
    sc, cam = bb.ensure_scene()
    sc.render.resolution_x = sc.render.resolution_y = 420
    c = Vector((ctr.x, ctr.y, top - 0.08))
    cam.location = c + Vector((0.45, -0.75, 0.12))
    cam.rotation_euler = (c - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = 50
    d = bb.OUT / 'review' / 'hats'
    d.mkdir(parents=True, exist_ok=True)
    sc.render.filepath = str(d / f'{sex}_{key}.png')
    bpy.ops.render.render(write_still=True)


def hats():
    report = {}
    for key, stem in MH_HATS.items():
        f = find_mhclo(stem)
        if not f:
            print('HL_HAT missing', key)
            continue
        body = open_body()
        bb.HS.add_mhclo_asset(f, body, asset_type='Clothes', material_type='MAKESKIN')
        report[key] = export_v2.run(OUT, f'SKV2_{sex}', {stem: f'hat@{key}'}, with_body=False)
        print('HL_HAT ok', key, flush=True)
    for key, (wk, dk) in RODIN.items():
        if not (HATS / f'{key}.glb').exists():
            print('HL_HAT no model yet', key)
            continue
        body = open_body()
        top, ctr, hw, hh = head_metrics(body)
        h = import_rodin(key, body)
        seat(h, body, top, ctr, hw, hh, wk, dk, key in BRIM)
        shot(key, top, ctr)
        report[key] = export_v2.run(OUT, f'SKV2_{sex}', {key: f'hat@{key}'}, with_body=False)
        print('HL_HAT ok', key, flush=True)
    return report


def hair():
    for stem, _ in wardrobe.HAIR[sex]:
        if not stem:
            continue
        body = open_body()
        top, ctr, hw, hh = head_metrics(body)
        bb.HS.add_mhclo_asset(find_mhclo(stem), body, asset_type='Hair', material_type='MAKESKIN')
        hobj = next(o for o in bpy.data.objects if o.name.endswith('.' + stem))
        # bake the fit, then press everything above the brim line in under the crown (a dome a little smaller than the
        # inside of any hat): the brim sits about 0.32 head heights under the crown
        dg = bpy.context.evaluated_depsgraph_get()
        me = bpy.data.meshes.new_from_object(hobj.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
        brim = top - hh * 0.32
        mw = hobj.matrix_world
        for v in me.vertices:
            p = mw @ v.co
            if p.z > brim:
                t = (p.z - brim) / max(top + 0.03 - brim, 1e-6)
                d = Vector((p.x - ctr.x, p.y - ctr.y))
                lim = hw * 0.5 * (1.0 - 0.55 * t * t) + 0.004  # the skull's own taper towards the crown
                if d.length > lim:
                    d = d.normalized() * lim
                z = brim + (min(p.z, top + 0.01) - brim) * 0.92
                v.co = mw.inverted() @ Vector((ctr.x + d.x, ctr.y + d.y, z))
        for m in list(hobj.modifiers):
            if m.type != 'ARMATURE':
                hobj.modifiers.remove(m)
        if hobj.data.shape_keys:
            hobj.shape_key_clear()
        hobj.data = me
        export_v2.run(OUT, f'SKV2_{sex}', {stem: f'hairhat@{stem}'}, with_body=False)
        print('HL_HAT hair', stem, flush=True)


if 'hats' in what:
    r = hats()
    (OUT / f'hats_{sex}.json').write_text(json.dumps(r, indent=1))
if 'hair' in what:
    hair()
print('HL_HAT done')
