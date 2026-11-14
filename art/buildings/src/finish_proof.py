"""Bake downloaded base color onto authored meshes, export, round-trip measure, render.
Run with Blender --background --python art/buildings/src/finish_proof.py.
"""
import bpy,bmesh,json,math,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[1]
IDS=['BRK_G_Bay6A','BRK_U_Bay3A','LM_Drugstore']
if '--' in sys.argv: IDS=sys.argv[sys.argv.index('--')+1:]
report=[];checks={}
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
scene.render.engine='CYCLES';scene.cycles.samples=1
atlas=bpy.data.images.new('T_ATL_BRK',width=2048,height=2048,alpha=False)
atlas.generated_color=(.3,.2,.12,1)
def imported(path):
 old=set(bpy.data.objects);bpy.ops.import_scene.gltf(filepath=str(path))
 return [o for o in set(bpy.data.objects)-old if o.type=='MESH']
def mat_image(name,im):
 for existing in bpy.data.materials:
  if existing.name==name and existing.use_nodes and any(n.type=='TEX_IMAGE' and n.image==im for n in existing.node_tree.nodes):return existing
 m=bpy.data.materials.new(name);m.use_nodes=True;n=m.node_tree.nodes
 p=next(n for n in n if n.type=='BSDF_PRINCIPLED');p.inputs['Roughness'].default_value=.85
 t=n.new('ShaderNodeTexImage');t.image=im;m.node_tree.links.new(t.outputs['Color'],p.inputs['Base Color']);n.active=t
 return m
