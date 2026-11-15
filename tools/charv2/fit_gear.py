"""Belts, wrist pieces and cuirasses (gear.py's Rodin GLBs) fitted to the v2 bodies as skinned garments.

Each model is oriented by its principal axes, scaled to the body region it wraps (a band around the waist, the forearm,
the torso), pushed out so it clears the body and the clothes under it, and skinned with the weights of the nearest body
vertices of that region. Exports SKV2_<Sex>_<key>_<slot>.fbx and SKV2_<Sex>_<key>_D.png like the wardrobe pieces, and a
review render to review/gear/<Sex>_<key>.png.

Run: blender -b --python tools/charv2/fit_gear.py -- <Male|Female> [key,key]
"""
import bpy, sys, math
from pathlib import Path
import numpy as np
from mathutils import Vector, Matrix
from mathutils.kdtree import KDTree
from mathutils.bvhtree import BVHTree
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_body as bb
import export_v2

args = sys.argv[sys.argv.index('--') + 1:]
sex = args[0]
only = set(args[1].split(',')) if len(args) > 1 else set()
BODY = bb.OUT / 'src' / f'Body_{sex}.blend'
OUT = bb.OUT / 'export'
GEAR_DIR = bb.OUT / 'gear'

# key: slot, region kind and its parameters (metres)
GEAR = {
    'belt_slim': dict(slot='belt', kind='belt', dz=0.075, height=0.038, clear=0.014),
    'belt_wide': dict(slot='belt', kind='belt', dz=0.035, height=0.07, clear=0.02),
    'cuff': dict(slot='wrist', kind='wrist', sides=('l',), t=0.86, length=0.055, clear=0.012, axis='short'),
    'bracer': dict(slot='wrist', kind='wrist', sides=('l', 'r'), t=0.6, length=0.17, clear=0.02, axis='long'),
    'cuirass_m': dict(slot='armour', kind='torso', sex='Male', clear=0.035),
    'cuirass_f': dict(slot='armour', kind='torso', sex='Female', clear=0.035),
}
ARM = ('upperarm', 'lowerarm', 'hand', 'thumb', 'index', 'middle', 'ring', 'pinky')


def open_body():
    bpy.ops.wm.open_mainfile(filepath=str(BODY))
    body = next(o for o in bpy.data.objects if o.name.startswith('Body_') and '.' not in o.name)
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    return body, rig


def body_data(body):
    """World positions, bone weights {name: w} and polygons of the body's own surface (the 'body' group)."""
    me = body.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    names = {g.index: g.name for g in body.vertex_groups}
    gb = body.vertex_groups['body'].index
    mw = body.matrix_world
    pos, wts, ok = [], [], []
    for v in me.vertices:
        pos.append(mw @ v.co)
        w = {names[g.group]: g.weight for g in v.groups if g.weight > 0.0}
        ok.append(w.get('body', 0) > 0.5)
        wts.append(w)
    polys = [list(p.vertices) for p in me.polygons if all(ok[i] for i in p.vertices)]
    return pos, wts, ok, polys, gb


def is_arm(w, thr=0.3):
    return any(v > thr for k, v in w.items() if k.split('_')[0] in ARM)


def import_glb(key, body):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(GEAR_DIR / f'{key}.glb'))
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
    g = bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    g.name = f'{body.name}.{key}'
    for m in g.data.materials:
        if m and m.use_nodes:
            for n in list(m.node_tree.nodes):
                if n.type == 'TEX_IMAGE' and n.image and not n.image.filepath:
                    p = GEAR_DIR / f'{key}_{n.image.name}.png'
                    n.image.filepath_raw = str(p)
                    n.image.file_format = 'PNG'
                    n.image.save()
    return g


def axes(pts):
    """Principal axes (columns, largest variance first) and the centre."""
    a = np.array([tuple(p) for p in pts])
    c = a.mean(axis=0)
    w, v = np.linalg.eigh(np.cov((a - c).T))
    order = np.argsort(w)[::-1]
    return [Vector(v[:, i]) for i in order], Vector(c)


