"""SPEC15 Roosevelt projection fallback after two failed Rodin server jobs.
Camera-project generated features onto representative atlas planes, Cycles EMIT
bake into the frozen authored UV atlas, then paint repeated seams and padding.
The uploaded GLB and all original geometry/UVs remain unchanged.
Run Blender5.2 --background --python src/hub_hotels/project_roosevelt.py.
"""
import bpy,bmesh,json,sys,hashlib,math
import numpy as np
from pathlib import Path
R=Path(__file__).resolve().parents[2];SRC=R/'src/hub_hotels';AID='LM_Roosevelt';STAGE=R/'work/LM_Roosevelt_candidate';TEX=STAGE/'textures';TEX.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(R/'src'));from asset_io import export_asset
meta=json.loads((SRC/'LM_Roosevelt.json').read_text());layout=json.loads((SRC/'uv_layout.json').read_text());input_path=R/'work/LM_Roosevelt_input.glb';input_sha=hashlib.sha256(input_path.read_bytes()).hexdigest()
def signature(o):
 uv=o.data.uv_layers.active;records=[]
 for li,l in enumerate(o.data.loops):records.append(tuple(round(x,6) for x in (*(o.matrix_world@o.data.vertices[l.vertex_index].co),*uv.data[li].uv)))
 return {'vertices':len(o.data.vertices),'faces':len(o.data.polygons),'position_uv_hash':hashlib.sha256(repr(sorted(records)).encode()).hexdigest()}
def strict(o):
 bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.triangulate(bm,faces=list(bm.faces))
 a={'tris':len(bm.faces),'branch_edges':sum(len(e.link_faces)>2 for e in bm.edges),'zero_area_faces':sum(f.calc_area()<1e-10 for f in bm.faces),'loose_vertices':sum(not v.link_faces for v in bm.verts),'orientation_discontinuities':sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges),'boundary_edges':sum(e.is_boundary for e in bm.edges)};bm.free()
 assert a['branch_edges']==a['zero_area_faces']==a['loose_vertices']==a['orientation_discontinuities']==0,a
 return a
bpy.ops.wm.read_factory_settings(use_empty=True);sc=bpy.context.scene;sc.unit_settings.system='METRIC';sc.unit_settings.scale_length=1;sc.render.engine='CYCLES';sc.cycles.samples=1
bpy.ops.import_scene.gltf(filepath=str(input_path));target=next(o for o in sc.objects if o.type=='MESH');target.name=AID
world=target.matrix_world.copy();target.parent=None;target.matrix_world=world;bpy.context.view_layer.objects.active=target;target.select_set(True);bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
before=signature(target);precheck=strict(target);target.hide_render=True;target.hide_set(True)
atlas=bpy.data.images.new('Roosevelt_AuthoredUV_Projection',width=2048,height=2048,alpha=False);atlas.colorspace_settings.name='sRGB'
pix=np.ones((2048,2048,4),dtype=np.float32);pix[:,:,:3]=(.50,.37,.22);atlas.pixels.foreach_set(pix.ravel());atlas.update()
images={key:bpy.data.images.load(str(R/path),check_existing=True) for key,path in {
 'front':'concept/LM_Roosevelt_front.png','threequarter':'concept/LM_Roosevelt_34.png',
 'side':'work/hub_hotels/projection/roosevelt_side.png','back':'work/hub_hotels/projection/roosevelt_back.png','roof':'work/hub_hotels/projection/roosevelt_roof.png'}.items()}
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
# Paint broad surfaces from clean generated plaster, roof and timber samples.
for role in ['upper','door','wall','trim','canvas','gable']:
 allrole(role,'side',[81,185,111,272])
