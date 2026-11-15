"""v2 bodies: MakeHuman/MPFB human per sex with heroic proportions, eyes, brows, lashes, a skin and the game_engine rig.

Everything stays in MPFB's native frame (metres, facing -Y) so MHCLO garments fit exactly; the export step converts.
Writes art/characters_v2/src/Body_<Sex>.blend and review renders in art/characters_v2/review/body_<sex>_*.png.

Run: blender -b --python tools/charv2/build_body.py -- [Male|Female|both]
"""
import bpy, sys, os, math, json
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'tmp' / 'character-art-source'
SYS = SRC / 'mh_system'
PACKS = SRC / 'mh_packs'
OUT = ROOT / 'art' / 'characters_v2'


def mpfb():
    import importlib
    for name in ('bl_ext.user_default.mpfb',):
        try:
            importlib.import_module(name)
        except ImportError:
            pass
    prefix = next(k.rsplit('.services.humanservice', 1)[0] for k in sys.modules if k.endswith('.services.humanservice'))
    hs = importlib.import_module(prefix + '.services.humanservice').HumanService
    ts = importlib.import_module(prefix + '.services.targetservice').TargetService
    return hs, ts


HS, TS = mpfb()

HEIGHT = {'Male': 1.85, 'Female': 1.72}
MACRO = {
    'Male': dict(gender=1.0, age=0.5, muscle=0.92, weight=0.56, proportions=1.0, cupsize=0.5, firmness=0.5),
    'Female': dict(gender=0.0, age=0.5, muscle=0.58, weight=0.46, proportions=1.0, cupsize=0.55, firmness=0.6),
}
# heroic stylization on top of the macros: bigger hands and feet, a slightly smaller head, broad male shoulders
DETAIL = {
    'Male': {'hands/{s}-hand-scale-incr': 0.45, 'feet/{s}-foot-scale-incr': 0.35, 'head/head-scale-vert-decr': 0.12,
             'head/head-scale-horiz-decr': 0.08, 'head/head-scale-depth-decr': 0.08, 'torso/measure-shoulder-dist-incr': 0.2,
             'torso/torso-muscle-dorsi-incr': 0.35, 'neck/neck-scale-horiz-incr': 0.3,
             'head/head-square': 0.35, 'chin/chin-width-incr': 0.4, 'chin/chin-prominent-incr': 0.25, 'chin/chin-bones-incr': 0.3},
    'Female': {'hands/{s}-hand-scale-incr': 0.3, 'feet/{s}-foot-scale-incr': 0.2, 'head/head-scale-vert-decr': 0.08,
               'head/head-scale-horiz-decr': 0.06, 'head/head-scale-depth-decr': 0.06, 'torso/measure-shoulder-dist-incr': 0.15,
               'head/head-oval': 0.4},
}
SKIN = {'Male': SYS / 'skins' / 'young_caucasian_male' / 'young_caucasian_male.mhmat',
        'Female': SYS / 'skins' / 'young_caucasian_female' / 'young_caucasian_female.mhmat'}
BROWS = {'Male': 'eyebrow001', 'Female': 'eyebrow010'}
LASHES = {'Male': 'eyelashes01', 'Female': 'eyelashes02'}


def target_path(rel):
    base = Path(TS.target_full_path(rel.split('/')[-1]) or '')
    return base


def body_height(obj):
    me = obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    g = obj.vertex_groups['body'].index
    zs = [obj.matrix_world @ v.co for v in me.vertices if any(x.group == g and x.weight > 0.5 for x in v.groups)]
    lo, hi = min(p.z for p in zs), max(p.z for p in zs)
    obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh_clear()
    return lo, hi


