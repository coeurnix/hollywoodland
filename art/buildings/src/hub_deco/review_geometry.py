"""Neutral geometry review of authored sources; final textured review is separate."""
import bpy,json,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2]
ids=sys.argv[sys.argv.index('--')+1:]
for aid in ids:
 bpy.ops.wm.open_mainfile(filepath=str(R/f'src/hub_deco/{aid}.blend'))
 o=bpy.data.objects[aid];s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=8
 s.render.resolution_x=640;s.render.resolution_y=640;s.render.resolution_percentage=100
 s.world=bpy.data.worlds.new('ReviewNeutral');s.world.use_nodes=True
 bg=s.world.node_tree.nodes.get('Background');bg.inputs['Color'].default_value=(.45,.45,.45,1);bg.inputs['Strength'].default_value=.7
 s.view_settings.view_transform='Standard'
 for loc in [(-25,-40,70),(40,25,55)]:
  bpy.ops.object.light_add(type='AREA',location=loc);l=bpy.context.object;l.data.energy=10000;l.data.size=30;l.rotation_euler=(-l.location).to_track_quat('-Z','Y').to_euler()
 bpy.ops.object.camera_add();c=bpy.context.object;s.camera=c;c.data.type='ORTHO'
 vv=[o.matrix_world@v.co for v in o.data.vertices];lo=Vector([min(v[k] for v in vv) for k in range(3)]);hi=Vector([max(v[k] for v in vv) for k in range(3)]);center=(lo+hi)/2;span=max(hi-lo);c.data.ortho_scale=span*1.6
 out=R/f'work/hub_deco/{aid}_review';out.mkdir(exist_ok=True)
 for view,vec in [('front',(-1,-1.5,.85)),('back',(1,1.5,.85)),('top',(-1,-1,2))]:
  c.location=center+Vector(vec).normalized()*span*2;c.rotation_euler=(center-c.location).to_track_quat('-Z','Y').to_euler();s.render.filepath=str(out/f'{view}.png');bpy.ops.render.render(write_still=True)
 print('REVIEWED_RENDER',aid)