allrole('timber','front',[153,602,190,686]);allrole('metal','threequarter',[580,76,691,88]);allrole('sign','threequarter',[580,76,691,88]);allrole('ground','threequarter',[130,884,262,907])
# Roof source is cropped inside its generated edge frame, then made periodic.
allrole('roof','roof',[82,82,942,942]);allrole('tile','front',[335,77,610,113])
# Replace upper window region by a genuine camera crop. The wall skin omits it.
# Projection planes are nonoverlapping: re-split the upper wall around this ROI.
for o in list(planes):
 if o.name.startswith('CameraProjection_upper'):
  planes.remove(o);bpy.data.objects.remove(o,do_unlink=True)
for box in [[0,0,1.90,7],[4.10,0,7,7],[1.90,0,4.10,.43],[1.90,2.28,4.10,7]]:project('upper',box,'side',[81,185,111,272])
project('upper',[1.90,.43,4.10,2.28],'front',[360,215,410,253])
# Ordinary ground arcade and special warm entrance get separate regions.
allrole('arc','front',[78,520,276,698]);allrole('entry','front',[994,515,1199,698])
# Each top triple-arch opening uses its own repeated narrow feature region.
allrole('archupper','back',[95,105,155,170])
# Shrink crop projection so the glass lands inside the authored arch silhouette.
# The final ROI is inset from all source neutral background pixels.
for o in list(planes):
 if o.name.startswith('CameraProjection_archupper'):
  planes.remove(o);bpy.data.objects.remove(o,do_unlink=True)
for box in [[0,0,.27,3.1],[1.79,0,3.1,3.1],[.27,2.72,1.79,3.1]]:project('archupper',box,'side',[81,185,111,272])
project('archupper',[.27,0,1.79,2.72],'back',[115,119,143,156])
# Emit-only bake of the representative camera projections to our own atlas UVs.
bpy.ops.object.select_all(action='DESELECT')
for o in planes:o.select_set(True)
bpy.context.view_layer.objects.active=planes[0];bpy.ops.object.join();proxy=bpy.context.object;proxy.name='Roosevelt_ProjectionRepresentatives'
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
roof_pixels=region('roof');roof_mean=roof_pixels[:,:,:3].mean(axis=(0,1));roof_pixels[:,:,:3]=roof_mean[None,None,:]+(roof_pixels[:,:,:3]-roof_mean[None,None,:])*.55
# Roof quads reset at their authored5.692307x5.8m sizes; match these actual ends.
r,s=layout['roof'];x0=int(round(r[0]*2048));y0=int(round(r[1]*2048));x1=int(round((r[0]+(74/13)/s[0]*(r[2]-r[0]))*2048));y1=int(round((r[1]+5.8/s[1]*(r[3]-r[1]))*2048));periodic(data[y0:y1,x0:x1],20)
for role in ['metal','sign']:
 a=region(role);hh,ww=a.shape[:2];yy,xx=np.mgrid[0:hh,0:ww];grain=(np.sin(xx*.17+np.sin(yy*.11))*np.sin(yy*.07))*.003;a[:,:,:3]=np.array([.055,.105,.09])[None,None,:]+grain[:,:,None];a[:,:,3]=1
# Fill8px of external region padding by edge extension, preserving all gaps.
for role,(r,s) in layout.items():
 x0,y0,x1,y1=[int(round(v*2048)) for v in r];pad=8
 data[y0:y1,x0-pad:x0]=data[y0:y1,x0:x0+1];data[y0:y1,x1:x1+pad]=data[y0:y1,x1-1:x1]
 data[y0-pad:y0,x0-pad:x1+pad]=data[y0:y0+1,x0-pad:x1+pad];data[y1:y1+pad,x0-pad:x1+pad]=data[y1-1:y1,x0-pad:x1+pad]
