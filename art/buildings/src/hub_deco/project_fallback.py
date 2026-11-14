"""SPEC15 projection delivery using generated orthographic architecture plates.

Blender --background --python .../project_fallback.py -- LM_Pantages
Original GLB position/UV data is frozen. Each plate is projected through an
orthographic camera onto its canonical authored architectural UV-role plane;
the projected source copy is emitted/baked onto the original UVMap in Cycles.
Repeated identical bays share the same elevation on front/side/back. No API here.
"""
import bpy,bmesh,json,hashlib,sys,runpy
import numpy as np
from pathlib import Path
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'src'))
from asset_io import export_asset
aid=sys.argv[sys.argv.index('--')+1];assert aid in ['LM_Pantages','LM_Taft','LM_FirstNational','LM_BrownDerby','LM_PoliceStation']
meta=json.loads((R/f'src/hub_deco/{aid}_metadata.json').read_text())
stage=R/f'work/{aid}_candidate';(stage/'textures').mkdir(parents=True,exist_ok=True)
P=R/'work/hub_deco/projection'
input_path=R/meta['input_glb'];input_hash=hashlib.sha256(input_path.read_bytes()).hexdigest()
assert input_hash==meta['input_sha256'],'Frozen input changed'
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
bpy.ops.import_scene.gltf(filepath=str(input_path));target=next(o for o in scene.objects if o.type=='MESH');target.name=aid
bpy.ops.object.select_all(action='DESELECT');target.select_set(True);bpy.context.view_layer.objects.active=target
world=target.matrix_world.copy();target.parent=None;target.matrix_world=world
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
for polygon in target.data.polygons:polygon.use_smooth=False
def signature(o):
 uv=o.data.uv_layers['UVMap'];rows=[]
 for l in o.data.loops:
  v=o.matrix_world@o.data.vertices[l.vertex_index].co
  rows.append(tuple(round(x,5) for x in (*v,*uv.data[l.index].uv)))
 return {'vertices':len(o.data.vertices),'faces':len(o.data.polygons),'position_uv_hash':hashlib.sha256(repr(sorted(rows)).encode()).hexdigest()}
frozen_before=signature(target);authored_correction=None
if aid=='LM_PoliceStation':
 # These six small projected window surrounds were authored at a different
 # spacing from the stacked wall bays. Their painted equivalents now provide
 # the same detail, without overlapping unrelated windows. Keep entrance trim,
 # cornice, dentils, lamps and silhouette geometry intact.
 bm=bmesh.new();bm.from_mesh(target.data)
 redundant=[]
 for f in bm.faces:
  xs=[v.co.x for v in f.verts];ys=[v.co.y for v in f.verts];zs=[v.co.z for v in f.verts]
  outside_entry=min(xs)>5 or max(xs)<-5
  lower=outside_entry and min(ys)>=-17.1301 and max(ys)<=-17.0799 and min(zs)>=.4799 and max(zs)<=4.6
  sill=outside_entry and min(ys)>=-17.2001 and max(ys)<=-16.9999 and min(zs)>=5.6999 and max(zs)<=5.9001
  if lower or sill:redundant.append(f)
 removed=len(redundant);assert removed>0
 bmesh.ops.delete(bm,geom=redundant,context='FACES_ONLY')
 loose=[v for v in bm.verts if not v.link_faces]
 if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6)
 bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(target.data);bm.free()
 authored_correction={'reason':'Remove six window-trim/bar groups and upper sills whose spacing conflicted with repeated facade bays; generated texture supplies arched surrounds, security bars and sills. Entrance trim, silhouette and box dimensions retained.','removed_triangles':removed,'before':frozen_before,'after':signature(target)}
before=signature(target)
rects=({'wall':(.715,.015,.985,.495),'upper':(.715,.515,.985,.985),
 'roof':(.008,.008,.692,.692),'stone':(.012,.715,.245,.985),
 'metal':(.265,.715,.365,.985),'glass':(.385,.715,.485,.985),
 'sign':(.505,.715,.695,.985),'bulb':(.65,.95,.68,.98)} if aid in ['LM_Pantages','LM_BrownDerby','LM_PoliceStation'] else
 {'roof':(.008,.008,.62,.62),'wall':(.64,.008,.82,.12),'base':(.64,.15,.99,.45),
 'upper':(.64,.48,.99,.75),'stone':(.008,.65,.20,.99),'metal':(.22,.65,.32,.99),
 'glass':(.34,.65,.44,.99),'sign':(.46,.65,.62,.99),'bulb':(.84,.80,.98,.98)})