def frame_to(pts, src_axes, dst_axes):
    """Rotation taking the orthonormal src_axes onto dst_axes."""
    S = Matrix([tuple(a) for a in src_axes]).transposed()
    D = Matrix([tuple(a) for a in dst_axes]).transposed()
    return D @ S.inverted()


def extents(pts, ax):
    lo = [min(p.dot(a) for p in pts) for a in ax]
    hi = [max(p.dot(a) for p in pts) for a in ax]
    return lo, hi


def place(g, pts):
    inv = g.matrix_world.inverted()
    for v, p in zip(g.data.vertices, pts):
        v.co = inv @ p


def push_out(pts, tree, clear):
    """Moves every point that sits closer than clear to the body surface out along the surface normal."""
    out = []
    for p in pts:
        loc, n, _, d = tree.find_nearest(p)
        if loc is not None:
            s = (p - loc).dot(n)
            if s < clear:
                p = p + n * (clear - s)
        out.append(p)
    return out


def brass(g):
    """Per-vertex 'brass-ness' (yellow metal in the base colour map), for finding a buckle."""
    img = None
    for m in g.data.materials:
        if m and m.use_nodes:
            for n in m.node_tree.nodes:
                if n.type == 'TEX_IMAGE' and n.image and 'diffuse' in n.image.filepath_raw.lower():
                    img = n.image
    if img is None or not g.data.uv_layers:
        return None
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)
    uv = g.data.uv_layers.active.data
    score = np.zeros(len(g.data.vertices))
    for loop in g.data.loops:
        u, v = uv[loop.index].uv
        r, gg, b, _ = px[min(h - 1, max(0, int(v * h))), min(w - 1, max(0, int(u * w)))]
        y = max(0.0, min(r, gg) - b * 1.4) * (1.0 if gg > 0.55 * r else 0.0)  # yellow: red and green high, blue low
        score[loop.vertex_index] = max(score[loop.vertex_index], y)
    return score


def fit_belt(pts, spec, pos, wts, ok, rig, score=None):
    pel = rig.matrix_world @ rig.data.bones['pelvis'].head_local
    z0 = pel.z + spec['dz']
    band = [p for p, w, k in zip(pos, wts, ok) if k and abs(p.z - z0) < 0.015 and not is_arm(w, 0.15)]
    xs, ys = [p.x for p in band], [p.y for p in band]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    rx, ry = (max(xs) - min(xs)) / 2 + spec['clear'], (max(ys) - min(ys)) / 2 + spec['clear']
    ax, c = axes(pts)
    ring_axis = ax[2]  # the loop's thinnest direction is its axis
    if ring_axis.z < 0:
        ring_axis = -ring_axis
    # the buckle side carries more geometry: the centroid sits towards it
    a0, a1 = ax[0], ring_axis.cross(ax[0]).normalized()
    lo, hi = extents(pts, [a0, a1, ring_axis])
    mid = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2))
    cent = Vector((c.dot(a0), c.dot(a1), 0))
    front = (cent - Vector((mid.x, mid.y, 0)))
    if score is not None and score.sum() > 1e-3:  # the buckle: the brass-weighted centroid
        bc = sum((p * float(k) for p, k in zip(pts, score)), Vector()) / float(score.sum())
        front = Vector((bc.dot(a0), bc.dot(a1), 0)) - Vector((mid.x, mid.y, 0))
    f = (a0 * front.x + a1 * front.y)
    f = (f - ring_axis * f.dot(ring_axis)).normalized() if f.length > 1e-6 else a0
    side = ring_axis.cross(f).normalized()
    R = frame_to(pts, [side, f, ring_axis], [Vector((1, 0, 0)), Vector((0, -1, 0)), Vector((0, 0, 1))])
    q = [R @ (p - c) for p in pts]
    lo = [min(p[i] for p in q) for i in range(3)]
    hi = [max(p[i] for p in q) for i in range(3)]
    sx, sy, sz = 2 * rx / (hi[0] - lo[0]), 2 * ry / (hi[1] - lo[1]), spec['height'] / (hi[2] - lo[2])
    m = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2))
    out = [Vector(((p.x - m.x) * sx + cx, (p.y - m.y) * sy + cy, (p.z - m.z) * sz + z0)) for p in q]
    pool = [i for i, (p, w, k) in enumerate(zip(pos, wts, ok)) if k and abs(p.z - z0) < 0.12 and not is_arm(w, 0.15)]
    return out, pool


