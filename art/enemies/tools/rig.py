# Rig and animate a static creature mesh procedurally, then export FBX (Unreal) + GLB (preview) + textures.
# Usage: blender -b --factory-startup -P rig.py -- <spec.json> <in.glb> <out_dir>
# spec: {"name": "InvaderDrone", "kind": "insect|flier|quad|sac", "yaw": 0, "size": 2.0, "legs": 6,
#        "arms": false, "clips": ["Idle", ...], "mirror_fix": false}
import bpy, sys, os, json, math
import numpy as np
from mathutils import Vector, Quaternion, Matrix

argv = sys.argv[sys.argv.index('--') + 1:]
SPEC = json.load(open(argv[0])); SRC, OUT = argv[1], argv[2]
NAME, KIND = SPEC['name'], SPEC['kind']
FPS = 30
os.makedirs(os.path.join(OUT, 'textures'), exist_ok=True)

# ------------------------------------------------------------------ load and normalise
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.fps = FPS
bpy.ops.import_scene.gltf(filepath=SRC)
meshes = [o for o in sc.objects if o.type == 'MESH']
for o in sc.objects: o.select_set(o.type == 'MESH')
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1: bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
for o in list(sc.objects):
    if o is not body: bpy.data.objects.remove(o)
body.parent = None
body.data.transform(body.matrix_world); body.matrix_world = Matrix.Identity(4)
body.data.transform(Matrix.Rotation(math.radians(SPEC.get('pitch', 0)), 4, 'X'))   # some Rodin outputs lie on their back plane
body.data.transform(Matrix.Rotation(math.radians(SPEC.get('roll', 0)), 4, 'Y'))
body.data.transform(Matrix.Rotation(math.radians(SPEC.get('yaw', 0)), 4, 'Z'))
V = np.array([v.co[:] for v in body.data.vertices])
lo, hi = V.min(0), V.max(0)
s = SPEC['size'] / (hi[0] - lo[0])           # size = length along X (nose to tail)
body.data.transform(Matrix.Diagonal((s, s, s, 1)) @ Matrix.Translation((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2])))
body.name = body.data.name = f'SK_{NAME}'
V = np.array([v.co[:] for v in body.data.vertices])
lo, hi = V.min(0), V.max(0)
L, W, H = hi - lo
print(f'RIG {NAME}: {len(V)} verts, L {L:.2f} W {W:.2f} H {H:.2f}')

# ------------------------------------------------------------------ measure the body
def kmeans(P, k, init, iters=30):
    C = np.array(init, float)
    for _ in range(iters):
        lab = np.argmin(((P[:, None, :] - C[None]) ** 2).sum(-1), 1)
        for j in range(k):
            if (lab == j).any(): C[j] = P[lab == j].mean(0)
    return C, lab

centre = V[np.abs(V[:, 1]) < 0.12 * W]
_cz = centre[(centre[:, 2] > 0.08 * H) & (centre[:, 0] > lo[0] + 0.2 * (hi[0] - lo[0])) & (centre[:, 0] < hi[0] - 0.2 * (hi[0] - lo[0]))]
zc = float(np.median(_cz[:, 2])) if len(_cz) else 0.5 * H
z_top = float(np.quantile(_cz[:, 2], 0.97)) if len(_cz) else H     # top of the back, ignoring antennae and horns at the ends
def z_at(x, band=0.06):
    sel = centre[np.abs(centre[:, 0] - x) < band * L]
    return float(np.median(sel[:, 2])) if len(sel) else zc

def _shift(m, dx, dy):
    o = np.zeros_like(m); h, w = m.shape
    o[max(dy, 0):h + min(dy, 0), max(dx, 0):w + min(dx, 0)] = m[max(-dy, 0):h + min(-dy, 0), max(-dx, 0):w + min(-dx, 0)]
    return o
def _dilate(m, r):
    o = m.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dx * dx + dy * dy <= r * r: o |= _shift(m, dx, dy)
    return o
def _erode(m, r): return ~_dilate(~m, r)

def leg_components(r=10, zmax=None):
    """Top-view raster: opening removes thin legs, leaving the body; what's left over are legs (and antennae)."""
    n = 140; cell = max(L, W) * 1.02 / n
    Vs = V if zmax is None else V[V[:, 2] < zmax]          # a low slab separates legs hidden under a wide body
    ix = ((Vs[:, 0] - lo[0]) / cell).astype(int).clip(0, n - 1); iy = ((Vs[:, 1] - lo[1]) / cell).astype(int).clip(0, n - 1)
    occ = np.zeros((n, n), bool); occ[iy, ix] = True
    occ = _erode(_dilate(occ, 3), 3)                        # close gaps between sparse vertices
    body_m = _dilate(_erode(occ, r), r + 2)                 # opening wipes out anything thinner than 2r cells (legs)
    legm = occ & ~body_m
    yc = (np.arange(n) + 0.5) * cell + lo[1]                # cut the centreline band: waists, necks and antenna roots
    legm[np.abs(yc) < 0.1 * W, :] = False
    if os.environ.get('RIG_DEBUG') or SPEC.get('debug'):
        img = (occ * 80 + body_m * 80 + legm * 95 - body_m * occ * 0).astype(np.uint8)[::-1]
        with open(os.path.join(OUT, f'{NAME}_raster.pgm'), 'wb') as fh: fh.write(b'P5 %d %d 255\n' % (n, n) + img.tobytes())
    lab = -np.ones((n, n), int); comps = []
    for y0, x0 in zip(*np.nonzero(legm)):
        if lab[y0, x0] >= 0: continue
        k = len(comps); stack = [(y0, x0)]; lab[y0, x0] = k; cells = []
        while stack:
            y, x = stack.pop(); cells.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < n and 0 <= xx < n and legm[yy, xx] and lab[yy, xx] < 0: lab[yy, xx] = k; stack.append((yy, xx))
        comps.append(cells)
    vlab = lab[iy, ix]
    out = []
    for k in range(len(comps)):
        P = Vs[vlab == k]
        if len(P) < 12: continue
        out.append(P)
    return out, body_m, cell