if aid=='LM_PoliceStation':rects['stone']=(.008,.715,.245,.985)
paths={'roof':P/'LM_Pantages_roof_v2.png'}
if aid=='LM_Pantages':
 paths.update(wall=P/f'{aid}_front_side_back_bay.png',upper=P/f'{aid}_front_side_back_bay.png',stone=P/f'{aid}_stone_relief.png')
elif aid=='LM_BrownDerby':
 paths.update(wall=P/f'{aid}_bay.png',upper=P/f'{aid}_tiles.png',stone=P/f'{aid}_stucco.png')
elif aid=='LM_PoliceStation':
 paths.update(wall=P/f'{aid}_bay.png',upper=P/f'{aid}_bay.png',stone=P/'LM_Taft_stone.png')
else:
 paths.update(wall=P/f'{aid}_front_side_back_floor.png',base=P/f'{aid}_front_side_back_base.png',upper=P/(f'{aid}_front_side_back_'+('crown.png' if aid=='LM_Taft' else 'belfry.png')),stone=P/'LM_Taft_stone.png')
paths['glass']=P/'LM_FirstNational_door.png'
if aid=='LM_BrownDerby':paths['glass']=P/f'{aid}_door.png'
def swatch(name):
 im=bpy.data.images.new(aid+'_'+name+'_paint',width=128,height=512,alpha=False)
 yy,xx=np.mgrid[0:512,0:128];v=yy/511;noise=np.sin(xx*.2+np.sin(yy*.12))*.008
 if name=='metal' and aid=='LM_BrownDerby':
  intensity=.22+.20*v+noise+.04*np.cos(xx/127*np.pi*10)**10
  rgb=np.stack([intensity*.52,intensity,intensity*.55],axis=2)
 elif name=='metal':
  intensity=.27+.34*v+.16*np.cos(xx/127*np.pi*4)**10
  rgb=np.stack([intensity*1.02,intensity,intensity*.87],axis=2)
 elif name=='bulb':rgb=np.zeros((512,128,3))+np.array([.95,.78,.43])
 else:
  intensity=.38+.23*v+noise;rgb=np.stack([intensity,intensity*.84,intensity*.58],axis=2)
 rgba=np.concatenate([np.clip(rgb,0,1),np.ones((512,128,1))],axis=2).astype(np.float32)
 im.pixels.foreach_set(rgba.ravel());im.update();return im
images={role:bpy.data.images.load(str(path),check_existing=True) for role,path in paths.items()}
for role in ['metal','sign','bulb']:images[role]=swatch(role)
source=target.copy();source.data=target.data.copy();scene.collection.objects.link(source);source.name='ProjectionSource_'+aid
source.data.materials.clear();proj=source.data.uv_layers.new(name='Projection')
source_uv=source.data.uv_layers['UVMap']
# Camera projection of a role's normalized elevation plane. Aspect is preserved
# in camera-space; different architectural roles use their own generated plate.
bpy.ops.object.camera_add(location=(.5,.5,1));camera=bpy.context.object;camera.name='OrthographicPlateProjector';camera.data.type='ORTHO';camera.data.ortho_scale=1;camera.rotation_euler=(0,0,0);scene.camera=camera
materials={};counts={}
for role,im in images.items():
 mat=bpy.data.materials.new('Projection_'+aid+'_'+role);mat.use_nodes=True;nt=mat.node_tree;nt.nodes.clear()
 tex=nt.nodes.new('ShaderNodeTexImage');tex.image=im;tex.extension='EXTEND'
 uvn=nt.nodes.new('ShaderNodeUVMap');uvn.uv_map='Projection';nt.links.new(uvn.outputs['UV'],tex.inputs['Vector'])
 if aid=='LM_PoliceStation' and role=='wall':
  # A piecewise vertical camera-plate fit is evaluated per sample, so it also
  # works on the large unsplit authored wall quads without changing their UVs.
  sep=nt.nodes.new('ShaderNodeSeparateXYZ');nt.links.new(uvn.outputs['UV'],sep.inputs[0])
  def calc(operation,upstream,constant):
   node=nt.nodes.new('ShaderNodeMath');node.operation=operation
   nt.links.new(upstream,node.inputs[0]);node.inputs[1].default_value=constant
   return node.outputs[0]
  low=calc('MULTIPLY',calc('MINIMUM',sep.outputs['Y'],.45),1.10)
  high=calc('MULTIPLY',calc('MAXIMUM',calc('SUBTRACT',sep.outputs['Y'],.45),0),.365/.55)
  add=nt.nodes.new('ShaderNodeMath');add.operation='ADD';nt.links.new(low,add.inputs[0]);nt.links.new(high,add.inputs[1])
  mapped=calc('ADD',add.outputs[0],.02)
  combine=nt.nodes.new('ShaderNodeCombineXYZ');nt.links.new(sep.outputs['X'],combine.inputs['X']);nt.links.new(mapped,combine.inputs['Y']);nt.links.new(combine.outputs[0],tex.inputs['Vector'])
 em=nt.nodes.new('ShaderNodeEmission');nt.links.new(tex.outputs['Color'],em.inputs['Color']);out=nt.nodes.new('ShaderNodeOutputMaterial');nt.links.new(em.outputs[0],out.inputs['Surface'])
 source.data.materials.append(mat);materials[role]=len(source.data.materials)-1
