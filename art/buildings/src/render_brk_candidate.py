"""Render staged authored meshes, never a stand-in for the actual exported delivery."""
import bpy,json,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[1];FAM=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'BRK'
OUT=R/f'work/{FAM}_candidate/preview';OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{FAM}_kit_textured.blend'))
entries=json.loads((R/f'work/{FAM}_kit_assets.json').read_text())
if entries and entries[0].get('kind')=='landmark':
 sys.path.insert(0,str(R/'src'))
 from landmark_sign_attachments import attach
 for a in entries:attach(bpy.data.objects[a['id']])
if '--' in sys.argv:
 selected=sys.argv[sys.argv.index('--')+2:]
 if selected:entries=[a for a in entries if a['id'] in selected]
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=8
scene.render.resolution_x=384;scene.render.resolution_y=384;scene.render.resolution_percentage=100
scene.view_settings.view_transform='Standard'
scene.world=bpy.data.worlds.new('Neutral review');scene.world.use_nodes=True
bg=next(n for n in scene.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs['Color'].default_value=(.55,.55,.55,1);bg.inputs['Strength'].default_value=.8
for o in scene.objects:
 if o.type=='MESH':o.hide_render=True
bpy.ops.object.camera_add();cam=bpy.context.object;scene.camera=cam;cam.data.type='ORTHO'
for loc in [(15,-20,25),(-15,20,25)]:
 bpy.ops.object.light_add(type='AREA',location=loc);light=bpy.context.object;light.data.energy=2500;light.data.size=20;light.rotation_euler=(-light.location).to_track_quat('-Z','Y').to_euler()
for a in entries:
 o=bpy.data.objects[a['id']];o.hide_set(False);o.hide_render=False
 for child in o.children_recursive:child.hide_render=False
 lo,hi=map(Vector,a['bounds']);center=(lo+hi)/2;span=max(hi-lo)
 for view,vec in [('front',(1,-2,1)),('back',(-1,2,1)),('top',(1,-1,1.414))]:
  cam.location=center+Vector(vec).normalized()*span*2;cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=span*1.25
  scene.render.filepath=str(OUT/f"{a['id']}_{view}.png");bpy.ops.render.render(write_still=True)
 o.hide_render=True
 for child in o.children_recursive:child.hide_render=True
print('Candidate review renders complete')
