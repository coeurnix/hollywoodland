"""Blender 5.3 half of tools/propprep.py: GLB -> scaled, decimated, turned FBX. Run by propprep.py:
  blender --background --factory-startup --python tools/propprep_blender.py -- <jobs.json>
Each job: {glb, fbx, scale: [x, y, z] in glTF model axes, tris: budget}. Prints 'HL_PROP SM_<Name> tris <n>' per mesh."""
import json, math, os, sys
import bpy

jobs = json.load(open(sys.argv[sys.argv.index('--') + 1]))
for job in jobs:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=job['glb'])
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    bpy.ops.object.select_all(action='DESELECT')
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    # parents (glTF root nodes) carry no transform here, but clear them so the mesh owns its placement
    if ob.parent:
        mw = ob.matrix_world.copy(); ob.parent = None; ob.matrix_world = mw
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # glTF (x, y-up, z-front) imports as Blender (x, -z, y): width x, length along -Y, height z
    sx, sy, sz = job['scale']
    ob.scale = (sx, sz, sy)
    # front (Blender -Y) to Blender +X, which Unreal's FBX import keeps as +X
    ob.rotation_euler = (0, 0, math.radians(90))
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    if job.get('ground'):
        # generated foliage: pivot at the bottom centre (Rodin centres its meshes)
        import mathutils
        vs = [v.co for v in ob.data.vertices]
        mn = mathutils.Vector((min(v.x for v in vs), min(v.y for v in vs), min(v.z for v in vs)))
        mx = mathutils.Vector((max(v.x for v in vs), max(v.y for v in vs), max(v.z for v in vs)))
        off = mathutils.Vector(((mn.x + mx.x) / 2, (mn.y + mx.y) / 2, mn.z))
        for v in ob.data.vertices:
            v.co -= off
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    if tris > job['tris']:
        mod = ob.modifiers.new('decimate', 'DECIMATE')
        mod.decimate_type = 'COLLAPSE'
        mod.ratio = job['tris'] / tris
        mod.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=mod.name)
    # strip materials: Unreal assigns its own instance
    ob.data.materials.clear()
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    os.makedirs(os.path.dirname(job['fbx']), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=job['fbx'], use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_UNITS', mesh_smooth_type='OFF', use_mesh_modifiers=True,
                             add_leaf_bones=False, bake_anim=False, path_mode='STRIP', embed_textures=False)
    print(f"HL_PROP {os.path.splitext(os.path.basename(job['fbx']))[0]} tris {tris}", flush=True)
