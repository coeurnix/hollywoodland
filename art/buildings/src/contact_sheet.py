"""One contact sheet per batch: the exported FBXs side by side, textured, 3/4 front view, flat light.

    blender -b --factory-startup -P art/buildings/src/contact_sheet.py -- <out png> <id>[*n] <id> ...
`id*3` lays three copies end to end along X (tiling check). Front is -Y; the camera looks from front-left above.
"""
import bpy, math, sys
from pathlib import Path
from mathutils import Vector

R = Path(__file__).resolve().parents[1]
args = sys.argv[sys.argv.index('--') + 1:]
out, ids = args[0], args[1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
x = 0.0; lo = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3)
for spec in ids:
    aid, n = (spec.split('*') + ['1'])[:2]
    for i in range(int(n)):
        old = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(R / 'meshes' / f'{aid}.fbx'))
        ob = next(o for o in set(bpy.data.objects) - old if o.type == 'MESH')
        tex = R / 'textures' / f"T_{ob.active_material.name.split('.')[0][2:]}.png"
        m = bpy.data.materials.new('c'); m.use_nodes = True; nt = m.node_tree
        t = nt.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(str(tex), check_existing=True)
        em = nt.nodes.new('ShaderNodeEmission'); nt.links.new(t.outputs['Color'], em.inputs['Color'])
        nt.links.new(em.outputs[0], nt.nodes['Material Output'].inputs[0])
        ob.data.materials.clear(); ob.data.materials.append(m)
        w = ob.dimensions.x; bx = min((ob.matrix_world @ v.co).x for v in ob.data.vertices)
        ob.location.x += x - bx; bpy.context.view_layer.update()
        for v in ob.data.vertices:
            p = ob.matrix_world @ v.co; lo = Vector(map(min, lo, p)); hi = Vector(map(max, hi, p))
        x += w + (0 if i < int(n) - 1 else max(2.0, w * 0.15))
c = (lo + hi) / 2; size = max((hi - lo).x, (hi - lo).z * 2.2)
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam
cam.data.type = 'ORTHO'; cam.data.ortho_scale = size * 1.1
d = Vector((-0.45, -1.0, 0.55)).normalized()
cam.location = c + d * size * 2; cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
cam.data.clip_end = size * 10
sc.render.engine = 'BLENDER_EEVEE' if 'BLENDER_EEVEE' in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items.keys() else 'BLENDER_EEVEE_NEXT'
sc.render.resolution_x, sc.render.resolution_y = 1800, 900
sc.world = bpy.data.worlds.new('w'); sc.world.color = (0.85, 0.85, 0.85)
sc.render.filepath = str(Path(out).resolve()); bpy.ops.render.render(write_still=True)
print('CONTACT', out)
