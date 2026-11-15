"""Renders both hands of a body in the GRIP pose (export_v2.grip) from four sides for review.
Run: blender -b --python tools/charv2/grip_test.py -- <Male|Female> [sign]"""
import bpy, sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_body as bb
import export_v2

args = sys.argv[sys.argv.index('--') + 1:]
sex = args[0]
sign = float(args[1]) if len(args) > 1 else 1.0
bpy.ops.wm.open_mainfile(filepath=str(bb.OUT / 'src' / f'Body_{sex}.blend'))
rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
print('posed', export_v2.grip(rig, sign))
sc, cam = bb.ensure_scene()
sc.render.resolution_x = sc.render.resolution_y = 500
out = bb.OUT / 'review' / 'grip'
out.mkdir(parents=True, exist_ok=True)
for side in ('r', 'l'):
    hb = rig.data.bones[f'middle_01_{side}']
    c = rig.matrix_world @ hb.head_local
    for n, off in enumerate([Vector((0, -0.25, 0)), Vector((0, 0.25, 0)), Vector((0.25, 0, -0.05)), Vector((-0.25, 0, -0.05))]):
        cam.location = c + off
        cam.rotation_euler = (c - cam.location).to_track_quat('-Z', 'Y').to_euler()
        cam.data.lens = 50
        sc.render.filepath = str(out / f'{sex}_{side}{n}.png')
        bpy.ops.render.render(write_still=True)
print('grip renders done')
