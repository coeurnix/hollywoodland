"""Front-detail review of actual separately attached sign cards; never saves blend."""
import bpy,json,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];sys.path.insert(0,str(R/'src'))
from landmark_sign_attachments import attach
aid,sign=sys.argv[sys.argv.index('--')+1:]
bpy.ops.wm.open_mainfile(filepath=str(R/f'src/{aid}_kit_textured.blend'))
asset=bpy.data.objects[aid];attach(asset)
card=next(o for o in asset.children if o.name.startswith(sign))
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=16
bpy.context.view_layer.update();pts=[card.matrix_world@Vector(v) for v in card.bound_box]
lo=Vector([min(v[i] for v in pts) for i in range(3)]);hi=Vector([max(v[i] for v in pts) for i in range(3)])
center=(lo+hi)/2;width=hi.x-lo.x;height=hi.z-lo.z
scene.render.resolution_x=1536;scene.render.resolution_y=max(384,min(1536,int(1536*height/width)));scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.render.image_settings.color_mode='RGB';scene.view_settings.view_transform='Standard'
world=bpy.data.worlds.new('Review_FrontDetail');world.use_nodes=True;scene.world=world
bg=world.node_tree.nodes.get('Background');bg.inputs['Color'].default_value=(.55,.55,.55,1);bg.inputs['Strength'].default_value=.8
bpy.ops.object.camera_add(location=center+Vector((0,-60,0)));cam=bpy.context.object;cam.rotation_euler=(center-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=max(width,height)*1.15;scene.camera=cam
bpy.ops.object.light_add(type='AREA',location=center+Vector((0,-30,20)));lamp=bpy.context.object;lamp.data.energy=5000;lamp.data.size=25;lamp.rotation_euler=(center-lamp.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(R/f'work/{aid}_candidate/preview/{sign}_front_detail.png');bpy.ops.render.render(write_still=True)