def fit_wrist(pts, spec, pos, wts, ok, rig, side, normals):
    b = rig.data.bones
    e = rig.matrix_world @ b[f'lowerarm_{side}'].head_local
    w = rig.matrix_world @ b[f'hand_{side}'].head_local
    A = (w - e).normalized()
    P = e + (w - e) * spec['t']
    ring = [p for p, wt, k in zip(pos, wts, ok) if k and abs((p - P).dot(A)) < 0.012 and ((p - P) - A * (p - P).dot(A)).length < 0.08]
    r = sum(((p - P) - A * (p - P).dot(A)).length for p in ring) / max(len(ring), 1)
    ax, c = axes(pts)
    # a tube's axis is the direction its surface normals avoid: the least-variance direction of the normals
    nm = np.array([tuple(n) for n in normals])
    w_, v_ = np.linalg.eigh(nm.T @ nm)
    axis = Vector(v_[:, int(np.argmin(w_))]).normalized()
    # orient the tube so its narrow end (the bracer's wrist end) points to the hand
    t = [(p - c).dot(axis) for p in pts]
    tl, th = min(t), max(t)
    rad = lambda sel: sum(((p - c) - axis * (p - c).dot(axis)).length for p in sel) / max(len(sel), 1)
    lo_end = [p for p, s in zip(pts, t) if s < tl + 0.15 * (th - tl)]
    hi_end = [p for p, s in zip(pts, t) if s > th - 0.15 * (th - tl)]
    if rad(lo_end) < rad(hi_end):
        axis = -axis
    # the decorated side (the watch face, the lacing) carries more geometry: point it at the back of the forearm
    perp = [(p - c) - axis * (p - c).dot(axis) for p in pts]
    deco = sum(perp, Vector()) / len(perp)
    deco = deco.normalized() if deco.length > 1e-6 else ax[1]
    out_dir = Vector((1 if side == 'l' else -1, 0, 0))
    back = (out_dir - A * out_dir.dot(A)).normalized()
    R = frame_to(pts, [axis, deco, axis.cross(deco).normalized()], [A, back, A.cross(back).normalized()])
    q = [R @ (p - c) for p in pts]
    B1 = back
    B2 = A.cross(back).normalized()
    al = [p.dot(A) for p in q]
    def half(d):  # half the 5th-95th percentile spread across the tube (the watch face and flares do not count)
        v = sorted(p.dot(d) for p in q)
        lo, hi = v[int(0.05 * len(v))], v[int(0.95 * len(v))]
        return (hi - lo) / 2, (hi + lo) / 2
    h1, m1 = half(B1)
    h2, m2 = half(B2)
    s_ax = spec['length'] / (max(al) - min(al))
    s1, s2 = (r + spec['clear']) / max(h1, 1e-6), (r + spec['clear']) / max(h2, 1e-6)
    mid = (max(al) + min(al)) / 2
    out = [P + A * ((p.dot(A) - mid) * s_ax) + B1 * ((p.dot(B1) - m1) * s1) + B2 * ((p.dot(B2) - m2) * s2) for p in q]
    pool = [i for i, (p, wt, k) in enumerate(zip(pos, wts, ok)) if k and (p - P).length < 0.16 and
            any(v > 0.2 for kk, v in wt.items() if kk in (f'lowerarm_{side}', f'hand_{side}'))]
    return out, pool


