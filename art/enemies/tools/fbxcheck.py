import bpy, sys, glob, os, json
root = sys.argv[sys.argv.index('--') + 1]
for f in sorted(glob.glob(os.path.join(root, '*', 'SK_*.fbx'))):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=f)
    arms = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE']; meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    m = meshes[0]; import numpy as np
    V = np.array([(m.matrix_world @ v.co)[:] for v in m.data.vertices])
    ext = (V.max(0) - V.min(0)).round(2)
    rig = json.load(open(glob.glob(os.path.join(os.path.dirname(f), '*.rig.json'))[0]))
    acts = sorted(a.name for a in bpy.data.actions)
    print('FBX', os.path.basename(f), 'bones', len(arms[0].data.bones), 'tris', sum(len(p.vertices) - 2 for p in m.data.polygons), 'ext', ext.tolist(),
          'actions', len(acts), 'expected', len(rig['clips']), 'groups', len(m.vertex_groups))