def contains(rect,uv):return rect[0]-.00001<=uv.x<=rect[2]+.00001 and rect[1]-.00001<=uv.y<=rect[3]+.00001
for face in source.data.polygons:
 coords=[source_uv.data[i].uv for i in face.loop_indices]
 # Bulb triangles occupy their own tiny patch inside the sign backing field.
 roles=[r for r in ['bulb']+list(rects) if all(contains(rects[r],p) for p in coords)]
 assert roles,(aid,face.index,[list(p) for p in coords])
 role=roles[0];counts[role]=counts.get(role,0)+1;face.material_index=materials[role]
 u0,v0,u1,v1=rects[role];im=images[role]
 scene.render.resolution_x=im.size[0];scene.render.resolution_y=im.size[1];aspect=im.size[0]/im.size[1]
 camera.data.ortho_scale=max(1,aspect)
 for i in face.loop_indices:
  uv=source_uv.data[i].uv;u=(uv.x-u0)/(u1-u0);v=(uv.y-v0)/(v1-v0)
  projected=world_to_camera_view(scene,camera,Vector(((u-.5)*aspect+.5,v,0)))
  if aid=='LM_Taft' and role=='wall':
   # Crop excess brick margin in the generated bay so the painted window
   # matches the authored two-metre sill and reads at MMO camera distance.
   projected.x=.20+.60*projected.x
  if aid=='LM_PoliceStation' and role=='wall':
   # Generated plate included a narrow neutral surround beyond the masonry.
   # Frame only its uninterrupted architectural field and omit the parapet.
   projected.x=.13+.74*projected.x
   # Fit the lower arch below the authored 4.5m floor belt; reserve the
   # remaining elevation for the upper window and existing top cornice.
  proj.data[i].uv=(projected.x,projected.y)
source.data.uv_layers.active=proj
target.data.materials.clear()
# glTF retains the original authored material datablock after clearing its slot.
# Reuse its exact name rather than silently exporting a .001 material suffix.
previous=bpy.data.materials.get('M_'+aid)
if previous is not None:
 assert previous.users==0,(aid,'Original material still referenced')
 bpy.data.materials.remove(previous)
mat=bpy.data.materials.new('M_'+aid);mat.use_nodes=True;target.data.materials.append(mat)
assert mat.name=='M_'+aid
for f in target.data.polygons:f.material_index=0
image=bpy.data.images.new('T_'+aid,width=2048,height=2048,alpha=False);image.colorspace_settings.name='sRGB'
nt=mat.node_tree;tex=nt.nodes.new('ShaderNodeTexImage');tex.image=image;nt.nodes.active=tex
shader=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED');nt.links.new(tex.outputs['Color'],shader.inputs['Base Color']);shader.inputs['Roughness'].default_value=.88
bpy.ops.object.select_all(action='DESELECT');source.select_set(True);target.select_set(True);bpy.context.view_layer.objects.active=target
scene.render.engine='CYCLES';scene.cycles.samples=1;scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.04;scene.render.bake.max_ray_distance=.15;scene.render.bake.margin=8
bpy.ops.object.bake(type='EMIT');assert signature(target)==before,'Authored geometry/UVs changed during projection bake'
bpy.data.objects.remove(source,do_unlink=True);bpy.data.objects.remove(camera,do_unlink=True)
cleanup=None
if aid=='LM_Pantages':
 runpy.run_path(str(R/'src/hub_deco/LM_Pantages_finish.py'))['apply'](target);cleanup='Post-bake crown exterior/back cleanup; existing stone UV field retained.'