def find_feet(per_side):
    cx0 = float(np.median(centre[:, 0])) if len(centre) else 0.0
    if KIND != 'quad':
        reach = max(lo[2] + 0.9 * (zc - lo[2]), 0.3 * H)
        for r, slab in [(10, None), (14, None), (10, 0.6), (14, 0.6), (8, 0.4), (12, 0.4)]:
            comps, _, _ = leg_components(r, None if slab is None else lo[2] + slab * (zc - lo[2]))
            big = max((len(P) for P in comps), default=0)
            legs = [P for P in comps if P[:, 2].min() < reach and len(P) >= 0.2 * big]
            print('STRIPS', NAME, f'r {r} slab {slab} zc {zc:.2f}', [(len(P), round(float(np.median(P[:, 0])), 2), round(float(np.median(P[:, 1])), 2), round(float(P[:, 2].min()), 2)) for P in comps])
            feet = []
            for side in (1, -1):
                mine = sorted([P for P in legs if np.median(P[:, 1]) * side > 0], key=len, reverse=True)[:per_side]
                if len(mine) < per_side: break
                mine.sort(key=lambda P: -np.median(P[:, 0]))
                for j, P in enumerate(mine):
                    d = np.hypot(P[:, 0] - cx0, P[:, 1]) - 2.0 * (P[:, 2] - lo[2])
                    tip = P[np.argmax(d)]
                    root = P[np.argmin(np.hypot(P[:, 0] - cx0, P[:, 1]))]
                    feet.append((side, j, Vector((tip[0], tip[1], max(tip[2], 0.0))), Vector(root)))
            if len(feet) == 2 * per_side:
                print('FEET', NAME, 'raster', [(s_, j, tuple(round(v, 2) for v in f)) for s_, j, f, r_ in feet])
                return feet
        print('FEET', NAME, f'raster found {len(legs)} leg strips, falling back')
    tries = [(0.22, 0.0), (0.15, 0.1), (0.1, 0.25)] if KIND != 'quad' else [(0.02, 0.12), (0.02, 0.2), (0.02, 0.3)]
    for latf, hf in tries:
        feet, ok = [], True
        for side in (1, -1):
            if KIND != 'quad': sel = (V[:, 1] * side > latf * W) & (V[:, 2] < lo[2] + (0.55 + hf) * (zc - lo[2]))
            else: sel = (V[:, 1] * side > latf * W) & (V[:, 2] < lo[2] + hf * H)
            P = V[sel]
            if len(P) < per_side * 6: ok = False; break
            init = [[np.quantile(P[:, 0], q), np.median(P[:, 1])] for q in np.linspace(0.9, 0.1, per_side)]
            C, lab = kmeans(P[:, :2], per_side, init)
            if min((lab == j).sum() for j in range(per_side)) < 4: ok = False; break
            for j, c in enumerate(np.argsort(-C[:, 0])):
                Q = P[lab == c]
                if KIND != 'quad':
                    tip = Q[np.argmax(np.hypot(Q[:, 0] - cx0, Q[:, 1]) - 3.0 * (Q[:, 2] - lo[2]))]
                else:
                    tip = Q.mean(0); tip[2] = lo[2]
                feet.append((side, j, Vector((tip[0], tip[1], max(tip[2], 0.0))), None))
        if ok:
            print('FEET', NAME, 'kmeans', [(s, j, tuple(round(v, 2) for v in f)) for s, j, f, r in feet])
            return feet
    raise SystemExit(f'feet not found for {NAME}')

# ------------------------------------------------------------------ build the armature
arm_data = bpy.data.armatures.new('Armature')
arm = bpy.data.objects.new('Armature', arm_data)
sc.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
EB = arm_data.edit_bones
def bone(name, head, tail, parent=None, deform=True):
    b = EB.new(name); b.head = Vector(head); b.tail = Vector(tail)
    if (b.tail - b.head).length < 1e-4: b.tail = b.head + Vector((0, 0, 0.02 * H))
    b.parent = EB.get(parent) if parent else None; b.use_deform = deform
    return b

