"""Pack the authored roof/dressing UVs together with an explicit eight-pixel gutter."""
import bpy,json,sys
from pathlib import Path
R=Path(__file__).resolve().parents[1]
roof=json.loads((R/'src/roof_clutter_texture_queue.json').read_text())['assets']
dressing=json.loads((R/'work/dressing/dressing_assets.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
records=[]
for source,ids in [(r['source'],[r['id']]) for r in roof]+[('src/dressing_authored.blend',[a['id'] for a in dressing])]:
 with bpy.data.libraries.load(str(R/source),link=False) as (src,dst):dst.objects=ids
 for o in dst.objects:
  if o:bpy.context.collection.objects.link(o);o.hide_set(False);o.hide_render=False
for r in roof:
 a={'id':r['id'],'kind':'prop','family':None,'nominal':r['metadata']['nominal'],'origin':'ground-centre','fits_box':None,'also_fits':[],'recess':[],'quest_object':None,'concept':[r['reference']],'notes':r['metadata']['notes'],'budget':r['audit']['budget'],'model':r['model'],'reference':r['reference']}
 if 'poster_face' in r['metadata']:a['poster_face']=r['metadata']['poster_face']
 records.append(a)
for r in dressing:
 records.append({'id':r['id'],'kind':'dressing','family':None,'nominal':r['size'],'origin':r['origin'],'fits_box':None,'also_fits':[],'recess':[],'quest_object':None,'concept':r['concept'],'notes':' '.join(r['notes']),'budget':r['budget'],'model':r['input'],'reference':r['concept'][0]})
bpy.ops.object.select_all(action='SELECT');bpy.context.view_layer.objects.active=bpy.context.selected_objects[0]
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.average_islands_scale()
bpy.ops.object.mode_set(mode='OBJECT')
notice=bpy.data.objects['DR_MayorNotice']
for uv in notice.data.uv_layers.active.data:uv.uv*=6
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.pack_islands(rotate=True,margin_method='FRACTION',margin=8/2048)
bpy.ops.object.mode_set(mode='OBJECT')
mat=bpy.data.materials.new('M_ATL_NEUTRAL');mat.use_nodes=True
im=bpy.data.images.new('ATL_NEUTRAL',width=2048,height=2048,alpha=False);im.generated_color=(.35,.25,.15,1)
node=mat.node_tree.nodes.new('ShaderNodeTexImage');node.image=im
shader=mat.node_tree.nodes.get('Principled BSDF');mat.node_tree.links.new(node.outputs['Color'],shader.inputs['Base Color']);mat.node_tree.nodes.active=node
for a in records:
 o=bpy.data.objects[a['id']];o.data.materials.clear();o.data.materials.append(mat)
 for p in o.data.polygons:p.material_index=0
 a.update(material=mat.name,texture='atlas:ATL_NEUTRAL',emissive=None)
(R/'work/neutral_assets.json').write_text(json.dumps(records,indent=2)+'\n')
bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/neutral_authored.blend'))
print('Packed',len(records),'authored neutral assets')
