"""Post-bake fix, preserving the submitted input and its bake rays.

Import with runpy.run_path(path)['apply'](bpy.data.objects['LM_Pantages'])
AFTER baking the original authored mesh. Clean crown exterior uses existing
painted stone UVs, never Ray-bake onto stacked front UVs again.
Root performs the final export/audit with its usual process after this helper.
"""
import bpy,bmesh
def apply(obj):
 if obj.get('pantages_roof_finished'):return
 bm=bmesh.new();bm.from_mesh(obj.data);layer=bm.loops.layers.uv.active
 # Replace the nested texture-proxy crown plates with their single exterior
 # envelope. This removes covered horizontal/front faces and supplies the back.
 old=[f for f in bm.faces if all(v.co.z>=17.9999 and -12.0001<=v.co.x<=12.0001 and -29.2501<=v.co.y<=-28.6499 for v in f.verts)]
 bmesh.ops.delete(bm,geom=old,context='FACES_ONLY')
 def f(coords):
  face=bm.faces.new([bm.verts.new(v) for v in coords])
  for i,l in enumerate(face.loops):l[layer].uv=[(.025,.74),(.20,.74),(.20,.96),(.025,.96)][i%4]
 outline=[(-12,18),(12,18),(12,19),(9,19),(9,20),(6,20),(6,21),(3,21),(3,22),(-3,22),(-3,21),(-6,21),(-6,20),(-9,20),(-9,19),(-12,19)]
 front=[(x,-29.25,z) for x,z in outline];back=[(x,-28.65,z) for x,z in outline]
 cuts=[-12,-9,-6,-3,3,6,9,12];heights=[19,20,21,22,21,20,19]
 for x0,x1,height in zip(cuts,cuts[1:],heights):
  for z in range(18,height):
   f([(x0,-29.25,z),(x1,-29.25,z),(x1,-29.25,z+1),(x0,-29.25,z+1)])
   f([(x1,-28.65,z),(x0,-28.65,z),(x0,-28.65,z+1),(x1,-28.65,z+1)])
 for i in range(1,len(front)):
  j=(i+1)%len(front);f([front[i],front[j],back[j],back[i]])
 loose=[v for v in bm.verts if not v.link_faces]
 if loose:bmesh.ops.delete(bm,geom=loose,context='VERTS')
 bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=1e-6);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bmesh.ops.triangulate(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free();obj['pantages_roof_finished']=True
