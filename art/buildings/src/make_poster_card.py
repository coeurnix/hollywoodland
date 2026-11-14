"""SIGN_Graumans_Poster: a 1.2 x 1.8 m card (front-plane-bottom-left, faces -Y) on sign page T_SIGN_04, rect top-left [0, 0, 0.375, 0.5625].
    blender -b --factory-startup -P art/buildings/src/make_poster_card.py
"""
import bpy, bmesh, json, sys
from pathlib import Path
R = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(Path(__file__).resolve().parent))
from asset_io import export_asset
bpy.ops.wm.read_factory_settings(use_empty=True)
w, h, ru, rv = 1.2, 1.8, 0.375, 0.5625
bm = bmesh.new(); uv = bm.loops.layers.uv.new('UVMap')
f = bm.faces.new([bm.verts.new(c) for c in [(0, 0, 0), (w, 0, 0), (w, 0, h), (0, 0, h)]])
for l, t in zip(f.loops, [(0, 1 - rv), (ru, 1 - rv), (ru, 1), (0, 1)]): l[uv].uv = t
me = bpy.data.meshes.new('SIGN_Graumans_Poster'); bm.to_mesh(me); bm.free()
ob = bpy.data.objects.new(me.name, me); bpy.context.scene.collection.objects.link(ob)
mat = bpy.data.materials.new('M_SIGN_04'); mat.use_nodes = True; me.materials.append(mat)
t = mat.node_tree.nodes.new('ShaderNodeTexImage'); t.image = bpy.data.images.load(str(R / 'signs' / 'T_SIGN_04.png'))
mat.node_tree.links.new(t.outputs['Color'], mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
meta = {'id': 'SIGN_Graumans_Poster', 'kind': 'sign_card', 'family': None, 'material': 'M_SIGN_04', 'texture': 'signs/T_SIGN_04.png', 'emissive': None,
        'nominal': [w, h], 'origin': 'front-plane-bottom-left'}
entry, audit = export_asset(ob, meta, 50)
res_p = R / 'work' / 'fit_results.json'; res = json.loads(res_p.read_text()); res[entry['id']] = {**entry, 'audit': audit}; res_p.write_text(json.dumps(res, indent=1))
print('CARD', entry['tris'], entry['bounds'])