bone('root', (0, 0, 0), (0, 0, 0.3 * H), deform=False)
LEGS, ARMS, WINGS, TAILS = [], [], [], []
xs = V[:, 0]
if KIND in ('insect', 'flier'):
    feet = find_feet(SPEC.get('legs', 6) // 2) if KIND == 'insect' else []
    hip_x = [f[2].x for f in feet] or [0.0]
    x_tf = (max(hip_x) + 0.12 * L) if feet else 0.15 * L
    x_tr = (min(hip_x) - 0.06 * L) if feet else -0.12 * L
    x_tf, x_tr = min(x_tf, hi[0] - 0.12 * L), max(x_tr, lo[0] + 0.15 * L)
    zh = float(np.median(V[xs > hi[0] - 0.08 * L][:, 2])); za = float(np.median(V[xs < lo[0] + 0.08 * L][:, 2]))
    bone('thorax', (x_tr, 0, z_at(x_tr)), (x_tf, 0, z_at(x_tf)), 'root')
    bone('head', (x_tf, 0, z_at(x_tf)), (hi[0] - 0.02 * L, 0, zh), 'thorax')
    bone('abdomen', (x_tr, 0, z_at(x_tr)), (lo[0] + 0.03 * L, 0, za), 'thorax')
    cx = 0.5 * (x_tf + x_tr)
    for side, j, foot, root in feet:
        S = 'L' if side > 0 else 'R'
        hz = z_at(foot.x) - 0.05 * H
        hip = Vector((cx + 0.55 * (foot.x - cx), side * max(0.3 * abs(foot.y), 0.07 * W), hz))
        if root is not None:   # strip root is where the leg leaves the body outline; the hip sits a bit further in
            hip = Vector((root.x * 0.8 + cx * 0.2, root.y * 0.6, min(max(root.z, 0.5 * zc), zc)))
        d2 = Vector((foot.x - hip.x, foot.y - hip.y, 0)); ln = d2.length; d2.normalize()
        rel = V[:, :2] - np.array([hip.x, hip.y]); t = rel @ np.array([d2.x, d2.y]) / max(ln, 1e-6)
        lat = np.abs(rel @ np.array([-d2.y, d2.x]))
        cand = V[(t > 0.25) & (t < 0.75) & (lat < 0.14 * ln) & (V[:, 1] * side > abs(hip.y))]
        if len(cand) >= 3:
            top = cand[np.argsort(cand[:, 2])[-5:]].mean(0); knee = Vector((top[0], top[1], max(top[2] - 0.02 * H, hz * 0.6)))
        else:
            knee = (hip + foot) / 2 + Vector((0, 0, 0.3 * ln))
        n = f'leg_{S}{j + 1}'
        bone(n + '_upper', hip, knee, 'thorax'); bone(n + '_lower', knee, foot + Vector((0, 0, 0.01 * H)), n + '_upper')
        LEGS.append(dict(name=n, side=side, idx=j, hip=hip, foot=foot))
    if KIND == 'flier' or SPEC.get('wings'):
        for side in (1, -1):
            S = 'L' if side > 0 else 'R'
            Wv = V[(V[:, 1] * side > 0.22 * W) & (V[:, 2] > zc - 0.05 * H)]
            tip = Wv[np.argmax(np.abs(Wv[:, 1]))]
            root = Vector((cx + 0.1 * L, side * 0.1 * W, zc + 0.08 * H)); tipv = Vector(tip)
            mid = root.lerp(tipv, 0.5)
            bone(f'wing_{S}_1', root, mid, 'thorax'); bone(f'wing_{S}_2', mid, tipv, f'wing_{S}_1')
            WINGS.append(dict(name=f'wing_{S}', side=side))
    if SPEC.get('arms'):
        for side in (1, -1):
            S = 'L' if side > 0 else 'R'
            A = V[(V[:, 0] > x_tf - 0.05 * L) & (V[:, 1] * side > 0.12 * W) & (V[:, 2] > zc + 0.05 * H)]
            sh = Vector((x_tf, side * 0.12 * W, zc + 0.05 * H))
            tip = Vector(A[np.argmax(((A - np.array(sh)) ** 2).sum(1))]) if len(A) else sh + Vector((0.25 * L, side * 0.1 * W, 0.3 * H))
            el = sh.lerp(tip, 0.5) + Vector((0, side * 0.03 * W, 0.08 * H))
            bone(f'arm_{S}_upper', sh, el, 'thorax'); bone(f'arm_{S}_lower', el, tip, f'arm_{S}_upper')
            ARMS.append(dict(name=f'arm_{S}', side=side))
elif KIND == 'quad':
    feet = find_feet(2)
    fx = [f[2].x for f in feet]; x_front, x_rear = max(fx), min(fx)
    z_sh, z_hip = z_at(x_front), z_at(x_rear)
    x_n = x_front + 0.35 * (hi[0] - x_front)
    z_n = float(np.quantile(V[np.abs(xs - x_n) < 0.05 * L][:, 2], 0.6))
    z_nose = float(np.median(V[xs > hi[0] - 0.04 * L][:, 2]))
    bone('pelvis', (x_rear, 0, z_hip), ((x_rear + x_front) / 2, 0, (z_hip + z_sh) / 2), 'root')
    bone('chest', ((x_rear + x_front) / 2, 0, (z_hip + z_sh) / 2), (x_front, 0, z_sh), 'pelvis')
    bone('neck', (x_front, 0, z_sh), (x_n, 0, z_n), 'chest')
    bone('head', (x_n, 0, z_n), (hi[0], 0, z_nose), 'neck')
    T = V[xs < x_rear - 0.06 * L]
    if len(T) > 20:
        tip = T[np.argmin(T[:, 0])]; base = Vector((x_rear - 0.06 * L, 0, z_hip + 0.03 * H))
        midp = T[np.abs(T[:, 0] - (base.x + tip[0]) / 2) < 0.04 * L]
        mid = Vector(midp.mean(0)) if len(midp) else base.lerp(Vector(tip), 0.5)
        mid.y = 0
        bone('tail_1', base, mid, 'pelvis'); bone('tail_2', mid, (tip[0], 0, tip[2]), 'tail_1'); TAILS = ['tail_1', 'tail_2']
    for side, j, foot, _r in feet:
        S = 'L' if side > 0 else 'R'; F = 'F' if foot.x > 0.5 * (x_front + x_rear) else 'H'
        top = Vector((foot.x, foot.y * 0.85, (z_sh if F == 'F' else z_hip) - 0.02 * H))
        knee = Vector((foot.x + (0.02 if F == 'F' else -0.04) * L, foot.y * 0.95, top.z * 0.45))
        ank = Vector((foot.x, foot.y, 0.1 * top.z))
        n = f'leg_{F}{S}'
        bone(n + '_upper', top, knee, 'chest' if F == 'F' else 'pelvis'); bone(n + '_lower', knee, ank, n + '_upper')
        bone(n + '_foot', ank, foot + Vector((0.04 * L, 0, 0.01 * H)), n + '_lower')
        LEGS.append(dict(name=n, side=side, front=F == 'F', foot=foot))
elif KIND == 'sac':
    bone('sac', (0, 0, 0.05 * H), (0, 0, H), 'root')
bpy.ops.object.mode_set(mode='OBJECT')

# ------------------------------------------------------------------ skin
bpy.ops.object.select_all(action='DESELECT')
body.select_set(True); arm.select_set(True); bpy.context.view_layer.objects.active = arm
bpy.ops.object.parent_set(type='ARMATURE_NAME')   # empty groups; weights computed below
deform = [b for b in arm_data.bones if b.use_deform]
A = np.array([b.head_local for b in deform]); B = np.array([b.tail_local for b in deform])
AB = B - A; ab2 = np.maximum((AB ** 2).sum(1), 1e-9)
Vw = np.array([v.co[:] for v in body.data.vertices])
t = np.clip(((Vw[:, None, :] - A[None]) * AB[None]).sum(2) / ab2[None], 0, 1)
D = np.linalg.norm(A[None] + t[..., None] * AB[None] - Vw[:, None, :], axis=2)
# a bone's reach scales with its length; body bones get a little extra so the trunk stays rigid
reach = np.sqrt(ab2) * np.array([1.4 if b.name in ('thorax', 'abdomen', 'pelvis', 'chest', 'sac') else 1.0 for b in deform])
Dn = D / np.maximum(reach, 0.05 * L)[None]
Wt = np.exp(-(Dn / 0.35) ** 2) + 1e-12
Wt[D > (D.min(1, keepdims=True) * 1.8 + 0.02 * L)] = 0          # only bones near the closest one share a vertex
top = np.argsort(-Wt, 1)[:, :3]
for j, b in enumerate(deform): body.vertex_groups.new(name=b.name) if b.name not in body.vertex_groups else None
for i in range(len(Vw)):
    w = Wt[i, top[i]]; w = w / w.sum() if w.sum() > 0 else np.array([1, 0, 0])
    for k, j in enumerate(top[i]):
        if w[k] > 0.02: body.vertex_groups[deform[j].name].add([i], float(w[k]), 'REPLACE')
unw = 0
bpy.context.view_layer.objects.active = body
bpy.ops.object.vertex_group_normalize_all(lock_active=False)
print(f'SKIN {NAME}: {len(deform)} deform bones, {unw} verts filled by nearest bone')

# ------------------------------------------------------------------ animation helpers
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='POSE')
PB = arm.pose.bones
for p in PB: p.rotation_mode = 'QUATERNION'
REST = {b.name: b.matrix_local.to_3x3() for b in arm_data.bones}
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
def q(axis, deg): return Quaternion(axis, math.radians(deg))
def smooth(a, b, t): t = min(1, max(0, (t - a) / (b - a))); return t * t * (3 - 2 * t)
def env(t, pts):  # piecewise smooth envelope through (time, value) points
    for (t0, v0), (t1, v1) in zip(pts, pts[1:]):
        if t <= t1: return v0 + (v1 - v0) * smooth(t0, t1, t)
    return pts[-1][1]

