"""Retargets a mocap dance (Rokoko / UE5-mannequin-style FBX, T-pose) onto the game's 37-bone character rigs and
exports the gameplay Dance clip in the same FBX form as art/characters/anim (30 fps, A-pose reference, cm, no scale
tracks, identical loop endpoints).

  blender -b --factory-startup --python tools/danceprep_blender.py -- <source.fbx> <out dir> [first last]

Without first/last it picks the loop itself: the 3.5-8 s span (whole mocap frames, skipping the first and last 1.5 s)
whose end pose and motion best match its start. Writes A_Male_Dance.fbx and A_Female_Dance.fbx and prints
'HL_DANCE <sex> frames <n> loop <a>-<b> err <deg>'.

Retarget: every target bone takes its source bone's armature-space rotation times a fixed offset. For limbs the offset
comes from the source rest pose bent so each bone points the way the target's rest bone points (T-pose to A-pose);
for pelvis/spine/neck/head it is the plain rest-to-rest difference. The pelvis moves like the source pelvis (scaled
to the rig's hip height) with its drift over the loop removed, so the dance stays in place.
"""
import bpy, sys, math, json
from pathlib import Path
from collections import Counter
from mathutils import Vector, Matrix, Quaternion
from io_scene_fbx import parse_fbx, encode_bin
import numpy as np

argv = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT = Path(argv[0]), Path(argv[1])
FORCE = (int(argv[2]), int(argv[3])) if len(argv) >= 4 else None
ROOT = Path(__file__).resolve().parents[1]
RIGS = ROOT / 'art' / 'characters' / 'src' / 'makehuman'
OUT.mkdir(parents=True, exist_ok=True)

# target bone: (source bone, target direction child or None, source direction child or None, align direction)
TORSO = {'pelvis': 'pelvis', 'spine_01': 'spine_02', 'spine_02': 'spine_03', 'spine_03': 'spine_05', 'neck_01': 'neck_01', 'head': 'head'}
MAP = {t: (s, None, None, False) for t, s in TORSO.items()}
for x in ('l', 'r'):
    MAP.update({
        f'clavicle_{x}': (f'clavicle_{x}', f'upperarm_{x}', f'upperarm_{x}', True),
        f'upperarm_{x}': (f'upperarm_{x}', f'lowerarm_{x}', f'lowerarm_{x}', True),
        f'lowerarm_{x}': (f'lowerarm_{x}', f'hand_{x}', f'hand_{x}', True),
        f'lowerarm_twist_01_{x}': (f'lowerarm_twist_02_{x}', f'hand_{x}', f'hand_{x}', True),
        f'hand_{x}': (f'hand_{x}', f'middle_01_{x}', f'middle_01_{x}', True),
        f'index_01_{x}': (f'index_01_{x}', f'index_02_{x}', f'index_02_{x}', True),
        f'index_02_{x}': (f'index_02_{x}', None, f'index_03_{x}', True),
        f'middle_01_{x}': (f'middle_01_{x}', f'middle_02_{x}', f'middle_02_{x}', True),
        f'middle_02_{x}': (f'middle_02_{x}', None, f'middle_03_{x}', True),
        f'thumb_01_{x}': (f'thumb_01_{x}', f'thumb_02_{x}', f'thumb_02_{x}', True),
        f'thumb_02_{x}': (f'thumb_02_{x}', None, f'thumb_03_{x}', True),
        f'thigh_{x}': (f'thigh_{x}', f'calf_{x}', f'calf_{x}', True),
        f'calf_{x}': (f'calf_{x}', f'foot_{x}', f'foot_{x}', True),
        f'foot_{x}': (f'foot_{x}', f'ball_{x}', f'ball_{x}', True),
        f'ball_{x}': (f'ball_{x}', None, None, True),
    })

METHODS = {'B': 'add_bool', 'C': 'add_char', 'Z': 'add_int8', 'Y': 'add_int16', 'I': 'add_int32', 'L': 'add_int64', 'F': 'add_float32',
           'D': 'add_float64', 'R': 'add_bytes', 'S': 'add_string', 'i': 'add_int32_array', 'l': 'add_int64_array', 'f': 'add_float32_array',
           'd': 'add_float64_array', 'b': 'add_bool_array', 'c': 'add_byte_array'}


def encoded(n):
    r = encode_bin.FBXElem(n.id)
    for k, v in zip(n.props_type, n.props):
        getattr(r, METHODS[chr(k)])(v)
    r.elems = [encoded(c) for c in n.elems]
    return r


