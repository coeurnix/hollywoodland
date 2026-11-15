"""Exports a fitted v2 scene (a body plus MHCLO garments) as game meshes for Unreal.

Per mesh: modifiers and shape keys baked (helper geometry masked out), only bone vertex groups kept, a 'Zone' colour
attribute (R = zone id / 31, see ZONES) from the dominant bone of the nearest body vertex, armature object named
'Armature' (so Unreal does not add a root bone), metres with unit scale applied. Garments marked in SPLIT are cut into
slot pieces by connected part (a part whose lowest point is under the hips is trousers, the rest coat).

Run: blender -b <scene.blend> --python tools/charv2/export_v2.py -- <out_dir> <prefix> [garment=slot ...]
  e.g. -- art/characters_v2/export SKV2_Male toigo_male_suit_3=coat+trousers mindfront_shoes_oxford_male=shoes fedora01=hat
"""
import bpy, sys, json, bmesh
from pathlib import Path
from mathutils import Vector, Matrix
from mathutils.kdtree import KDTree

# zone ids (the runtime hides zones covered by an outer layer); keep in sync with HLCharacter.cpp
ZONES = ['head', 'neck', 'chest', 'belly', 'hips', 'upperarm', 'forearm', 'hand', 'thigh', 'calf', 'foot']
BONE_ZONE = {'head': 'head', 'neck_01': 'neck', 'spine_03': 'chest', 'clavicle_l': 'chest', 'clavicle_r': 'chest',
             'spine_02': 'belly', 'spine_01': 'belly', 'pelvis': 'hips', 'Root': 'hips', 'upperarm': 'upperarm',
             'lowerarm': 'forearm', 'hand': 'hand', 'thumb': 'hand', 'index': 'hand', 'middle': 'hand', 'ring': 'hand',
             'pinky': 'hand', 'thigh': 'thigh', 'calf': 'calf', 'foot': 'foot', 'ball': 'foot'}


def zone_of_bone(name):
    if name in BONE_ZONE:
        return ZONES.index(BONE_ZONE[name])
    return ZONES.index(BONE_ZONE.get(name.split('_')[0], 'chest'))


def bake(obj, rig):
    """Replaces obj's data with its evaluated mesh (no armature deformation), keeps bone groups, re-adds the armature."""
    for m in obj.modifiers:
        if m.type == 'ARMATURE' or m.name.startswith('Delete.'):
            m.show_viewport = False
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(obj.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
    for m in list(obj.modifiers):
        obj.modifiers.remove(m)
    old = obj.data
    obj.shape_key_clear() if old.shape_keys else None
    obj.data = me
    bones = {b.name for b in rig.data.bones}
    for g in list(obj.vertex_groups):
        if g.name not in bones:
            obj.vertex_groups.remove(g)
    if not obj.vertex_groups:
        g = obj.vertex_groups.new(name='head')
        g.add(range(len(obj.data.vertices)), 1.0, 'REPLACE')
    mod = obj.modifiers.new('Armature', 'ARMATURE')
    mod.object = rig
    obj.parent = rig
    return obj


def stamp_zones(obj, body_kd, body_zone):
    me = obj.data
    if 'Zone' in me.color_attributes:
        me.color_attributes.remove(me.color_attributes['Zone'])
    ca = me.color_attributes.new('Zone', 'BYTE_COLOR', 'POINT')
    mw = obj.matrix_world
    for v in me.vertices:
        _, i, _ = body_kd.find(mw @ v.co)
        z = body_zone[i]
        ca.data[v.index].color = (z / 31.0, 0, 0, 1)
    me.color_attributes.active_color = ca


def dominant_zones(body, rig):
    names = {g.index: g.name for g in body.vertex_groups}
    kd = KDTree(len(body.data.vertices))
    zones = []
    for v in body.data.vertices:
        kd.insert(body.matrix_world @ v.co, v.index)
        best = max(v.groups, key=lambda g: g.weight, default=None)
        zones.append(zone_of_bone(names[best.group]) if best else ZONES.index('chest'))
    kd.balance()
    return kd, zones


def split_parts(obj, hip_z):
    """Returns {'trousers': [face idx], 'coat': [...]} by connected part."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bm.faces.ensure_lookup_table()
    seen, out = set(), {'trousers': [], 'coat': []}
    for f in bm.faces:
        if f.index in seen:
            continue
        stack, comp = [f], []
        seen.add(f.index)
        while stack:
            x = stack.pop()
            comp.append(x)
            for e in x.edges:
                for o in e.link_faces:
                    if o.index not in seen:
                        seen.add(o.index)
                        stack.append(o)
        lo = min((obj.matrix_world @ v.co).z for x in comp for v in x.verts)
        hi = max((obj.matrix_world @ v.co).z for x in comp for v in x.verts)
        out['trousers' if lo < hip_z and hi < hip_z + 25.0 else 'coat'] += [x.index for x in comp]
    bm.free()
    return out


def separate(obj, faces, name):
    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.object.mode_set(mode='OBJECT')
    fs = set(faces)
    vs = set()
    for p in obj.data.polygons:
        p.select = p.index in fs
        if p.select:
            vs.update(p.vertices)
    for v in obj.data.vertices:
        v.select = v.index in vs
    for e in obj.data.edges:
        e.select = e.vertices[0] in vs and e.vertices[1] in vs
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.separate(type='SELECTED')
    bpy.ops.object.mode_set(mode='OBJECT')
    new = next(o for o in bpy.context.selected_objects if o != obj)
    new.name = name
    return new


def textures(obj, out, name, report):
    """Copies the base-colour image of obj's material next to the FBX as <name>_D.png (max 2048) and records it."""
    imgs = []
    for slot in obj.material_slots:
        m = slot.material
        if not m or not m.use_nodes:
            continue
        for n in m.node_tree.nodes:
            if n.type == 'TEX_IMAGE' and n.image and n.image.filepath:
                f = bpy.path.abspath(n.image.filepath)
                low = Path(f).name.lower()
                if any(k in low for k in ('norm', 'nrm', 'bump', 'spec', 'rough', 'normal', '_sss', 'ao.')):
                    continue
                imgs.append(f)
    if not imgs:
        return None
    src = max(imgs, key=lambda f: Path(f).stat().st_size if Path(f).exists() else 0)
    im = bpy.data.images.load(src, check_existing=False)
    if max(im.size) > 2048:
        im.scale(2048, 2048)
    dst = out / f'{name}_D.png'
    alpha = im.channels == 4 and any(k in name.lower() for k in ('brow', 'lash', 'hair'))
    im.file_format = 'PNG'
    im.save(filepath=str(dst))
    report.setdefault('_textures', {})[name] = {'file': dst.name, 'source': Path(src).name, 'alpha': alpha}
    return dst


def export(objs, rig, path):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE', 'MESH'},
                             apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', add_leaf_bones=False,
                             use_armature_deform_only=True, bake_anim=False, mesh_smooth_type='FACE',
                             colors_type='LINEAR', use_tspace=True, primary_bone_axis='Y', secondary_bone_axis='X')


