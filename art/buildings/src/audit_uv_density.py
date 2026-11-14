"""Measure texel density from exported FBX triangle UV/world areas."""
import bpy,json,math
from pathlib import Path
from mathutils import Vector
R=Path(__file__).resolve().parents[1]
out={}
for aid in ['BRK_G_Bay6A','BRK_U_Bay3A','LM_Drugstore']:
 bpy.ops.wm.read_factory_settings(use_empty=True)
 bpy.ops.import_scene.fbx(filepath=str(R/'meshes'/f'{aid}.fbx'))
 o=next(o for o in bpy.context.scene.objects if o.type=='MESH');groups={}
 for p in o.data.polygons:
  v=[o.matrix_world@o.data.vertices[o.data.loops[i].vertex_index].co for i in p.loop_indices]
  uv=[Vector(o.data.uv_layers.active.data[i].uv) for i in p.loop_indices]
  area=(v[1]-v[0]).cross(v[2]-v[0]).length*.5
  u,vv=uv[1]-uv[0],uv[2]-uv[0];uvarea=abs(u.x*vv.y-u.y*vv.x)*.5
  n=o.matrix_world.to_3x3()@p.normal
  group='street' if n.y<-.5 else ('roof' if n.z>.5 else 'other')
  if min(x.y for x in v)<-18.1:group='awning'
  groups.setdefault(group,[]).append(math.sqrt(uvarea/area)*2048 if area else 0)
 out[aid]={k:{'min_px_m':round(min(v),3),'max_px_m':round(max(v),3)} for k,v in groups.items()}
(R/'src/uv_density_audit.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out))
