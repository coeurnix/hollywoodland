"""Blender paint cleanup and contact-shape refinement, preserving authored UVs."""
import bpy,bmesh,json,sys,numpy as np
from pathlib import Path
R=Path(__file__).resolve().parents[1];stage=R/'work/NEUTRAL_candidate'
bpy.ops.wm.open_mainfile(filepath=str(R/'src/NEUTRAL_kit_textured.blend'))
entries=json.loads((R/'work/NEUTRAL_kit_assets.json').read_text())
if not bpy.context.scene.get('neutral_paint_finished'):
 im=next(n.image for n in bpy.data.materials['M_ATL_NEUTRAL'].node_tree.nodes if n.type=='TEX_IMAGE')
 if im.packed_file:im.unpack(method='REMOVE')
 pixels=np.empty(2048*2048*4,dtype=np.float32);im.pixels.foreach_get(pixels);pixels=pixels.reshape(2048,2048,4)
 for aid in ['DR_SootA','DR_SootB','DR_Crack']:
  obj=bpy.data.objects[aid];mask=np.zeros((2048,2048),dtype=bool);uv=obj.data.uv_layers.active.data
  for poly in obj.data.polygons:
   pts=np.array([uv[i].uv[:]*np.array([2048,2048]) for i in poly.loop_indices]);lo=np.maximum(0,np.floor(pts.min(axis=0)).astype(int));hi=np.minimum(2047,np.ceil(pts.max(axis=0)).astype(int))
   yy,xx=np.mgrid[lo[1]:hi[1]+1,lo[0]:hi[0]+1];signs=[]
   for j in range(3):
    a,b=pts[j],pts[(j+1)%3];signs.append((b[0]-a[0])*(yy+.5-a[1])-(b[1]-a[1])*(xx+.5-a[0]))
   inside=(np.minimum.reduce(signs)>=-.001)|(np.maximum.reduce(signs)<=.001);mask[lo[1]:hi[1]+1,lo[0]:hi[0]+1]|=inside
  for _ in range(3):
   pad=np.pad(mask,1);mask=np.logical_or.reduce([pad[y:y+2048,x:x+2048] for y,x in [(0,1),(1,0),(1,1),(1,2),(2,1)]])
  old=pixels[mask,:3];lum=old.mean(axis=1)
  # Keep the generated/Rodin brush variation, grade all bright exposed wall paint to charcoal.
  value=.012+.028*np.clip(lum,0,1) if 'Soot' in aid else .018+.045*np.clip(lum,0,1)
  pixels[mask,:3]=value[:,None]*np.array([1,.82,.66])[None,:]
  next(a for a in entries if a['id']==aid)['notes']+=' Rodin-painted colour graded in Blender to charcoal/sepia, removing bright surrounding masonry from the cutout.'
 im.pixels.foreach_set(pixels.ravel());im.update();im.filepath_raw=str(stage/'textures/T_ATL_NEUTRAL.png');im.file_format='PNG';im.save();im.pack()
 for aid in ['DR_SandbagStack3','DR_SandbagDoorway4']:
  obj=bpy.data.objects[aid];bm=bmesh.new();bm.from_mesh(obj.data);unseen=set(bm.verts)
  while unseen:
   first=unseen.pop();todo=[first];component={first}
   while todo:
    v=todo.pop()
    for edge in v.link_edges:
     other=edge.other_vert(v)
     if other in unseen:unseen.remove(other);component.add(other);todo.append(other)
   if len(component)!=24:continue
   lo=min(v.co.z for v in component);hi=max(v.co.z for v in component)
   if hi-lo>.5:continue
   cx=sum(v.co.x for v in component)/24;cy=sum(v.co.y for v in component)/24
   for v in component:
    if abs(v.co.z-lo)<.00001 or abs(v.co.z-hi)<.00001:
     v.co.x=cx+(v.co.x-cx)*(.95/.72);v.co.y=cy+(v.co.y-cy)*(.95/.72)
  bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(obj.data);bm.free()
  next(a for a in entries if a['id']==aid)['notes']+=' Broadened authored bag contact rings remove conspicuous gaps between courses; UVs and painted fabric retained.'
 next(a for a in entries if a['id']=='DR_MayorNotice')['notes']+=' Sixfold UV-density priority preserves the correctly generated civic lettering.'
 bpy.context.scene['neutral_paint_finished']=True
 (R/'work/NEUTRAL_kit_assets.json').write_text(json.dumps(entries,indent=2)+'\n')
 bpy.ops.wm.save_as_mainfile(filepath=str(R/'src/NEUTRAL_kit_textured.blend'))
