"""Bake texture-only output onto the exact submitted authored GLB and UVs."""
import bpy,json,sys,hashlib,shutil
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];metadata_path=R/sys.argv[sys.argv.index('--')+1];meta=json.loads(metadata_path.read_text());meta={**meta,**meta.get('metadata',{})};aid=meta['id'];stage=R/f'work/{aid}_candidate';(stage/'textures').mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True);scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
def load(path):
 old=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(path));return [o for o in set(bpy.data.objects)-old if o.type=='MESH']
def sig(objects):
 data=[]
 for o in objects:
  uv=o.data.uv_layers.active
  for l in o.data.loops:
   v=o.matrix_world@o.data.vertices[l.vertex_index].co;data.append(tuple(round(x,5) for x in (*v,*uv.data[l.index].uv)))
 return {'vertices':sum(len(o.data.vertices) for o in objects),'faces':sum(len(o.data.polygons) for o in objects),'position_uv_hash':hashlib.sha256(repr(sorted(data)).encode()).hexdigest()}
model=R/meta.get('input_glb',meta.get('model',f'work/{aid}_input.glb'));own=load(model);assert len(own)==1;target=own[0];target.name=aid;uploaded=sig(own)
bpy.ops.object.select_all(action='DESELECT');target.select_set(True);bpy.context.view_layer.objects.active=target
world=target.matrix_world.copy();target.parent=None;target.matrix_world=world;bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
cleanup=None
if meta['source'].startswith('src/hub_egyptian/'):
 import runpy
 cleanup=runpy.run_path(str(R/'src/hub_egyptian/cleanup.py'))['apply'](target)
before=sig([target])
image=bpy.data.images.new('Albedo_'+aid,width=2048,height=2048,alpha=False);image.colorspace_settings.name='sRGB'
old_materials=list(target.data.materials);target.data.materials.clear()
for mat in old_materials:
 if mat.users==0:bpy.data.materials.remove(mat)
material=bpy.data.materials.new('M_'+aid);material.use_nodes=True;target.data.materials.append(material)
for p in target.data.polygons:p.material_index=0
nt=material.node_tree;t=nt.nodes.new('ShaderNodeTexImage');t.image=image;nt.nodes.active=t;nt.links.new(t.outputs['Color'],nt.nodes['Principled BSDF'].inputs['Base Color'])
raw=R/f'work/{aid}/base_basic_shaded.glb'
if not raw.exists():raw=R/f'src/rodin/{aid}/base_basic_shaded.glb'
assert raw.exists(),'Rodin output not ready: '+aid
source=load(raw);returned=sig(source)
for o in source:
 for sm in o.data.materials:
  nt=sm.node_tree;p=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED');em=nt.nodes.new('ShaderNodeEmission');color=p.inputs['Base Color'] if p.inputs['Base Color'].links else p.inputs['Emission Color']
  if color.links:nt.links.new(color.links[0].from_socket,em.inputs['Color'])
  else:em.inputs['Color'].default_value=color.default_value
  nt.links.new(em.outputs[0],next(n for n in nt.nodes if n.type=='OUTPUT_MATERIAL').inputs['Surface'])
bpy.ops.object.select_all(action='DESELECT')
for o in source:o.select_set(True)
target.select_set(True);bpy.context.view_layer.objects.active=target
scene.render.engine='CYCLES';scene.cycles.samples=1;scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.05;scene.render.bake.max_ray_distance=.25;scene.render.bake.margin=8
bpy.ops.object.bake(type='EMIT');assert sig([target])==before,'Authored mesh/UV changed during bake'
for o in source:bpy.data.objects.remove(o,do_unlink=True)
image.filepath_raw=str(stage/f'textures/T_{aid}.png');image.file_format='PNG';image.save();image.pack()
entry={k:v for k,v in meta.items() if k not in ['audit','uv','input_glb','model','object']}
labels={'LM_Graumans':"Grauman's Chinese Theatre",'LM_Egyptian':'The Egyptian Theatre','LM_Roosevelt':'Roosevelt Hotel','LM_Pantages':'Pantages Theatre','LM_Taft':'Taft Building','LM_HollywoodHotel':'Hollywood Hotel','LM_FirstNational':'First National Bank','LM_PoliceStation':'Hollywood Police Station','LM_BrownDerby':'Brown Derby'}
entry.update(kind='landmark',family=None,material=material.name,texture=f'textures/T_{aid}.png',emissive=None,origin='footprint-centre-ground',fits_box=meta.get('fits_box',labels.get(aid)),also_fits=meta.get('also_fits',[]),recess=meta.get('recess',[]),quest_object=meta.get('quest_object'),rodin={'used':True,'material':'Shaded','baked_to_own_uvs':True},source=meta['source'])
if aid.endswith(('_Wing','_ForecourtWall','_CourtWall')):entry['also_fits']=[{'size':meta['nominal'],'mirror':True}]
prompts=meta.get('prompts',{})
if isinstance(prompts,str):prompts=json.loads((R/prompts).read_text()) if prompts.endswith('.json') else {'prompt':(R/prompts).read_text(encoding='utf-8')}
elif isinstance(prompts,list):prompts={p:(R/p).read_text(encoding='utf-8') for p in prompts}
entry['prompts']=prompts
budget=20000 if aid in ['LM_Graumans','LM_Egyptian','LM_Roosevelt','LM_Pantages','LM_Taft'] else 10000
entry,audit=export_asset(target,entry,budget,output_root=stage)
(R/f'work/{aid}_kit_assets.json').write_text(json.dumps([entry],indent=2)+'\n');(R/f'src/{aid}_export_audit.json').write_text(json.dumps(audit,indent=2)+'\n')
checks={'uploaded_authored':uploaded,'cleaned_authored':before,'returned':returned,'equal':uploaded==returned,'baked_to_authored_mesh':True,'authored_geometry_uv_unchanged_during_bake':True,'cleanup':cleanup}
(R/f'src/{aid}_source_checks.json').write_text(json.dumps(checks,indent=2)+'\n')
retained=R/f'src/rodin/{aid}';retained.mkdir(parents=True,exist_ok=True)
if raw.resolve()!=(retained/raw.name).resolve():shutil.copy2(raw,retained/raw.name)
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{aid}_kit_textured.blend'))
print('Landmark baked to authored mesh:',aid,entry['tris'],'triangles; inspect before publishing')