def strip_scale(path):
    """Drop the constant Lcl Scaling curves Blender bakes (same post-process as the delivered clips)."""
    root, version = parse_fbx.parse(str(path))
    objects = next(n for n in root.elems if n.id == b'Objects')
    con = next(n for n in root.elems if n.id == b'Connections')
    ids = {n.props[0]: n for n in objects.elems if n.props}
    nodes = {c.props[1] for c in con.elems if len(c.props) > 3 and c.props[3] == b'Lcl Scaling'}
    curves = {c.props[1] for c in con.elems if len(c.props) > 2 and c.props[2] in nodes and c.props[1] in ids and ids[c.props[1]].id == b'AnimationCurve'}
    remove = nodes | curves
    counts = Counter(ids[i].id for i in remove)
    objects.elems[:] = [n for n in objects.elems if not n.props or n.props[0] not in remove]
    con.elems[:] = [c for c in con.elems if len(c.props) < 3 or (c.props[1] not in remove and c.props[2] not in remove)]
    for n in next(n for n in root.elems if n.id == b'Definitions').elems:
        if n.id == b'Count':
            n.props[0] -= len(remove)
        elif n.id == b'ObjectType' and n.props[0] in counts:
            next(c for c in n.elems if c.id == b'Count').props[0] -= counts[n.props[0]]
    tmp = path.with_suffix('.tmp.fbx')
    encode_bin.write(str(tmp), encoded(root), version)
    tmp.replace(path)


def flat(v):
    v = Vector((v.x, v.y, 0))
    return v.normalized()


