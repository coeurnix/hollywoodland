"""120 m assembly from published manifest FBXs only; no source meshes used."""
import bpy,json,math,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[1]
m=json.loads((R/'manifest.json').read_text());assets={a['id']:a for a in m['assets']}
assert all(f in m['recipes'] for f in ['BRK','SPN','DECO','MSN'])
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
cache={};placed=[];checks=[]
def template(aid):
 if aid not in cache:
  old=set(bpy.data.objects);bpy.ops.import_scene.fbx(filepath=str(R/assets[aid]['file']))
  obj=next(o for o in set(bpy.data.objects)-old if o.type=='MESH')
  obj.hide_render=True;obj.hide_set(True);cache[aid]=obj
  # The manifest is authoritative for texture paths, independent of FBX path resolution.
  texture=assets[aid]['texture'];path=m['atlases'][texture.split(':',1)[1]]['texture'] if texture.startswith('atlas:') else texture
  mat=obj.data.materials[0];mat.use_nodes=True;nt=mat.node_tree
  bs=next(n for n in nt.nodes if n.type=='BSDF_PRINCIPLED')
  tex=nt.nodes.new('ShaderNodeTexImage');tex.image=bpy.data.images.load(str(R/path),check_existing=True)
  nt.links.new(tex.outputs['Color'],bs.inputs['Base Color']);bs.inputs['Roughness'].default_value=.85
 return cache[aid]
def place(aid,x,y,z,scale=(1,1,1),angle=0,tile=False):
 base=template(aid);o=base.copy();o.data=base.data.copy() if tile else base.data
 bpy.context.collection.objects.link(o);o.hide_set(False);o.hide_render=False;o.location=(x,y,z);o.scale=scale;o.rotation_euler.z=angle
 if tile:
  for uv in o.data.uv_layers.active.data:uv.uv.x*=abs(scale[0]);uv.uv.y*=abs(scale[2] if assets[aid]['kind']=='wall' else scale[1])
 placed.append({'id':aid,'location':[x,y,z],'scale':list(scale),'rotation_z':angle})
 return o
def width_parts(width):
 result=[]
 for w in [8,6,4,1]:
  while width>=w:result.append(w);width-=w
 assert abs(width)<1e-6
 return result