def fit_torso(pts, spec, pos, wts, ok, rig):
    b = rig.data.bones
    pel = rig.matrix_world @ b['pelvis'].head_local
    neck = rig.matrix_world @ b['neck_01'].head_local
    z_bot, z_top = pel.z + 0.0, neck.z + 0.03
    torso = [p for p, w, k in zip(pos, wts, ok) if k and z_bot < p.z < neck.z and not is_arm(w)]
    xs, ys = [p.x for p in torso], [p.y for p in torso]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    W, D = max(xs) - min(xs) + 2 * spec['clear'], max(ys) - min(ys) + 2 * spec['clear']
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    m = (lo + hi) / 2
    sx, sy, sz = W * 1.12 / (hi.x - lo.x), D / (hi.y - lo.y), (z_top - z_bot) / (hi.z - lo.z)
    out = [Vector(((p.x - m.x) * sx + cx, (p.y - m.y) * sy + cy, (p.z - lo.z) * sz + z_bot)) for p in pts]
    pool = [i for i, (p, w, k) in enumerate(zip(pos, wts, ok)) if k and z_bot - 0.08 < p.z < z_top + 0.05 and not is_arm(w)]
    return out, pool


def skin(g, out, pool, pos, wts, rig):
    bones = {bn.name for bn in rig.data.bones}
    kd = KDTree(len(pool))
    for j, i in enumerate(pool):
        kd.insert(pos[i], j)
    kd.balance()
    for vg in list(g.vertex_groups):
        g.vertex_groups.remove(vg)
    groups = {}
    for vi, p in enumerate(out):
        acc = {}
        near = kd.find_n(p, 4)
        tot = 0.0
        for _, j, d in near:
            k = 1.0 / max(d, 1e-4)
            tot += k
            for name, w in wts[pool[j]].items():
                if name in bones:
                    acc[name] = acc.get(name, 0.0) + w * k
        s = sum(acc.values())
        for name, w in acc.items():
            if w / s > 0.02:
                if name not in groups:
                    groups[name] = g.vertex_groups.new(name=name)
                groups[name].add([vi], w / s, 'REPLACE')
    mod = g.modifiers.new('Armature', 'ARMATURE')
    mod.object = rig
    g.parent = rig
    g.matrix_parent_inverse = rig.matrix_world.inverted()


def shot(key, focus, dist):
    sc, cam = bb.ensure_scene()
    sc.render.resolution_x = sc.render.resolution_y = 480
    cam.location = focus + Vector((dist * 0.55, -dist, dist * 0.15))
    cam.rotation_euler = (focus - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = 50
    d = bb.OUT / 'review' / 'gear'
    d.mkdir(parents=True, exist_ok=True)
    sc.render.filepath = str(d / f'{sex}_{key}.png')
    bpy.ops.render.render(write_still=True)


def one(key, spec):
    body, rig = open_body()
    pos, wts, ok, polys, _ = body_data(body)
    tree = BVHTree.FromPolygons(pos, polys)
    sides = spec.get('sides', (None,))
    parts = []
    for side in sides:
        g = import_glb(key, body)
        pts = [g.matrix_world @ v.co for v in g.data.vertices]
        if spec['kind'] == 'belt':
            out, pool = fit_belt(pts, spec, pos, wts, ok, rig, brass(g))
        elif spec['kind'] == 'wrist':
            nrm = [(g.matrix_world.to_3x3() @ v.normal).normalized() for v in g.data.vertices]
            out, pool = fit_wrist(pts, spec, pos, wts, ok, rig, side, nrm)
        else:
            out, pool = fit_torso(pts, spec, pos, wts, ok, rig)
        out = push_out(out, tree, spec['clear'])
        place(g, out)
        skin(g, out, pool, pos, wts, rig)
        parts.append(g)
    focus = sum((parts[0].matrix_world @ v.co for v in parts[0].data.vertices), Vector()) / len(parts[0].data.vertices)
    if len(parts) > 1:
        bpy.ops.object.select_all(action='DESELECT')
        for o in parts:
            o.select_set(True)
        bpy.context.view_layer.objects.active = parts[0]
        bpy.ops.object.join()
        parts[0].name = f'{body.name}.{key}'
    g = parts[0]
    shot(key, focus, 0.9 if spec['kind'] == 'torso' else 0.6)
    r = export_v2.run(OUT, f'SKV2_{sex}', {key: f"{spec['slot']}@{key}"}, with_body=False)
    print('HL_GEAR ok', key, r, flush=True)


for key, spec in GEAR.items():
    if (only and key not in only) or spec.get('sex', sex) != sex or not (GEAR_DIR / f'{key}.glb').exists():
        continue
    one(key, spec)
print('HL_GEAR done')