def build(sex):
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    macro = TS.get_default_macro_info_dict()
    macro.update(MACRO[sex])
    # solve the height macro for the target height (soles to crown)
    lo_h, hi_h = 0.0, 1.0
    obj = None
    for it in range(9):
        macro['height'] = (lo_h + hi_h) / 2
        if obj:
            bpy.data.objects.remove(obj, do_unlink=True)
        obj = HS.create_human(macro_detail_dict=dict(macro))
        lo, hi = body_height(obj)
        h = hi - lo
        if h < HEIGHT[sex]:
            lo_h = macro['height']
        else:
            hi_h = macro['height']
    for rel, w in DETAIL[sex].items():
        sides = ['l', 'r'] if '{s}' in rel else [None]
        for s in sides:
            name = rel.split('/')[-1].format(s=s) if s else rel.split('/')[-1]
            p = TS.target_full_path(name)
            if not p:
                print('HL_BODY missing target', name)
                continue
            TS.load_target(obj, p, weight=w, name=name)
    bpy.context.view_layer.update()
    lo, hi = body_height(obj)
    print(f'HL_BODY {sex} height {hi - lo:.3f} m (macro height {macro["height"]:.3f})')
    obj.name = f'Body_{sex}'
    HS.set_character_skin(str(SKIN[sex]), obj, skin_type='MAKESKIN')
    HS.add_mhclo_asset(str(SYS / 'eyes' / 'high-poly' / 'high-poly.mhclo'), obj, asset_type='Eyes', material_type='MAKESKIN')
    HS.add_mhclo_asset(str(SYS / 'eyebrows' / BROWS[sex] / f'{BROWS[sex]}.mhclo'), obj, asset_type='Eyebrows', material_type='MAKESKIN')
    HS.add_mhclo_asset(str(SYS / 'eyelashes' / LASHES[sex] / f'{LASHES[sex]}.mhclo'), obj, asset_type='Eyelashes', material_type='MAKESKIN')
    rig = HS.add_builtin_rig(obj, 'game_engine', import_weights=True)
    rig.name = f'Rig_{sex}'
    print(f'HL_BODY {sex} verts {len(obj.data.vertices)} bones {len(rig.data.bones)}')
    (OUT / 'src').mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'src' / f'Body_{sex}.blend'))
    review(sex, obj, hi - lo)


def ensure_scene():
    sc = bpy.context.scene
    if 'cam' in bpy.data.objects:
        return sc, bpy.data.objects['cam']
    sc.render.engine = 'BLENDER_EEVEE' if 'BLENDER_EEVEE' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE_NEXT'
    sc.render.resolution_x, sc.render.resolution_y = 900, 1200
    sc.render.film_transparent = False
    w = bpy.data.worlds.new('w')
    w.use_nodes = True
    w.node_tree.nodes['Background'].inputs[0].default_value = (0.32, 0.33, 0.36, 1)
    w.node_tree.nodes['Background'].inputs[1].default_value = 0.6
    sc.world = w
    sun = bpy.data.objects.new('key', bpy.data.lights.new('key', 'SUN'))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-30))
    sc.collection.objects.link(sun)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam)
    sc.camera = cam
    return sc, cam


def review(sex, obj, h):
    sc, cam = ensure_scene()
    rv = OUT / 'review'
    rv.mkdir(parents=True, exist_ok=True)
    shots = [('front', (0, -6, h * 0.5), (0, 0, h * 0.5), 'ORTHO', h * 1.1), ('side', (6, 0, h * 0.5), (0, 0, h * 0.5), 'ORTHO', h * 1.1),
             ('back', (0, 6, h * 0.5), (0, 0, h * 0.5), 'ORTHO', h * 1.1), ('face', (0.25, -0.75, h * 0.93), (0, 0, h * 0.92), 'PERSP', 0)]
    for name, loc, tgt, kind, ortho in shots:
        cam.location = loc
        cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        cam.data.type = kind
        if kind == 'ORTHO':
            cam.data.ortho_scale = ortho
        else:
            cam.data.lens = 85
        sc.render.filepath = str(rv / f'body_{sex.lower()}_{name}.png')
        bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    arg = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'both'
    for sex in (['Male', 'Female'] if arg == 'both' else [arg]):
        build(sex)
