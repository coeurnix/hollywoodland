"""Untextured shape-review renders, intentionally not final phase previews."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];W=R/'work/hub_egyptian'
assets=json.loads((W/'assets.json').read_text())
if (W/'shelter_assets.json').exists():assets+=json.loads((W/'shelter_assets.json').read_text())
if (W/'graumans_assets.json').exists():assets+=json.loads((W/'graumans_assets.json').read_text())
for a in assets:
 bpy.ops.wm.open_mainfile(filepath=str(R/a['source']));ob=bpy.data.objects[a['id']]
 scene=bpy.context.scene;scene.world=bpy.data.worlds.new('ReviewWorld');scene.render.engine='BLENDER_WORKBENCH';scene.render.resolution_x=700;scene.render.resolution_y=500;scene.render.resolution_percentage=100
 scene.display.shading.light='STUDIO';scene.display.shading.studiolight_rotate_z=.5;scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.cavity_type='BOTH';scene.display.shading.color_type='MATERIAL';scene.display.shading.background_type='WORLD';scene.world.color=(.15,.15,.15)
 camera_data=bpy.data.cameras.new('ReviewCamera');cam=bpy.data.objects.new('ReviewCamera',camera_data);scene.collection.objects.link(cam);scene.camera=cam;camera_data.type='ORTHO'
 w,d,h=a['nominal'];target=Vector((0,0,h*.45));span=max(w,d,h)
 for name,direction in [('front34',(1,-1.6,1.15)),('back34',(-1,1.6,1.15)),('top',(0,-1,1))]:
  cam.location=target+Vector(direction).normalized()*span*2;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();camera_data.ortho_scale=span*1.4
  scene.render.filepath=str(W/f"{a['id']}_{name}_clay.png");bpy.ops.render.render(write_still=True)
