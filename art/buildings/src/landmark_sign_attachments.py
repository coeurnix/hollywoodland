"""Preview-only placement of separately exported sign cards from manifest."""
import bpy,json,math
from pathlib import Path
R=Path(__file__).resolve().parents[1]

def attach(asset):
    manifest=json.loads((R/'manifest.json').read_text());index={a['id']:a for a in manifest['assets']}
    entry=index.get(asset.name,{})
    for rec in entry.get('attachments',[]):
        a=index[rec['asset']];old=set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(R/a['file']))
        card=next(o for o in set(bpy.data.objects)-old if o.type=='MESH')
        mat=bpy.data.materials.new('Review_'+a['material']);mat.use_nodes=True
        tex=mat.node_tree.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(R/a['texture']),check_existing=True)
        shader=mat.node_tree.nodes.get('Principled BSDF');shader.inputs['Roughness'].default_value=1
        mat.node_tree.links.new(tex.outputs['Color'],shader.inputs['Base Color'])
        card.data.materials.clear();card.data.materials.append(mat)
        card.parent=asset;card.location=rec['location'];card.rotation_euler=[math.radians(v) for v in rec.get('rotation',[0,0,0])];card.scale=rec.get('scale',[1,1,1])
