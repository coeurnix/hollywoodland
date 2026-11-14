"""Option B: bake independent representative textures into disjoint role rectangles."""
import bpy,json,sys,hashlib,shutil
from pathlib import Path
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).resolve().parent))
from asset_io import export_asset
R=Path(__file__).resolve().parents[1];FAM=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'BRK';STAGE=R/f'work/{FAM}_candidate'
(STAGE/'textures').mkdir(parents=True,exist_ok=True)
roles=[FAM+'_'+s for s in ['G_Bay6A','U_Bay3A','Pilaster1_G','Parapet4','CrestA','CrestB','Awning6']]
ledger=json.loads((R/'rodin_ledger.json').read_text())
overrides=json.loads((R/'src/projection_overrides.json').read_text()) if (R/'src/projection_overrides.json').exists() else {}
for aid in (roles[2:] if FAM=='BRK' else roles):
 if not (R/'work'/aid/'base_basic_shaded.glb').exists() and not (R/'src/rodin'/aid/'base_basic_shaded.glb').exists() and sum(c['asset']==aid and c['status']=='failed' for c in ledger['calls'])<2:
  raise RuntimeError('Texture representative not finished: '+aid)
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{FAM}_kit_authored.blend'))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
image=bpy.data.images.new(FAM+'_Atlas_OptionB',width=2048,height=2048,alpha=False)
mat=bpy.data.materials['M_ATL_'+FAM];node=next(n for n in mat.node_tree.nodes if n.type=='TEX_IMAGE');node.image=image;mat.node_tree.nodes.active=node
for o in scene.objects:
 if o.type=='MESH':o.hide_render=True
def load_glb(path):
 before=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(path));return [o for o in set(bpy.data.objects)-before if o.type=='MESH']
def emit(material):
 nt=material.node_tree;p=next((n for n in nt.nodes if n.type=='BSDF_PRINCIPLED'),None)
 if p is None:return
 output=next(n for n in nt.nodes if n.type=='OUTPUT_MATERIAL');em=nt.nodes.new('ShaderNodeEmission')
 color=p.inputs['Base Color'] if p.inputs['Base Color'].links else p.inputs['Emission Color']
 if color.links:nt.links.new(color.links[0].from_socket,em.inputs['Color'])
 else:em.inputs['Color'].default_value=color.default_value
 nt.links.new(em.outputs[0],output.inputs['Surface'])
def sig(objects):
 records=[]
 for o in objects:
  uv=o.data.uv_layers.active
  for l in o.data.loops:
   p=o.matrix_world@o.data.vertices[l.vertex_index].co;records.append(tuple(round(v,5) for v in (*p,*uv.data[l.index].uv)))
 return {'vertices':sum(len(o.data.vertices) for o in objects),'faces':sum(len(o.data.polygons) for o in objects),'position_uv_hash':hashlib.sha256(repr(sorted(records)).encode()).hexdigest()}
checks={};methods={};first=True
for aid in roles:
 target=bpy.data.objects[aid];target.hide_render=False
 source_path=R/'work'/aid/'base_basic_shaded.glb'
 if not source_path.exists():source_path=R/'src/rodin'/aid/'base_basic_shaded.glb'
 if source_path.exists() and aid not in overrides:
  source=load_glb(source_path);before=sig(source)
  upload=load_glb(R/'work'/f'{aid}_input.glb');upload_sig=sig(upload)
  for o in upload:bpy.data.objects.remove(o,do_unlink=True)
  checks[aid]={'uploaded':upload_sig,'returned':before,'equal':upload_sig==before,'baked_to_authored_mesh':True}
  methods[aid]={'used':True,'material':'Shaded','baked_to_own_uvs':True}
  dest=R/'src/rodin'/aid;dest.mkdir(parents=True,exist_ok=True)
  if source_path.resolve()!=(dest/source_path.name).resolve():shutil.copy2(source_path,dest/source_path.name)
 else:
  source=[target.copy()];source[0].data=target.data.copy();bpy.context.collection.objects.link(source[0])
  tex=bpy.data.images.load(str(R/'concept'/f'{aid}_front.png'))
  import numpy as np
  rgba=np.empty(tex.size[0]*tex.size[1]*4,dtype=np.float32);tex.pixels.foreach_get(rgba);rgba=rgba.reshape(tex.size[1],tex.size[0],4)
  rgb=rgba[:,:,:3];colored=(rgb.max(axis=2)-rgb.min(axis=2)>.065)&(rgb.max(axis=2)>.1)
  cy,cx=np.where(colored);umin,umax=cx.min()/tex.size[0],cx.max()/tex.size[0];vmin,vmax=cy.min()/tex.size[1],cy.max()/tex.size[1]
  if 'Crest' in aid:
   # Flood only exterior neutral surround; enclosed glass/iron stays unchanged.
   from collections import deque
   neutral=(rgb.max(axis=2)-rgb.min(axis=2)<.045)&(rgb.max(axis=2)>.06)
   hh,ww=neutral.shape;seen=np.zeros_like(neutral);todo=deque()
   for y,x in [(0,x) for x in range(ww)]+[(hh-1,x) for x in range(ww)]+[(y,0) for y in range(hh)]+[(y,ww-1) for y in range(hh)]:
    if neutral[y,x] and not seen[y,x]:seen[y,x]=True;todo.append((y,x))
   while todo:
    y,x=todo.popleft()
    for ny,nx in [(y-1,x),(y+1,x),(y,x-1),(y,x+1)]:
     if 0<=ny<hh and 0<=nx<ww and neutral[ny,nx] and not seen[ny,nx]:seen[ny,nx]=True;todo.append((ny,nx))
   lum=rgb.mean(axis=2);rgba[seen,:3]=np.array([.65,.5,.3])[None,:]*(.8+lum[seen,None]*.3)
   tex=tex.copy();tex.pixels.foreach_set(rgba.ravel());tex.update()
  m=bpy.data.materials.new('Projection_'+aid);m.use_nodes=True;nt=m.node_tree
  t=nt.nodes.new('ShaderNodeTexImage');t.image=tex;p=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED');nt.links.new(t.outputs['Color'],p.inputs['Base Color'])
  source[0].data.materials.clear();source[0].data.materials.append(m)
  vv=[v.co for v in source[0].data.vertices];x0=min(v.x for v in vv);x1=max(v.x for v in vv);z0=min(v.z for v in vv);z1=max(v.z for v in vv)
  for l in source[0].data.loops:
   v=source[0].data.vertices[l.vertex_index].co;source[0].data.uv_layers.active.data[l.index].uv=(umin+(umax-umin)*(v.x-x0)/(x1-x0),vmin+(vmax-vmin)*(v.z-z0)/(z1-z0))
  methods[aid]={'used':False,'method':'projection'};checks[aid]={'projection_fallback_after_two_failures':True,'baked_to_authored_mesh':True}
 converted=set()
 for src in source:
  src.hide_render=False
  for sm in src.data.materials:
   if sm not in converted:emit(sm);converted.add(sm)
 bpy.ops.object.select_all(action='DESELECT')
 for src in source:src.select_set(True)
 target.select_set(True);bpy.context.view_layer.objects.active=target
 scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.03;scene.render.bake.max_ray_distance=.15;scene.render.bake.margin=8;scene.render.bake.use_clear=first
 bpy.ops.object.bake(type='EMIT');first=False
 for src in source:bpy.data.objects.remove(src,do_unlink=True)
 target.hide_render=True