data[:,:,3]=1;atlas.pixels.foreach_set(data.ravel());atlas.update();atlas.filepath_raw=str(TEX/'T_LM_Roosevelt.png');atlas.file_format='PNG';atlas.save();atlas.pack()
# Retain only the authored mesh in the final Blender scene; all rebuild logic is above.
bpy.data.objects.remove(proxy,do_unlink=True);target.hide_set(False);target.hide_render=False
material=bpy.data.materials.new('M_LM_Roosevelt_Final');material.use_nodes=True;nt=material.node_tree;tex=nt.nodes.new('ShaderNodeTexImage');tex.image=atlas;uv=nt.nodes.new('ShaderNodeUVMap');uv.uv_map=target.data.uv_layers.active.name;nt.links.new(uv.outputs['UV'],tex.inputs['Vector']);nt.links.new(tex.outputs['Color'],nt.nodes['Principled BSDF'].inputs['Base Color']);nt.nodes['Principled BSDF'].inputs['Roughness'].default_value=1.0;nt.nodes['Principled BSDF'].inputs['Specular IOR Level'].default_value=0.0
target.data.materials.clear();target.data.materials.append(material)
for p in target.data.polygons:p.material_index=0
for m in list(bpy.data.materials):
 if m!=material and m.users==0:bpy.data.materials.remove(m)
material.name='M_LM_Roosevelt';assert signature(target)==before;after_bake=signature(target)
bpy.ops.object.select_all(action='DESELECT');target.select_set(True);bpy.context.view_layer.objects.active=target
entry={'id':AID,'kind':'landmark','family':None,'material':material.name,'texture':'textures/T_LM_Roosevelt.png','emissive':None,'nominal':[74,58,24],'origin':'footprint-centre-ground','fits_box':'Roosevelt Hotel','also_fits':[],'recess':[],'quest_object':None,'concept':meta['concept']+['work/hub_hotels/projection/roosevelt_side.png','work/hub_hotels/projection/roosevelt_back.png','work/hub_hotels/projection/roosevelt_roof.png'],'prompts':meta['prompts'],'rodin':{'used':False,'method':'projection'},'source':meta['source'],'textured_source':'src/LM_Roosevelt_kit_textured.blend','sign_attachments':meta['metadata']['sign_attachments'],'notes':meta['metadata']['notes']+' SPEC15 fallback after two server-side failed Rodin jobs. Generated front/side/back/roof feature camera projections emission-baked into the authored repeatedUV atlas; repeated seams painted; blank roof sign takes a separate letter card. Side/back reference generation via user-authorized fal.ai FLUX3Klein.'}
entry,audit=export_asset(target,entry,20000,STAGE);postcheck=strict(target)
old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(STAGE/entry['file']));exported=next(o for o in set(bpy.data.objects)-old if o.type=='MESH');fbxcheck=strict(exported)
for o in set(bpy.data.objects)-old:bpy.data.objects.remove(o,do_unlink=True)
assert hashlib.sha256(input_path.read_bytes()).hexdigest()==input_sha,'Frozen input changed'
checks={'method':'camera_projection_emit_bake','rodin_failures_before_fallback':2,'frozen_input_sha256':input_sha,'authored_before':before,'authored_after_bake':after_bake,'authored_after_export_weld':signature(target),'authored_geometry_uv_unchanged_during_bake':True,'position_uv_hash_preserved_after_export':signature(target)['position_uv_hash']==before['position_uv_hash'],'strict_before':precheck,'strict_after':postcheck,'fbx_roundtrip_welded':fbxcheck,'projection_sources':{k:im.filepath for k,im in images.items()},'projection_planes':projection_records,'paint_fixes':['Periodic roof seams matched actual5.692307x5.8m tile extents','Warm stone trim/wall padding8px','Blank metal sign backings painted without text or background','Terracotta tile and paving seams matched'],'texture':{'width':2048,'height':2048,'channels':'RGB','bit_depth':8,'color_space':'sRGB'}}
(R/'work/LM_Roosevelt_kit_assets.json').write_text(json.dumps([entry],indent=2)+'\n');(R/'src/LM_Roosevelt_export_audit.json').write_text(json.dumps(audit,indent=2)+'\n');(R/'src/LM_Roosevelt_source_checks.json').write_text(json.dumps(checks,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/LM_Roosevelt_kit_textured.blend'))
print('ROOSEVELT_PROJECTION_STAGED',entry['tris'],json.dumps(audit),flush=True)