def lift_axis(leg):
    d = Vector((leg['foot'].x - leg.get('hip', Vector()).x, leg['foot'].y - leg.get('hip', Vector()).y, 0))
    if d.length < 1e-6: d = Vector((0, leg['side'], 0))
    return d.normalized().cross(Z)

def apply_pose(pose, frame):
    for n, p in PB.items():
        R, loc, sc_ = pose.get(n, (Quaternion(), Vector(), 1.0)) if isinstance(pose.get(n), tuple) else (pose.get(n, Quaternion()), Vector(), 1.0)
        M = REST[n]
        p.rotation_quaternion = (M.inverted() @ R.to_matrix() @ M).to_quaternion()
        p.location = M.inverted() @ loc
        p.scale = (sc_, sc_, sc_)
        p.keyframe_insert('rotation_quaternion', frame=frame)
        p.keyframe_insert('location', frame=frame)
        p.keyframe_insert('scale', frame=frame)

CLIPS = {}
def clip(name, frames, loop=True):
    def deco(fn): CLIPS[name] = (frames, loop, fn); return fn
    return deco

def make_actions():
    arm.animation_data_create()
    for name in SPEC['clips']:
        frames, loop, fn = CLIPS[name]
        act = bpy.data.actions.new(f'A_{NAME}_{name}'); act.use_fake_user = True
        arm.animation_data.action = act
        for f in list(range(0, frames, 2)) + [frames]:
            t = f / frames
            apply_pose(fn(0.0 if (loop and f == frames) else t), f + 1)
        act.frame_range = (1, frames + 1); act.use_frame_range = True
    arm.animation_data.action = bpy.data.actions[f'A_{NAME}_{SPEC["clips"][0]}']

def P(**kw): return kw
def rot_loc(R=None, loc=None, s=1.0): return (R or Quaternion(), loc or Vector(), s)

# ---------------- insect gait and clips
def insect_legs(pose, t, swing, lift, phase_shift=0.0, extra_lift=0.0):
    for leg in LEGS:
        grp = (leg['idx'] + (0 if leg['side'] > 0 else 1)) % 2
        p = (t + 0.5 * grp + phase_shift) % 1.0
        fwd = -math.cos(2 * math.pi * p); up = max(0.0, math.sin(2 * math.pi * p))
        a_sw = swing * fwd * (-1 if leg['side'] > 0 else 1)
        la = lift_axis(leg)
        pose[leg['name'] + '_upper'] = q(Z, a_sw) @ q(la, lift * up + extra_lift)
        pose[leg['name'] + '_lower'] = q(la, -0.5 * lift * up)