# Wood plank trim is reused from the proof's Rodin-painted door and graded to raw timber.
import numpy as np
pixels=np.empty(2048*2048*4,dtype=np.float32);image.pixels.foreach_get(pixels);pixels=pixels.reshape(2048,2048,4)
# Paint warm stone backing where the front-only parapet projection included neutral surround.
region=pixels[1170:1260,248:1048,:3];high=region.max(axis=2);low=region.min(axis=2);neutral=(high-low<.04)&(high>.08)
stone={'BRK':[.63,.46,.24],'SPN':[.83,.73,.52],'DECO':[.75,.62,.4],'MSN':[.72,.59,.38]}[FAM]
shade=high[neutral];region[neutral]=np.clip(np.array(stone)[None,:]*(.65+shade[:,None]*.6),0,1)
door=pixels[120:220,900:1080,:3].mean(axis=(0,1));wood=np.empty((52,600,3),dtype=np.float32)
yy,xx=np.mgrid[0:52,0:600];grain=(np.sin(xx*.12+np.sin(yy*.4))*np.sin(yy*.8))*.015
wood[:]=np.clip(np.array([.23,.125,.065])+grain[:,:,None]+door*.08,0,1)
pixels[740:808,1256:1872,:3]=np.pad(wood,((8,8),(8,8),(0,0)),mode='edge');pixels[740:808,1256:1872,3]=1
image.pixels.foreach_set(pixels.ravel());image.update();image.filepath_raw=str(STAGE/f'textures/T_ATL_{FAM}.png');image.file_format='PNG';image.save()
entries=[];audits={};plan=[a for a in json.loads((R/'src/kit_plan.json').read_text()) if a['family']==FAM]
for a in plan:
 aid=a['id'];obj=bpy.data.objects[aid]
 if a['kind']=='module':
  role=(FAM+'_G_Bay6A' if '_G_' in aid else FAM+'_U_Bay3A')
  if 'Pilaster' in aid or 'Corner' in aid:role=FAM+'_Pilaster1_G'
  elif 'Parapet' in aid:role=FAM+'_Parapet4'
  elif 'Crest' in aid:role=FAM+('_CrestA' if aid.endswith('A') else '_CrestB')
  elif 'Awning' in aid:role=FAM+'_Awning6'
  texture='atlas:ATL_'+FAM;rodin=dict(methods[role]);rodin['shared_representative']=role
 else:
  tile=({'BRK':'Brick_Red','SPN':'Stucco_White','DECO':'Brick_Buff','MSN':'Stucco_Cream'}[FAM]) if a['kind']=='wall' else 'Roof_TarGravel';texture='textures/T_TILE_'+tile+'.png';rodin={'used':False,'method':'tileable'}
  t=next(n for n in obj.data.materials[0].node_tree.nodes if n.type=='TEX_IMAGE');name=f'T_TILE_{tile}.png';shutil.copy2(R/'textures'/name,STAGE/'textures'/name);t.image.filepath=str(STAGE/'textures'/name)
 meta={'id':aid,'kind':a['kind'],'family':FAM,'material':obj.data.materials[0].name,'texture':texture,'emissive':None,'nominal':a['nominal'],'origin':'front-plane-bottom-left','fits_box':None,'also_fits':[],'recess':[],'quest_object':None,'concept':[f'concept/{aid}_front.png',f'concept/{aid}_34.png'],'rodin':rodin,'notes':'Shared atlas Option B; repeated architectural features share UVs. Deliberately open wall-facing boundaries.'}
 if 'Corner' in aid:meta['notes']+=' 1 m wrap length, two perpendicular 0.5 m faces.'
 entry,audit=export_asset(obj,meta,600 if a['kind']=='module' else 60,output_root=STAGE);entries.append(entry);audits[aid]=audit
(R/f'work/{FAM}_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n');(R/f'src/{FAM}_kit_export_audit.json').write_text(json.dumps(audits,indent=2)+'\n');(R/f'src/{FAM}_option_b_source_checks.json').write_text(json.dumps(checks,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
print('Option B staged; visual review required')
