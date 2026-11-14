# Destroyed / burned states for hive structures (static meshes). Darkens the base colour like char and slumps
# or snaps the mesh. Usage: blender -b --factory-startup -P destroyed.py -- <in.glb> <out_dir> <Name> <mode> [size]
#   mode: burst  = egg cluster / egg sac broken open and slumped
#         snap   = hive spire snapped off at 45% height, stump kept
import bpy, sys, os, math, random
import numpy as np
from mathutils import Vector, Matrix

a = sys.argv[sys.argv.index('--') + 1:]
SRC, OUT, NAME, MODE = a[:4]
SIZE = float(a[4]) if len(a) > 4 else None
os.makedirs(os.path.join(OUT, 'textures'), exist_ok=True)
random.seed(7)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=SRC)
ms = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in bpy.context.scene.objects: o.select_set(o.type == 'MESH')
bpy.context.view_layer.objects.active = ms[0]
if len(ms) > 1: bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
for o in list(bpy.context.scene.objects):
    if o is not ob: bpy.data.objects.remove(o)
ob.parent = None
ob.data.transform(ob.matrix_world); ob.matrix_world = Matrix.Identity(4)
me = ob.data
V = np.array([v.co[:] for v in me.vertices]); lo, hi = V.min(0), V.max(0)
if SIZE:  # match the live asset's scale (length along X)
    s = SIZE / (hi[0] - lo[0]); me.transform(Matrix.Diagonal((s, s, s, 1)))
    V = np.array([v.co[:] for v in me.vertices]); lo, hi = V.min(0), V.max(0)
me.transform(Matrix.Translation((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2])))
V = np.array([v.co[:] for v in me.vertices]); H = V[:, 2].max()

if MODE == 'burst':
    # slump: the upper part sags and spreads; noise breaks the clean egg shapes
    for v in me.vertices:
        z = v.co.z / H
        sag = 0.42 + 0.58 * (1 - z) ** 0.5 if z > 0.15 else 1.0
        n = 1 + 0.12 * math.sin(v.co.x * 17.0) * math.sin(v.co.y * 13.0)
        v.co.z *= sag * n if z > 0.15 else 1.0
        spread = 1 + 0.18 * max(0.0, z - 0.2)
        v.co.x *= spread; v.co.y *= spread
elif MODE == 'snap':
    import bmesh
    bm = bmesh.new(); bm.from_mesh(me)
    cut = 0.45 * H
    geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
    res = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, cut), plane_no=(0.25, 0.1, 1.0), clear_outer=True)
    edges = [e for e in res['geom_cut'] if isinstance(e, bmesh.types.BMEdge)]
    if edges:
        f = bmesh.ops.holes_fill(bm, edges=edges, sides=0)
    for v in bm.verts:   # jagged break
        if v.co.z > cut * 0.8: v.co.z += (random.random() - 0.5) * 0.08 * H
    bm.to_mesh(me); bm.free()

# char the base colour (dark, desaturated, a little ember in the cracks)
for mat in me.materials:
    for node in mat.node_tree.nodes:
        if node.type == 'TEX_IMAGE' and node.image and any(l.to_socket.name == 'Base Color' for l in mat.node_tree.links if l.from_node == node):
            img = node.image
            px = np.array(img.pixels[:]).reshape(-1, 4)
            lum = px[:, :3] @ np.array([0.3, 0.59, 0.11])
            out = np.stack([lum * 0.38 + 0.03, lum * 0.3 + 0.015, lum * 0.26 + 0.01], 1)
            hot = lum > 0.65
            out[hot] = out[hot] * 0.5 + np.array([0.45, 0.16, 0.03])
            px[:, :3] = out
            img.pixels[:] = px.ravel()
            fp = os.path.join(OUT, 'textures', f'T_{NAME}_BaseColor.png')
            img.file_format = 'PNG'; img.save(filepath=fp)
            node.image = bpy.data.images.load(fp); node.image.name = f'T_{NAME}_BaseColor'   # export the charred pixels, not the packed original
        elif node.type == 'TEX_IMAGE' and node.image:
            kind = 'Normal' if any(l.to_socket.name == 'Color' and l.to_node.type == 'NORMAL_MAP' for l in mat.node_tree.links if l.from_node == node) else 'MetallicRoughness'
            node.image.file_format = 'PNG'; node.image.save(filepath=os.path.join(OUT, 'textures', f'T_{NAME}_{kind}.png')); node.image.name = f'T_{NAME}_{kind}'
    mat.name = f'M_{NAME}'
ob.name = me.name = f'SM_{NAME}'
bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, f'SM_{NAME}.fbx'), use_selection=True, object_types={'MESH'}, apply_unit_scale=True,
                         path_mode='STRIP', embed_textures=False, mesh_smooth_type='OFF', bake_anim=False, apply_scale_options='FBX_SCALE_UNITS')
bpy.ops.export_scene.gltf(filepath=os.path.join(OUT, f'SM_{NAME}.glb'), export_format='GLB', use_selection=True, export_image_format='JPEG')
V = np.array([v.co[:] for v in me.vertices])
print('DESTROYED', NAME, MODE, 'ext', (V.max(0) - V.min(0)).round(2).tolist(), 'tris', sum(len(p.vertices) - 2 for p in me.polygons))