def body_bob(t, amt, freq=2, pitch=0.0, roll=0.0):
    return rot_loc(q(Y, pitch) @ q(X, roll * math.sin(2 * math.pi * t)), Vector((0, 0, amt * H * abs(math.sin(math.pi * freq * t)))))

@clip('Idle', 60)
def _(t):
    s2 = math.sin(2 * math.pi * t)
    pose = {'root': rot_loc(loc=Vector((0, 0, 0.006 * H * s2))), 'head': q(Z, 6 * math.sin(4 * math.pi * t)) @ q(Y, 3 * s2),
            'abdomen': rot_loc(q(Y, -3 * s2), s=1 + 0.03 * s2), 'thorax': q(X, 1.5 * s2)}
    for w in WINGS: pose[w['name'] + '_1'] = q(X, -w['side'] * 4 * s2)
    for a in ARMS: pose[a['name'] + '_upper'] = q(Y, -6 * s2)
    return pose

@clip('Walk', 32)
def _(t):
    pose = {'root': body_bob(t, 0.012, roll=2), 'head': q(Y, 2 * math.sin(4 * math.pi * t)), 'abdomen': q(Z, 4 * math.sin(2 * math.pi * t))}
    insect_legs(pose, t, 18, 20); return pose

@clip('Run', 20)
def _(t):
    pose = {'root': body_bob(t, 0.025, roll=3, pitch=4), 'head': q(Y, 6), 'abdomen': q(Z, 6 * math.sin(2 * math.pi * t))}
    insect_legs(pose, t, 26, 28); return pose

@clip('Attack', 24, loop=False)   # hit lands at frame 12
def _(t):
    pitch = env(t, [(0, 0), (0.35, -14), (0.5, 8), (1, 0)]); x = env(t, [(0, 0), (0.35, -0.04), (0.5, 0.08), (1, 0)])
    head = env(t, [(0, 0), (0.35, -18), (0.5, 26), (1, 0)])
    pose = {'root': rot_loc(q(Y, pitch), Vector((x * L, 0, 0.02 * H * (pitch < 0) * -pitch / 14))), 'head': q(Y, head)}
    for a in ARMS: pose[a['name'] + '_upper'] = q(Y, env(t, [(0, 0), (0.35, -50), (0.5, 30), (1, 0)]))
    for leg in LEGS:
        if leg['idx'] == 0: pose[leg['name'] + '_upper'] = q(lift_axis(leg), env(t, [(0, 0), (0.35, 25), (0.5, 0), (1, 0)]))
    return pose

@clip('Hit', 12, loop=False)
def _(t):
    k = env(t, [(0, 0), (0.25, 1), (1, 0)])
    return {'root': rot_loc(q(Y, -7 * k), Vector((-0.05 * L * k, 0, 0))), 'head': q(Y, -12 * k), 'abdomen': q(Y, 6 * k)}

@clip('Death', 40, loop=False)    # ends on its back, legs curled: the corpse pose
def _(t):
    roll = env(t, [(0, 0), (0.2, 12), (0.35, 12), (0.75, 180), (1, 180)])
    z = env(t, [(0, 0), (0.2, -0.05), (0.55, 0.25), (0.75, 0.0), (1, 0.0)]) * H + z_top * smooth(0.45, 0.75, t)
    pose = {'root': rot_loc(q(X, roll), Vector((0, 0, z))), 'head': q(Y, env(t, [(0, 0), (0.3, 15), (1, 30)])),
            'abdomen': q(Y, env(t, [(0, 0), (1, -10)]))}
    for leg in LEGS:
        tw = 10 * math.sin(14 * math.pi * t) * (1 - smooth(0.7, 1.0, t)) * smooth(0.6, 0.75, t)
        la = lift_axis(leg)
        pose[leg['name'] + '_upper'] = q(la, env(t, [(0, 0), (0.6, 20), (1, 55)]) + tw)
        pose[leg['name'] + '_lower'] = q(la, -env(t, [(0, 0), (0.6, 20), (1, 70)]))
    for w in WINGS: pose[w['name'] + '_1'] = q(X, -w['side'] * env(t, [(0, 0), (1, -25)]))
    for a in ARMS: pose[a['name'] + '_upper'] = q(Y, env(t, [(0, 0), (1, 40)]))
    return pose

@clip('Spit', 28, loop=False)     # acid leaves the mouth at frame 14
def _(t):
    h = env(t, [(0, 0), (0.4, -22), (0.52, 14), (1, 0)]); a = env(t, [(0, 1), (0.4, 1.16), (0.52, 0.9), (1, 1)])
    x = env(t, [(0, 0), (0.4, -0.03), (0.52, 0.04), (1, 0)])
    return {'root': rot_loc(q(Y, -0.4 * h), Vector((x * L, 0, 0))), 'head': q(Y, h), 'abdomen': rot_loc(q(Y, 8 * (a - 1) * 5), s=a)}

@clip('SpitPool', 40, loop=False)  # Matriarch: rears, lifts the sac and sprays the floor; pool lands at frame 26
def _(t):
    pitch = env(t, [(0, 0), (0.4, -20), (0.65, 6), (1, 0)]); a = env(t, [(0, 1), (0.4, 1.2), (0.65, 0.85), (1, 1)])
    shake = 4 * math.sin(30 * math.pi * t) * smooth(0.55, 0.62, t) * (1 - smooth(0.75, 0.85, t))
    pose = {'root': rot_loc(q(Y, pitch) @ q(Z, shake), Vector((0, 0, 0.06 * H * -min(pitch, 0) / 20))), 'head': q(Y, env(t, [(0, 0), (0.4, -10), (0.65, 30), (1, 0)])),
            'abdomen': rot_loc(q(Y, env(t, [(0, 0), (0.4, 25), (0.65, -10), (1, 0)])), s=a)}
    insect_legs(pose, 0, 0, 0)
    return pose

