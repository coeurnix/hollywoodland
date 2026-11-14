"""Rodin GLB -> compact world-piece GLB for the prop pipeline (art/world/models/<category>/<name>.glb).

    blender -b --factory-startup -P art/buildings/src/export_prop.py -- <rodin glb> <out glb> <height m> <max tris> [width m]
Collapse-decimates to the triangle budget (SPEC 3: small props 300, large 1500), scales uniformly to the height, pivot at
the bottom centre, front stays glTF +Z (Blender -Y, as Rodin returns it), base colour resized to 1024 and written as JPEG.
Prints PROP <name> <tris> <w> <d> <h>.
"""
import bpy, bmesh, sys
from pathlib import Path

a = sys.argv[sys.argv.index('--') + 1:]
src, out, height, budget = a[:4]
height, budget = float(height), int(budget)
width = float(a[4]) if len(a) > 4 else None   # optional exact width (X), e.g. a 4 m fence section
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in bpy.context.scene.objects: o.select_set(o.type == 'MESH')
bpy.context.view_layer.objects.active = objs[0]
if len(objs) > 1: bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
for o in list(bpy.context.scene.objects):
    if o is not ob: bpy.data.objects.remove(o, do_unlink=True)
ob.parent = None; bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bm = bmesh.new(); bm.from_mesh(ob.data); bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-4); bm.to_mesh(ob.data); bm.free()
for _ in range(6):
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    if tris <= budget: break
    md = ob.modifiers.new('d', 'DECIMATE'); md.ratio = max(0.02, budget / tris * 0.97); bpy.ops.object.modifier_apply(modifier='d')
    if sum(len(p.vertices) - 2 for p in ob.data.polygons) > tris * 0.95:
        md = ob.modifiers.new('d', 'DECIMATE'); md.ratio = max(0.02, budget / tris * 0.97); md.delimit = set(); bpy.ops.object.modifier_apply(modifier='d')
vs = [v.co for v in ob.data.vertices]
lo = [min(v[i] for v in vs) for i in range(3)]; hi = [max(v[i] for v in vs) for i in range(3)]
s = height / (hi[2] - lo[2]); sx = width / (hi[0] - lo[0]) if width else s
for v in ob.data.vertices:
    v.co.x = (v.co.x - (lo[0] + hi[0]) / 2) * sx; v.co.y = (v.co.y - (lo[1] + hi[1]) / 2) * s; v.co.z = (v.co.z - lo[2]) * s
name = Path(out).stem
for m in ob.data.materials:
    if m and m.use_nodes:
        m.name = 'M_' + name
        for n in m.node_tree.nodes:
            if n.type == 'TEX_IMAGE' and n.image:
                if n.image.size[0] > 1024: n.image.scale(1024, 1024)
                n.image.name = 'T_' + name
ob.name = ob.data.name = 'SM_' + name
Path(out).parent.mkdir(parents=True, exist_ok=True)
bpy.ops.export_scene.gltf(filepath=out, export_format='GLB', export_image_format='JPEG', export_jpeg_quality=88, export_tangents=False,
                          export_draco_mesh_compression_enable=False, export_apply=True)
tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
d = ob.dimensions
print('PROP', name, tris, round(d.x, 2), round(d.y, 2), round(d.z, 2))
