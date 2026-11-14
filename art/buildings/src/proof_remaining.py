import bpy,bmesh
from pathlib import Path
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.unit_settings.system='METRIC';bpy.context.scene.unit_settings.scale_length=1
def finish(name,verts,faces):
 m=bpy.data.meshes.new(name);m.from_pydata(verts,[],faces);m.update()
 o=bpy.data.objects.new(name,m);bpy.context.collection.objects.link(o)
 bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
 mat=bpy.data.materials.new('M_'+name);mat.use_nodes=True;m.materials.append(mat)
 bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT');bpy.ops.mesh.normals_make_consistent(inside=False) if hasattr(bpy.ops.mesh,'normals_make_consistent') else None
 bpy.ops.uv.smart_project(island_margin=.008);bpy.ops.object.mode_set(mode='OBJECT')
 bm=bmesh.new();bm.from_mesh(m);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(m);bm.free()
 bpy.ops.export_scene.gltf(filepath=str(R/'work'/f'{name}_input.glb'),export_format='GLB',use_selection=True)
 bpy.ops.wm.save_as_mainfile(filepath=str(R/'src'/f'{name}_work.blend'))
 return o
# Upper bay profile; sill remains painted to avoid tiny geometry.
v=[(x,y,z) for z,y in [(0,0),(3,0),(3.25,-.125),(3.5,-.125)] for x in [0,3]]
o=finish('BRK_U_Bay3A',v,[(i,i+1,i+3,i+2) for i in [0,2,4]])
o.hide_set(True);o.hide_render=True
# Drugstore continuous perimeter profile, no bottom faces. Cornice contained in footprint.
v=[];f=[]
for z,inset in [(0,0),(5.25,0),(5.5,.125),(5.75,0),(6,0),(6,.4),(5.6,.4)]:
 v.extend([(-9+inset,-18+inset,z),(9-inset,-18+inset,z),(9-inset,18-inset,z),(-9+inset,18-inset,z)])
for ring in range(6):
 for i in range(4):
  a=ring*4+i;b=ring*4+(i+1)%4;f.append((a,b,b+4,a+4))
f.append((24,25,26,27))
# awnings are open-bottom surfaces, entirely within front-only protrusion allowance.
for x0,x1 in [(-7.5,-1.25),(1.25,7.5)]:
 a=len(v);v.extend([(x0,-18,3.8),(x1,-18,3.8),(x1,-19.25,2.9),(x0,-19.25,2.9),(x0,-19.25,2.7),(x1,-19.25,2.7)])
 f.extend([(a,a+1,a+2,a+3),(a+3,a+2,a+5,a+4)])
finish('LM_Drugstore',v,f)