@clip('Slam', 36, loop=False)     # knockback: rears up, slams down at frame 20
def _(t):
    pitch = env(t, [(0, 0), (0.45, -28), (0.55, 6), (1, 0)]); z = env(t, [(0, 0), (0.45, 0.12), (0.55, -0.03), (1, 0)])
    pose = {'root': rot_loc(q(Y, pitch), Vector((0, 0, z * H))), 'head': q(Y, env(t, [(0, 0), (0.45, -15), (0.55, 20), (1, 0)]))}
    for leg in LEGS:
        if leg['idx'] == 0: pose[leg['name'] + '_upper'] = q(lift_axis(leg), env(t, [(0, 0), (0.45, 40), (0.55, -5), (1, 0)]))
    for a in ARMS: pose[a['name'] + '_upper'] = q(Y, env(t, [(0, 0), (0.45, -70), (0.55, 25), (1, 0)]))
    return pose

@clip('Charge', 16)               # head-down sprint used for the Warden's charge
def _(t):
    pose = {'root': body_bob(t, 0.03, roll=2, pitch=8), 'head': q(Y, 16), 'abdomen': q(Y, -4)}
    insect_legs(pose, t, 30, 30); return pose

@clip('Summon', 48, loop=False)   # Hive Mother calls Drones: rears, arms up, abdomen pulses
def _(t):
    k = env(t, [(0, 0), (0.3, 1), (0.8, 1), (1, 0)])
    pulse = 1 + 0.1 * k * abs(math.sin(4 * math.pi * t))
    pose = {'root': rot_loc(q(Y, -18 * k), Vector((0, 0, 0.08 * H * k))), 'head': q(Y, -25 * k) @ q(Z, 10 * math.sin(6 * math.pi * t) * k),
            'abdomen': rot_loc(q(Y, 8 * k), s=pulse)}
    for a in ARMS: pose[a['name'] + '_upper'] = q(Y, -75 * k) @ q(X, a['side'] * 20 * k)
    for leg in LEGS:
        if leg['idx'] == 0: pose[leg['name'] + '_upper'] = q(lift_axis(leg), 35 * k)
    return pose

@clip('Burrow', 30, loop=False)   # digs in; ends with only the back above ground (matches BurrowedIdle)
def _(t):
    z = -0.72 * H * smooth(0.15, 1.0, t)
    pose = {'root': rot_loc(q(Y, 14 * math.sin(math.pi * t)) @ q(Z, 12 * math.sin(10 * math.pi * t) * (1 - t)), Vector((0, 0, z)))}
    insect_legs(pose, (4 * t) % 1, 30, 25); return pose

@clip('BurrowedIdle', 40)
def _(t):
    return {'root': rot_loc(q(Z, 3 * math.sin(2 * math.pi * t)) @ q(X, 2 * math.sin(4 * math.pi * t)), Vector((0, 0, -0.72 * H + 0.015 * H * math.sin(2 * math.pi * t))))}

@clip('Emerge', 22, loop=False)   # bursts up; the ambush (double-damage first hit) lands at frame 16
def _(t):
    z = env(t, [(0, -0.72), (0.55, 0.12), (0.75, 0.0), (1, 0)]) * H
    pose = {'root': rot_loc(q(Y, env(t, [(0, 0), (0.55, -25), (0.75, 10), (1, 0)])), Vector((env(t, [(0, 0), (0.75, 0.1), (1, 0)]) * L, 0, z))),
            'head': q(Y, env(t, [(0, 0), (0.55, -15), (0.75, 25), (1, 0)]))}
    insect_legs(pose, 0.25, 0, env(t, [(0, 0), (0.55, 35), (1, 0)])); return pose

@clip('Hatch', 24, loop=False)    # Broodling bursts from an egg: grows from 30% and shakes off
def _(t):
    s_ = env(t, [(0, 0.3), (0.6, 1.08), (0.8, 1.0), (1, 1.0)])
    return {'root': rot_loc(q(Z, 15 * math.sin(8 * math.pi * t) * (1 - t)), s=s_), 'head': q(Y, -20 * math.sin(math.pi * t))}

@clip('Leap', 20, loop=False)     # Broodling pounce, lands at frame 12
def _(t):
    x = env(t, [(0, 0), (0.25, -0.05), (0.6, 0.25), (1, 0)]); z = math.sin(math.pi * smooth(0.25, 0.6, t)) * 0.35 * H * (t < 0.6)
    pose = {'root': rot_loc(q(Y, env(t, [(0, 0), (0.25, 10), (0.45, -15), (0.6, 10), (1, 0)])), Vector((x * L, 0, z)))}
    insect_legs(pose, 0.25, 0, env(t, [(0, 0), (0.3, -15), (0.45, 30), (0.6, 0), (1, 0)])); return pose

# ---------------- flier
def flap(t, cycles, amp, fold=0.0):
    pose = {}
    for w in WINGS:
        a = amp * math.sin(2 * math.pi * cycles * t)
        pose[w['name'] + '_1'] = q(Z, w['side'] * fold) @ q(X, -w['side'] * a)
        pose[w['name'] + '_2'] = q(X, -w['side'] * 0.4 * a)
    return pose

@clip('FlyIdle', 30)
def _(t):
    pose = flap(t, 5, 38); pose['root'] = rot_loc(loc=Vector((0, 0, 0.04 * H * math.sin(2 * math.pi * t)))); pose['abdomen'] = q(Y, 5 * math.sin(2 * math.pi * t)); return pose

