"""Independent Rodin texture-only results baked onto one authored neutral atlas."""
import bpy,json,sys,shutil,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];stage=R/'work/NEUTRAL_candidate';(stage/'textures').mkdir(parents=True,exist_ok=True)
records=json.loads((R/'work/neutral_assets.json').read_text())
if '--' in sys.argv:
 selected=sys.argv[sys.argv.index('--')+1:]
 if selected:records=[a for a in records if a['id'] in selected]
bpy.ops.wm.open_mainfile(filepath=str(R/'src/neutral_authored.blend'))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
atlas=bpy.data.images['ATL_NEUTRAL'];checks={};audits={};entries=[]
def load(path):
 before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(path));return [o for o in set(bpy.data.objects)-before if o.type=='MESH']
def signature(objects):
 values=[]
 for o in objects:
  uv=o.data.uv_layers.active
  for l in o.data.loops:
   v=o.matrix_world@o.data.vertices[l.vertex_index].co
   values.append(tuple(round(c,5) for c in (*v,*uv.data[l.index].uv)))
 return {'vertices':sum(len(o.data.vertices) for o in objects),'faces':sum(len(o.data.polygons) for o in objects),'position_uv_hash':hashlib.sha256(repr(sorted(values)).encode()).hexdigest()}
for o in scene.objects:o.hide_render=True
for index,a in enumerate(records):
 aid=a['id'];target=bpy.data.objects[aid];target.hide_render=False;target.hide_set(False)
 if aid=='DR_MayorNotice':
  # The generated straight elevation spells the required wording correctly.
  import numpy as np
  source=[target.copy()];source[0].data=target.data.copy();bpy.context.collection.objects.link(source[0])
  image=bpy.data.images.load(str(R/f'concept/{aid}_front.png'));arr=np.empty(image.size[0]*image.size[1]*4,dtype=np.float32);image.pixels.foreach_get(arr);arr=arr.reshape(image.size[1],image.size[0],4)
  mask=(arr[:,:,0]-arr[:,:,2]>.15)&(arr[:,:,0]>.2);yy,xx=np.where(mask)
  crop=(xx.min()/image.size[0],xx.max()/image.size[0],yy.min()/image.size[1],yy.max()/image.size[1])
  neutral=arr[:,:,:3].max(axis=2)-arr[:,:,:3].min(axis=2)<.055
  lum=arr[:,:,:3].mean(axis=2);arr[neutral,:3]=np.array([.65,.39,.15])[None,:]*(.8+lum[neutral,None]*.3)
  image=image.copy();image.pixels.foreach_set(arr.ravel());image.update()
  sm=bpy.data.materials.new('Generated_civic_notice');sm.use_nodes=True;nt=sm.node_tree;t=nt.nodes.new('ShaderNodeTexImage');t.image=image;p=nt.nodes.get('Principled BSDF');nt.links.new(t.outputs['Color'],p.inputs['Base Color'])
  source[0].data.materials.clear();source[0].data.materials.append(sm)
  for l in source[0].data.loops:
   v=source[0].data.vertices[l.vertex_index].co;source[0].data.uv_layers.active.data[l.index].uv=(crop[0]+v.x/.75*(crop[1]-crop[0]),crop[2]+v.z*(crop[3]-crop[2]))
  a['rodin']={'used':False,'method':'generated_sign'}
 else:
  raw=R/f'work/{aid}/base_basic_shaded.glb';retained=R/f'src/rodin/{aid}/base_basic_shaded.glb'
  if not raw.exists():raw=retained
  assert raw.exists(),'Texture not yet ready: '+aid
  source=load(raw);ret=signature(source);uploaded=load(R/a['model']);up=signature(uploaded)
  for o in uploaded:bpy.data.objects.remove(o,do_unlink=True)
  checks[aid]={'uploaded':up,'returned':ret,'equal':up==ret,'baked_to_authored_mesh':True}
  retained.parent.mkdir(parents=True,exist_ok=True)
  if raw.resolve()!=retained.resolve():shutil.copy2(raw,retained)
  a['rodin']={'used':True,'material':'Shaded','baked_to_own_uvs':True}
 converted=set()
 for o in source:
  o.hide_render=False;o.hide_set(False)
  for mat in o.data.materials:
   if mat in converted:continue
   converted.add(mat);nt=mat.node_tree;p=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED');em=nt.nodes.new('ShaderNodeEmission');color=p.inputs['Base Color'] if p.inputs['Base Color'].links else p.inputs['Emission Color']
   if color.links:nt.links.new(color.links[0].from_socket,em.inputs['Color'])
   else:em.inputs['Color'].default_value=color.default_value
   nt.links.new(em.outputs[0],next(n for n in nt.nodes if n.type=='OUTPUT_MATERIAL').inputs['Surface'])
 bpy.ops.object.select_all(action='DESELECT')
 for o in source:o.select_set(True)
 target.select_set(True);bpy.context.view_layer.objects.active=target
 scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.015;scene.render.bake.max_ray_distance=.1;scene.render.bake.margin=8;scene.render.bake.use_clear=index==0
 bpy.ops.object.bake(type='EMIT')
 for o in source:bpy.data.objects.remove(o,do_unlink=True)
 target.hide_render=True
atlas.filepath_raw=str(stage/'textures/T_ATL_NEUTRAL.png');atlas.file_format='PNG';atlas.save();atlas.pack()
for a in records:
 obj=bpy.data.objects[a['id']];meta={k:v for k,v in a.items() if k not in ['model','reference','budget']}
 entry,audit=export_asset(obj,meta,a['budget'],output_root=stage)
 if a['origin']=='ground-centre':entry['nominal']=[round(entry['bounds'][1][i]-entry['bounds'][0][i],6) for i in range(3)]
 entries.append(entry);audits[a['id']]=audit
(R/'work/NEUTRAL_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n');(R/'src/NEUTRAL_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n');(R/'src/NEUTRAL_option_b_source_checks.json').write_text(json.dumps(checks,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/NEUTRAL_kit_textured.blend'))
print('Neutral atlas baked; visual review required')