cursor=0
buildings=[('BRK',16,6),('SPN',14,9),('DECO',20,12),('MSN',18,15),('SPN',12,6),('DECO',10,9),('MSN',16,12),('BRK',14,15)]
for index,(fam,w,h) in enumerate(buildings):
 recipe=m['recipes'][fam];n=max(0,round((h-4.5-1.5)/3.5));body=4.5+n*3.5;ph=h-body;corner=index==0
 parts=width_parts(w-(1 if corner else 0));start=.5 if corner else 0
 for floor in range(n+1):
  ground=floor==0;z=0 if ground else 4.5+(floor-1)*3.5;fh=4.5 if ground else 3.5
  if corner:
   cid=recipe['corner'] if ground else recipe['corner_upper']
   place(cid,cursor+.5,0,z,(-1,1,1));place(cid,cursor+w-.5,0,z)
  x=start
  for j,span in enumerate(parts):
   variant='AB'[(index+j)%2]
   if span==1:
    aid=recipe['pilaster'] if ground else recipe['pilaster_upper'];place(aid,cursor+x,0,z)
   elif ground:
    aid=f'{fam}_Damaged_G_Bay6' if span==6 and (index+j)%2==0 else f'{fam}_G_Bay{span}{variant}';place(aid,cursor+x,0,z)
    if j%2==0:
     aw=f'{fam}_Awning{6 if span>=6 else 4}';place(aw,cursor+x,0,0)
   else:
    sub=span//2
    for k in range(2):place(f'{fam}_U_Bay{sub}{variant}',cursor+x+k*sub,0,z)
   x+=span
  checks.append({'building':index,'floor':floor,'front_span_m':x+(.5 if corner else 0),'expected_m':w,'gap_m':round(w-x-(.5 if corner else 0),9)})
 if ph>0:
  x=0
  for span in width_parts(w):
   if span in [8,6]:
    for j in range(span//2):place(fam+'_Parapet2',cursor+x+2*j,0,body,(1,1,ph/1.5))
   else:place(fam+'_Parapet2',cursor+x,0,body,(span/2,1,ph/1.5))
   x+=span
 if w>=12:
  crest=assets[recipe['crest'][index%2]];place(crest['id'],cursor+(w-crest['nominal'][0])/2,0,h)
 # The left-end corner has a completed second storefront facing the cross street.
 if corner:
  for y in range(0,36,6):place(fam+'_G_Bay6A',cursor,y+6,0,angle=-math.pi/2)
 else:place(recipe['wall'],cursor,36,0,(9,1,body/3.5),angle=-math.pi/2,tile=True)
 place(recipe['wall'],cursor+w,0,0,(9,1,body/3.5),angle=math.pi/2,tile=True)
 place(recipe['wall'],cursor+w,36,0,(w/4,1,body/3.5),angle=math.pi,tile=True)
 place(recipe['roof'],cursor,0,body,(w/4,9,1),tile=True)
 for aid,xx,yy in [('RF_WaterTank',w*.5,26),('RF_VentMetal',w*.75,9)]:
  if aid in assets:place(aid,cursor+xx,yy,body)
 if w>=14 and 'RF_Billboard8x4' in assets:place('RF_Billboard8x4',cursor+w/2,17,body)
 if index%2==0 and 'DR_BoardedWindow15' in assets:place('DR_BoardedWindow15',cursor+1.2,-.02,1.1)
 if index==3 and 'DR_SandbagStack3' in assets:place('DR_SandbagStack3',cursor+4,-.15,0)
 if index==6 and 'DR_Rubble2' in assets:place('DR_Rubble2',cursor+4,-1.1,0)
 cursor+=w
assert cursor==120 and all(c['gap_m']==0 for c in checks)
def material(name,color):
 mat=bpy.data.materials.new(name);mat.diffuse_color=(*color,1);mat.use_nodes=True;mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(*color,1);return mat
road=material('Proof neutral roadway',(.18,.16,.14));pavement=material('Proof sidewalk',(.5,.45,.36));silhouette=material('1.8 m scale figure',(.07,.08,.09))
def box(name,loc,scale,mat):
 bpy.ops.mesh.primitive_cube_add(size=1,location=loc);o=bpy.context.object;o.name=name;o.scale=scale;o.data.materials.append(mat);return o
box('Street',(60,-11,-.18),(150,14,.2),road);box('Four metre sidewalk',(60,-2,-.08),(150,4,.12),pavement)
actor=Vector((-5,-5,0));box('Scale figure torso',actor+Vector((0,0,1.1)),(.44,.28,.65),silhouette)
for dx in [-.13,.13]:box('Scale figure leg',actor+Vector((dx,0,.43)),(.18,.21,.86),silhouette)
bpy.ops.mesh.primitive_uv_sphere_add(segments=8,ring_count=4,radius=.16,location=actor+Vector((0,0,1.64)));bpy.context.object.data.materials.append(silhouette)
scene.render.engine='CYCLES';scene.cycles.samples=24;scene.render.resolution_x=2400;scene.render.resolution_y=1200;scene.render.resolution_percentage=100;scene.view_settings.view_transform='Standard'
scene.world=bpy.data.worlds.new('Neutral proof');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.55,.55,.55,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.8
bpy.ops.object.light_add(type='SUN',location=(40,-40,60));sun=bpy.context.object;sun.data.energy=2;sun.rotation_euler=(math.radians(25),math.radians(-20),math.radians(-25));sun.data.angle=.25
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam
direction=Vector((1,.25,0)).normalized();cam.location=actor-direction*3+Vector((0,0,1.2+3*math.tan(math.radians(20))))
look=direction*math.cos(math.radians(20))+Vector((0,0,-math.sin(math.radians(20))));cam.rotation_euler=look.to_track_quat('-Z','Y').to_euler();cam.data.lens=10;cam.data.shift_y=.12
scene.render.filepath=str(R/'preview/KitProof_street.png');bpy.ops.render.render(write_still=True)
cam.location=(50,-38,30);target=Vector((60,12,5));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=142;cam.data.shift_y=0
scene.render.filepath=str(R/'preview/KitProof_30m.png');bpy.ops.render.render(write_still=True)
report={'length_m':120,'families':sorted(m['recipes']),'heights_m':[6,9,12,15],'placements':placed,'module_fit_checks':checks,'gap_check_passed':True,'street_camera':{'behind_figure_m':3,'pitch_down_degrees':20,'figure_h_m':1.8},'aerial_camera_z_m':30,'notes':'12/15 m parapet recipe exception documented in README. Neutral proof staging roadway and silhouette are not runtime assets.'}
(R/'src/kit_proof_audit.json').write_text(json.dumps(report,indent=2)+'\n')
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/kit_proof.blend'))
