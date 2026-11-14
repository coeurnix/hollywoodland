"""SPEC15 HollywoodHotel projection fallback after two failed Rodin server jobs.
Camera-project generated features onto representative atlas planes, Cycles EMIT
bake into the frozen authored UV atlas, then paint repeated seams and padding.
The uploaded GLB and all original faces/UVs remain unchanged. A deliberate own-mesh
repair adds two small entry-side returns to the main range, documented in checks.
Run Blender5.2 --background --python src/hub_hotels/project_hotels.py -- <assetID>.
"""
import bpy,bmesh,json,sys,hashlib,math
import numpy as np
from pathlib import Path
R=Path(__file__).resolve().parents[2];SRC=R/'src/hub_hotels';AID=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'LM_HollywoodHotel';assert AID in ['LM_HollywoodHotel','LM_HollywoodHotel_Wing'];STAGE=R/f'work/{AID}_candidate';TEX=STAGE/'textures';TEX.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(R/'src'));from asset_io import export_asset
meta=json.loads((SRC/f'{AID}.json').read_text());layout=json.loads((SRC/'uv_layout.json').read_text());input_path=R/f'work/{AID}_input.glb';input_sha=hashlib.sha256(input_path.read_bytes()).hexdigest()
def signature(o):
 uv=o.data.uv_layers.active;records=[]
 for li,l in enumerate(o.data.loops):records.append(tuple(round(x,6) for x in (*(o.matrix_world@o.data.vertices[l.vertex_index].co),*uv.data[li].uv)))
 return {'vertices':len(o.data.vertices),'faces':len(o.data.polygons),'position_uv_hash':hashlib.sha256(repr(sorted(records)).encode()).hexdigest()}
def strict(o):
 bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces))
 a={'tris':len(bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'loose_vertices':sum(not v.link_faces for v in bm.verts),'orientation_discontinuities':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges)}
 if a['branch_edges']:print('PATCH_BRANCH',[[list(v.co) for v in e.verts] for e in bm.edges if len(e.link_faces)>2])
 bm.free()
 assert a['branch_edges']==a['zero_area_faces']==a['loose_vertices']==a['orientation_discontinuities']==0,a
 return a
bpy.ops.wm.read_factory_settings(use_empty=True);sc=bpy.context.scene;sc.unit_settings.system='METRIC';sc.unit_settings.scale_length=1;sc.render.engine='CYCLES';sc.cycles.samples=1
bpy.ops.import_scene.gltf(filepath=str(input_path));target=next(o for o in sc.objects if o.type=='MESH');target.name=AID
world=target.matrix_world.copy();target.parent=None;target.matrix_world=world;bpy.context.view_layer.objects.active=target;target.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
frozen_import=signature(target);own_repairs=[]
if AID=='LM_HollywoodHotel':
 # Own authored quality fix: close two veranda-to-entry side returns.
 # Adjacent front veranda wall is recessed1.25m but entry skin is on front plane.
 bm=bmesh.new();bm.from_mesh(target.data);luv=bm.loops.layers.uv.active
 xedge=min((v.co.x for v in bm.verts if v.co.x>0),key=lambda x:abs(x-116/19/2))
 for sign in [-1,1]:
  coords=[(sign*xedge,-13,.0005),(sign*xedge,-11.75,.0005),(sign*xedge,-11.75,3.5995),(sign*xedge,-13,3.5995)]
  if sign<0:coords.reverse()
  verts=[bm.verts.new(p) for p in coords];face=bm.faces.new(verts)
  for loop in face.loops:
   loop[luv].uv=(.01+(loop.vert.co.y+13)/6*.23,.26+loop.vert.co.z/6*.23)
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(target.data);bm.free();target.data.update()
 own_repairs=[{'change':'Added two1.25m-deep x3.599m-high plaster return faces at x=+/-3.0526316m beside front central arch','reason':'Close background slits where on-plane entry skin meets1.25m recessed adjacent verandas','floor_edge_clearance_m':0.0005,'added_triangles':4,'nominal_box_unchanged':True,'original_faces_and_uvs_preserved':True}]
before=signature(target);precheck=strict(target);target.hide_render=True;target.hide_set(True)
atlas=bpy.data.images.new(AID+'_AuthoredUV_Projection',width=2048,height=2048,alpha=False);atlas.colorspace_settings.name='sRGB'
pix=np.ones((2048,2048,4),dtype=np.float32);pix[:,:,:3]=(.50,.37,.22);atlas.pixels.foreach_set(pix.ravel());atlas.update()
images={key:bpy.data.images.load(str(R/path),check_existing=True) for key,path in {
 'front':'concept/LM_HollywoodHotel_front.png','threequarter':f'concept/{AID}_34.png',
 'side':'work/hub_hotels/projection/'+('wing_side.png' if AID.endswith('_Wing') else 'hotel_side.png'),
 'back':'work/hub_hotels/projection/'+('wing_back.png' if AID.endswith('_Wing') else 'hotel_back.png'),
 'roof':'work/hub_hotels/projection/hotel_roof.png'}.items()}