for aid in IDS:
 target=imported(R/'work'/f'{aid}_input.glb')[0];target.name=aid
 origverts=[tuple(v.co) for v in target.data.vertices];origuv=[tuple(u.uv) for u in target.data.uv_layers.active.data]
 fallback=aid=='BRK_U_Bay3A' and not (R/'work'/aid/'base_basic_shaded.glb').exists()
 if fallback:
  src=target.copy();src.data=target.data.copy();bpy.context.collection.objects.link(src)
  reference=bpy.data.images.load(str(R/'concept'/f'{aid}_front.png'))
  src.data.materials.clear();src.data.materials.append(mat_image('ProjectionSource',reference))
  for p in src.data.polygons:
   for li in p.loop_indices:
    v=src.data.vertices[src.data.loops[li].vertex_index].co
    src.data.uv_layers.active.data[li].uv=(.024+v.x/3*.95,.012+v.z/3.5*.976)
 else:
  source=R/'src/rodin'/aid/'base_basic_shaded.glb'
  src=imported(source if source.exists() else R/'work'/aid/'base_basic_shaded.glb')[0]
 checks[aid]={'upload_vertices':len(origverts),'returned_vertices':len(src.data.vertices),'upload_faces':len(target.data.polygons),'returned_faces':len(src.data.polygons),'uv_equal':len(origuv)==len(src.data.uv_layers.active.data) and all((Vector(a)-b.uv).length<1e-6 for a,b in zip(origuv,src.data.uv_layers.active.data)),'baked_to_own_uvs':True,'source':'concept_projection' if fallback else 'rodin_shaded'}
 if not fallback and len(src.data.vertices)==len(target.data.vertices):
  checks[aid]['max_vertex_delta']=max((src.matrix_world@s.co-target.matrix_world@t.co).length for s,t in zip(src.data.vertices,target.data.vertices))
 # Atlas Option B: repack original bay UVs, preserving authored topology.
 if aid.startswith('BRK'):
  im=atlas
  for p in target.data.polygons:
   for li in p.loop_indices:
    v=target.data.vertices[target.data.loops[li].vertex_index].co
    if aid==IDS[0]: u,w=(16+v.x*200)/2048,(16+v.z*200)/2048
    else: u,w=(1248+v.x*200)/2048,(16+v.z*200)/2048
    target.data.uv_layers.active.data[li].uv=(u,w)
  m=mat_image('M_ATL_BRK',im)
 else:
  im=bpy.data.images.new('T_'+aid,width=2048,height=2048,alpha=False)
  # Street face 70 px/m, side/back faces 50 px/m, roof 40 px/m.
  # Stack mirrored side walls and matching awnings to retain density in one 2K set.
  for poly in target.data.polygons:
   normal=target.matrix_world.to_3x3()@poly.normal
   coords=[target.matrix_world@target.data.vertices[target.data.loops[li].vertex_index].co for li in poly.loop_indices]
   awning=any(v.y < -18.1 for v in coords)
   for li,v in zip(poly.loop_indices,coords):
    if awning:
     localx=v.x+7.5 if v.x<0 else v.x-1.25
     u,pv=1320+localx*100,16+(v.z-2.7)*200
    elif normal.y < -.5:u,pv=16+(v.x+9)*70,16+v.z*70
    elif normal.y > .5:u,pv=16+(9-v.x)*50,1680+v.z*50
    elif abs(normal.x)>.5:u,pv=16+(v.y+18)*50,1300+v.z*50
    else:u,pv=16+(v.y+18)*40,500+(v.x+9)*40
    target.data.uv_layers.active.data[li].uv=(u/2048,pv/2048)
  m=mat_image('M_'+aid,im)
 target.data.materials.clear();target.data.materials.append(m)
 for unused in list(bpy.data.materials):
  if unused.users==0:bpy.data.materials.remove(unused)
 m.name='M_ATL_BRK' if aid.startswith('BRK') else 'M_'+aid
 # Bake color only. Convert source material base color to emission, independent of lights.
 for sm in src.data.materials:
  nt=sm.node_tree;p=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED');out=next(n for n in nt.nodes if n.type=='OUTPUT_MATERIAL')
  em=nt.nodes.new('ShaderNodeEmission')
  color=p.inputs['Base Color'] if p.inputs['Base Color'].links else p.inputs['Emission Color']
  if color.links: nt.links.new(color.links[0].from_socket,em.inputs['Color'])
  else: em.inputs['Color'].default_value=p.inputs['Base Color'].default_value
  nt.links.new(em.outputs[0],out.inputs['Surface'])
 bpy.ops.object.select_all(action='DESELECT');src.select_set(True);target.select_set(True);bpy.context.view_layer.objects.active=target
 scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.02;scene.render.bake.max_ray_distance=.1;scene.render.bake.margin=8;scene.render.bake.use_clear=(aid!='BRK_U_Bay3A')
 bpy.ops.object.bake(type='EMIT')
 bpy.data.objects.remove(src,do_unlink=True)
 if aid=='LM_Drugstore':
  # Texture-paint blank sign backings over Rodin's garbled lettering on front/back.
  import numpy as np
  pix=np.empty(2048*2048*4,dtype=np.float32);im.pixels.foreach_get(pix);pix=pix.reshape(2048,2048,4)
  for poly in target.data.polygons:
   vs=[target.matrix_world@target.data.vertices[target.data.loops[li].vertex_index].co for li in poly.loop_indices]
   if not all(abs(abs(v.y)-18)<.01 for v in vs):continue
   uvs=np.array([target.data.uv_layers.active.data[li].uv[:] for li in poly.loop_indices])*2048
   lo=np.maximum(0,np.floor(uvs.min(axis=0)).astype(int));hi=np.minimum(2047,np.ceil(uvs.max(axis=0)).astype(int))
   xx,yy=np.meshgrid(np.arange(lo[0],hi[0]+1),np.arange(lo[1],hi[1]+1));points=np.stack((xx+.5,yy+.5),axis=-1)
   matrix=np.column_stack((uvs[1]-uvs[0],uvs[2]-uvs[0]))
   if abs(np.linalg.det(matrix))<1e-8:continue
   bc=(points-uvs[0])@np.linalg.inv(matrix).T;a=bc[...,0];b=bc[...,1]
   wx=vs[0].x+a*(vs[1].x-vs[0].x)+b*(vs[2].x-vs[0].x)
   wz=vs[0].z+a*(vs[1].z-vs[0].z)+b*(vs[2].z-vs[0].z)
   mask=(a>=0)&(b>=0)&(a+b<=1)&(abs(wx)<8)&(wz>3.7)&(wz<5.4)
   section=pix[lo[1]:hi[1]+1,lo[0]:hi[0]+1];section[mask,:3]=(.68,.52,.29)
  im.pixels.foreach_set(pix.ravel());im.update()
 texture='textures/T_ATL_BRK.png' if aid.startswith('BRK') else 'textures/T_'+aid+'.png'
 im.filepath_raw=str(R/texture);im.file_format='PNG';im.save()
 bpy.ops.object.select_all(action='DESELECT');target.select_set(True);bpy.context.view_layer.objects.active=target
 bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
 # Weld export-split seams; retain UV corner data and hard flat architectural normals.
 bm=bmesh.new();bm.from_mesh(target.data);bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=.00001);bm.to_mesh(target.data);bm.free()
 target.data.materials[0].name='M_ATL_BRK' if aid.startswith('BRK') else 'M_'+aid
 bpy.ops.export_scene.fbx(filepath=str(R/'meshes'/f'{aid}.fbx'),use_selection=True,object_types={'MESH'},axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',bake_anim=False,use_mesh_modifiers=True,use_triangles=True,path_mode='RELATIVE',add_leaf_bones=False)
 bpy.ops.export_scene.gltf(filepath=str(R/'meshes'/f'{aid}.glb'),use_selection=True,export_format='GLB')
 target.hide_render=True;target.hide_set(True)
 # Measure the actual FBX, not source intentions.
 old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(R/'meshes'/f'{aid}.fbx'))
 check=next(o for o in set(bpy.data.objects)-old if o.type=='MESH')
 coords=[check.matrix_world@v.co for v in check.data.vertices]
 bounds=[[round(fn(v[i] for v in coords),6) for i in range(3)] for fn in [min,max]]
 tris=sum(len(p.vertices)-2 for p in check.data.polygons)
 bm=bmesh.new();bm.from_mesh(check.data)
 checks[aid].update(export_material_slots=len(check.material_slots),export_material=check.material_slots[0].material.name.split('.')[0],export_triangulated=all(len(p.vertices)==3 for p in check.data.polygons),export_scale=list(check.scale),export_rotation=list(check.rotation_euler),bounds=bounds,tris=tris,zero_area_faces=sum(f.calc_area()<1e-10 for f in bm.faces),loose_vertices=sum(not v.link_faces for v in bm.verts),nonmanifold_branch_edges=sum(len(e.link_faces)>2 for e in bm.edges),intentional_boundary_edges=sum(e.is_boundary for e in bm.edges))
 bm.free()
 bpy.data.objects.remove(check,do_unlink=True)
 report.append({'id':aid,'kind':'module' if aid.startswith('BRK') else 'landmark',**({'family':'BRK'} if aid.startswith('BRK') else {}),'file':f'meshes/{aid}.fbx','material':'M_ATL_BRK' if aid.startswith('BRK') else 'M_'+aid,'texture':'atlas:ATL_BRK' if aid.startswith('BRK') else texture,'emissive':None,'nominal':{'BRK_G_Bay6A':[6,4.5],'BRK_U_Bay3A':[3,3.5],'LM_Drugstore':[18,36,6]}[aid],'bounds':bounds,'tris':tris,'origin':'front-plane-bottom-left' if aid.startswith('BRK') else 'footprint-centre-ground','fits_box':'Drugstore' if aid=='LM_Drugstore' else None,'also_fits':[],'recess':[],'quest_object':None,'concept':[f'concept/{aid}_front.png',f'concept/{aid}_34.png'],'rodin':{'used':True,'material':'Shaded','baked_to_own_uvs':True},'notes':'Phase 1 proof; family atlas Option B.'})
 if fallback:
  report[-1]['rodin']={'used':False,'method':'projection'}
  report[-1]['notes']='Two failed Rodin attempts. Orthographic front concept projection baked through Cycles; no side/back faces on this wall-facing module.'