@clip('FlyForward', 30)
def _(t):
    pose = flap(t, 6, 42); pose['root'] = rot_loc(q(Y, 12), Vector((0, 0, 0.03 * H * math.sin(4 * math.pi * t)))); pose['abdomen'] = q(Y, -6); return pose

@clip('Dive', 36, loop=False)     # every 12 s: climbs, folds wings, stoops; sting lands at frame 22
def _(t):
    fold = env(t, [(0, 0), (0.4, 55), (0.62, 55), (0.8, 0), (1, 0)])
    pose = flap(t, 6, 40 * (1 - smooth(0.3, 0.4, t)) + 40 * smooth(0.62, 0.8, t), fold)
    pose['root'] = rot_loc(q(Y, env(t, [(0, 0), (0.4, -20), (0.55, 45), (0.7, 0), (1, 0)])),
                           Vector((env(t, [(0, 0), (0.4, -0.1), (0.62, 0.3), (1, 0)]) * L, 0, env(t, [(0, 0), (0.4, 0.35), (0.62, -0.15), (1, 0)]) * H)))
    pose['abdomen'] = q(Y, env(t, [(0, 0), (0.5, 0), (0.62, -45), (0.8, 0), (1, 0)])); return pose

@clip('Sting', 20, loop=False)    # flier melee, sting lands at frame 11
def _(t):
    pose = flap(t, 3, 30); pose['abdomen'] = q(Y, env(t, [(0, 0), (0.4, 15), (0.55, -55), (1, 0)]))
    pose['root'] = rot_loc(q(Y, env(t, [(0, 0), (0.55, 10), (1, 0)])), Vector((env(t, [(0, 0), (0.55, 0.06), (1, 0)]) * L, 0, 0))); return pose

@clip('FlierDeath', 40, loop=False)  # wings stall, tumbles to the ground on its back
def _(t):
    pose = flap(t, 3 * (1 - t), 30 * (1 - smooth(0, 0.4, t)))
    for w in WINGS: pose[w['name'] + '_1'] = pose[w['name'] + '_1'] @ q(X, w['side'] * 35 * smooth(0.3, 1, t))
    roll = env(t, [(0, 0), (0.7, 200), (1, 180)])
    pose['root'] = rot_loc(q(X, roll) @ q(Y, 20 * math.sin(math.pi * t)), Vector((0, 0, z_top * smooth(0.5, 0.9, t) + env(t, [(0, 0), (0.15, 0.1), (0.7, -0.1), (1, 0)]) * H)))
    pose['abdomen'] = q(Y, -20 * smooth(0, 1, t)); return pose

# ---------------- quadruped
def quad_legs(pose, t, phases, swing, flex):
    for leg in LEGS:
        p = (t + phases[('F' if leg['front'] else 'H') + ('L' if leg['side'] > 0 else 'R')]) % 1
        fwd = -math.cos(2 * math.pi * p); up = max(0.0, math.sin(2 * math.pi * p))
        pose[leg['name'] + '_upper'] = q(Y, -swing * fwd)
        pose[leg['name'] + '_lower'] = q(Y, (flex if leg['front'] else -flex * 0.6) * up)
        pose[leg['name'] + '_foot'] = q(Y, -0.5 * flex * up)

WALK = {'HL': 0.0, 'FL': 0.25, 'HR': 0.5, 'FR': 0.75}
GALLOP = {'FL': 0.0, 'FR': 0.08, 'HL': 0.5, 'HR': 0.58}

@clip('QIdle', 60)
def _(t):
    s2 = math.sin(2 * math.pi * t)
    pose = {'chest': rot_loc(s=1 + 0.015 * s2), 'neck': q(Y, 2 * s2), 'head': q(Z, 10 * math.sin(2 * math.pi * t)) @ q(Y, 3 * math.sin(4 * math.pi * t))}
    for i, n in enumerate(TAILS): pose[n] = q(Z, 8 * math.sin(2 * math.pi * t - i))
    return pose

@clip('QWalk', 32)
def _(t):
    pose = {'root': rot_loc(loc=Vector((0, 0, 0.012 * H * math.sin(4 * math.pi * t)))), 'head': q(Y, -3 * math.sin(4 * math.pi * t)), 'neck': q(Y, 4)}
    for i, n in enumerate(TAILS): pose[n] = q(Z, 10 * math.sin(2 * math.pi * t - i))
    quad_legs(pose, t, WALK, 22, 40); return pose

@clip('QRun', 18)
def _(t):
    s2 = math.sin(2 * math.pi * t)
    pose = {'root': rot_loc(q(Y, 4 * s2), Vector((0, 0, 0.05 * H * max(0, math.sin(2 * math.pi * t + 0.6))))), 'pelvis': q(Y, -5 * s2), 'chest': q(Y, 6 * s2),
            'neck': q(Y, 12), 'head': q(Y, -6 * s2)}
    for i, n in enumerate(TAILS): pose[n] = q(Y, -10 + 6 * s2)
    quad_legs(pose, t, GALLOP, 36, 70); return pose

@clip('Bite', 20, loop=False)     # lunges and snaps, bite lands at frame 10
def _(t):
    x = env(t, [(0, 0), (0.3, -0.04), (0.5, 0.08), (1, 0)])
    pose = {'root': rot_loc(q(Y, env(t, [(0, 0), (0.3, -4), (0.5, 5), (1, 0)])), Vector((x * L, 0, 0))), 'neck': q(Y, env(t, [(0, 0), (0.3, -10), (0.5, 22), (1, 0)])),
            'head': q(Y, env(t, [(0, 0), (0.3, -15), (0.5, 15), (0.6, -10), (1, 0)]))}
    for leg in LEGS:
        if not leg['front']: pose[leg['name'] + '_upper'] = q(Y, env(t, [(0, 0), (0.3, 15), (0.5, -10), (1, 0)]))
    return pose

