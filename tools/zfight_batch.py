"""Batch z-fighting fix for the building kit (see tools/zfight.py for the method): every landmark and facade module FBX in
art/buildings/meshes is scanned; those with coplanar overlapping parts are written, nudged, to
hollywoodland-ue/Saved/BuildStage/Fixed/<name>.fbx, and a round trip checks the bounds did not change scale.

Run: blender -b --python tools/zfight_batch.py -- [name-regex]
"""
import bpy, bmesh, sys, json, math, os, re, glob
from collections import defaultdict
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'art', 'buildings', 'meshes')
OUT = os.path.join(ROOT, 'hollywoodland-ue', 'Saved', 'BuildStage', 'Fixed')
args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
pat = re.compile(args[0] if args else r'^(LM_|BD_|[A-Z]+_(G|U)_|[A-Z]+_(Corner|Pilaster|Damaged|Entry|Parapet|Crest))')
os.makedirs(OUT, exist_ok=True)
CELL = 0.05


def load(path):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    return [o for o in bpy.data.objects if o.type == 'MESH']


def bounds(objs):
    pts = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    return [min(p[i] for p in pts) for i in range(3)], [max(p[i] for p in pts) for i in range(3)]


def scan_fix(o):
    bm = bmesh.new(); bm.from_mesh(o.data); bm.faces.ensure_lookup_table()
    # world-space metres: the object may carry an import scale
    mw = o.matrix_world
    part, pid = {}, 0
    for f in bm.faces:
        if f.index in part:
            continue
        stack = [f]; part[f.index] = pid
        while stack:
            g = stack.pop()
            for e in g.edges:
                for h in e.link_faces:
                    if h.index not in part:
                        part[h.index] = pid; stack.append(h)
        pid += 1
    size = defaultdict(int)
    for v in part.values():
        size[v] += 1
    wco = {v.index: mw @ v.co for v in bm.verts}
    groups = defaultdict(list)
    for f in bm.faces:
        n = (mw.to_3x3() @ f.normal).normalized()
        if n.z < -0.9:  # bottoms
            continue
        d = n.dot(wco[f.verts[0].index])
        groups[(round(n.x, 2), round(n.y, 2), round(n.z, 2), round(d / 0.002))].append((f, n))
    bad = defaultdict(set)
    hits = 0
    for key, fs in groups.items():
        parts = {part[f.index] for f, _ in fs}
        if len(parts) < 2:
            continue
        n = fs[0][1]
        t = n.orthogonal().normalized(); b = n.cross(t)
        cells = defaultdict(set)
        for f, _ in fs:
            p = part[f.index]
            pts = [(wco[v.index].dot(t) / CELL, wco[v.index].dot(b) / CELL) for v in f.verts]
            for k in range(1, len(pts) - 1):
                (x0, y0), (x1, y1), (x2, y2) = pts[0], pts[k], pts[k + 1]
                lo_x, hi_x = int(math.floor(min(x0, x1, x2))), int(math.ceil(max(x0, x1, x2)))
                lo_y, hi_y = int(math.floor(min(y0, y1, y2))), int(math.ceil(max(y0, y1, y2)))
                if (hi_x - lo_x) * (hi_y - lo_y) > 400000:
                    continue
                den = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2)
                if abs(den) < 1e-12:
                    continue
                for cx in range(lo_x, hi_x):
                    for cy in range(lo_y, hi_y):
                        px, py = cx + 0.5, cy + 0.5
                        l1 = ((y1 - y2) * (px - x2) + (x2 - x1) * (py - y2)) / den
                        l2 = ((y2 - y0) * (px - x2) + (x0 - x2) * (py - y2)) / den
                        if l1 >= 0 and l2 >= 0 and l1 + l2 <= 1:
                            cells[p].add((cx, cy))
        ps = sorted(parts, key=lambda p: size[p])
        for i, p in enumerate(ps):
            for q in ps[i + 1:]:
                if len(cells[p] & cells[q]) >= 3:
                    bad[p].add((n.x, n.y, n.z)); hits += 1
    if bad:
        inv = mw.to_3x3().inverted()
        push = 0.012  # metres: the smaller piece stands 12 mm proud of the one it lay flush with
        for v in bm.verts:
            off = Vector()
            for p in {part[f.index] for f in v.link_faces}:
                for nn in bad.get(p, ()):
                    off += Vector(nn)
            if off.length > 0:
                v.co += inv @ (off.normalized() * push)
        bm.to_mesh(o.data)
    bm.free()
    return hits, len(bad)


report = {}
for f in sorted(glob.glob(os.path.join(SRC, '*.fbx'))):
    name = os.path.basename(f)[:-4]
    if not pat.search(name):
        continue
    objs = load(f)
    b0 = bounds(objs)
    hits = parts = 0
    for o in objs:
        h, p = scan_fix(o)
        hits += h; parts += p
    if not parts:
        print('HL_ZB clean', name, flush=True)
        continue
    out = os.path.join(OUT, name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=out, use_selection=False, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Z', axis_up='Y',
                             mesh_smooth_type='FACE', path_mode='STRIP', bake_anim=False)
    b1 = bounds(load(out))
    ok = all(abs(b1[k][i] - b0[k][i]) < 0.05 for k in range(2) for i in range(3))
    report[name] = {'overlaps': hits, 'parts': parts, 'roundtrip_ok': ok, 'b0': b0, 'b1': b1}
    print('HL_ZB fixed', name, hits, parts, 'roundtrip', ok, flush=True)
json.dump(report, open(os.path.join(OUT, 'report.json'), 'w'), indent=1)
print('HL_ZB done', len(report))