MATS={};planes=[];projection_records=[]
def material(key):
 if key in MATS:return MATS[key]
 m=bpy.data.materials.new('ProjectionSource_'+key);m.use_nodes=True;nt=m.node_tree;nt.nodes.clear();out=nt.nodes.new('ShaderNodeOutputMaterial');em=nt.nodes.new('ShaderNodeEmission');tex=nt.nodes.new('ShaderNodeTexImage');tex.image=images[key];tex.extension='EXTEND';coord=nt.nodes.new('ShaderNodeUVMap');coord.uv_map='ProjectionUV';nt.links.new(coord.outputs['UV'],tex.inputs['Vector']);nt.links.new(tex.outputs['Color'],em.inputs['Color']);nt.links.new(em.outputs[0],out.inputs['Surface']);dest=nt.nodes.new('ShaderNodeTexImage');dest.image=atlas;nt.nodes.active=dest;MATS[key]=m;return m
def project(role,box,key,crop):
 """box is local metres [x0,y0,x1,y1]; crop is source top-left pixels."""
 rect,span=layout[role];a,b,c,d=box;u0=rect[0]+a/span[0]*(rect[2]-rect[0]);u1=rect[0]+c/span[0]*(rect[2]-rect[0]);v0=rect[1]+b/span[1]*(rect[3]-rect[1]);v1=rect[1]+d/span[1]*(rect[3]-rect[1])
 me=bpy.data.meshes.new('CameraProjection_'+role);me.from_pydata([(u0*10,v0*10,0),(u1*10,v0*10,0),(u1*10,v1*10,0),(u0*10,v1*10,0)],[],[(0,1,2,3)]);me.update();ob=bpy.data.objects.new(me.name,me);sc.collection.objects.link(ob);me.materials.append(material(key));uv=me.uv_layers.new(name='BakeUV');puv=me.uv_layers.new(name='ProjectionUV');me.uv_layers.active=uv;me.uv_layers.active_index=0
 sw,sh=images[key].size;x0,y0,x1,y1=crop
 for i,(xy,p) in enumerate(zip([(u0,v0),(u1,v0),(u1,v1),(u0,v1)],[(x0/sw,1-y1/sh),(x1/sw,1-y1/sh),(x1/sw,1-y0/sh),(x0/sw,1-y0/sh)])):uv.data[i].uv=xy;puv.data[i].uv=p
 planes.append(ob);projection_records.append({'role':role,'local_m':box,'source':key,'crop_pixels':crop,'atlas_rect':[u0,v0,u1,v1]})
def allrole(role,key,crop):project(role,[0,0,*layout[role][1]],key,crop)
# Representative orthographic cameras crop only the generated architectural surfaces.
# Roof and rear plates provide source information unseen by the front concept.
for role in ['upper','door','wall','trim','gable','archupper','arc']:
 allrole(role,'front',[181,298,202,339])
allrole('trim','front',[44,558,62,621])
allrole('timber','front',[247,465,260,489])
allrole('metal','front',[247,465,260,489]);allrole('sign','front',[1000,272,1090,301])
allrole('ground','front',[82,649,187,655])
allrole('roof','roof',[0,0,1024,1024]);allrole('tile','front',[329,191,408,201])
allrole('canvas','front',[35,367,203,390])
# Windows and French doors occupy the authored openings; skin outside stays plaster.
def replace_feature(role,box,key,crop):
 for ob in list(planes):
  if ob.name.startswith('CameraProjection_'+role):
   planes.remove(ob);bpy.data.objects.remove(ob,do_unlink=True)
 x0,y0,x1,y1=box
 for margin in [[0,0,x0,7],[x1,0,7,7],[x0,0,x1,y0],[x0,y1,x1,7]]:
  if margin[2]>margin[0] and margin[3]>margin[1]:project(role,margin,'front',[181,298,202,339])
 project(role,box,key,crop)
cx=2.94 if AID.endswith('_Wing') else 3.05263
replace_feature('upper',[cx-1,.43,cx+1,2.28],'front',[107,300,150,345])
replace_feature('door',[cx-1.2,.08,cx+1.2,2.73],'front',[113,561,148,648])
replace_feature('entry',[cx-1.548,0,cx+1.548,3.3],'front',[1034,510,1148,649])
# Tiny back/side plaster-camera samples add the same brushwork without background.
# Crops measured within neutral-background-free generated plaster.
for role in ['wall','gable']:
 for ob in list(planes):
  if ob.name.startswith('CameraProjection_'+role):planes.remove(ob);bpy.data.objects.remove(ob,do_unlink=True)
