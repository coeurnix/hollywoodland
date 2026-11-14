import bpy,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];aid=sys.argv[sys.argv.index('--')+1]
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{aid}_kit_textured.blend'))
s=bpy.context.scene;s.render.engine='CYCLES';s.cycles.samples=12;s.render.resolution_x=1200;s.render.resolution_y=800;s.render.resolution_percentage=100;s.view_settings.view_transform='Standard'
s.world=bpy.data.worlds.new('Detail neutral');s.world.use_nodes=True;bg=next(n for n in s.world.node_tree.nodes if n.type=='BACKGROUND');bg.inputs['Color'].default_value=(.6,.6,.6,1);bg.inputs['Strength'].default_value=.8
bpy.ops.object.camera_add();c=bpy.context.object;s.camera=c;c.data.type='ORTHO';c.data.ortho_scale=30
front=-26 if aid.endswith('_Wing') else -13
center=Vector((0,front,6.5));c.location=center+Vector((6,-22,11));c.rotation_euler=(center-c.location).to_track_quat('-Z','Y').to_euler()
s.render.filepath=str(R/f'work/{aid}_candidate/preview/{aid}_detail.png');bpy.ops.render.render(write_still=True)
