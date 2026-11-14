"""Render a staged landmark with its exact nominal box and1.8m figure.

Usage: blender --background --python art/buildings/src/render_landmark_boxes.py
       -- LM_Egyptian
Reads src/<id>_kit_textured.blend and work/<id>_kit_assets.json. Writes only
work/<id>_candidate/preview/<id>_box.png. Never saves the loaded source blend.
The1024px review uses a single large view so the human remains readable beside
wide landmarks; both the textured mass and exact box are visible together.
"""
import bpy,json,sys,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[1]
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
if len(args)!=1:raise SystemExit('Usage: blender --background --python src/render_landmark_boxes.py -- <assetID>')
aid=args[0]
if not aid or Path(aid).name!=aid:raise ValueError('Expected one plain asset ID')
source=R/f'src/{aid}_kit_textured.blend';metadata=R/f'work/{aid}_kit_assets.json'
entries=json.loads(metadata.read_text(encoding='utf-8'));entries=entries.get('assets',[]) if isinstance(entries,dict) else entries
entry=next((a for a in entries if a.get('id')==aid),None)
if not entry:raise ValueError('Asset missing from staged metadata: '+aid)
if len(entry.get('nominal',[]))!=3:raise ValueError('Landmark requires nominal [width,depth,height]')
w,d,h=map(float,entry['nominal']);assert min(w,d,h)>0
out=R/f'work/{aid}_candidate/preview';out.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(source))
scene=bpy.context.scene;asset=bpy.data.objects.get(aid)
if asset is None:raise ValueError('Named staged landmark mesh missing: '+aid)
sys.path.insert(0,str(R/'src'))
from landmark_sign_attachments import attach
attach(asset)
visible={asset,*asset.children_recursive}
for ob in scene.objects:
 if ob.type in {'MESH','CURVE','FONT','SURFACE','META','LIGHT'}:ob.hide_render=ob not in visible
for ob in visible:ob.hide_render=False;ob.hide_set(False)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=12
scene.render.resolution_x=1024;scene.render.resolution_y=768;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGB';scene.render.film_transparent=False
scene.view_settings.view_transform='Standard';scene.view_settings.look='Medium High Contrast' if 'Medium High Contrast' in [i.name for i in bpy.types.ColorManagedViewSettings.bl_rna.properties['look'].enum_items] else 'None'
scene.view_settings.exposure=0;scene.view_settings.gamma=1
world=bpy.data.worlds.new('Review_NeutralWorld');world.use_nodes=True;scene.world=world
bg=next(n for n in world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs['Color'].default_value=(.64,.64,.64,1);bg.inputs['Strength'].default_value=.8

def flatmat(name,color):
 mat=bpy.data.materials.new('Review_'+name);mat.use_nodes=True;nodes=mat.node_tree.nodes;nodes.clear();shader=nodes.new('ShaderNodeEmission');shader.inputs['Color'].default_value=(*color,1);shader.inputs['Strength'].default_value=1;output=nodes.new('ShaderNodeOutputMaterial');mat.node_tree.links.new(shader.outputs[0],output.inputs['Surface']);return mat
black=flatmat('Silhouette',(.018,.022,.026));teal=flatmat('NominalBox',(.015,.30,.43));ink=flatmat('Labels',(.025,.035,.045))
span=max(w,d,h)

def mesh_bounds(ob):return [ob.matrix_world@Vector(p) for p in ob.bound_box]
fit=[]
for ob in visible:
 if ob.type=='MESH':fit+=mesh_bounds(ob)
if not fit:raise ValueError('Staged asset has no visible mesh geometry')

# Wire centre-lines lie on the EXACT nominal box, not on measured protrusions.
corners=[Vector((x,y,z)) for z in [0,h] for y in [-d/2,d/2] for x in [-w/2,w/2]]
edges=[(0,1),(1,3),(3,2),(2,0),(4,5),(5,7),(7,6),(6,4),(0,4),(1,5),(2,6),(3,7)]
curve=bpy.data.curves.new('Review_ExactNominalBox','CURVE');curve.dimensions='3D';curve.resolution_u=1;curve.bevel_depth=max(.018,min(.065,span*.0008));curve.bevel_resolution=0;curve.resolution_u=1
for a,b in edges:
 line=curve.splines.new('POLY');line.points.add(1)
 for point,v in zip(line.points,[corners[a],corners[b]]):point.co=(*v,1)
wire=bpy.data.objects.new('Review_ExactNominalBox',curve);scene.collection.objects.link(wire);curve.materials.append(teal);fit+=corners

# Put the human just outside the front-left footprint corner. A real3D figure
# retains its silhouette at the3/4 review angle and is exactly1.8m from sole to crown.
human_base=Vector((-w/2-max(1.4,span*.018),-d/2-max(.9,span*.012),0))
def ellipsoid(name,center,scale):
 bpy.ops.mesh.primitive_uv_sphere_add(segments=12,ring_count=6,radius=1,location=human_base+Vector(center));ob=bpy.context.object;ob.name='Review_Human_'+name;ob.scale=scale;ob.data.materials.append(black);fit.extend(mesh_bounds(ob));return ob
def limb(name,a,b,radius):
 a,b=human_base+Vector(a),human_base+Vector(b);vec=b-a;bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=radius,depth=vec.length,location=(a+b)/2);ob=bpy.context.object;ob.name='Review_Human_'+name;ob.rotation_euler=vec.to_track_quat('Z','Y').to_euler();ob.data.materials.append(black);fit.extend(mesh_bounds(ob));return ob
