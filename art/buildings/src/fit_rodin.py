"""Fit a Rodin full-generation GLB to an exact box, decimate it to budget and export it (FBX + 2048 base colour).

    blender -b -P art/buildings/src/fit_rodin.py -- <job json, relative to art/buildings>

job: {"id", "glb": "work/<id>/base_basic_shaded.glb", "size": [w, d, h], "tris": N, "origin": "ground-centre"|"footprint-centre-ground",
      "kind", "texture_from": optional other asset id (share its texture instead of writing a new one), "mirror": bool,
      "long_axis": "x" (rotate so the longest horizontal extent runs along X), "yaw": extra degrees about Z}
The mesh keeps Rodin's UVs; its texture is downsampled to 2048 and saved as textures/T_<id or texture_from>.png.
Appends/updates the asset in work/fit_results.json (bounds, tris), which publish_batch.py writes into the manifest.
"""
import bpy, bmesh, json, math, sys
from pathlib import Path
from mathutils import Vector

R = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from asset_io import export_asset

job = json.loads((R / sys.argv[sys.argv.index('--') + 1]).read_text())
aid = job['id']
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(R / job['glb']))
objs = [o for o in bpy.context.scene.objects if o.type == 'MESH']
for o in bpy.context.scene.objects: o.select_set(o.type == 'MESH')
bpy.context.view_layer.objects.active = objs[0]
if len(objs) > 1: bpy.ops.object.join()
ob = bpy.context.view_layer.objects.active
for o in list(bpy.context.scene.objects):
    if o is not ob and o.type != 'MESH': bpy.data.objects.remove(o, do_unlink=True)
ob.parent = None
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

# the texture Rodin baked (Shaded: one base colour image)
img = None
for m in ob.data.materials:
    for n in (m.node_tree.nodes if m and m.use_nodes else []):
        if n.type == 'TEX_IMAGE' and n.image: img = img or n.image

# orientation: longest horizontal extent along X, optional extra yaw
def dims():
    vs = [ob.matrix_world @ v.co for v in ob.data.vertices]
    lo = Vector([min(v[i] for v in vs) for i in range(3)]); hi = Vector([max(v[i] for v in vs) for i in range(3)])
    return lo, hi
lo, hi = dims()
yaw = job.get('yaw', 0)
if job.get('long_axis', 'x') == 'x' and (hi - lo).y > (hi - lo).x: yaw += 90
if yaw:
    ob.rotation_euler.z = math.radians(yaw); bpy.ops.object.transform_apply(rotation=True)

# decimate to the budget (collapse keeps UVs), then clean
bm = bmesh.new(); bm.from_mesh(ob.data)
bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=1e-4)   # GLB splits vertices at UV seams; loops keep their UVs
bm.to_mesh(ob.data); bm.free()
for _ in range(6):
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    if tris <= job['tris']: break
    md = ob.modifiers.new('dec', 'DECIMATE'); md.ratio = max(0.05, job['tris'] / tris * 0.97); md.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier='dec')
    if sum(len(p.vertices) - 2 for p in ob.data.polygons) > tris * 0.95:   # collapse stuck at seams: drop UV-seam protection
        md = ob.modifiers.new('dec', 'DECIMATE'); md.ratio = max(0.05, job['tris'] / tris * 0.97); md.delimit = set()
        bpy.ops.object.modifier_apply(modifier='dec')
bm = bmesh.new(); bm.from_mesh(ob.data)
bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=1e-5)
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bmesh.ops.delete(bm, geom=[e for e in bm.edges if len(e.link_faces) > 2], context='EDGES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(ob.data); bm.free()

# exact box: scale each axis, then place the origin
lo, hi = dims(); w, d, h = job['size']
ob.scale = (w / (hi - lo).x, d / (hi - lo).y, h / (hi - lo).z)
if job.get('mirror'): ob.scale.x *= -1
bpy.ops.object.transform_apply(scale=True)
if job.get('mirror'):
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT'); bpy.ops.mesh.flip_normals(); bpy.ops.object.mode_set(mode='OBJECT')
lo, hi = dims()
ob.location = (-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z); bpy.ops.object.transform_apply(location=True)

# one material, its texture at 2048
tex_id = job.get('texture_from', aid)
mat = bpy.data.materials.new('M_' + tex_id); mat.use_nodes = True
ob.data.materials.clear(); ob.data.materials.append(mat)
for p in ob.data.polygons: p.material_index = 0
tex_path = R / 'textures' / f'T_{tex_id}.png'
if not job.get('texture_from'):
    assert img is not None, 'no base colour texture in the GLB'
    if img.size[0] > 2048: img.scale(2048, 2048)
    img.filepath_raw = str(tex_path); img.file_format = 'PNG'; img.save()
t = mat.node_tree.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(str(tex_path))
mat.node_tree.links.new(t.outputs['Color'], mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])

meta = {'id': aid, 'kind': job.get('kind', 'hive'), 'material': 'M_' + tex_id, 'texture': f'textures/T_{tex_id}.png', 'emissive': None,
        'nominal': job['size'], 'origin': job.get('origin', 'ground-centre')}
# non-manifold edges from Rodin are tolerated by export_asset only if none branch; boundary edges are fine for organic scans
entry, audit = export_asset(ob, meta, int(job['tris'] * 1.05))
res_path = R / 'work' / 'fit_results.json'
res = json.loads(res_path.read_text()) if res_path.exists() else {}
res[aid] = {**entry, 'audit': audit}
res_path.write_text(json.dumps(res, indent=1))
print('FIT', aid, entry['tris'], entry['bounds'])