@clip('QHit', 12, loop=False)
def _(t):
    k = env(t, [(0, 0), (0.25, 1), (1, 0)])
    return {'root': rot_loc(q(X, 5 * k), Vector((-0.05 * L * k, 0, 0))), 'head': q(Y, -15 * k), 'neck': q(Z, 10 * k)}

@clip('QDeath', 40, loop=False)   # legs buckle, falls on its side: the corpse pose
def _(t):
    roll = env(t, [(0, 0), (0.25, 5), (0.7, 88), (1, 90)]); z = env(t, [(0, 0), (0.25, -0.12), (0.7, 0), (1, 0)]) * H
    zside = max(abs(LEGS[0]['foot'].y), 0.18 * W) * smooth(0.3, 0.7, t)
    pose = {'root': rot_loc(q(X, roll), Vector((0, 0, z + zside))), 'neck': q(Y, env(t, [(0, 0), (0.7, 15), (1, 25)])), 'head': q(Y, env(t, [(0, 0), (1, 15)]))}
    for leg in LEGS: pose[leg['name'] + '_upper'] = q(Y, env(t, [(0, 0), (0.25, -20 if leg['front'] else 20), (1, -12 if leg['front'] else 15)])); pose[leg['name'] + '_lower'] = q(Y, env(t, [(0, 0), (0.25, 40), (1, 25)]))
    for n in TAILS: pose[n] = q(Y, env(t, [(0, 0), (1, 20)]))
    return pose

@clip('Howl', 60, loop=False)
def _(t):
    k = env(t, [(0, 0), (0.25, 1), (0.8, 1), (1, 0)])
    return {'neck': q(Y, -40 * k), 'head': q(Y, -25 * k + 3 * math.sin(12 * math.pi * t) * k), 'pelvis': q(Y, 6 * k), 'root': rot_loc(loc=Vector((0, 0, -0.04 * H * k)))}

# ---------------- egg sac
@clip('Pulse', 40)
def _(t): return {'sac': rot_loc(s=1 + 0.05 * math.sin(2 * math.pi * t))}

@clip('SacHit', 12, loop=False)
def _(t): k = env(t, [(0, 0), (0.3, 1), (1, 0)]); return {'sac': rot_loc(q(X, 6 * k), s=1 - 0.08 * k)}

make_actions()
bpy.ops.object.mode_set(mode='OBJECT')

# ------------------------------------------------------------------ textures (kept as separate files for Unreal)
mat = body.data.materials[0]; mat.name = f'M_{NAME}'
slots = {'Base Color': 'BaseColor', 'Normal': 'Normal', 'Roughness': 'MetallicRoughness', 'Metallic': 'MetallicRoughness'}
saved = {}
nt = mat.node_tree
for node in nt.nodes:
    if node.type != 'TEX_IMAGE' or not node.image: continue
    links = [l for l in nt.links if l.from_node == node]
    kind = None
    for l in links:
        tgt = l.to_node
        while tgt.type not in ('BSDF_PRINCIPLED',) and tgt.outputs and any(ll.from_node == tgt for ll in nt.links):
            nxt = [ll for ll in nt.links if ll.from_node == tgt]
            if not nxt: break
            l = nxt[0]; tgt = l.to_node
        if tgt.type == 'BSDF_PRINCIPLED': kind = slots.get(l.to_socket.name, kind)
    kind = kind or 'Tex'
    fn = f'T_{NAME}_{kind}.png'
    g = SPEC.get('grade')
    if g and kind == 'BaseColor':   # colour grade toward the design text (e.g. Old Scar's grey coat)
        px = np.array(node.image.pixels[:]).reshape(-1, 4); rgb = px[:, :3]
        lum = rgb @ np.array([0.3, 0.59, 0.11])
        rgb = lum[:, None] + (rgb - lum[:, None]) * g.get('sat', 1.0)
        px[:, :3] = np.clip(rgb * g.get('gain', 1.0) + g.get('lift', 0.0), 0, 1); node.image.pixels[:] = px.ravel()
    if fn in saved.values(): continue
    img = node.image; img.name = fn[:-4]
    img.file_format = 'PNG'; img.save(filepath=os.path.join(OUT, 'textures', fn))
    saved[img.name] = fn
print('TEX', NAME, sorted(saved.values()))

# ------------------------------------------------------------------ export
bpy.ops.object.select_all(action='DESELECT')
arm.select_set(True); body.select_set(True); bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f'SK_{NAME}.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'},
                         add_leaf_bones=False, apply_unit_scale=True, bake_anim=True, bake_anim_use_all_actions=True,
                         bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
                         path_mode='STRIP', embed_textures=False, mesh_smooth_type='OFF', use_armature_deform_only=True,
                         apply_scale_options='FBX_SCALE_UNITS')   # same FBX convention as tools/propprep_blender.py
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, f'SK_{NAME}.glb'), export_format='GLB', use_selection=True,
                          export_animations=True, export_animation_mode='ACTIONS', export_force_sampling=True,
                          export_image_format='JPEG', export_def_bones=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, f'{NAME}.blend'), compress=True)
info = dict(name=NAME, kind=KIND, size_m=dict(length=round(L, 3), width=round(W, 3), height=round(H, 3)), tris=sum(len(p.vertices) - 2 for p in body.data.polygons),
            bones=[b.name for b in arm_data.bones], clips={c: dict(frames=CLIPS[c][0], loop=CLIPS[c][1]) for c in SPEC['clips']})
json.dump(info, open(os.path.join(OUT, f'{NAME}.rig.json'), 'w'), indent=1)
print('DONE', NAME)