def to_centimetres(rig):
    """Scene in centimetres (unit scale 0.01) with the rig and meshes scaled up and the scale applied: the FBX then has
    an unscaled armature with centimetre bones, which Unreal imports without folding a scale into the root bone."""
    sc = bpy.context.scene
    sc.unit_settings.system = 'METRIC'
    sc.unit_settings.scale_length = 0.01
    sc.unit_settings.length_unit = 'CENTIMETERS'
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    for o in meshes:  # unparent keeping world transforms, scale everything about the origin, re-parent after
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
    for o in meshes + [rig]:
        o.matrix_world = Matrix.Scale(100.0, 4) @ o.matrix_world
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes + [rig]:
        o.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for o in meshes:
        o.parent = rig
        for m in o.modifiers:
            if m.type == 'ARMATURE':
                m.object = rig


def run(out, prefix, specs, with_body=True):
    """specs: {mhclo stem: 'slot' | 'slot+slot' | 'hat' [ '@name' ]}; pieces are named <prefix>_<name>_<slot>."""
    out = Path(out)
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    rig.name = 'Armature'
    to_centimetres(rig)
    body = next(o for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith('Body_') and '.' not in o.name)
    bake(body, rig)
    kd, zones = dominant_zones(body, rig)
    hip_z = rig.matrix_world @ rig.data.bones['pelvis'].head_local
    hip_z = hip_z.z - 6.0  # centimetres (the scene is converted above)
    report = {}
    if with_body:
        # the head's extras (eyes, brows, lashes) ride with the body
        extras = [o for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith(body.name + '.') and
                  any(k in o.name for k in ('high-poly', 'low-poly', 'eyebrow', 'eyelash'))]
        for e in extras:
            bake(e, rig)
            stamp_zones(e, kd, zones)
        stamp_zones(body, kd, zones)
        export([body] + extras, rig, out / f'{prefix}_Body.fbx')
        textures(body, out, f'{prefix}_Body', report)
        for e in extras:
            textures(e, out, f'{prefix}_Body_' + e.name.split('.')[-1], report)
        report['Body'] = {'verts': len(body.data.vertices), 'extras': [e.name for e in extras]}
    for key, slot in specs.items():
        name = key
        if '@' in slot:
            slot, name = slot.split('@')
        g = next((o for o in bpy.data.objects if o.type == 'MESH' and o.name.endswith('.' + key)), None)
        if not g:
            print('HL_EXPORT missing', key)
            continue
        bake(g, rig)
        stamp_zones(g, kd, zones)
        textures(g, out, f'{prefix}_{name}', report)
        if slot == 'hat':
            # hats are static (the head socket carries them): no armature, exported on their own
            for m in list(g.modifiers):
                g.modifiers.remove(m)
            g.parent = None
            bpy.ops.object.select_all(action='DESELECT')
            g.select_set(True)
            bpy.context.view_layer.objects.active = g
            p = out / f'{prefix}_{name}_hat.fbx'
            bpy.ops.export_scene.fbx(filepath=str(p), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                                     apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE')
            report[f'{name}:hat'] = {'verts': len(g.data.vertices)}
            continue
        if '+' in slot:
            parts = split_parts(g, hip_z)
            for s in slot.split('+'):
                if s == 'coat':
                    continue
                if parts[s]:
                    piece = separate(g, parts[s], f'{name}_{s}')
                    export([piece], rig, out / f'{prefix}_{name}_{s}.fbx')
                    report[f'{name}:{s}'] = {'verts': len(piece.data.vertices)}
            slot = 'coat'
        export([g], rig, out / f'{prefix}_{name}_{slot}.fbx')
        report[f'{name}:{slot}'] = {'verts': len(g.data.vertices)}
    return report


def main():
    a = sys.argv[sys.argv.index('--') + 1:]
    out, prefix, specs = Path(a[0]), a[1], dict(x.split('=') for x in a[2:])
    report = run(out, prefix, specs)
    (out / f'{prefix}_export.json').write_text(json.dumps(report, indent=1))
    print('HL_EXPORT', json.dumps(report))


if __name__ == '__main__' and 'export_v2.py' in ' '.join(sys.argv):
    main()