if AID.endswith('_Wing'):
 allrole('wall','side',[340,330,390,402]);allrole('gable','back',[617,208,665,246])
else:
 allrole('wall','side',[338,259,378,351]);allrole('gable','back',[1240,270,1280,295])
# Emit-only bake of the representative camera projections to our own atlas UVs.
bpy.ops.object.select_all(action='DESELECT')
for o in planes:o.select_set(True)
bpy.context.view_layer.objects.active=planes[0];bpy.ops.object.join();proxy=bpy.context.object;proxy.name=AID+'_ProjectionRepresentatives'
sc.render.bake.use_selected_to_active=False;sc.render.bake.use_clear=False;sc.render.bake.margin=8;bpy.ops.object.bake(type='EMIT')
assert signature(target)==before,'Authored position/UV change during projection bake'
# Texture-paint periodic seams and keep blank sign backings free of image context.
data=np.empty(2048*2048*4,dtype=np.float32);atlas.pixels.foreach_get(data);data=data.reshape(2048,2048,4)
def region(role):
 r,s=layout[role];x0,y0,x1,y1=[int(round(v*2048)) for v in r];return data[y0:y1,x0:x1]
def periodic(arr,band=16):
 # Blend opposite edge bands symmetrically without introducing a bright border.
 h,w=arr.shape[:2];band=min(band,h//5,w//5)
 for i in range(band):
  f=(band-i)/band*.5;a=arr[:,i,:3].copy();b=arr[:,-1-i,:3].copy();avg=(a+b)*.5;arr[:,i,:3]=a*(1-f)+avg*f;arr[:,-1-i,:3]=b*(1-f)+avg*f
  a=arr[i,:,:3].copy();b=arr[-1-i,:,:3].copy();avg=(a+b)*.5;arr[i,:,:3]=a*(1-f)+avg*f;arr[-1-i,:,:3]=b*(1-f)+avg*f
 arr[:,0,:3]=arr[:,-1,:3]=(arr[:,0,:3]+arr[:,-1,:3])*.5;arr[0,:,:3]=arr[-1,:,:3]=(arr[0,:,:3]+arr[-1,:,:3])*.5
for role in ['roof','tile','trim','wall','ground']:
 periodic(region(role))
roof_pixels=region('roof');roof_mean=roof_pixels[:,:,:3].mean(axis=(0,1));roof_pixels[:,:,:3]=roof_mean[None,None,:]+(roof_pixels[:,:,:3]-roof_mean[None,None,:])*.65
# Hand-paint striped canvas from its generated colors, correcting perspective.
a=region('canvas');hh,ww=a.shape[:2];yy,xx=np.mgrid[0:hh,0:ww];stripe=((xx/(ww/6.5)*1.075).astype(int)%2)==0
base=np.where(stripe[:,:,None],np.array([.065,.145,.055]),np.array([.72,.56,.30]))
a[:,:,:3]=base*(.93+.07*(yy/hh))[:,:,None];a[:,:,3]=1
# Frozen canopy planes are partly covered by the z6.9 upper wall.
# Paint the same stripe cadence into its bottom0.28m to continue the awning.
a=region('upper');hh,ww=a.shape[:2];band=int(round(hh*.28/7));xx=np.arange(ww);stripe=((xx/(ww/7)*1.075).astype(int)%2)==0
base=np.where(stripe[:,None],np.array([.065,.145,.055]),np.array([.72,.56,.30]));a[:band,:,:3]=base[None,:,:]*np.linspace(.78,1,band)[:,None,None]
# The plain sign backing is intentionally free of lettering for the separate card.
a=region('sign');a[:,:,:3]=np.array([.65,.47,.25]);a[:,:,3]=1
# Fill8px of external region padding by edge extension, preserving all gaps.
for role,(r,s) in layout.items():
 x0,y0,x1,y1=[int(round(v*2048)) for v in r];pad=8
 data[y0:y1,x0-pad:x0]=data[y0:y1,x0:x0+1];data[y0:y1,x1:x1+pad]=data[y0:y1,x1-1:x1]
 data[y0-pad:y0,x0-pad:x1+pad]=data[y0:y0+1,x0-pad:x1+pad];data[y1:y1+pad,x0-pad:x1+pad]=data[y1-1:y1,x0-pad:x1+pad]
data[:,:,3]=1;atlas.pixels.foreach_set(data.ravel());atlas.update();atlas.filepath_raw=str(TEX/f'T_{AID}.png');atlas.file_format='PNG';atlas.save();atlas.pack()
# Retain only the authored mesh in the final Blender scene; all rebuild logic is above.
bpy.data.objects.remove(proxy,do_unlink=True);target.hide_set(False);target.hide_render=False
material=bpy.data.materials.new('M_'+AID+'_Final');material.use_nodes=True;nt=material.node_tree;tex=nt.nodes.new('ShaderNodeTexImage');tex.image=atlas;uv=nt.nodes.new('ShaderNodeUVMap');uv.uv_map=target.data.uv_layers.active.name;nt.links.new(uv.outputs['UV'],tex.inputs['Vector']);nt.links.new(tex.outputs['Color'],nt.nodes['Principled BSDF'].inputs['Base Color']);nt.nodes['Principled BSDF'].inputs['Roughness'].default_value=1.0;nt.nodes['Principled BSDF'].inputs['Specular IOR Level'].default_value=0.0
target.data.materials.clear();target.data.materials.append(material)
for p in target.data.polygons:p.material_index=0
for m in list(bpy.data.materials):
 if m!=material and m.users==0:bpy.data.materials.remove(m)
material.name='M_'+AID;assert signature(target)==before;after_bake=signature(target)
bpy.ops.object.select_all(action='DESELECT');target.select_set(True);bpy.context.view_layer.objects.active=target
entry={'id':AID,'kind':'landmark','family':None,'material':material.name,'texture':f'textures/T_{AID}.png','emissive':None,'nominal':meta['metadata']['nominal'],'origin':'footprint-centre-ground','fits_box':meta['metadata']['fits_box'],'also_fits':meta['metadata'].get('also_fits',[]),'recess':[],'quest_object':None,'concept':meta['concept']+[str(Path(im.filepath).relative_to(R)).replace('\\','/') for key,im in images.items() if key in ['side','back','roof']],'prompts':dict(meta['prompts'],projection_queue='src/hub_hotels/hotel_projection_queue.json'),'rodin':{'used':False,'method':'projection'},'source':meta['source'],'textured_source':f'src/{AID}_kit_textured.blend','sign_attachments':meta['metadata']['sign_attachments'],'notes':meta['metadata']['notes']+(' Own authored repair: two plaster return faces close narrow slits beside front central entry;4 added triangles, original UVs preserved.' if own_repairs else '')+' SPEC15 projection fallback after two failed Rodin jobs. Generated front/side/back/roof representative camera plates emission-baked into the frozen repeatedUV atlas; canvas perspective and patch seams hand-fixed. User-authorized fal.ai FLUX3Klein side/back/roof generation.'}
entry,audit=export_asset(target,entry,10000,STAGE);postcheck=strict(target)
old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(STAGE/entry['file']));exported=next(o for o in set(bpy.data.objects)-old if o.type=='MESH');fbxcheck=strict(exported)
for o in set(bpy.data.objects)-old:bpy.data.objects.remove(o,do_unlink=True)
assert hashlib.sha256(input_path.read_bytes()).hexdigest()==input_sha,'Frozen input changed'
checks={'method':'camera_projection_emit_bake','rodin_failures_before_fallback':2,'frozen_input_sha256':input_sha,'frozen_import_geometry_uv':frozen_import,'deliberate_authored_mesh_repairs':own_repairs,'hash_before_own_repair':frozen_import['position_uv_hash'],'hash_after_own_repair':before['position_uv_hash'],'authored_before':before,'authored_after_bake':after_bake,'authored_after_export_weld':signature(target),'authored_geometry_uv_unchanged_during_bake':True,'position_uv_hash_preserved_after_export':signature(target)['position_uv_hash']==before['position_uv_hash'],'strict_before':precheck,'strict_after':postcheck,'fbx_roundtrip_welded':fbxcheck,'projection_sources':{k:im.filepath for k,im in images.items()},'projection_planes':projection_records,'paint_fixes':['Periodic roof surface edges and low contrast shingles','Warm stone trim and wall padding8px','Generated canvas colors repainted as straight green-cream stripes; same0.28m painted continuation on upper wall corrects frozen canopy occlusion','Blank sign backing free of background and lettering','Terracotta ridge and porch paving seams matched'],'texture':{'width':2048,'height':2048,'channels':'RGB','bit_depth':8,'color_space':'sRGB'}}
(R/f'work/{AID}_kit_assets.json').write_text(json.dumps([entry],indent=2)+'\n');(R/f'src/{AID}_export_audit.json').write_text(json.dumps(audit,indent=2)+'\n');(R/f'src/{AID}_source_checks.json').write_text(json.dumps(checks,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{AID}_kit_textured.blend'))
print('HOTEL_PROJECTION_STAGED',AID,entry['tris'],json.dumps(audit),flush=True)