ellipsoid('Head',(0,0,1.655),(.135,.12,.145))
ellipsoid('Torso',(0,0,1.20),(.205,.12,.30));ellipsoid('Hips',(0,0,.88),(.15,.105,.15))
for side in [-1,1]:
 limb('Leg',(side*.09,0,.86),(side*.115,0,.08),.065)
 ellipsoid('Foot',(side*.115,-.055,.055),(.075,.135,.055))
 limb('Arm',(side*.18,0,1.40),(side*.27,-.015,.94),.047)
human_top=human_base+Vector((0,0,1.8))

# Flat neutral floor and broad soft lights anchor both objects without changing
# any staged mesh, material or source-file state on disk.
bpy.ops.mesh.primitive_plane_add(size=max(w,d)*4,location=(0,0,-.012));floor=bpy.context.object;floor.name='Review_NeutralFloor'
mat=bpy.data.materials.new('Review_Floor');mat.use_nodes=True;bs=mat.node_tree.nodes.get('Principled BSDF');bs.inputs['Base Color'].default_value=(.68,.68,.68,1);bs.inputs['Roughness'].default_value=1;floor.data.materials.append(mat)
for vec in [(1,-1,1.7),(-1,.7,1.2)]:
 bpy.ops.object.light_add(type='AREA',location=Vector(vec)*span);light=bpy.context.object;light.name='Review_Softbox';light.data.energy=span*span*6;light.data.shape='DISK';light.data.size=span*1.6;target=Vector((0,0,h*.35));light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()

# Fit actual mesh bounds (including high signs/props), nominal box AND human.
# Project into an orthographic camera basis; reserve room for review labels.
view=Vector((1,-1.65,.95)).normalized();rotation=(-view).to_track_quat('-Z','Y');right=rotation@Vector((1,0,0));up=rotation@Vector((0,1,0))
uu=[p.dot(right) for p in fit];vv=[p.dot(up) for p in fit];zz=[p.dot(view) for p in fit]
aspect=1024/768;scale=max((max(uu)-min(uu))/.90,(max(vv)-min(vv))*aspect/.79)
target=right*((min(uu)+max(uu))/2)+up*((min(vv)+max(vv))/2+scale/aspect*.035)+view*((min(zz)+max(zz))/2)
bpy.ops.object.camera_add(location=target+view*span*3);cam=bpy.context.object;cam.name='Review_Camera';cam.rotation_euler=rotation.to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=scale;cam.data.clip_start=.1;cam.data.clip_end=span*12+100;scene.camera=cam

def overlay(name,body,x,y,pixels=15,align='LEFT',color=ink):
 data=bpy.data.curves.new('Review_Label_'+name,'FONT');data.body=body;data.size=scale*pixels/1024/.72;data.align_x=align;ob=bpy.data.objects.new('Review_Label_'+name,data);scene.collection.objects.link(ob);ob.location=cam.location-view*max(2,span*.15)+right*x+up*y;ob.rotation_euler=cam.rotation_euler;data.materials.append(color)
 if hasattr(ob,'visible_shadow'):ob.visible_shadow=False
 return ob
height=scale/aspect
overlay('Title',aid,-scale*.475,height*.435,17)
boxlabel='Nominal box (non-colliding)' if entry.get('non_colliding') else 'Collision box'
overlay('BoxLegend',f'{boxlabel}: {w:g} x {d:g} x {h:g} m',-scale*.475,-height*.465,14,color=teal)
hp=human_top-target;overlay('Human','1.8 m',hp.dot(right),hp.dot(up)+height*.025,13,align='CENTER')
scene.render.filepath=str(out/f'{aid}_box.png');bpy.ops.render.render(write_still=True)
print('LANDMARK BOX REVIEW',scene.render.filepath,flush=True)