for polygon in target.data.polygons:polygon.use_smooth=False
scene.render.image_settings.color_mode='RGB';scene.render.image_settings.color_depth='8'
image.filepath_raw=str(stage/f'textures/T_{aid}.png');image.file_format='PNG';image.save();image.pack()
entry={k:v for k,v in meta.items() if k not in ['authored_audit','audit','uv','input_glb','object','input_sha256','main_box_bounds']}
labels={'LM_Pantages':'Pantages Theatre','LM_Taft':'Taft Building','LM_FirstNational':'First National Bank','LM_BrownDerby':'Brown Derby','LM_PoliceStation':'Hollywood Police Station'}
entry.update(kind='landmark',family=None,material='M_'+aid,texture=f'textures/T_{aid}.png',emissive=None,origin='footprint-centre-ground',fits_box=labels[aid],also_fits=[],recess=[],quest_object=None,rodin={'used':False,'method':'projection','reason':'Two Rodin server failures; SPEC15 fallback','baked_to_own_uvs':True},source=f'src/{aid}_kit_textured.blend')
entry['authored_source']=meta['source'];entry['prompts']={'concept':json.loads((R/meta['prompts']).read_text()),'projection_plates':json.loads((R/'src/hub_deco/projection_view_queue.json').read_text()),'roof_revision':json.loads((R/'src/hub_deco/projection_roof_retry.json').read_text())}
if aid in ['LM_BrownDerby','LM_PoliceStation']:
 entry['prompts']['projection_civic_plates']=json.loads((R/'src/hub_deco/projection_civic_queue.json').read_text())
entry['notes']+=' SPEC15 fallback: FAL FLUX3Klein generated orthographic role elevations projected/baked into unchanged authored UVMap. Repeated canonical facade bays serve all orientations; top-down tar/gravel roof. Warm trim/chrome/sign-backing swatches painted in source.'
if authored_correction:entry['notes']+=' Deliberate own-mesh correction: redundant lower-window trim/bars and upper sills removed to avoid conflicting spacing with painted wall bays; texture supplies those details; main mass, entrance, cornice and silhouette retained.'
budget=20000 if aid in ['LM_Pantages','LM_Taft'] else 10000
entry,audit=export_asset(target,entry,budget,output_root=stage)
old_objects=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(stage/f'meshes/{aid}.fbx'))
roundtrip=next(o for o in set(bpy.data.objects)-old_objects if o.type=='MESH')
bm=bmesh.new();bm.from_mesh(roundtrip.data)
audit['inconsistent_shared_edges']=sum(len(e.link_faces)==2 and not e.is_contiguous for e in bm.edges)
bm.free();bpy.data.objects.remove(roundtrip,do_unlink=True)
assert audit['inconsistent_shared_edges']==0,(aid,audit)
(R/f'work/{aid}_kit_assets.json').write_text(json.dumps([entry],indent=2)+'\n')
(R/f'src/{aid}_export_audit.json').write_text(json.dumps(audit,indent=2)+'\n')
checks={'method':'orthographic role-plane camera projection and Cycles selected-to-active EMIT bake','frozen_input_sha256':input_hash,'uploaded_authored':before,'authored_geometry_uv_unchanged_during_bake':True,'after_bake_before_cleanup':before,'after_cleanup':signature(target),'projection_face_counts':counts,'projection_images':{r:str(p.relative_to(R)) for r,p in paths.items()},'cleanup':cleanup,'material_slots':len(target.data.materials),'material_name':target.data.materials[0].name,'two_failed_Rodin_attempts':True}
checks['frozen_input_geometry']=frozen_before;checks['deliberate_authored_geometry_correction']=authored_correction
if aid=='LM_PoliceStation':
 checks['normal_orientation_repair']='Weld split GLB vertices before recalculating face normals; avoids independently orienting disconnected triangles that later share welded edges.'
 checks['actual_fbx_inconsistent_shared_edges']=audit['inconsistent_shared_edges']
(R/f'src/{aid}_source_checks.json').write_text(json.dumps(checks,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/f'src/{aid}_kit_textured.blend'))
print('PROJECTION_STAGED',aid,audit)
