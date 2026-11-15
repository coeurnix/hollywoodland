"""Finds (and optionally fixes) z-fighting in a building mesh: faces of different loose parts lying in the same plane and
overlapping. The fix nudges each smaller part's offending faces' whole part outward along that face normal by a few
millimetres, so the part in front always wins the depth test.

Run: blender -b --python tools/zfight.py -- <in.fbx|glb> [--fix out.fbx] [--report out.json]
"""
import bpy, bmesh, sys, json, math
from collections import defaultdict
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
src = args[0]
fix = args[args.index('--fix') + 1] if '--fix' in args else None
rep = args[args.index('--report') + 1] if '--report' in args else None

bpy.ops.wm.read_factory_settings(use_empty=True)
if src.lower().endswith('.glb'):
    bpy.ops.import_scene.gltf(filepath=src)
else:
    bpy.ops.import_scene.fbx(filepath=src)
objs = [o for o in bpy.data.objects if o.type == 'MESH']
found = []
for o in objs:
    bm = bmesh.new(); bm.from_mesh(o.data)
    bm.faces.ensure_lookup_table()
    # loose parts
    part = {}
    pid = 0
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
    for k, v in part.items():
        size[v] += 1
    scale = max(o.dimensions) or 1
    eps = scale * 2e-5
    groups = defaultdict(list)
    for f in bm.faces:
        if f.calc_area() < (scale * 1e-4) ** 2:
            continue
        n = f.normal.normalized()
        d = n.dot(f.verts[0].co)
        key = (round(n.x, 2), round(n.y, 2), round(n.z, 2), round(d / max(eps, 1e-9)))
        groups[key].append(f)
    bad = defaultdict(set)  # part -> normals to push along
    for key, fs in groups.items():
        parts = {part[f.index] for f in fs}
        if len(parts) < 2:
            continue
        # overlap test in the plane: rasterise each part's faces on a 5 cm grid, count shared cells
        n = fs[0].normal.normalized()
        if n.y < -0.9:  # bottoms (glTF y up): never seen
            continue
        t = n.orthogonal().normalized(); b = n.cross(t)
        cell = 0.05
        cells = defaultdict(set)
        for f in fs:
            p = part[f.index]
            pts = [(v.co.dot(t) / cell, v.co.dot(b) / cell) for v in f.verts]
            # triangles of a fan; sample the cell centres inside each
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
                shared = len(cells[p] & cells[q])
                if shared >= 4:
                    bad[p].add((round(n.x, 3), round(n.y, 3), round(n.z, 3)))
                    c = sum((v.co for f in fs if part[f.index] == p for v in f.verts), Vector()) / max(1, sum(len(f.verts) for f in fs if part[f.index] == p))
                    c = o.matrix_world @ c
                    found.append({'obj': o.name, 'part': p, 'other': q, 'normal': [round(x, 2) for x in n], 'at': [round(c.x, 2), round(c.y, 2), round(c.z, 2)], 'area_m2': round(shared * cell * cell, 3)})
    if fix and bad:
        push = scale * 4e-4  # a few millimetres on a building
        for f in bm.faces:
            pass
        for v in bm.verts:
            ps = {part[f.index] for f in v.link_faces}
            off = Vector()
            for p in ps:
                for nn in bad.get(p, ()):
                    off += Vector(nn)
            if off.length > 0:
                v.co += off.normalized() * push
        bm.to_mesh(o.data)
    bm.free()
print('HL_ZF', len(found), 'overlaps')
for r in found[:40]:
    print('HL_ZF', r)
if rep:
    json.dump(found, open(rep, 'w'), indent=1)
if fix:
    bpy.ops.export_scene.fbx(filepath=fix, use_selection=False, apply_unit_scale=True, bake_space_transform=False, mesh_smooth_type='FACE', path_mode='COPY', embed_textures=False)
    print('HL_ZF wrote', fix)
