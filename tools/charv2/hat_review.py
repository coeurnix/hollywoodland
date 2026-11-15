"""Review renders of a v2 head wearing a hat over its hat-safe hair, from the front, side and back.

Run: blender -b --python tools/charv2/hat_review.py -- <Male|Female> <hat> <hair stem> [out.png]
Writes art/characters_v2/review/hats/<sex>_<hat>_<hair>_views.png (or out.png).
"""
import bpy, sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_body as bb

args = sys.argv[sys.argv.index('--') + 1:]
sex, hat, hair = args[0], args[1], args[2]
EXP = bb.OUT / 'export'
out = Path(args[3]) if len(args) > 3 else bb.OUT / 'review' / 'hats' / f'{sex}_{hat}_{hair}_views.png'

bpy.ops.wm.read_factory_settings(use_empty=True)
for f in (f'SKV2_{sex}_Body', f'SKV2_{sex}_{hat}_hat', f'SKV2_{sex}_{hair}hat'):
    p = EXP / f'{f}.fbx'
    if p.exists():
        bpy.ops.import_scene.fbx(filepath=str(p))
    else:
        print('HL_REVIEW missing', p)
for o in bpy.data.objects:
    if o.type == 'ARMATURE':
        o.hide_render = True
meshes = [o for o in bpy.data.objects if o.type == 'MESH']
for o in meshes:
    tex = None
    nm = o.name.lower()
    m = bpy.data.materials.new(o.name)
    m.use_nodes = True
    bsdf = m.node_tree.nodes['Principled BSDF']
    col = (0.85, 0.65, 0.5, 1) if 'body' in nm else (0.85, 0.85, 0.2, 1) if 'hair' in nm else (0.25, 0.4, 0.8, 1)
    bsdf.inputs['Base Color'].default_value = col
    o.data.materials.clear(); o.data.materials.append(m)
body = max(meshes, key=lambda o: len(o.data.vertices))
top = max((body.matrix_world @ v.co).z for v in body.data.vertices)
H = top - min((body.matrix_world @ v.co).z for v in body.data.vertices)  # the import units vary: work in body heights
pts = [body.matrix_world @ v.co for v in body.data.vertices if (body.matrix_world @ v.co).z > top - H * 0.13]
c = sum(pts, Vector()) / len(pts)
sc = bpy.context.scene
sc.render.engine = 'BLENDER_EEVEE' if 'BLENDER_EEVEE' in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items] else 'BLENDER_EEVEE_NEXT'
sc.render.resolution_x, sc.render.resolution_y = 360, 360
w = bpy.data.worlds.new('w'); w.use_nodes = True; w.node_tree.nodes['Background'].inputs['Strength'].default_value = 0.25; sc.world = w
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.lens = 50
sun = bpy.data.objects.new('sun', bpy.data.lights.new('sun', 'SUN')); sc.collection.objects.link(sun); sun.rotation_euler = (0.8, 0.3, 0.5); sun.data.energy = 2.5
sc.view_settings.view_transform = 'Standard'
frames = []
for i, (dx, dy) in enumerate(((0, -1), (1, 0), (0, 1), (-0.7, 0.7))):
    cam.location = c + Vector((dx, dy, 0.25)).normalized() * H * 0.45
    cam.rotation_euler = (c - cam.location).to_track_quat('-Z', 'Y').to_euler()
    p = out.with_name(out.stem + f'_{i}.png')
    sc.render.filepath = str(p)
    bpy.ops.render.render(write_still=True)
    frames.append(p)
try:
    from PIL import Image
except ImportError:
    Image = None
if Image:
    ims = [Image.open(f) for f in frames]
    s = Image.new('RGB', (360 * len(ims), 360))
    for i, im in enumerate(ims):
        s.paste(im.convert('RGB'), (360 * i, 0))
    s.save(out)
print('HL_REVIEW', out)