# Re-export previews after the shared atlas has received every bay.
for aid in IDS[:2]:
 o=bpy.data.objects[aid];o.hide_set(False)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 bpy.ops.export_scene.gltf(filepath=str(R/'meshes'/f'{aid}.glb'),use_selection=True,export_format='GLB')
 o.hide_set(True)
# Save scene with original author topology and final atlas.
scene.render.engine='CYCLES';scene.cycles.samples=16
scene.world=bpy.data.worlds.new('Neutral');scene.world.color=(.45,.45,.45)
scene.render.resolution_x=600;scene.render.resolution_y=600;scene.render.resolution_percentage=100
scene.view_settings.view_transform='Standard'
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam;cam.data.type='ORTHO'
for loc,energy,size in [((0,-30,40),25000,25),((25,10,25),18000,20)]:
 bpy.ops.object.light_add(type='AREA',location=loc);light=bpy.context.object;light.data.energy=energy;light.data.shape='DISK';light.data.size=size;light.rotation_euler=(-light.location).to_track_quat('-Z','Y').to_euler()
for aid in IDS:
 o=bpy.data.objects[aid];o.hide_set(False);o.hide_render=False
 signs=[]
 if aid=='LM_Drugstore':
  for sid,loc in [('SIGN_Drugstore',(-6,-18.025,4.3)),('SIGN_SodaFountain',(-6,-18.025,3.6))]:
   old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(R/'meshes'/f'{sid}.fbx'));s=next(q for q in set(bpy.data.objects)-old if q.type=='MESH');s.location=loc;signs.append(s)
 coords=[o.matrix_world@v.co for v in o.data.vertices];lo=Vector([min(v[i] for v in coords) for i in range(3)]);hi=Vector([max(v[i] for v in coords) for i in range(3)]);center=(lo+hi)/2;span=max(hi-lo)
 for name,direction in [('front',(1,-2,1)),('back',(-1,2,1)),('top',(1,-1,1.414))]:
  cam.location=center+Vector(direction).normalized()*span*2;cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=span*1.4
  scene.render.filepath=str(R/'preview'/f'{aid}_{name}.png');bpy.ops.render.render(write_still=True)
 if aid=='LM_Drugstore':
  # Collision envelope and human silhouette sit beside actual textured landmark.
  bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,3));box=bpy.context.object;box.dimensions=(18,36,6)
  bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
  mod=box.modifiers.new('Collision box wireframe','WIREFRAME');mod.thickness=.06
  silhouette=[]
  for loc,scale in [((-11,-18,.95),(.45,.2,.85)),((-11.13,-18,.35),(.17,.18,.7)),((-10.87,-18,.35),(.17,.18,.7))]:
   bpy.ops.mesh.primitive_cube_add(size=1,location=loc);p=bpy.context.object;p.scale=scale;silhouette.append(p)
  bpy.ops.mesh.primitive_uv_sphere_add(segments=8,ring_count=4,radius=.2,location=(-11,-18,1.6));silhouette.append(bpy.context.object)
  cam.location=(45,-65,38);cam.rotation_euler=(Vector((0,0,3))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=50
  scene.render.filepath=str(R/'preview'/'LM_Drugstore_box_human.png');bpy.ops.render.render(write_still=True)
  for p in silhouette+[box]:bpy.data.objects.remove(p,do_unlink=True)
 o.hide_render=True;o.hide_set(True)
 for s in signs:bpy.data.objects.remove(s,do_unlink=True)
bpy.ops.file.pack_all()
bpy.ops.wm.save_as_mainfile(filepath=str(R/'src'/'phase1_textured.blend'))
(R/'work'/'phase1_assets.json').write_text(json.dumps(report,indent=2))
(R/'work'/'phase1_checks.json').write_text(json.dumps(checks,indent=2))