def run(sex):
    bpy.ops.wm.open_mainfile(filepath=str(RIGS / f'SK_{sex}.blend'))
    sc = bpy.context.scene
    sc.render.fps, sc.render.fps_base = 30, 1
    rig = bpy.data.objects['Armature']
    rig.animation_data_clear()
    for o in list(bpy.data.objects):
        if o != rig:
            bpy.data.objects.remove(o, do_unlink=True)
    names_before = set(o.name for o in bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(SRC))
    src = next(o for o in bpy.data.objects if o.name not in names_before and o.type == 'ARMATURE')
    for o in list(bpy.data.objects):
        if o not in (rig, src):
            bpy.data.objects.remove(o, do_unlink=True)
    act = src.animation_data.action
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])

    TW = rig.matrix_world.copy()
    TWi = TW.inverted()
    tb = rig.data.bones
    # source -> target armature space, with a yaw so the source faces the way the rig faces
    sc.frame_set(f0)
    src_rest = {b.name: src.matrix_world @ b.matrix_local for b in src.data.bones}
    s_fwd = flat(TWi.to_3x3() @ ((src_rest['ball_l'].translation + src_rest['ball_r'].translation) - (src_rest['foot_l'].translation + src_rest['foot_r'].translation)))
    t_fwd = flat((tb['ball_l'].head_local + tb['ball_r'].head_local) - (tb['foot_l'].head_local + tb['foot_r'].head_local))
    yaw = Quaternion((0, 0, 1), math.atan2(s_fwd.cross(t_fwd).z, s_fwd.dot(t_fwd)))
    S = Matrix.LocRotScale(None, yaw, None) @ TWi  # world -> target armature space (rotated)

    def src_bone_mats(rest):
        out = {}
        for b in src.data.bones:
            m = src.matrix_world @ (b.matrix_local if rest else src.pose.bones[b.name].matrix)
            out[b.name] = S @ m
        return out

    def rot(m):
        return m.to_3x3().normalized().to_quaternion()

    SR = src_bone_mats(True)
    # per-bone offsets
    C = {}
    for t, (s, tc, scn, align) in MAP.items():
        qs = rot(SR[s])
        if align:
            dt = (tb[tc].head_local - tb[t].head_local) if tc else (tb[t].matrix_local.to_3x3() @ Vector((0, 1, 0)))
            ds = (SR[scn].translation - SR[s].translation) if scn else (SR[s].to_3x3().normalized() @ Vector((0, 1, 0)))
            qs = ds.normalized().rotation_difference(dt.normalized()) @ qs
        C[t] = qs.inverted() @ tb[t].matrix_local.to_quaternion()

    # sample the source once
    frames = list(range(f0, f1 + 1))
    samp = []
    for f in frames:
        sc.frame_set(f)
        M = src_bone_mats(False)
        samp.append({s: (rot(M[s]), M[s].translation.copy()) for s in set(v[0] for v in MAP.values())})

    # loop choice (source only): pose difference + velocity difference between candidate start and end
    keys = sorted(set(v[0] for v in MAP.values() if not any(k in v[0] for k in ('index', 'middle', 'thumb'))))
    Q = np.array([[list(fr[k][0]) for k in keys] for fr in samp])            # F x B x 4
    P = np.array([fr['pelvis'][1].z for fr in samp])
    def qdist(a, b):
        d = np.abs((Q[a] * Q[b]).sum(-1)).clip(0, 1)
        return np.degrees(2 * np.arccos(d)).mean()
    if FORCE:
        a, b = FORCE[0] - f0, FORCE[1] - f0
    else:
        best = None
        margin = 45
        for a in range(margin, len(frames) - margin):
            for L in range(105, 241):
                b = a + L
                if b >= len(frames) - margin:
                    break
                e = qdist(a, b) + 0.5 * qdist(a + 1, b + 1) + 0.5 * qdist(a - 1, b - 1) + abs(P[a] - P[b]) * 0.5 + L * 0.004
                if best is None or e < best[0]:
                    best = (e, a, b)
        _, a, b = best
    err = qdist(a, b)

    n = b - a + 1
    sc.frame_start, sc.frame_end = 1, n
    action = bpy.data.actions.new(f'A_{sex}_Dance')
    rig.animation_data_create()
    rig.animation_data.action = action
    for pb in rig.pose.bones:
        pb.rotation_mode = 'QUATERNION'

    hip = tb['pelvis'].head_local
    s_hip = SR['pelvis'].translation
    scale = hip.z / s_hip.z
    # pelvis offsets over the loop, minus a linear drift so the loop closes and the dancer stays put
    offs = [(samp[a + i]['pelvis'][1] - s_hip) * scale for i in range(n)]
    drift = offs[-1] - offs[0]
    offs = [o - drift * (i / (n - 1)) for i, o in enumerate(offs)]
    mean = sum((Vector((o.x, o.y, 0)) for o in offs), Vector()) / n
    offs = [o - mean for o in offs]

    order = [b.name for b in rig.data.bones]  # parents first
    blend = 6  # frames eased into the loop start at the end
    prev = {}
    for i in range(n):
        fr = samp[a + i]
        fq = {t: fr[MAP[t][0]][0] @ C[t] for t in MAP}
        if i >= n - blend:
            w = (i - (n - blend - 1)) / blend
            st = samp[a]
            for t in MAP:
                fq[t] = fq[t].slerp(st[MAP[t][0]][0] @ C[t], w)
        if i == n - 1:
            pass
        pose = {}
        for name in order:
            bone = tb[name]
            if name == 'root':
                pose[name] = bone.matrix_local.copy()
                continue
            par = bone.parent.name
            rel = bone.parent.matrix_local.inverted() @ bone.matrix_local
            loc = (pose[par] @ rel).translation
            if name == 'pelvis':
                o = offs[i] if i < n - 1 else offs[0]
                loc = hip + o
            q = fq.get(name)
            if q is None:
                q = (pose[par] @ rel).to_quaternion()
            pose[name] = Matrix.LocRotScale(loc, q, None)
        for name in order:
            bone = tb[name]
            pb = rig.pose.bones[name]
            if name == 'root':
                basis = Matrix.Identity(4)
            else:
                basis = bone.matrix_local.inverted() @ bone.parent.matrix_local @ pose[bone.parent.name].inverted() @ pose[name]
            loc, q, _ = basis.decompose()
            if name in prev and q.dot(prev[name]) < 0:
                q.negate()
            prev[name] = q.copy()
            pb.rotation_quaternion = q
            pb.keyframe_insert('rotation_quaternion', frame=i + 1, group=name)
            if name in ('root', 'pelvis'):
                pb.location = loc if name == 'pelvis' else Vector()
                pb.keyframe_insert('location', frame=i + 1, group=name)
    # identical loop endpoints: the last key is the first
    for layer in action.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for fc in bag.fcurves:
                    kp = fc.keyframe_points
                    kp[-1].co.y = kp[0].co.y
                    for k in kp:
                        k.interpolation = 'LINEAR'
                    fc.update()
    bpy.data.objects.remove(src, do_unlink=True)
    # skeleton-only FBX reference = pose at the current frame: key the A-pose at frame 0 (outside the export range)
    sc.frame_set(0)
    for pb in rig.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
        pb.keyframe_insert('rotation_quaternion', frame=0, group=pb.name)
    for name in ('root', 'pelvis'):
        rig.pose.bones[name].keyframe_insert('location', frame=0, group=name)
    sc.frame_set(0)
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    path = OUT / f'A_{sex}_Dance.fbx'
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE'}, global_scale=1, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_NONE', axis_forward='-Z', axis_up='Y', add_leaf_bones=False,
                             use_armature_deform_only=True, bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_simplify_factor=0, bake_anim_step=1, use_custom_props=False)
    strip_scale(path)
    print(f'HL_DANCE {sex} frames {n} loop {frames[a]}-{frames[b]} ({n / 30:.2f} s) err {err:.2f} deg size {path.stat().st_size}', flush=True)
    return frames[a], frames[b]


rng = run('Male')
if not FORCE:
    FORCE = rng  # the same cut for both rigs
run('Female')
