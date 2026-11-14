# Render orthographic top and side views of GLBs so facing can be judged. +X is image-right in both views.
# Usage: blender -b --factory-startup -P views.py -- <out_dir> <a.glb> <b.glb> ...
import bpy, sys, os, math
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
out_dir, files = args[0], args[1:]
os.makedirs(out_dir, exist_ok=True)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'TEXTURE'
    sc.render.resolution_x = sc.render.resolution_y = 384
    sc.render.film_transparent = False
    sc.world = bpy.data.worlds.new('w')
    return sc


def bounds(objs):
    lo, hi = Vector((1e9,) * 3), Vector((-1e9,) * 3)
    for o in objs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    return lo, hi


for f in files:
    sc = reset()
    bpy.ops.import_scene.gltf(filepath=f)
    meshes = [o for o in sc.objects if o.type == 'MESH']
    lo, hi = bounds(meshes)
    c, size = (lo + hi) / 2, max(hi - lo)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam'))
    sc.collection.objects.link(cam); sc.camera = cam
    cam.data.type = 'ORTHO'; cam.data.ortho_scale = size * 1.15
    name = os.path.basename(os.path.dirname(f)) if os.path.basename(f).startswith('base') else os.path.splitext(os.path.basename(f))[0]
    for view, loc, rot in [('top', c + Vector((0, 0, size * 3)), (0, 0, 0)),
                           ('side', c + Vector((0, -size * 3, 0)), (math.pi / 2, 0, 0))]:
        cam.location = loc; cam.rotation_euler = rot
        sc.render.filepath = os.path.join(out_dir, f'{name}__{view}.png')
        bpy.ops.render.render(write_still=True)
    print('VIEWS', name, 'dims', tuple(round(v, 3) for v in (hi - lo)))
