"""Curved quarter-grid Spanish parapets; run after authored family build."""
import bpy,bmesh,math
from pathlib import Path
R=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(R/'src/SPN_kit_authored.blend'))
for width in [2,4]:
 obj=bpy.data.objects[f'SPN_Parapet{width}'];mat=obj.data.materials[0]
 # A readable barrel-tile cap is painted later; its silhouette is modelled here.
 points=[(0,0),(width,0)]+[(width-i*.25,1.25+.25*math.sin(math.pi*i*.25/width)) for i in range(int(width*4)+1)]
 mesh=bpy.data.meshes.new(obj.name+'_curved')
 mesh.from_pydata([(x,0,z) for x,z in points],[],[list(range(len(points)))])
 mesh.materials.append(mat);uv=mesh.uv_layers.new(name='UVMap')
 for loop in mesh.loops:
  x,y,z=mesh.vertices[loop.vertex_index].co
  uv.data[loop.index].uv=((248+x/width*800)/2048,(960+z/1.5*300)/2048)
 bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free()
 obj.data=mesh
bpy.ops.file.pack_all();bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/SPN_kit_authored.blend'))
