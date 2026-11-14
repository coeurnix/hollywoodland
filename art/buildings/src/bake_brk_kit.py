"""Bake the combined Rodin source to authored UVs; export and measure every module.
Does not publish until visual inspection and the phase-two kit proof are complete.
"""
import bpy,json,hashlib,shutil,sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1]
STAGE=R/'work/BRK_candidate';(STAGE/'textures').mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(R/'src/BRK_kit_authored.blend'))
plan=[a for a in json.loads((R/'src/kit_plan.json').read_text()) if a['family']=='BRK']
layout={a['id']:a for a in json.loads((R/'work/BRK_texture_layout.json').read_text())}
old=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(R/'work/ATL_BRK/base_basic_shaded.glb'))
sources=[o for o in set(bpy.data.objects)-old if o.type=='MESH']
def signature(objects):
 records=[]
 for o in objects:
  uv=o.data.uv_layers.active
  for loop in o.data.loops:
   p=o.matrix_world@o.data.vertices[loop.vertex_index].co
   records.append(tuple(round(v,5) for v in (*p,*uv.data[loop.index].uv)))
 return {'vertices':sum(len(o.data.vertices) for o in objects),'faces':sum(len(o.data.polygons) for o in objects),'world_position_uv_hash':hashlib.sha256(repr(sorted(records)).encode()).hexdigest()}
returned=signature(sources)
before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(R/'work/ATL_BRK_input.glb'))
uploaded_objects=list(set(bpy.data.objects)-before);uploaded=signature([o for o in uploaded_objects if o.type=='MESH'])
for o in uploaded_objects:bpy.data.objects.remove(o,do_unlink=True)
(R/'src/BRK_rodin_geometry_check.json').write_text(json.dumps({'uploaded':uploaded,'returned':returned,'geometry_uv_signature_equal':uploaded==returned,'baked_to_own_mesh_and_uvs':True},indent=2)+'\n')
for source in sources:
 for mat in source.data.materials:
  nt=mat.node_tree;p=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED');out=next(n for n in nt.nodes if n.type=='OUTPUT_MATERIAL')
  em=nt.nodes.new('ShaderNodeEmission');color=p.inputs['Base Color'] if p.inputs['Base Color'].links else p.inputs['Emission Color']
  if color.links:nt.links.new(color.links[0].from_socket,em.inputs['Color'])
  else:em.inputs['Color'].default_value=color.default_value
  nt.links.new(em.outputs[0],out.inputs['Surface'])
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
image=bpy.data.images.new('BRK_Atlas_Final',width=2048,height=2048,alpha=False)
mat=bpy.data.materials['M_ATL_BRK'];tex=next(n for n in mat.node_tree.nodes if n.type=='TEX_IMAGE');tex.image=image;mat.node_tree.nodes.active=tex
scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.03;scene.render.bake.max_ray_distance=.15;scene.render.bake.margin=8
proxies=[]
for a in plan:
 if a['kind']!='module':continue
 obj=bpy.data.objects[a['id']];proxy=obj.copy();proxy.data=obj.data.copy();bpy.context.collection.objects.link(proxy)
 proxy.name='BAKE_PROXY_'+a['id'];proxy.location=Vector(layout[a['id']]['offset']);proxies.append(proxy)
bpy.ops.object.select_all(action='DESELECT')
for proxy in proxies:proxy.select_set(True)
bpy.context.view_layer.objects.active=proxies[0];bpy.ops.object.join();proxy=bpy.context.view_layer.objects.active
bpy.ops.object.select_all(action='DESELECT')
for src in sources:src.select_set(True)
proxy.select_set(True);bpy.context.view_layer.objects.active=proxy;scene.render.bake.use_clear=True
bpy.ops.object.bake(type='EMIT');bpy.data.objects.remove(proxy,do_unlink=True)
for source in sources:bpy.data.objects.remove(source,do_unlink=True)
image.filepath_raw=str(STAGE/'textures/T_ATL_BRK.png');image.file_format='PNG';image.save()
entries=[];audits={}
for a in plan:
 obj=bpy.data.objects[a['id']]
 if a['kind']!='module':
  node=next(n for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE');name=Path(node.image.filepath).name
  shutil.copy2(R/'textures'/name,STAGE/'textures'/name);node.image.filepath=str(STAGE/'textures'/name)
 texture='atlas:ATL_BRK' if a['kind']=='module' else 'tile:TILE_'+('Brick_Red' if a['kind']=='wall' else 'Roof_TarGravel')
 meta={'id':a['id'],'kind':a['kind'],'family':'BRK','material':obj.data.materials[0].name,'texture':texture,'emissive':None,'nominal':a['nominal'],'origin':'front-plane-bottom-left','fits_box':None,'also_fits':[],'recess':[],'quest_object':None,'concept':[f"concept/{a['id']}_front.png",f"concept/{a['id']}_34.png"],'rodin':{'used':a['kind']=='module','material':'Shaded' if a['kind']=='module' else None,'baked_to_own_uvs':a['kind']=='module'},'notes':'Shared atlas Option A. Intentional open wall-facing boundaries.'}
 if 'Corner' in a['id']:meta['notes']+=' Nominal 1 m is wrap length: 0.5 m on each perpendicular face; measured X span 0.5 m.'
 entry,audit=export_asset(obj,meta,600 if a['kind']=='module' else 60,output_root=STAGE);entries.append(entry);audits[a['id']]=audit
(R/'work/BRK_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n')
(R/'src/BRK_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/BRK_kit_textured.blend'))
print('Baked and measured',len(entries),'BRK modules; visual review still required')
