import bpy,json
from pathlib import Path
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
im=bpy.data.images.load(str(R/'signs/T_SIGN_01.png'));m=bpy.data.materials.new('M_SIGN_01');m.use_nodes=True
node=m.node_tree.nodes.new('ShaderNodeTexImage');node.image=im
p=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED');m.node_tree.links.new(node.outputs['Color'],p.inputs['Base Color'])
entries=[]
for sign in json.loads((R/'work/sign_records.json').read_text()):
 aid=sign['id'];w,h=sign['nominal'];x,y,uw,vh=sign['rect']
 mesh=bpy.data.meshes.new(aid);mesh.from_pydata([(0,0,0),(w,0,0),(w,0,h),(0,0,h)],[],[(0,1,2),(0,2,3)]);mesh.update()
 uv=mesh.uv_layers.new();coords=[(x,1-y-vh),(x+uw,1-y-vh),(x+uw,1-y),(x,1-y)]
 for loop in mesh.loops:uv.data[loop.index].uv=coords[loop.vertex_index]
 o=bpy.data.objects.new(aid,mesh);bpy.context.collection.objects.link(o);mesh.materials.append(m)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.fbx(filepath=str(R/sign['card_mesh']),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',bake_anim=False,path_mode='RELATIVE')
 old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(R/sign['card_mesh']));q=next(q for q in set(bpy.data.objects)-old if q.type=='MESH')
 bounds=[[round(fn((q.matrix_world@v.co)[i] for v in q.data.vertices),6) for i in range(3)] for fn in [min,max]]
 entries.append({'id':aid,'kind':'sign_card','file':sign['card_mesh'],'material':'M_SIGN_01','texture':sign['page'],'emissive':None,'nominal':[w,h],'bounds':bounds,'tris':len(q.data.polygons),'origin':'front-plane-bottom-left','fits_box':None,'also_fits':[],'recess':[],'quest_object':None,'concept':[],'rodin':{'used':False,'method':'font-composition'},'notes':'Section 10.1 lettering exception; OFL Limelight. Top-left normalized sign rectangle.'})
 bpy.data.objects.remove(q,do_unlink=True)
(R/'work/sign_assets.json').write_text(json.dumps(entries,indent=2))
bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/signs.blend'))
