"""Render authored hotel geometry for front/back/roof inspection (not final textures)."""
import bpy,math,sys
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[2];SRC=R/'src/hub_hotels';OUT=R/'work/hub_hotels/preview';OUT.mkdir(exist_ok=True)
ids=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else ['LM_Roosevelt','LM_HollywoodHotel','LM_HollywoodHotel_Wing','LM_HollywoodHotel_CourtWall']
for aid in ids:
 bpy.ops.wm.open_mainfile(filepath=str(SRC/f'{aid}.blend'));ob=bpy.data.objects[aid];sc=bpy.context.scene
 sc.render.engine='BLENDER_WORKBENCH';sc.render.resolution_x=960;sc.render.resolution_y=640;sc.render.resolution_percentage=100
 sh=sc.display.shading;sh.light='STUDIO';sh.color_type='VERTEX';sh.show_shadows=True;sh.show_cavity=True;sh.cavity_type='BOTH';sh.curvature_ridge_factor=1.4;sh.curvature_valley_factor=1.2;sh.background_type='WORLD'
 sc.world=bpy.data.worlds.new('ReviewNeutral');sc.world.color=(.11,.105,.095);sc.view_settings.view_transform='Standard';sc.render.film_transparent=False
 lo=Vector([min(v.co[i] for v in ob.data.vertices) for i in range(3)]);hi=Vector([max(v.co[i] for v in ob.data.vertices) for i in range(3)]);target=(lo+hi)/2;extent=max(hi-lo)
 for suffix,vec in [('front34',(.65,-1,.50)),('back34',(-.65,1,.50)),('roof45',(.35,-.6,1.0))]:
  data=bpy.data.cameras.new('ReviewCam');cam=bpy.data.objects.new('ReviewCam',data);sc.collection.objects.link(cam);cam.location=target+Vector(vec).normalized()*extent*2;cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();data.type='ORTHO';data.ortho_scale=extent*(1.6 if aid=='LM_Roosevelt' else 1.2);data.clip_end=2000;sc.camera=cam
  sc.render.filepath=str(OUT/f'{aid}_{suffix}.png');bpy.ops.render.render(write_still=True);bpy.data.objects.remove(cam,do_unlink=True)
